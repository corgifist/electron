#include "editor_core.h"
#include "app.h"
#include "utils/AudioFile.h"
#define CSTR(x) ((x).c_str())

using namespace Electron;
using namespace glm;

#define TIMESHIFT(layer) (JSON_AS_TYPE(layer->properties["InternalTimeShift"], float))

extern "C" {

    ELECTRON_EXPORT std::string LayerName = "Audio";
    ELECTRON_EXPORT glm::vec4 LayerTimelineColor = {
        0.831, 0.569, 0.286, 1
    };

    ELECTRON_EXPORT void LayerInitialize(RenderLayer* owner) {
        owner->properties["Volume"] = {
            GeneralizedPropertyType::Float,
            {0, 100}
        };
        owner->properties["Panning"] = {
            GeneralizedPropertyType::Float,
            {0, 0}
        };
        owner->properties["AudioID"] = "";
        owner->properties["OverrideAudioPath"] = "";
        owner->internalData["FFMpegOperationID"] = -1;
        owner->internalData["TimelineSliders"] = {
            {"Volume", 0.0f, 100.0f},
            {"Panning", -0.5f, 0.5f}
        };
        owner->properties["PropertyAlias"] = {
            {"Volume", ICON_FA_VOLUME_HIGH " Volume"},
            {"Panning", ICON_FA_RIGHT_LEFT " Panning"},
            {"Audio", ICON_FA_FILE_AUDIO " Audio"}
        };
        owner->properties["PreviewAudioChannel"] = 0;
        owner->internalData["LoadID"] = -1;
        owner->internalData["PreviousInBounds"] = false;
        owner->internalData["AudioBufferPtr"] = 0;

        owner->anyData.resize(3);
        owner->anyData[0] = -1; // audio handle
        owner->anyData[1] = false; // playing
        owner->anyData[2] = (AsyncFFMpegOperation*) nullptr;
    }


    void ReloadAudioHandle(RenderLayer* layer, TextureUnion* asset, AudioFile<float>* buffer) {
        if (asset == nullptr || asset->type != TextureUnionType::Audio) return;
        if (std::any_cast<int>(layer->anyData[0]) != -1) {
            Servers::AudioServerRequest({
                {"action", "stop_sample"},
                {"handle", std::any_cast<int>(layer->anyData[0])}
            });
        }
        std::string audioPath = asset->path;
        if (JSON_AS_TYPE(layer->properties["OverrideAudioPath"], std::string) != "") {
            audioPath = JSON_AS_TYPE(layer->properties["OverrideAudioPath"], std::string);
        }
        layer->internalData["LoadID"] = JSON_AS_TYPE(Servers::AudioServerRequest({
                {"action", "load_sample"},
                {"override", true},
                {"path", audioPath},
                {"buffer_ptr", (unsigned long) buffer},
                {"fake_name", true},
                {"dispose_after_loading", buffer != nullptr}
        })["id"], int);
    }

    void AudioLayerProcessAsset(RenderLayer* layer, TextureUnion* asset) {
        if (asset == nullptr) return;
        json_t& volume = layer->properties["Volume"];
        std::string audioPath= asset->path;
        layer->properties["OverrideAudioPath"] = audioPath;

        ReloadAudioHandle(layer, asset, nullptr);
    }

    ELECTRON_EXPORT void LayerRender(RenderLayer* owner) {
        bool playing = std::any_cast<bool>(owner->anyData[1]);
        std::string audioID = JSON_AS_TYPE(owner->properties["AudioID"], std::string);
        TextureUnion* asset = AssetCore::GetAsset(audioID);
        if (asset != nullptr)
            owner->endFrame = std::clamp(owner->endFrame, owner->beginFrame, owner->beginFrame + std::get<AudioMetadata>(asset->as).audioLength * GraphicsCore::renderFramerate);
        if (std::any_cast<int>(owner->anyData[0]) != -1) {
            int handle = std::any_cast<int>(owner->anyData[0]);
            float gain = JSON_AS_TYPE(owner->InterpolateProperty(owner->properties["Volume"]).at(0), float);
            float pan = JSON_AS_TYPE(owner->InterpolateProperty(owner->properties["Panning"]).at(0), float);
            Servers::AudioServerRequest({
                {"action", "gain_sample"},
                {"handle", handle},
                {"gain", gain / 100.0f}
            });
            Servers::AudioServerRequest({
                {"action", "pan_sample"},
                {"handle", handle},
                {"pan", pan}
            });
        }
    }

    ELECTRON_EXPORT void LayerPropertiesRender(RenderLayer* layer) {
        json_t previousAudioID = layer->properties["AudioID"];
        json_t& audioID = layer->properties["AudioID"];
        layer->RenderAssetProperty(audioID, "Audio", TextureUnionType::Audio);
        std::string audioSID = JSON_AS_TYPE(layer->properties["AudioID"], std::string);
        TextureUnion* asset = AssetCore::GetAsset(audioID);

        if (audioSID != "" && asset == nullptr) {
            layer->properties["AudioID"] = "";
            layer->anyData[0] = -1;
        }
        if (JSON_AS_TYPE(previousAudioID, std::string) != JSON_AS_TYPE(audioID, std::string)) {
            if (asset != nullptr) {
                ReloadAudioHandle(layer, asset, nullptr);
                AudioMetadata metadata = std::get<AudioMetadata>(asset->as);
                layer->endFrame = layer->beginFrame + (metadata.audioLength * GraphicsCore::renderFramerate);
            }
        }

        json_t& volume = layer->properties["Volume"];
        layer->RenderProperty(GeneralizedPropertyType::Float, volume, "Volume");

        json_t& panning = layer->properties["Panning"];
        layer->RenderProperty(GeneralizedPropertyType::Float, panning, "Panning");

        AudioFile<float>* audioBuffer = (AudioFile<float>*) JSON_AS_TYPE(layer->internalData["AudioBufferPtr"], unsigned long);
        int previewAudioChannel = JSON_AS_TYPE(layer->properties["PreviewAudioChannel"], int);
        int nextAudioChannel = previewAudioChannel;
        if (asset != nullptr && audioBuffer != nullptr) {
            AudioMetadata metadata = std::get<AudioMetadata>(asset->as);
            nextAudioChannel = previewAudioChannel + 1;
            if (nextAudioChannel >= audioBuffer->getNumChannels()) {
                nextAudioChannel = 0;
            }
            ImGui::Separator();
            int precision = 100;
            std::vector<float> waveform(precision);
            double elapsedTime = (double) (GraphicsCore::renderFrame - layer->beginFrame + TIMESHIFT(layer)) / GraphicsCore::renderFramerate;
            uint64_t beginSample = metadata.sampleRate * elapsedTime;
            uint64_t endSample = beginSample + metadata.sampleRate;
            uint64_t sampleStep = metadata.sampleRate / precision;
            endSample = glm::clamp(endSample, (uint64_t) 0, (uint64_t) metadata.audioLength * (uint64_t) metadata.sampleRate);
            int averageIndex = 0;
            for (uint64_t sample = beginSample; sample < endSample; sample += sampleStep) {
                float averageSample = 0;
                for (uint64_t subSample = sample; subSample < sample + sampleStep; subSample++) {
                    averageSample = (averageSample + audioBuffer->samples[previewAudioChannel][glm::clamp(
                        (int) subSample, 0, 
                        (int) metadata.audioLength * metadata.sampleRate)]) / 2.0f;
                }
                waveform[averageIndex] = averageSample;
                averageIndex++;
            }
            ImGui::Spacing();
            ImGui::PlotLines("##audioWaveform", waveform.data(), precision, 0, string_format("%s %s", ICON_FA_AUDIO_DESCRIPTION, metadata.codecName.c_str()).c_str(), -0.5f, 0.5f, ImVec2(ImGui::GetContentRegionAvail().x, 80));
            if (AppCore::ButtonCenteredOnLine(string_format("%s %s > %i", ICON_FA_FILE_WAVEFORM, ELECTRON_GET_LOCALIZATION("NEXT_AUDIO_CHANNEL"), nextAudioChannel).c_str())) {
                previewAudioChannel = nextAudioChannel;
            }
            layer->properties["PreviewAudioChannel"] = previewAudioChannel;
            ImGui::Spacing();
        }
    }

    ELECTRON_EXPORT void LayerSortKeyframes(RenderLayer* layer) {
        json_t& volume = layer->properties["Volume"];
        layer->SortKeyframes(volume);
        if (std::any_cast<int>(layer->anyData[0]) != -1) {
            Servers::AudioServerRequest({
                {"action", "pause_sample"},
                {"handle", std::any_cast<int>(layer->anyData[0])},
                {"pause", !(GraphicsCore::isPlaying && IsInBounds(GraphicsCore::renderFrame, layer->beginFrame, layer->endFrame) && layer->visible)}
            });
        }        
        bool inBounds = IsInBounds(GraphicsCore::renderFrame, layer->beginFrame, layer->endFrame);
        if (JSON_AS_TYPE(layer->internalData["PreviousInBounds"], bool) != inBounds) {
            if (std::any_cast<int>(layer->anyData[0]) != -1) {
                double elapsedTime = (double) (GraphicsCore::renderFrame - layer->beginFrame + TIMESHIFT(layer)) / GraphicsCore::renderFramerate;
                Servers::AudioServerRequest({
                    {"action", "seek_sample"},
                    {"handle", std::any_cast<int>(layer->anyData[0])},
                    {"seek", elapsedTime}
                });
            }
        }

        layer->internalData["PreviousInBounds"] = inBounds;

        int loadID = JSON_AS_TYPE(layer->internalData["LoadID"], int);
        if (loadID != -1) {
            bool loaded = JSON_AS_TYPE(
                Servers::AudioServerRequest({
                    {"action", "load_status"},
                    {"id", loadID}
                })["status"], bool
            );
            layer->properties["ShowLoadingSpinner"] = true;
            TextureUnion* asset = AssetCore::GetAsset(JSON_AS_TYPE(layer->properties["AudioID"], std::string));
            if (loaded) {
                std::string audioPath = asset->path;
                if (JSON_AS_TYPE(layer->properties["OverrideAudioPath"], std::string) != "") {
                    audioPath = JSON_AS_TYPE(layer->properties["OverrideAudioPath"], std::string);
                }
                double elapsedTime = (double) (GraphicsCore::renderFrame - layer->beginFrame + TIMESHIFT(layer)) / GraphicsCore::renderFramerate;
                elapsedTime = std::min(elapsedTime, (double) std::get<AudioMetadata>(asset->as).audioLength);
                layer->anyData[0] = JSON_AS_TYPE(
                    Servers::AudioServerRequest({
                        {"action", "play_sample"},
                        {"path", audioPath}
                    })["handle"], int
                );
                layer->internalData["AudioBufferPtr"] = JSON_AS_TYPE(
                    Servers::AudioServerRequest({
                        {"action", "audio_buffer_ptr"},
                        {"path", audioPath}
                    })["ptr"], unsigned long
                );
                Servers::AudioServerRequest({
                    {"action", "seek_sample"},
                    {"handle", std::any_cast<int>(layer->anyData[0])},
                    {"seek", elapsedTime}
                });
                layer->internalData["LoadID"] = -1;
            }
        } else layer->properties["ShowLoadingSpinner"] = false;
    }

    ELECTRON_EXPORT void LayerDestroy(RenderLayer* layer) {
    }

    ELECTRON_EXPORT void LayerOnPropertiesChange(RenderLayer* layer) {
    }

    ELECTRON_EXPORT void LayerOnPlaybackChange(RenderLayer* layer) {
    }

    ELECTRON_EXPORT void LayerOnTimelineSeek(RenderLayer* layer) {
        if (std::any_cast<int>(layer->anyData[0]) != -1) {
            Servers::AudioServerRequest({
                {"action", "seek_sample"},
                {"handle", std::any_cast<int>(layer->anyData[0])},
                {"seek", std::max(0.0, ((double) (GraphicsCore::renderFrame - layer->beginFrame + TIMESHIFT(layer))) / Shared::maxFramerate)}
            });
        }
    }

    ELECTRON_EXPORT json_t LayerGetPreviewProperties(RenderLayer* layer) {
        return {
            "Volume", "Panning"
        };
    }
}

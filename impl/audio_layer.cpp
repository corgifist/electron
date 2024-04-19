#include "editor_core.h"
#include "app.h"
#include "utils/AudioFile.h"
#define CSTR(x) ((x).c_str())

using namespace Electron;
using namespace glm;

#define TIMESHIFT(layer) (JSON_AS_TYPE(layer->properties["InternalTimeShift"], float))

#define BASS_BOOST_MIN_VALUE -0.5f
#define BASS_BOOST_MAX_VALUE 0.0f

#define AVERAGE_WAVEFORM_PRECISION 50

extern "C" {

    struct AudioLayerSyncStructure {
        float volume;
        float panning;
        float bass;
    };

    struct AudioLayerUserData {
        int64_t audioHandle;
        std::vector<float> averageWaveforms;
        ManagedAssetDecoder previewDecoder;
        AudioLayerSyncStructure sync;
        std::thread* audioWaveformThread;
        bool terminateWaveformThread;

        AudioLayerUserData() {
        }
    };

    void audio_layer_data_callback(float* pOutputF32, int framesCount, void* pUserData) {
        AudioLayerSyncStructure* sync = (AudioLayerSyncStructure*) pUserData;
        float bass = sync->bass;
        float volume = sync->volume;
        float pan = sync->panning;
        bool isRightChannel = false;
        for (int i = 0; i < framesCount * 2; i++) {
            if (pOutputF32[i] < 0) pOutputF32[i] *= bass;

            pOutputF32[i] *= volume;

            float panValue = isRightChannel ? 0.5f : -0.5f;
            pOutputF32[i] *= std::clamp((pan + panValue) / panValue, 0.0f, 1.0f);
            isRightChannel = !isRightChannel;
        }
    }

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
        owner->properties["BassBoost"] = {
            GeneralizedPropertyType::Float,
            {0, 100}
        };
        owner->properties["AudioID"] = "";
        owner->internalData["TimelineSliders"] = {
            {"Volume", 0.0f, 100.0f},
            {"Panning", -0.5f, 0.5f},
            {"BassBoost", 0.0f, 200.0f}
        };
        owner->properties["PropertyAlias"] = {
            {"Volume", ICON_FA_VOLUME_HIGH " Volume"},
            {"Panning", ICON_FA_RIGHT_LEFT " Panning"},
            {"Audio", ICON_FA_FILE_AUDIO " Audio"},
            {"BassBoost", ICON_FA_WAVE_SQUARE " Bass"}
        };
        owner->internalData["PreviewAudioChannel"] = 0;
        owner->internalData["LoadID"] = -1;
        owner->internalData["PreviousInBounds"] = false;
        owner->internalData["AudioBufferPtr"] = 0;
        owner->internalData["PreviousIsVisible"] = true;
        owner->internalData["PreviousBounds"] = {owner->beginFrame, owner->endFrame};

        owner->userData = new AudioLayerUserData();
        AudioLayerUserData* userData = (AudioLayerUserData*) owner->userData;
        userData->audioHandle = -1;
        userData->audioWaveformThread = nullptr;
        userData->terminateWaveformThread = false;
    }


    void ReloadAudioHandle(RenderLayer* layer, TextureUnion* asset, AudioFile<float>* buffer) {
        AudioLayerUserData* userData = (AudioLayerUserData*) layer->userData;
        if (asset == nullptr || asset->type != TextureUnionType::Audio) return;
    }

    void AudioLayerProcessAsset(RenderLayer* layer, TextureUnion* asset) {
        AudioLayerUserData* userData = (AudioLayerUserData*) layer->userData;
    }

    void UpdateSyncStructure(RenderLayer* owner, AudioLayerSyncStructure* sync) {
        sync->volume = JSON_AS_TYPE(owner->InterpolateProperty(owner->properties["Volume"])[0], float) / 100.0f;
        sync->panning = JSON_AS_TYPE(owner->InterpolateProperty(owner->properties["Panning"])[0], float);
        sync->bass = JSON_AS_TYPE(owner->InterpolateProperty(owner->properties["BassBoost"])[0], float) / 100.0f;
    }

    ELECTRON_EXPORT void LayerRender(RenderLayer* owner) {
        AudioLayerUserData* userData = (AudioLayerUserData*) owner->userData;
        std::string audioID = JSON_AS_TYPE(owner->properties["AudioID"], std::string);
        TextureUnion* asset = AssetCore::GetAsset(audioID);
        if (asset != nullptr)
            owner->endFrame = std::clamp(owner->endFrame, owner->beginFrame, owner->beginFrame + std::get<AudioMetadata>(asset->as).audioLength * GraphicsCore::renderFramerate);
        UpdateSyncStructure(owner, &userData->sync);
    }

    ELECTRON_EXPORT void LayerPropertiesRender(RenderLayer* layer) {
        AudioLayerUserData* userData = (AudioLayerUserData*) layer->userData;
        std::string previousAudioID = layer->properties["AudioID"];
        json_t& audioID = layer->properties["AudioID"];
        std::string audioSID = layer->properties["AudioID"];
        TextureUnion* asset = AssetCore::GetAsset(audioID);
        userData->previewDecoder.Update(asset);
        if (userData->previewDecoder.previewHandle) 
            layer->RenderAssetProperty(audioID, "Audio", TextureUnionType::Audio, userData->previewDecoder.previewHandle);

        json_t& volume = layer->properties["Volume"];
        layer->RenderProperty(GeneralizedPropertyType::Float, volume, "Volume");

        json_t& panning = layer->properties["Panning"];
        layer->RenderProperty(GeneralizedPropertyType::Float, panning, "Panning");

        json_t& bass = layer->properties["BassBoost"];
        layer->RenderProperty(GeneralizedPropertyType::Float, bass, "BassBoost");

        if (userData->averageWaveforms.size() > 0) {
            AudioMetadata metadata = std::get<AudioMetadata>(asset->as);
            ImGui::Separator();
            int precision = AVERAGE_WAVEFORM_PRECISION;
            static std::vector<float> waveform(AVERAGE_WAVEFORM_PRECISION);
            uint64_t waveformBeginIndex = ((GraphicsCore::renderFrame - layer->beginFrame + TIMESHIFT(layer)) / GraphicsCore::renderFramerate) * AVERAGE_WAVEFORM_PRECISION;
            for (int i = 0; i < AVERAGE_WAVEFORM_PRECISION; i++) {
                waveform[i] = userData->averageWaveforms[glm::min(userData->averageWaveforms.size() - 1, waveformBeginIndex + i)];
            }

            ImGui::Spacing();
            ImGui::PlotLines("##audioWaveform", waveform.data(), precision, 0, string_format("%s %s", ICON_FA_AUDIO_DESCRIPTION, metadata.codecName.c_str()).c_str(), -0.5f, 0.5f, ImVec2(ImGui::GetContentRegionAvail().x, 80));
            ImGui::Spacing();
        }
    }

    ELECTRON_EXPORT void LayerSortKeyframes(RenderLayer* layer) {
        AudioLayerUserData* userData = (AudioLayerUserData*) layer->userData;

        json_t& volume = layer->properties["Volume"];
        layer->SortKeyframes(volume);

        json_t& panning = layer->properties["Panning"];
        layer->SortKeyframes(panning);
        
        json_t& bass = layer->properties["BassBoost"];
        layer->SortKeyframes(bass);

        if (userData->audioHandle != -1) {
            Servers::AudioServerRequest({
                {"action", "pause_sample"},
                {"handle", userData->audioHandle},
                {"pause", !(GraphicsCore::isPlaying && IsInBounds(GraphicsCore::renderFrame, layer->beginFrame, layer->endFrame) && layer->visible)}
            });
        }        
        bool inBounds = IsInBounds(GraphicsCore::renderFrame, layer->beginFrame, layer->endFrame);
        bool isVisible = JSON_AS_TYPE(layer->internalData["PreviousIsVisible"], bool);
        std::vector<float> previousBounds = JSON_AS_TYPE(layer->internalData["PreviousBounds"], std::vector<float>);
        if (JSON_AS_TYPE(layer->internalData["PreviousInBounds"], bool) != inBounds || layer->visible != isVisible || previousBounds[0] != layer->beginFrame || previousBounds[1] != layer->endFrame) {
            if (userData->audioHandle != -1) {
                double elapsedTime = (double) (GraphicsCore::renderFrame - layer->beginFrame + TIMESHIFT(layer)) / GraphicsCore::renderFramerate;
                Servers::AudioServerRequest({
                    {"action", "seek_sample"},
                    {"handle", userData->audioHandle},
                    {"seek", elapsedTime}
                });
            }
        }

        layer->internalData["PreviousInBounds"] = inBounds;
        layer->internalData["PreviousIsVisible"] = layer->visible;
        layer->internalData["PreviousBounds"] = {layer->beginFrame, layer->endFrame};

        int loadID = JSON_AS_TYPE(layer->internalData["LoadID"], int);
        TextureUnion* asset = AssetCore::GetAsset(layer->properties["AudioID"]);
        layer->internalData["StatusText"] = asset == nullptr ? "" : asset->name;
    }

    ELECTRON_EXPORT void LayerDestroy(RenderLayer* layer) {
        AudioLayerUserData* userData = (AudioLayerUserData*) layer->userData;
        if (userData->audioHandle) {
            Servers::AudioServerRequest({
                {"action", "stop_sample"},
                {"handle", userData->audioHandle}
            });
        }
        delete userData;
    }

    ELECTRON_EXPORT void LayerOnPropertiesChange(RenderLayer* layer) {
        AudioLayerUserData* userData = (AudioLayerUserData*) layer->userData;
        std::string audioID = JSON_AS_TYPE(layer->properties["AudioID"], std::string);
        TextureUnion* asset = AssetCore::GetAsset(audioID);
        if (asset && layer->properties["AudioID"] != layer->previousProperties["AudioID"]) {
            if (userData->audioHandle != -1) {
                Servers::AudioServerRequest({
                    {"action", "stop_sample"},
                    {"handle", userData->audioHandle}
                });
            }
            userData->audioHandle = Servers::AudioServerRequest({
                {"action", "play_sample"},
                {"path", asset->path},
                {"audio_proc", (uint64_t) audio_layer_data_callback},
                {"user_data", (uint64_t) &userData->sync}
            })["handle"];

            if (userData->audioWaveformThread != nullptr) {
                userData->terminateWaveformThread = true;
                userData->audioWaveformThread->join();
                userData->terminateWaveformThread = false;
                delete userData->audioWaveformThread;                
            }

            AudioMetadata audio = std::get<AudioMetadata>(asset->as);
            userData->averageWaveforms = std::vector<float>(audio.sampleRate / AVERAGE_WAVEFORM_PRECISION * audio.audioLength);
            userData->audioWaveformThread = new std::thread([](RenderLayer* layer, TextureUnion* asset) {
                AudioLayerUserData* userData = (AudioLayerUserData*) layer->userData;
                uint64_t decoder = Servers::AudioServerRequest({
                    {"action", "create_decoder"},
                    {"path", asset->path}
                })["decoder"];
                AudioMetadata audio = std::get<AudioMetadata>(asset->as);
                float* temp = new float[audio.sampleRate * 2];
                uint64_t framesRead = 0;
                int averageWaveformIndex = 0;
                while (true) {
                    uint64_t framesReadThisIteration = Servers::AudioServerRequest({
                        {"action", "read_decoder"},
                        {"decoder", decoder},
                        {"dest", (uint64_t) temp},
                        {"frames_to_read", audio.sampleRate}
                    })["frames_read"];
                    if (framesReadThisIteration != audio.sampleRate) break;
                    for (int i = 0; i < audio.sampleRate * 2; i += audio.sampleRate / AVERAGE_WAVEFORM_PRECISION * 2) {
                        float averageAccumulator = 0.0f;
                        for (int subSample = i; subSample < i + audio.sampleRate * 2 / AVERAGE_WAVEFORM_PRECISION; subSample += 2) {
                            averageAccumulator = (averageAccumulator + temp[glm::min(subSample, audio.sampleRate * 2 - 1)]) / 2.0f;
                        }
                        userData->averageWaveforms[glm::min(averageWaveformIndex++, (int) userData->averageWaveforms.size() - 1)] = averageAccumulator;
                    }
                }
                Servers::AudioServerRequest({
                    {"action", "destroy_decoder"},
                    {"decoder", decoder}
                });
                delete[] temp;

            }, layer, asset);
        }
    }

    ELECTRON_EXPORT void LayerOnPlaybackChange(RenderLayer* layer) {
    }

    ELECTRON_EXPORT void LayerOnTimelineSeek(RenderLayer* layer) {
        AudioLayerUserData* userData = (AudioLayerUserData*) layer->userData;
        if (userData->audioHandle != -1) {
            Servers::AudioServerRequest({
                {"action", "seek_sample"},
                {"handle", userData->audioHandle},
                {"seek", std::max(0.0, ((double) (GraphicsCore::renderFrame - layer->beginFrame + TIMESHIFT(layer))) / Shared::maxFramerate)}
            });
        }
    }

    ELECTRON_EXPORT void LayerRenderMenu(RenderLayer* layer) {
    }

    ELECTRON_EXPORT void LayerTimelineRender(RenderLayer* layer, TimelineLayerRenderDesc desc) {
        if (desc.dispose) return;
        AudioLayerUserData* userData = (AudioLayerUserData*) layer->userData;
        std::string audioSID = JSON_AS_TYPE(layer->properties["AudioID"], std::string);
        TextureUnion* asset = AssetCore::GetAsset(audioSID);
        if (userData->averageWaveforms.size() == 0) return;
        ImVec2 originalCursor = ImGui::GetCursorScreenPos();
        ImVec2 layerEndCursor = {originalCursor.x + layer->endFrame * desc.pixelsPerFrame, originalCursor.y};
        ImVec2 dragSize = ImVec2((layer->endFrame - layer->beginFrame) * desc.pixelsPerFrame / 10, desc.layerSizeY);
        dragSize.x = glm::clamp(dragSize.x, 1.0f, 30.0f);
        float pixelAdvance = (GraphicsCore::renderFramerate / AVERAGE_WAVEFORM_PRECISION) * desc.pixelsPerFrame;
        float advanceAccumulator = 0;
        glm::vec4 waveformColor = LayerTimelineColor * 0.7f;
        waveformColor.w = 1.0f;
        for (uint64_t i = 0; i < userData->averageWaveforms.size(); i++) {
            if (advanceAccumulator > (layer->endFrame - layer->beginFrame) * desc.pixelsPerFrame) break;
            if (originalCursor.x < desc.legendOffset.x) {
                originalCursor.x += pixelAdvance;
                advanceAccumulator += pixelAdvance;
                continue;
            }
            if (originalCursor.x > ImGui::GetWindowSize().x + desc.legendOffset.x) break;
            float average = glm::abs(userData->averageWaveforms[i]);
            float averageInPixels = average * desc.layerSizeY;
            float invertedAverageInPixels = desc.layerSizeY - averageInPixels;
            ImVec2 bottomRight = originalCursor;
            bottomRight.y += desc.layerSizeY;
            bottomRight.x += pixelAdvance;
            ImVec2 upperLeft = originalCursor;
            upperLeft.y += invertedAverageInPixels;
            ImGui::GetWindowDrawList()->AddRectFilled(upperLeft, bottomRight, ImGui::GetColorU32(ImVec4{waveformColor.r, waveformColor.g, waveformColor.b, waveformColor.a}));
            originalCursor.x += pixelAdvance;
            advanceAccumulator += pixelAdvance;
        }
    }

    ELECTRON_EXPORT json_t LayerGetPreviewProperties(RenderLayer* layer) {
        return {
            "Volume", "Panning", "BassBoost"
        };
    }
}
#include "editor_core.h"
#include "app.h"
#include "utils/AudioFile.h"
#define CSTR(x) ((x).c_str())

using namespace Electron;
using namespace glm;

#define TIMESHIFT(layer) (JSON_AS_TYPE(layer->properties["InternalTimeShift"], float))

#define BASS_BOOST_MIN_VALUE -0.5f
#define BASS_BOOST_MAX_VALUE 0.0f

extern "C" {

    // provides full control of interpolation pipeline
    // much faster than built-in InterpolateKeyframe method
    // only suitable for performance critical parts of code
    struct FloatInterpolator {
        json_t property;
        float lastKeyframeSecond;
        float targetKeyframeSecond;
        bool overrideInterpolation;
        float lastVolume;
        float targetVolume;
        float keyframeDifference;
        int keyframeIndex;
        int keyframesLength;

        FloatInterpolator(json_t property) {
            this->property = property;
            this->lastKeyframeSecond = 0;
            this->targetKeyframeSecond = 0;
            this->overrideInterpolation = 0;
            this->lastVolume = JSON_AS_TYPE(property.at(1).at(1), float);
            this->targetVolume = 0;
            this->keyframeDifference = 0;
            this->keyframeIndex = 2;
            this->keyframesLength = property.size();
            if (keyframesLength == 2) {
                overrideInterpolation = true;
            } else {
                targetVolume = JSON_AS_TYPE(property.at(2).at(1), float);
                targetKeyframeSecond = JSON_AS_TYPE(property.at(2).at(0), float) / GraphicsCore::renderFramerate;
                keyframeDifference = targetKeyframeSecond;
            }
        }

        float GetValue(double second) {
            if (overrideInterpolation) {
                return lastVolume;
            } else {
                if (second > targetKeyframeSecond && keyframeIndex + 1 < keyframesLength) {
                    keyframeIndex++;
                    lastVolume = targetVolume;
                    lastKeyframeSecond = targetKeyframeSecond;
                    targetKeyframeSecond = JSON_AS_TYPE(property.at(keyframeIndex).at(0), float) / GraphicsCore::renderFramerate;
                    targetVolume = JSON_AS_TYPE(property.at(keyframeIndex).at(1), float);
                    keyframeDifference = targetKeyframeSecond - lastKeyframeSecond;
                }
                float lerpPercentage = std::clamp((float) (second - lastKeyframeSecond) / keyframeDifference, 0.0f, 1.0f);
                float lerpVolume = Electron::Lerp((float) lastVolume, (float) targetVolume, lerpPercentage);
                return lerpVolume;
            }
        }
    };

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
        owner->properties["OverrideAudioPath"] = "";
        owner->internalData["FFMpegOperationID"] = -1;
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
        owner->properties["PreviewAudioChannel"] = 0;
        owner->internalData["LoadID"] = -1;
        owner->internalData["PreviousInBounds"] = false;
        owner->internalData["AudioBufferPtr"] = 0;

        owner->anyData.resize(5);
        owner->anyData[0] = -1; // audio handle
        owner->anyData[1] = false; // playing
        owner->anyData[2] = (AsyncFFMpegOperation*) nullptr;
        owner->anyData[3] = (std::thread*) nullptr;
        owner->anyData[4] = (bool*) nullptr;
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
        Servers::AudioServerRequest({
            {"action", "destroy_sample"},
            {"path", audioPath}
        });
        Servers::AudioServerRequest({
            {"action", "load_raw_buffer"},
            {"path", audioPath},
            {"buffer_ptr", (uint64_t) buffer},
        });
        layer->internalData["LoadID"] = 0;
    }

    void AudioLayerProcessAsset(RenderLayer* layer, TextureUnion* asset) {
        if (JSON_AS_TYPE(layer->properties["OverrideAudioPath"], std::string) == "") {
            layer->properties["OverrideAudioPath"] = "cache/" + std::to_string(Cache::GetCacheIndex()) + ".wav";
        }
        std::thread* filterJob = std::any_cast<std::thread*>(layer->anyData[3]);
        bool* terminationFlag = std::any_cast<bool*>(layer->anyData[4]);
        if (filterJob != nullptr) {
            *terminationFlag = true;
            filterJob->join();
            delete filterJob;
            delete terminationFlag;
        }
        terminationFlag = new bool();
        filterJob = new std::thread(
            [](RenderLayer* layer, TextureUnion* asset, bool* terminationFlag) {
                if (asset == nullptr) return;
                layer->internalData["ShowLoadingSpinner"] = true;
                std::string audioPath = JSON_AS_TYPE(layer->properties["OverrideAudioPath"], std::string);
                AudioFile<float>* originalBuffer = (AudioFile<float>*) JSON_AS_TYPE(
                    Servers::AudioServerRequest({
                        {"action", "audio_buffer_ptr"},
                        {"path", asset->path}
                    })["ptr"], unsigned long
                );
                if (!originalBuffer) return;
                AudioFile<float> copyBuffer = *originalBuffer;
                json_t volumeKeyframes = layer->properties["Volume"];
                json_t panningKeyframes = layer->properties["Panning"];
                json_t bassBoostKeyframes = layer->properties["BassBoost"];
                std::vector<std::thread> channelProcessors{};
                for (int channel = 0; channel < copyBuffer.getNumChannels(); channel++) {
                    channelProcessors.push_back(std::thread(
                        [](int channel, json_t volumeKeyframes, json_t panningKeyframes, json_t bassBoostKeyframes, AudioFile<float>* copyBuffer, bool* terminationFlag) {
                            FloatInterpolator interpolator(volumeKeyframes);
                            FloatInterpolator panningInterpolator(panningKeyframes);
                            FloatInterpolator bassInterpolator(bassBoostKeyframes);
                            int samplesPerChannel = copyBuffer->getNumSamplesPerChannel();
                            int sampleRate = copyBuffer->getSampleRate();
                            float bakedMultiplier = 1 / (float) sampleRate;
                            float* samples = copyBuffer->samples[channel].data();
                            float panValue = channel == 0 ? -0.5f : 0.5f;
                            for (uint64_t sample = 0; sample < samplesPerChannel; sample++) {
                                if (*terminationFlag) {
                                    return;
                                }
                                float second = sample * bakedMultiplier;
                                samples[sample] *= interpolator.GetValue(second) * 0.01f; // Volume 
                                if (IsInBounds(samples[sample], BASS_BOOST_MIN_VALUE, BASS_BOOST_MAX_VALUE)) {
                                    samples[sample] *= bassInterpolator.GetValue(second) * 0.01f;
                                }
                                samples[sample] *= std::clamp((panningInterpolator.GetValue(second) + panValue) / panValue, 0.0f, 1.0f); // Panning
                            }
                        }
                    , channel, volumeKeyframes, panningKeyframes, bassBoostKeyframes, &copyBuffer, terminationFlag));
                }

                for (auto& thread : channelProcessors) {
                    thread.join();
                }
                if (*terminationFlag) {
                    return;
                }

                ReloadAudioHandle(layer, asset, &copyBuffer);
                layer->internalData["ShowLoadingSpinner"] = false;
            }, layer, asset, terminationFlag
        );
        layer->anyData[3] = filterJob;
        layer->anyData[4] = terminationFlag;
    }

    ELECTRON_EXPORT void LayerRender(RenderLayer* owner) {
        bool playing = std::any_cast<bool>(owner->anyData[1]);
        std::string audioID = JSON_AS_TYPE(owner->properties["AudioID"], std::string);
        TextureUnion* asset = AssetCore::GetAsset(audioID);
        if (asset != nullptr)
            owner->endFrame = std::clamp(owner->endFrame, owner->beginFrame, owner->beginFrame + std::get<AudioMetadata>(asset->as).audioLength * GraphicsCore::renderFramerate);
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
                AudioMetadata metadata = std::get<AudioMetadata>(asset->as);
                layer->endFrame = layer->beginFrame + (metadata.audioLength * GraphicsCore::renderFramerate);
                Servers::AudioServerRequest({
                    {"action", "stop_sample"},
                    {"handle", std::any_cast<int>(layer->anyData[0])}
                });
                Servers::AudioServerRequest({
                    {"action", "destroy_sample"},
                    {"path", JSON_AS_TYPE(layer->properties["OverrideAudioPath"], std::string)}
                });
                layer->anyData[0] = -1;
                layer->properties["OverrideAudioPath"] = "";
                return;
            }
        }

        json_t& volume = layer->properties["Volume"];
        layer->RenderProperty(GeneralizedPropertyType::Float, volume, "Volume");

        json_t& panning = layer->properties["Panning"];
        layer->RenderProperty(GeneralizedPropertyType::Float, panning, "Panning");

        json_t& bass = layer->properties["BassBoost"];
        layer->RenderProperty(GeneralizedPropertyType::Float, bass, "BassBoost");

        AudioFile<float>* audioBuffer = nullptr;
        if (asset != nullptr) {
            std::string audioPath = JSON_AS_TYPE(layer->properties["OverrideAudioPath"], std::string);
            audioBuffer = (AudioFile<float>*) JSON_AS_TYPE(
                Servers::AudioServerRequest({
                    {"action", "audio_buffer_ptr"},
                    {"path", audioPath}
                })["ptr"], unsigned long
            );
        }
        int previewAudioChannel = JSON_AS_TYPE(layer->properties["PreviewAudioChannel"], int);
        int nextAudioChannel = previewAudioChannel;
        if (asset != nullptr && audioBuffer) {
            AudioMetadata metadata = std::get<AudioMetadata>(asset->as);
            nextAudioChannel = previewAudioChannel + 1;
            if (nextAudioChannel >= audioBuffer->getNumChannels()) {
                nextAudioChannel = 0;
            }
            ImGui::Separator();
            int precision = 100;
            static std::vector<float> waveform(precision);
            if (audioBuffer->audioFileFormat != AudioFileFormat::NotLoaded && audioBuffer->getNumSamplesPerChannel() > 0) {
                double elapsedTime = (double) (GraphicsCore::renderFrame - layer->beginFrame + TIMESHIFT(layer)) / GraphicsCore::renderFramerate;
                uint64_t beginSample = metadata.sampleRate * elapsedTime;
                uint64_t endSample = beginSample + audioBuffer->getSampleRate();
                uint64_t sampleStep = metadata.sampleRate / precision;
                endSample = glm::clamp(endSample, (uint64_t) 0, (uint64_t) metadata.audioLength * (uint64_t) metadata.sampleRate);
                int averageIndex = 0;
                uint64_t samplesCount = audioBuffer->getNumSamplesPerChannel();
                float* samples = audioBuffer->samples[previewAudioChannel].data();
                waveform = std::vector<float>(precision);
                for (uint64_t sample = beginSample; sample < endSample; sample += sampleStep) {
                    float averageSample = 0;
                    for (uint64_t subSample = sample; subSample < sample + sampleStep; subSample++) {
                        averageSample = (averageSample + samples[glm::clamp(
                            subSample, (uint64_t) 0, 
                            samplesCount - 1)]) / 2.0f;
                    }
                    waveform[averageIndex] = averageSample;
                    averageIndex++;
                }
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
            layer->internalData["ShowLoadingSpinner"] = true;
            TextureUnion* asset = AssetCore::GetAsset(JSON_AS_TYPE(layer->properties["AudioID"], std::string));
            layer->internalData["StatusText"] = asset == nullptr ? "" : asset->name;
            if (loaded) {
                std::string audioPath = JSON_AS_TYPE(layer->properties["OverrideAudioPath"], std::string);
                double elapsedTime = (double) (GraphicsCore::renderFrame - layer->beginFrame + TIMESHIFT(layer)) / GraphicsCore::renderFramerate;
                elapsedTime = std::min(elapsedTime, (double) std::get<AudioMetadata>(asset->as).audioLength);
                layer->anyData[0] = JSON_AS_TYPE(
                    Servers::AudioServerRequest({
                        {"action", "play_sample"},
                        {"path", audioPath}
                    })["handle"], int
                );
                Servers::AudioServerRequest({
                    {"action", "seek_sample"},
                    {"handle", std::any_cast<int>(layer->anyData[0])},
                    {"seek", elapsedTime}
                });
                layer->internalData["AudioBufferPtr"] = JSON_AS_TYPE(
                    Servers::AudioServerRequest({
                        {"action", "audio_buffer_ptr"},
                        {"path", audioPath}
                    })["ptr"], unsigned long
                );
                layer->internalData["LoadID"] = -1;
            }
        } else layer->internalData["ShowLoadingSpinner"] = false;
    }

    ELECTRON_EXPORT void LayerDestroy(RenderLayer* layer) {
        if (JSON_AS_TYPE(layer->properties["OverrideAudioPath"], std::string) != "") {
            std::string audioPath = JSON_AS_TYPE(layer->properties["OverrideAudioPath"], std::string);
            Servers::AudioServerRequest({
                {"action", "destroy_sample"},
                {"path", audioPath}
            });
            std::thread* filterJob = std::any_cast<std::thread*>(layer->anyData[3]);
            if (filterJob != nullptr) {
                filterJob->join();
            }
            delete filterJob;
        }
    }

    ELECTRON_EXPORT void LayerOnPropertiesChange(RenderLayer* layer) {
        std::string audioID = JSON_AS_TYPE(layer->properties["AudioID"], std::string);
        if (audioID != "") 
            AudioLayerProcessAsset(layer, AssetCore::GetAsset(audioID));
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

    ELECTRON_EXPORT void LayerRenderMenu(RenderLayer* layer) {
        std::string audioSID = JSON_AS_TYPE(layer->properties["AudioID"], std::string);
        TextureUnion* asset = AssetCore::GetAsset(audioSID);
        AudioFile<float>* audioBuffer = nullptr;
        if (asset != nullptr) {
            std::string audioPath = JSON_AS_TYPE(layer->properties["OverrideAudioPath"], std::string);
            audioBuffer = (AudioFile<float>*) JSON_AS_TYPE(
                Servers::AudioServerRequest({
                    {"action", "audio_buffer_ptr"},
                    {"path", audioPath}
                })["ptr"], unsigned long
            );
        }
        ImGui::SetCurrentContext(AppCore::context);
        if (audioBuffer && ImGui::BeginMenu(ICON_FA_FILE_AUDIO " Save To WAV File")) {
            ImGui::SeparatorText(ICON_FA_FILE_EXPORT " WAV Export");
            static std::string path = "export.wav";
            ImGui::InputText("##wav_export", &path);
            if (AppCore::ButtonCenteredOnLine(ICON_FA_FLOPPY_DISK " Save")) {
                audioBuffer->save(path);
            }
            ImGui::EndMenu();
        }
    }

    ELECTRON_EXPORT json_t LayerGetPreviewProperties(RenderLayer* layer) {
        return {
            "Volume", "Panning", "BassBoost"
        };
    }
}

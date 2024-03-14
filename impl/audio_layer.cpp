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

    struct AudioLayerUserData {
        int64_t audioHandle;
        std::thread* filterJob;
        bool* terminationFlag;
        std::vector<float> averageWaveforms;
        GPUExtendedHandle audioCoverHandle;

        AudioLayerUserData() {
            this->audioCoverHandle = 0;
        }
    };

    ELECTRON_EXPORT std::string LayerName = "Audio";
    ELECTRON_EXPORT glm::vec4 LayerTimelineColor = {
        0.831, 0.569, 0.286, 1
    };

    std::mutex uploadMutex;

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
        owner->internalData["PreviewAudioChannel"] = 0;
        owner->internalData["LoadID"] = -1;
        owner->internalData["PreviousInBounds"] = false;
        owner->internalData["AudioBufferPtr"] = 0;
        owner->internalData["PreviousIsVisible"] = true;
        owner->internalData["PreviousBounds"] = {owner->beginFrame, owner->endFrame};

        owner->properties["OverrideAudioPath"] = string_format("cache/%i.wav", Cache::GetCacheIndex());

        owner->userData = new AudioLayerUserData();
        AudioLayerUserData* userData = (AudioLayerUserData*) owner->userData;
        userData->audioHandle = 0;
        userData->terminationFlag = nullptr;
        userData->filterJob = nullptr;
    }


    void ReloadAudioHandle(RenderLayer* layer, TextureUnion* asset, AudioFile<float>* buffer) {
        AudioLayerUserData* userData = (AudioLayerUserData*) layer->userData;
        if (asset == nullptr || asset->type != TextureUnionType::Audio) return;
        if (userData->audioHandle != -1) {
            Servers::AudioServerRequest({
                {"action", "stop_sample"},
                {"handle", userData->audioHandle}
            });
        }
        std::string audioPath = JSON_AS_TYPE(layer->properties["OverrideAudioPath"], std::string);
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
        AudioLayerUserData* userData = (AudioLayerUserData*) layer->userData;
        if (JSON_AS_TYPE(layer->properties["OverrideAudioPath"], std::string) == "") {
            layer->properties["OverrideAudioPath"] = "cache/" + std::to_string(Cache::GetCacheIndex()) + ".wav";
        }
        if (userData->filterJob != nullptr) {
            *userData->terminationFlag = true;
            userData->filterJob->join();
            delete userData->filterJob;
            delete userData->terminationFlag;
        }
        userData->terminationFlag = new bool();
        userData->filterJob = new std::thread(
            [](RenderLayer* layer, TextureUnion* asset, bool* terminationFlag, AudioLayerUserData* userData) {
                if (asset == nullptr) return;
                std::lock_guard<std::mutex> uploadGuard(uploadMutex);
                layer->internalData["ShowLoadingSpinner"] = true;
                std::string audioPath = JSON_AS_TYPE(layer->properties["OverrideAudioPath"], std::string);
                AudioMetadata metadata = std::get<AudioMetadata>(asset->as);
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
                uint64_t averageWaveformSize = glm::floor(copyBuffer.getNumSamplesPerChannel() / (uint64_t) AVERAGE_WAVEFORM_PRECISION);
                userData->averageWaveforms.resize(averageWaveformSize);
                uint64_t sampleStep = glm::floor(copyBuffer.getSampleRate() / AVERAGE_WAVEFORM_PRECISION);
                uint64_t averageIndex = 0;
                for (uint64_t sample = 0; sample < copyBuffer.getNumSamplesPerChannel(); sample += sampleStep) {
                    if (averageIndex >= averageWaveformSize) break;
                    if (*terminationFlag) return;
                    float averageAccumulator = 0;
                    for (uint64_t subSample = sample; subSample < sample + sampleStep; subSample++) {
                        averageAccumulator = (averageAccumulator + glm::abs(copyBuffer.samples[0][glm::clamp(subSample, (uint64_t) 0, copyBuffer.getNumSamplesPerChannel() - (uint64_t) 1)])) / 2.0f;
                    }
                    userData->averageWaveforms[averageIndex++] = averageAccumulator; 
                }

            }, layer, asset, userData->terminationFlag, userData
        );
    }

    ELECTRON_EXPORT void LayerRender(RenderLayer* owner) {
        AudioLayerUserData* userData = (AudioLayerUserData*) owner->userData;
        std::string audioID = JSON_AS_TYPE(owner->properties["AudioID"], std::string);
        TextureUnion* asset = AssetCore::GetAsset(audioID);
        if (asset != nullptr)
            owner->endFrame = std::clamp(owner->endFrame, owner->beginFrame, owner->beginFrame + std::get<AudioMetadata>(asset->as).audioLength * GraphicsCore::renderFramerate);
    }

    ELECTRON_EXPORT void LayerPropertiesRender(RenderLayer* layer) {
        AudioLayerUserData* userData = (AudioLayerUserData*) layer->userData;
        std::string previousAudioID = layer->properties["AudioID"];
        json_t& audioID = layer->properties["AudioID"];
        layer->RenderAssetProperty(audioID, "Audio", TextureUnionType::Audio, userData->audioCoverHandle);
        std::string audioSID = layer->properties["AudioID"];
        TextureUnion* asset = AssetCore::GetAsset(audioID);

        if (previousAudioID != audioSID) {
            if (asset) {
                DriverCore::DestroyImageHandleUI(userData->audioCoverHandle);
                userData->audioCoverHandle = DriverCore::GetImageHandleUI(asset->pboGpuTexture);
            } else {
                DriverCore::DestroyImageHandleUI(userData->audioCoverHandle);
                userData->audioCoverHandle = 0;
            }
        }

        if (asset && !userData->audioCoverHandle) {
            userData->audioCoverHandle = DriverCore::GetImageHandleUI(asset->pboGpuTexture);
        }

        if (audioSID != "" && asset == nullptr) {
            layer->properties["AudioID"] = "";
            userData->audioHandle = -1;
        }

        if (!asset) {
            Servers::AudioServerRequest({
                {"action", "stop_sample"},
                {"handle", userData->audioHandle}
            });
            userData->audioHandle = 0;
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
        int previewAudioChannel = JSON_AS_TYPE(layer->internalData["PreviewAudioChannel"], int);
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
            layer->internalData["PreviewAudioChannel"] = previewAudioChannel;
            ImGui::Spacing();
        }
    }

    ELECTRON_EXPORT void LayerSortKeyframes(RenderLayer* layer) {
        AudioLayerUserData* userData = (AudioLayerUserData*) layer->userData;
        if (Shared::selectedRenderLayer != layer->id && userData->audioCoverHandle) {
            DriverCore::DestroyImageHandleUI(userData->audioCoverHandle);
        } 

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
                userData->audioHandle = JSON_AS_TYPE(
                    Servers::AudioServerRequest({
                        {"action", "play_sample"},
                        {"path", audioPath}
                    })["handle"], int
                );
                Servers::AudioServerRequest({
                    {"action", "seek_sample"},
                    {"handle", userData->audioHandle},
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
        AudioLayerUserData* userData = (AudioLayerUserData*) layer->userData;
        if (JSON_AS_TYPE(layer->properties["OverrideAudioPath"], std::string) != "") {
            std::string audioPath = JSON_AS_TYPE(layer->properties["OverrideAudioPath"], std::string);
            Servers::AudioServerRequest({
                {"action", "stop_sample"},
                {"handle", userData->audioHandle}
            });
            Servers::AudioServerRequest({
                {"action", "destroy_sample"},
                {"path", audioPath}
            });
            if (userData->filterJob != nullptr) {
                userData->filterJob->join();
            }
            if (userData->terminationFlag != nullptr) {
                delete userData->terminationFlag;
            }
            delete userData->filterJob;
        }
        delete userData;
    }

    ELECTRON_EXPORT void LayerOnPropertiesChange(RenderLayer* layer) {
        AudioLayerUserData* userData = (AudioLayerUserData*) layer->userData;
        std::string audioID = JSON_AS_TYPE(layer->properties["AudioID"], std::string);
        TextureUnion* asset = AssetCore::GetAsset(audioID);
        if (asset != nullptr) {
            AudioMetadata metadata = std::get<AudioMetadata>(asset->as);
            layer->endFrame = layer->beginFrame + (metadata.audioLength * GraphicsCore::renderFramerate);
            Servers::AudioServerRequest({
                {"action", "stop_sample"},
                {"handle", userData->audioHandle}
            });
            Servers::AudioServerRequest({
                {"action", "destroy_sample"},
                {"path", JSON_AS_TYPE(layer->properties["OverrideAudioPath"], std::string)}
            });
            userData->audioHandle = -1;
            layer->properties["OverrideAudioPath"] = "";
        }
        if (audioID != "") 
            AudioLayerProcessAsset(layer, AssetCore::GetAsset(audioID));
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

    ELECTRON_EXPORT void LayerTimelineRender(RenderLayer* layer, TimelineLayerRenderDesc desc) {
        AudioLayerUserData* userData = (AudioLayerUserData*) layer->userData;
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
        if (!audioBuffer) return;
        if (userData->averageWaveforms.size() == 0) return;
        ImVec2 originalCursor = ImGui::GetCursorScreenPos();
        ImVec2 dragSize = ImVec2((layer->endFrame - layer->beginFrame) * desc.pixelsPerFrame / 10, desc.layerSizeY);
        dragSize.x = glm::clamp(dragSize.x, 1.0f, 30.0f);
        float pixelAdvance = (GraphicsCore::renderFramerate / AVERAGE_WAVEFORM_PRECISION) * desc.pixelsPerFrame;
        glm::vec4 waveformColor = LayerTimelineColor * 0.7f;
        waveformColor.w = 1.0f;
        for (uint64_t i = 0; i < userData->averageWaveforms.size(); i++) {
            if (originalCursor.x < desc.legendOffset.x) {
                originalCursor.x += pixelAdvance;
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
        }
    }

    ELECTRON_EXPORT json_t LayerGetPreviewProperties(RenderLayer* layer) {
        return {
            "Volume", "Panning", "BassBoost"
        };
    }
}

#include "editor_core.h"
#include "app.h"
#define CSTR(x) ((x).c_str())

using namespace Electron;
using namespace glm;

extern "C" {

    ELECTRON_EXPORT std::string LayerName = "Audio";
    ELECTRON_EXPORT glm::vec4 LayerTimelineColor = {
        0.831, 0.569, 0.286, 1
    };
    ELECTRON_EXPORT json_t LayerPreviewProperties = {
        "Volume"
    };

    ELECTRON_EXPORT void LayerInitialize(RenderLayer* owner) {
        owner->properties["Volume"] = {
            GeneralizedPropertyType::Float,
            {0, 100}
        };
        owner->properties["AudioID"] = "";
        owner->properties["OverrideAudioPath"] = "";
        owner->internalData["FFMpegOperationID"] = -1;
        owner->internalData["TimelineSliders"] = {
            {"Volume", 0.0f, 100.0f}
        };
        owner->internalData["LoadID"] = -1;

        owner->anyData.resize(3);
        owner->anyData[0] = -1; // audio handle
        owner->anyData[1] = false; // playing
        owner->anyData[2] = (AsyncFFMpegOperation*) nullptr;
    }

    std::string FFMpegAudioFilter(int beginFrame, int endFrame, int beginVolume, int endVolume) {
        return string_format(
            "volume=enable='between(t,%i,%i)':volume='(%0.2f + (%0.2f - %0.2f) * ((t - %0.2f) / (%0.2f - %0.2f))) / 100.0':eval=frame",
            beginFrame, endFrame, (float) beginVolume, (float) endVolume, (float) beginVolume, (float) beginFrame, (float) endFrame, (float) beginFrame
        );
    }

    void ReloadAudioHandle(RenderLayer* layer, TextureUnion* asset) {
        if (asset == nullptr || asset->type != TextureUnionType::Audio) return;
        if (std::any_cast<int>(layer->anyData[0]) != -1) {
            Servers::AudioServerRequest({
                {"action", "stop_sample"},
                {"handle", std::any_cast<int>(layer->anyData[0])}
            });
        }
        asset->DumpData();
        std::string audioPath = asset->path;
        AudioMetadata metadata = std::get<AudioMetadata>(asset->as);
        if (JSON_AS_TYPE(layer->properties["OverrideAudioPath"], std::string) != "") {
            audioPath = JSON_AS_TYPE(layer->properties["OverrideAudioPath"], std::string);
        }
        double elapsedTime = (double) (Shared::graphics->renderFrame - layer->beginFrame) / 60.0;
        elapsedTime = std::min(elapsedTime, (double) metadata.audioLength);
        layer->internalData["LoadID"] = JSON_AS_TYPE(Servers::AudioServerRequest({
                {"action", "load_sample"},
                {"override", true},
                {"path", audioPath}
        }).ResponseToJSON()["id"], int);
    }

    void AudioLayerProcessAsset(RenderLayer* layer, TextureUnion* asset) {
        if (asset == nullptr) return;
        std::vector<std::string> filters{};
        AudioMetadata metadata = std::get<AudioMetadata>(asset->as);
        json_t& volume = layer->properties["Volume"];
        std::string audioPath = "";
        if (JSON_AS_TYPE(layer->properties["OverrideAudioPath"], std::string) == "") {
            audioPath = string_format("cache/%i.wav", Cache::GetCacheIndex());
        } else {
            audioPath = JSON_AS_TYPE(layer->properties["OverrideAudioPath"], std::string);
        }
        int keyframesCount = volume.size() - 1;
        if (keyframesCount == 1) {
            int v = JSON_AS_TYPE(volume.at(1).at(1), int);
            filters.push_back(FFMpegAudioFilter(0, metadata.audioLength, v, v));
        } else {
            int previousKey = 0;
            int currentKey = JSON_AS_TYPE(volume.at(2).at(0), int);
            filters.push_back(FFMpegAudioFilter(previousKey / 60, currentKey / 60, JSON_AS_TYPE(volume.at(1).at(1), int), JSON_AS_TYPE(volume.at(2).at(1), int)));
            previousKey = currentKey;
            for (int i = 3; i < volume.size(); i++) {
                currentKey = JSON_AS_TYPE(volume.at(i).at(0), int);
                filters.push_back(FFMpegAudioFilter(previousKey / 60, currentKey / 60, JSON_AS_TYPE(volume.at(i - 1).at(1), int), JSON_AS_TYPE(volume.at(i).at(1), int)));
                previousKey = currentKey;
            }
            if (currentKey / 60 != metadata.audioLength) {
                filters.push_back(FFMpegAudioFilter(currentKey / 60, metadata.audioLength, JSON_AS_TYPE(volume.at(volume.size() - 1).at(1), float), JSON_AS_TYPE(volume.at(volume.size() - 1).at(1), float)));
            }

        }
        std::string complexFilter = "";
        for (int i = 0; i < filters.size(); i++) {
            complexFilter += filters[i] + (i + 1 == filters.size() ? "" : ",");
        }
        std::string cmd = string_format(
            "ffmpeg -y -i %s -af \"%s\" %s > /dev/null",
            asset->path.c_str(), complexFilter.c_str(), audioPath.c_str()
        );
        auto& operations = Shared::assets->operations;
        // Delete 
        if (JSON_AS_TYPE(layer->internalData["FFMpegOperationID"], int) != -1) {
            int operationID = JSON_AS_TYPE(layer->internalData["FFMpegOperationID"], int);
            int index = 0;
            for (auto& op : operations) {
                if (op.id == operationID) {
                    delete op.process;
                    operations.erase(operations.begin() + index);
                    break;
                }
                index++;
            }
        }
        layer->properties["OverrideAudioPath"] = audioPath;
        Shared::assets->operations.push_back(
            AsyncFFMpegOperation(
                [](OperationArgs_T& args) {
                    RenderLayer* layer = std::any_cast<RenderLayer*>(args[0]);
                    std::string audioPath = std::any_cast<std::string>(args[1]);
                    TextureUnion* asset = std::any_cast<TextureUnion*>(args[2]);
                    layer->properties["FFMpegOperationID"] = -1;
                    ReloadAudioHandle(layer, asset);
                }, {layer, audioPath, asset}, cmd, string_format("%s %s...", ICON_FA_FILE_AUDIO, ELECTRON_GET_LOCALIZATION("AUDIO_FILTERING_IN_PROGRESS"))
            )
        );
        layer->internalData["FFMpegOperationID"] = operations[operations.size() - 1].id;
    }

    ELECTRON_EXPORT void LayerRender(RenderLayer* owner) {

        bool playing = std::any_cast<bool>(owner->anyData[1]);
        std::string audioID = JSON_AS_TYPE(owner->properties["AudioID"], std::string);
        TextureUnion* asset = nullptr;
        auto& assets = Shared::assets->assets;
        for (int i = 0; i < assets.size(); i++) {
            if (intToHex(assets.at(i).id) == audioID) {
                if (assets.at(i).type == TextureUnionType::Audio)
                    asset = &assets.at(i);
            }
        }
        int loadID = JSON_AS_TYPE(owner->internalData["LoadID"], int);
        if (loadID != -1) {
            bool loaded = JSON_AS_TYPE(
                Servers::AudioServerRequest({
                    {"action", "load_status"},
                    {"id", loadID}
                }).ResponseToJSON()["status"], bool
            );
            if (loaded) {
                std::string audioPath = asset->path;
                RenderLayer* layer = owner;
                if (JSON_AS_TYPE(layer->properties["OverrideAudioPath"], std::string) != "") {
                    audioPath = JSON_AS_TYPE(layer->properties["OverrideAudioPath"], std::string);
                }
                double elapsedTime = (double) (Shared::graphics->renderFrame - layer->beginFrame) / 60.0;
                elapsedTime = std::min(elapsedTime, (double) std::get<AudioMetadata>(asset->as).audioLength);
                layer->anyData[0] = JSON_AS_TYPE(
                    Servers::AudioServerRequest({
                        {"action", "play_sample"},
                        {"path", audioPath}
                    }).ResponseToJSON()["handle"], int
                );
                Servers::AudioServerRequest({
                    {"action", "seek_sample"},
                    {"handle", std::any_cast<int>(layer->anyData[0])},
                    {"seek", elapsedTime}
                });
                owner->internalData["LoadID"] = -1;
            }
        }
    }

    ELECTRON_EXPORT void LayerPropertiesRender(RenderLayer* layer) {
        json_t previousAudioID = layer->properties["AudioID"];
        json_t& audioID = layer->properties["AudioID"];
        layer->RenderAssetProperty(audioID, string_format("%s %s", ICON_FA_FILE_AUDIO, "Audio"), TextureUnionType::Audio);
        TextureUnion* asset = nullptr;
        std::string audioSID = JSON_AS_TYPE(layer->properties["AudioID"], std::string);
        auto& assets = Shared::assets->assets;
        for (int i = 0; i < assets.size(); i++) {
            if (intToHex(assets.at(i).id) == audioSID) {
                if (assets.at(i).type == TextureUnionType::Audio)
                    asset = &assets.at(i);
            }
        }if (audioSID != "" && asset == nullptr) {
            layer->properties["AudioID"] = "";
            layer->anyData[0] = -1;
        }
        if (JSON_AS_TYPE(previousAudioID, std::string) != JSON_AS_TYPE(audioID, std::string)) {
            if (asset != nullptr) {
                ReloadAudioHandle(layer, asset);
                AudioMetadata metadata = std::get<AudioMetadata>(asset->as);
                layer->endFrame = layer->beginFrame + (metadata.audioLength * 60);
            }
        }

        json_t& volume = layer->properties["Volume"];
        layer->RenderProperty(GeneralizedPropertyType::Float, volume, string_format("%s %s", ICON_FA_VOLUME_HIGH, "Volume"), {0, 100});
    }

    ELECTRON_EXPORT void LayerSortKeyframes(RenderLayer* layer) {
        json_t& volume = layer->properties["Volume"];
        layer->SortKeyframes(volume);
        if (std::any_cast<int>(layer->anyData[0]) != -1) {
            Servers::AudioServerRequest({
                {"action", "pause_sample"},
                {"handle", std::any_cast<int>(layer->anyData[0])},
                {"pause", !(Shared::graphics->isPlaying && IsInBounds((int) Shared::graphics->renderFrame, layer->beginFrame, layer->endFrame))}
            });
        }
    }

    ELECTRON_EXPORT void LayerDestroy(RenderLayer* layer) {
    }

    ELECTRON_EXPORT void LayerOnPropertiesChange(RenderLayer* layer) {
        TextureUnion* asset = nullptr;
        std::string audioSID = JSON_AS_TYPE(layer->properties["AudioID"], std::string);
        auto& assets = Shared::assets->assets;
        for (int i = 0; i < assets.size(); i++) {
            if (intToHex(assets.at(i).id) == audioSID) {
                if (assets.at(i).type == TextureUnionType::Audio)
                    asset = &assets.at(i);
            }
        }
        AudioLayerProcessAsset(layer, asset);
    }

    ELECTRON_EXPORT void LayerOnPlaybackChange(RenderLayer* layer) {
    }

    ELECTRON_EXPORT void LayerOnTimelineSeek(RenderLayer* layer) {
        if (std::any_cast<int>(layer->anyData[0]) != -1) {
            Servers::AudioServerRequest({
                {"action", "seek_sample"},
                {"handle", std::any_cast<int>(layer->anyData[0])},
                {"seek", std::max(0.0, ((double) (Shared::graphics->renderFrame - layer->beginFrame)) / 60.0)}
            });
        }
    }
}

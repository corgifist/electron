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

    };

    ELECTRON_EXPORT void LayerInitialize(RenderLayer* owner) {
        owner->properties["AudioID"] = "";
    }

    ELECTRON_EXPORT void LayerRender(RenderLayer* owner) {
        if (owner->anyData.size() == 0) {
            owner->anyData.push_back(-1); // audio handle
            owner->anyData.push_back(false); // playing
        }

        int audioHandle = std::any_cast<int>(owner->anyData[0]);
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
        if (audioHandle != -1) {
            Servers::AudioServerRequest({
                {"action", "pause_sample"},
                {"handle", audioHandle},
                {"pause", !playing}
            });
        }

        owner->anyData[0] = audioHandle;
        owner->anyData[1] = playing;
    }

    ELECTRON_EXPORT void LayerPropertiesRender(RenderLayer* layer) {
        json_t previousAudioID = layer->properties["AudioID"];
        json_t& audioID = layer->properties["AudioID"];
        layer->RenderAssetProperty(audioID, "Audio", TextureUnionType::Audio);
        if (JSON_AS_TYPE(previousAudioID, std::string) != JSON_AS_TYPE(audioID, std::string)) {
            std::string audioID = JSON_AS_TYPE(layer->properties["AudioID"], std::string);
            TextureUnion* asset = nullptr;
            auto& assets = Shared::assets->assets;
            for (int i = 0; i < assets.size(); i++) {
                if (intToHex(assets.at(i).id) == audioID) {
                    if (assets.at(i).type == TextureUnionType::Audio)
                        asset = &assets.at(i);
                }
            }

            if (asset != nullptr) {
                AudioMetadata metadata = std::get<AudioMetadata>(asset->as);
                layer->endFrame = layer->beginFrame + metadata.audioLength * 60;

                if (JSON_AS_TYPE(previousAudioID, std::string) != "") {
                    Servers::AudioServerRequest({
                        {"action", "stop_sample"},
                        {"handle", std::any_cast<int>(layer->anyData[0])}
                    });
                }

                Servers::AudioServerRequest({
                    {"action", "load_sample"},
                    {"path", asset->path}
                });
                layer->anyData[0] = JSON_AS_TYPE(
                    Servers::AudioServerRequest({
                        {"action", "play_sample"},
                        {"path", asset->path}
                    }).ResponseToJSON()["handle"], int
                );
                Servers::AudioServerRequest({
                    {"action", "pause_sample"},
                    {"handle", std::any_cast<int>(layer->anyData[0])},
                    {"pause", true}
                });
            }
        }
    }

    ELECTRON_EXPORT void LayerSortKeyframes(RenderLayer* layer) {

    }

    ELECTRON_EXPORT void LayerDestroy(RenderLayer* layer) {

    }

    ELECTRON_EXPORT void LayerOnPlaybackChange(RenderLayer* layer) {
        layer->anyData[1] = Shared::graphics->isPlaying; // playing
    }

    ELECTRON_EXPORT void LayerOnTimelineSeek(RenderLayer* layer) {
        int audioHandle = std::any_cast<int>(layer->anyData[0]);
        if (audioHandle != -1) {
            Servers::AudioServerRequest({
                {"action", "seek_sample"},
                {"handle", audioHandle},
                {"seek", ((float) (Shared::graphics->renderFrame - layer->beginFrame)) / 60.0f}
            });
        }
    }
}
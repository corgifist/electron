#include "shared.h"

namespace Electron {

    void ProjectMap::SaveProject() {
        std::string propertiesDump = propertiesMap.dump();

        Servers::AsyncWriterRequest({
            {"action", "write"},
            {"path", path + "/project.json"},
            {"content", propertiesDump}
        });
    }

    int Shared::selectedRenderLayer = -1;
    ProjectMap Shared::project{};
    json_t Shared::configMap{};
    json_t Shared::localizationMap{};

    int Wavefront::x = 0;
    int Wavefront::y = 0;
    int Wavefront::z = 0;

    GLuint Shared::shadowTex = 0;

    int Shared::assetSelected = -128;
    std::string Shared::importErrorMessage = "";

    float Shared::deltaTime = 0.0f;

    GLuint Shared::fsVAO = UNDEFINED;

    ImVec2 Shared::displayPos{};
    ImVec2 Shared::displaySize{};

    std::string Shared::glslVersion = "#version 460"; // use standard OpenGL by default
    float Shared::maxFramerate = 60;

    int Shared::renderCalls = 0;
    int Shared::compositorCalls = 0;

    std::string Shared::defaultImageLayer = "sdf2d_layer";
    std::string Shared::defaultAudioLayer = "audio_layer";

    std::string Shared::assetManagerDragDropType;
}
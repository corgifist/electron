#include "shared.h"

namespace Electron {

    void ProjectMap::SaveProject() {
        std::string propertiesDump = propertiesMap.dump();
        std::ofstream outputStream(path + "/project.json");
        outputStream << propertiesDump;
        outputStream.close();
    }

    int Shared::selectedRenderLayer = -1;
    ProjectMap Shared::project{};
    json_t Shared::configMap{};
    json_t Shared::localizationMap{};
    int Wavefront::x = 0;
    int Wavefront::y = 0;
    int Wavefront::z = 0;
    int Shared::assetSelected = -128;
    std::string Shared::importErrorMessage = "";
    float Shared::deltaTime = 0.0f;
    ImVec2 Shared::displayPos{};
    ImVec2 Shared::displaySize{};
    float Shared::maxFramerate = 60;
    int Shared::renderCalls = 0;
    int Shared::compositorCalls = 0;
    std::string Shared::defaultImageLayer = "sdf2d_layer";
    std::string Shared::defaultAudioLayer = "audio_layer";
    std::string Shared::assetManagerDragDropType;
    uint64_t Shared::frameID = 0;
}
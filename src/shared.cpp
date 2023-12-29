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

    AppInstance* Shared::app = nullptr;
    GraphicsCore* Shared::graphics = nullptr;
    AssetRegistry* Shared::assets = nullptr;

    int Wavefront::x = 0;
    int Wavefront::y = 0;
    int Wavefront::z = 0;

    GLuint Shared::basic_compute = -1;
    GLuint Shared::compositor_compute = -1;
    GLuint Shared::tex_transfer_compute = -1;

    GLuint Shared::shadowTex = 0;

    int Shared::assetSelected = -128;
    std::string Shared::importErrorMessage = "";

    float Shared::deltaTime = 0.0f;
}
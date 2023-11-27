#pragma once

#include "electron.h"
#include "servers.h"

namespace Electron {

    struct ProjectMap {
        json_t propertiesMap;
        std::string path;

        ProjectMap() {};

        void SaveProject();
    };

    struct Wavefront {
        static int x, y, z;
    };

    class AppInstance;
    class GraphicsCore;
    class AssetRegistry;

    // Necessary variables that are not being 
    struct Shared {
        static int selectedRenderLayer;
        static ProjectMap project;
        static json_t localizationMap, configMap;
        static AppInstance* app;
        static GraphicsCore* graphics;
        static AssetRegistry* assets;

        static GLuint basic_compute, compositor_compute, tex_transfer_compute;
    };
}
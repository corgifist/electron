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

    // Needed for Waveform optimization
    struct Wavefront {
        static int x, y, z;
    };

    class AppInstance;
    class GraphicsCore;
    class AssetRegistry;

    // Important global variables
    struct Shared {
        static int selectedRenderLayer;
        static ProjectMap project;
        static json_t localizationMap, configMap;
        static AppInstance* app;
        static GraphicsCore* graphics;
        static AssetRegistry* assets;

        static GLuint basic_compute, compositor_compute, channel_manipulator_compute;
        static GLuint shadowTex;

        static int assetSelected;
        static std::string importErrorMessage;

        static float deltaTime;

        static GLuint fsVAO;

        static ImVec2 displayPos, displaySize;

        static std::string glslVersion;

        static float maxFramerate;
    };
}
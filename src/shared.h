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

    class AppCore;
    class GraphicsCore;
    class AssetCore;

    // Important global variables
    struct Shared {
        static int selectedRenderLayer;
        static ProjectMap project;
        static json_t localizationMap, configMap;
        static int assetSelected;
        static std::string importErrorMessage;
        static float deltaTime;
        static ImVec2 displayPos, displaySize;
        static float maxFramerate;
        static int renderCalls, compositorCalls;
        static uint64_t frameID;
        static std::string defaultImageLayer, defaultAudioLayer;
        static std::string assetManagerDragDropType;
        static std::string deviceName;
        static bool timelineSeekFired;
    };
}
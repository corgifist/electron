#pragma once

#include "electron.h"
#include "libraries.h"

#if defined(WIN32) || defined(WIN64)
    #define ELECTRON_EXPORT __declspec(dllexport)
    #define ELECTRON_IMPORT __declspec(dllimport)
#else
    #define ELECTRON_EXPORT
    #define ELECTRON_IMPORT
#endif

namespace Electron {

    class AppCore;

    struct ImplDylibs {
        static std::string ProjectConfigurationDylib;
        static std::string RenderPreviewDylib;
        static std::string LayerPropertiesDylib;
        static std::string AssetManagerDylib;
        static std::string DockspaceDylib;
    };


    struct UICounters {
        static int ProjectConfigurationCounter;
        static int RenderPreviewCounter;
        static int LayerPropertiesCounter;
        static int AssetManagerCounter;
        static int TimelineCounter;
        static int AssetExaminerCounter;
    };

    typedef void (*Electron_ImplF)();

    struct UIActivity {
        Electron_ImplF impl;
        int* counter;
        std::string libraryName;

        
        UIActivity(std::string libraryName, int* counter);
        ~UIActivity();

        void Render();
    };


#define TIMELINE_MAX_ZOOM 100.0f
#define TIMELINE_DEFAULT_ZOOM 5.0f
#define TIMELINE_MIN_ZOOM 0.1f
#define TIMELINE_SPLITTER_WIDTH 12.0f
#define TIMELINE_SPLITTER_RENDER_WIDTH 12.0f
#define TTIMELINE_RULLER_WIDTH 4.0f
#define TIMELINE_TICK_WIDTH 3.0f
#define TIMELINE_SCROLLBAR_SIZE ImVec2(50, 20)
}

#include "app.h"
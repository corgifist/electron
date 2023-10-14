#pragma once

#include "electron.h"
#include "dylib.hpp"

#if defined(WIN32) || defined(WIN64)
    #define ELECTRON_EXPORT __declspec(dllexport)
    #define ELECTRON_IMPORT __declspec(dllimport)
#else
    #define ELECTRON_EXPORT
    #define ELECTRON_IMPORT
#endif

namespace Electron {

    class AppInstance;
    class RenderPreview;
    class Timeline;

    enum class DragState : uint8_t {
	    None,
	    Hover,
	    Active,
    };

    enum class SegmentChangeType : uint8_t {
    	None,
    	Resized,
    	Moved
    };

    enum class ResizeFlags : uint8_t {
        EastWest = 0x1,
    	NorthSouth = 0x2
    };

#ifndef ELECTRON_IMPLEMENTATION_MODE
    struct ImplDylibs {
        static std::string ProjectConfigurationDylib;
        static std::string RenderPreviewDylib;
        static std::string LayerPropertiesDylib;
        static std::string AssetManagerDylib;
        static std::string DockspaceDylib;
    };

#endif

    struct UICounters {
        static int ProjectConfigurationCounter;
        static int RenderPreviewCounter;
        static int LayerPropertiesCounter;
        static int AssetManagerCounter;
        static int TimelineCounter;
    };

    class ElectronUI {
    public:
        virtual void Render(AppInstance* owner) = 0;
    };

    typedef void (*Electron_ImplF)(AppInstance*);
    typedef void (*Electron_RenderPreviewImplF)(AppInstance*, RenderPreview*);
    typedef void (*Electron_TimelineF)(AppInstance* owner, Timeline*);

    class ProjectConfiguration : public ElectronUI {
    private:
        bool pOpen;

        Electron_ImplF implFunction;

    public:
        ProjectConfiguration();
        ~ProjectConfiguration();

        void Render(AppInstance* instance);
    };

    class RenderPreview : public ElectronUI {
    private:
        Electron_RenderPreviewImplF implFunction;

    public:
        RenderPreview();
        ~RenderPreview();

        void Render(AppInstance* instance);
    };

    class LayerProperties : public ElectronUI {
    private:
        Electron_ImplF impl;
    
    public:
        LayerProperties();
        ~LayerProperties();

        void Render(AppInstance* instance);
    };

    class AssetManager : public ElectronUI {
    private:
        Electron_ImplF impl;

    public:
        AssetManager();
        ~AssetManager();

        void Render(AppInstance* instance);
    };

#define TIMELINE_MAX_ZOOM 100.0f
#define TIMELINE_DEFAULT_ZOOM 5.0f
#define TIMELINE_MIN_ZOOM 0.1f
#define TIMELINE_SPLITTER_WIDTH 12.0f
#define TIMELINE_SPLITTER_RENDER_WIDTH 12.0f
#define TTIMELINE_RULLER_WIDTH 4.0f
#define TIMELINE_TICK_WIDTH 3.0f
#define TIMELINE_SCROLLBAR_SIZE ImVec2(50, 20)
    class Timeline : public ElectronUI {    
    public:
        Timeline();

        void Render(AppInstance* instance);

        ~Timeline();
    };

    class Dockspace : public ElectronUI {
    public:
        Electron_ImplF impl;

        Dockspace();

        void Render(AppInstance* instance);

        ~Dockspace() {}
    };
}

#include "app.h"
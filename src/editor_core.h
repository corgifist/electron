#pragma once

#include "electron.h"
#include "dylib.hpp"

#if defined(WIN32) || defined(WIN64)
    #define ELECTRON_EXPORT __declspec(dllexport) __stdcall
    #define ELECTRON_IMPORT __declspec(dllimport)
#else
    #define ELECTRON_EXPORT
    #define ELECTRON_IMPORT
#endif

namespace Electron {

    class AppInstance;
    class RenderPreview;

    struct ImplDylibs {
        static dylib ProjectConfigurationDylib;
        static dylib RenderPreviewDylib;
    };


    static void InitializeDylibs() {
        ImplDylibs::ProjectConfigurationDylib = dylib(".", "project_configuration_impl");
        ImplDylibs::RenderPreviewDylib = dylib(".", "render_preview_impl");
    }

    static void DestroyDylibs() {
        ImplDylibs::ProjectConfigurationDylib = dylib();
        ImplDylibs::RenderPreviewDylib = dylib();
    }

    class ElectronUI {
    public:
        virtual void Render(AppInstance* owner) = 0;
    };


    typedef void (*Electron_ImplF)(AppInstance*);
    typedef void (*Electron_RenderPreviewImplF)(AppInstance*, RenderPreview*);

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
        LayerProperties() {}

        void Render(AppInstance* instance);
    };
}

#include "app.h"
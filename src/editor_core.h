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

#ifndef ELECTRON_IMPLEMENTATION_MODE
    struct ImplDylibs {
        static dylib ProjectConfigurationDylib;
        static dylib RenderPreviewDylib;
        static dylib LayerPropertiesDylib;
        static dylib AssetManagerDylib;
    };

    static void InitializeDylibs() {
        ImplDylibs::ProjectConfigurationDylib = dylib(".", "project_configuration_impl");
        ImplDylibs::RenderPreviewDylib = dylib(".", "render_preview_impl");
        ImplDylibs::LayerPropertiesDylib = dylib(".", "layer_properties_impl");
        ImplDylibs::AssetManagerDylib = dylib(".", "asset_manager_impl");
    }

    static void DestroyDylibs() {
        ImplDylibs::ProjectConfigurationDylib = dylib();
        ImplDylibs::RenderPreviewDylib = dylib();
        ImplDylibs::LayerPropertiesDylib = dylib();
        ImplDylibs::AssetManagerDylib = dylib();
    }


#endif


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
        LayerProperties();

        void Render(AppInstance* instance);
    };

    class AssetManager : public ElectronUI {
    private:
        Electron_ImplF impl;

    public:
        AssetManager();

        void Render(AppInstance* instance);
    };
}

#include "app.h"
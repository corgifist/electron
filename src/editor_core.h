#pragma once

#include "electron.h"

#define ELECTRON_EXPORT __declspec(dllexport) __stdcall
#define ELECTRON_IMPORT __declspec(dllimport)

namespace Electron {

    class AppInstance;
    class RenderPreview;

    class ElectronUI {
    public:
        virtual void Render(AppInstance* owner) = 0;
    };


    typedef void (__stdcall *Electron_ImplF)(AppInstance*);
    typedef void (__stdcall *Electron_RenderPreviewImplF)(AppInstance*, RenderPreview*);

    class ProjectConfiguration : public ElectronUI {
    private:
        bool pOpen;

        HINSTANCE impl;
        Electron_ImplF implFunction;

    public:
        ProjectConfiguration();
        ~ProjectConfiguration();

        void Render(AppInstance* instance);
    };

    class RenderPreview : public ElectronUI {
    private:
        HINSTANCE impl;
        Electron_RenderPreviewImplF implFunction;

    public:
        RenderPreview();
        ~RenderPreview();

        void Render(AppInstance* instance);
    };
}

#include "app.h"
#include "electron.h"

#define CSTR(str) str.c_str()

#define IMPLEMENT_UI_WRAPPER(ui_proc_name, args_signature, call_signature, typedef_args, fn_type) \
    typedef fn_type (__stdcall *ui_proc_name ## _T) typedef_args; \
    fn_type ui_proc_name args_signature { \
        HINSTANCE implLib = IMPL_LIBRARY; \
        if (!implLib) throw std::runtime_error(std::string("failed to load impl library in ui wrapper ") + #ui_proc_name); \
        ui_proc_name ## _T proc = (ui_proc_name ## _T) GetProcAddress(implLib, #ui_proc_name); \
        if (!proc) throw std::runtime_error(std::string("failed to load proc from impl library in ui wrapper ") + #ui_proc_name); \
        fn_type ret_val = proc call_signature; \
        return ret_val; \
    }

#define IMPLEMENT_UI_VOID_WRAPPER(ui_proc_name, args_signature, call_signature, typedef_args) \
    typedef void (__stdcall *ui_proc_name ## _T) typedef_args; \
    void ui_proc_name args_signature { \
        HINSTANCE implLib = IMPL_LIBRARY; \
        if (!implLib) throw std::runtime_error(std::string("failed to load impl library in ui wrapper ") + #ui_proc_name); \
        ui_proc_name ## _T proc = (ui_proc_name ## _T) GetProcAddress(implLib, #ui_proc_name); \
        if (!proc) throw std::runtime_error(std::string("failed to load proc from impl library in ui wrapper ") + #ui_proc_name); \
        proc call_signature; \
    }

static HINSTANCE IMPL_LIBRARY = GetModuleHandle(NULL);

IMPLEMENT_UI_WRAPPER(UIExportTest, (), (), (), float);

IMPLEMENT_UI_VOID_WRAPPER(UIBegin, (const char* name, Electron::ElectronSignal signal, ImGuiWindowFlags flags), (name, signal, flags), (const char*, Electron::ElectronSignal, ImGuiWindowFlags));
IMPLEMENT_UI_VOID_WRAPPER(UIBeginFlagless, (const char* name, Electron::ElectronSignal signal), (name, signal), (const char*, Electron::ElectronSignal));
IMPLEMENT_UI_VOID_WRAPPER(UIText, (const char* text), (text), (const char*));
IMPLEMENT_UI_VOID_WRAPPER(UIEnd, (), (), ());

IMPLEMENT_UI_WRAPPER(UICalcTextSize, (const char* text), (text), (const char*), ImVec2);
IMPLEMENT_UI_WRAPPER(UIGetWindowSize, (), (), (), ImVec2);

IMPLEMENT_UI_VOID_WRAPPER(UISetCursorPos, (ImVec2 cursor), (cursor), (ImVec2));
IMPLEMENT_UI_VOID_WRAPPER(UISetCursorX, (float x), (x), (float));
IMPLEMENT_UI_VOID_WRAPPER(UISetCursorY, (float y), (y), (float));

IMPLEMENT_UI_WRAPPER(UIBeginMainMenuBar, (), (), (), bool);
IMPLEMENT_UI_VOID_WRAPPER(UIEndMainMenuBar, (), (), ());

IMPLEMENT_UI_WRAPPER(UIBeginMenu, (const char* menu), (menu), (const char*), bool);
IMPLEMENT_UI_VOID_WRAPPER(UIEndMenu, (), (), ());

IMPLEMENT_UI_WRAPPER(UIMenuItem, (const char* item, const char* shortcut), (item, shortcut), (const char*, const char*), bool);
IMPLEMENT_UI_WRAPPER(UIMenuItemShortcutless, (const char* item), (item), (const char*), bool);

IMPLEMENT_UI_WRAPPER(UIBeginMenuBar, (), (), (), bool);
IMPLEMENT_UI_VOID_WRAPPER(UIEndMenuBar, (), (), ());

IMPLEMENT_UI_VOID_WRAPPER(ElectronImplThrow, (Electron::ElectronSignal signal), (signal), (Electron::ElectronSignal));
IMPLEMENT_UI_VOID_WRAPPER(ElectronImplDirectSignal, (void* instance, Electron::ElectronSignal signal), (instance, signal), (void*, Electron::ElectronSignal));

IMPLEMENT_UI_VOID_WRAPPER(UIPushID, (void* ptr), (ptr), (void*));
IMPLEMENT_UI_VOID_WRAPPER(UIPopID, (), (), ());

IMPLEMENT_UI_VOID_WRAPPER(UIImage, (GLuint texture, Electron::ElectronVector2f size), (texture, size), (GLuint, Electron::ElectronVector2f));
IMPLEMENT_UI_VOID_WRAPPER(UISliderFloat, (const char* label, float* value, float min, float max, const char* format, ImGuiSliderFlags flags), (label, value, min, max, format, flags), (const char*, float*, float, float, const char*, ImGuiSliderFlags))

IMPLEMENT_UI_VOID_WRAPPER(UISetNextWindowSize, (Electron::ElectronVector2f size, ImGuiCond_ cond), (size, cond), (Electron::ElectronVector2f, ImGuiCond_));

IMPLEMENT_UI_VOID_WRAPPER(UISeparator, (), (), ());
IMPLEMENT_UI_VOID_WRAPPER(UISpacing, (), (), ());

IMPLEMENT_UI_WRAPPER(UICollapsingHeader, (const char* name), (name), (const char*), bool);

IMPLEMENT_UI_VOID_WRAPPER(UIPushFont, (ImFont* font), (font), (ImFont*));
IMPLEMENT_UI_VOID_WRAPPER(UIPopFont, (), (), ());

IMPLEMENT_UI_VOID_WRAPPER(GraphicsImplRequestRenderWithinRegion, (void* instance, Electron::RenderRequestMetadata metadata), (instance, metadata), (void*, Electron::RenderRequestMetadata));
IMPLEMENT_UI_WRAPPER(PixelBufferImplBuildGPUTexture, (Electron::PixelBuffer& buffer), (buffer), (Electron::PixelBuffer&), GLuint);
IMPLEMENT_UI_VOID_WRAPPER(GraphicsImplCleanPreviewGPUTexture, (void* instance), (instance), (void*));
IMPLEMENT_UI_VOID_WRAPPER(GraphicsImplBuildPreviewGPUTexture, (void* instance), (instance), (void*));
IMPLEMENT_UI_WRAPPER(GraphicsImplGetPreviewGPUTexture, (void* instance), (instance), (void*), GLuint);
IMPLEMENT_UI_VOID_WRAPPER(PixelBufferImplSetFiltering, (int filtering), (filtering), (int));

IMPLEMENT_UI_WRAPPER(UIGetAvailZone, (), (), (), ImVec2);

IMPLEMENT_UI_WRAPPER(UIBeginCombo, (const char* label, const char* option), (label, option), (const char*, const char*), bool);
IMPLEMENT_UI_VOID_WRAPPER(UIEndCombo, (), (), ());

IMPLEMENT_UI_WRAPPER(UISelectable, (const char* text, bool& selected), (text, selected), (const char*, bool&), bool);

IMPLEMENT_UI_VOID_WRAPPER(UISetItemFocusDefault, (), (), ());

IMPLEMENT_UI_VOID_WRAPPER(UIInputField, (const char* label, std::string* string, ImGuiInputTextFlags flags), (label, string, flags), (const char*, std::string*, ImGuiInputTextFlags));
IMPLEMENT_UI_VOID_WRAPPER(UIInputInt2, (const char* label, int* ptr, ImGuiInputTextFlags flags), (label, ptr, flags), (const char*, int*, ImGuiInputTextFlags));
IMPLEMENT_UI_WRAPPER(ParentString, (const char* string), (string), (const char*), std::string);

IMPLEMENT_UI_VOID_WRAPPER(GraphicsImplResizeRenderBuffer, (void* instance, int width, int height), (instance, width, height), (void*, int, int));

IMPLEMENT_UI_WRAPPER(GraphicsImplPixelBufferFromImage, (void* instance, const char* filename), (instance, filename), (void*, const char*), Electron::PixelBuffer);

IMPLEMENT_UI_VOID_WRAPPER(FileDialogImplOpenDialog, (const char* internalName, const char* title, const char* extensions, const char* initialPath), (internalName, title, extensions, initialPath), (const char*, const char*, const char*, const char*));
IMPLEMENT_UI_WRAPPER(FileDialogImplDisplay, (const char* internalName), (internalName), (const char*), bool);
IMPLEMENT_UI_WRAPPER(FileDialogImplIsOK, (), (), (), bool);
IMPLEMENT_UI_WRAPPER(FileDialogImplGetFilePathName, (), (), (), const char*);
IMPLEMENT_UI_WRAPPER(FileDialogImplGetFilePath, (), (), (), const char*);
IMPLEMENT_UI_VOID_WRAPPER(FileDialogImplClose, (), (), ());

IMPLEMENT_UI_VOID_WRAPPER(UIInputColor3, (const char* label, float* colors, ImGuiColorEditFlags flags), (label, colors, flags), (const char*, float*, ImGuiColorEditFlags));

IMPLEMENT_UI_VOID_WRAPPER(ShortcutsImplCtrlWR, (void* instance), (instance), (void*));
IMPLEMENT_UI_VOID_WRAPPER(ShortcutsImplCtrlPO, (void* instance, Electron::ProjectMap map), (instance, map), (void*, Electron::ProjectMap));
IMPLEMENT_UI_VOID_WRAPPER(ShortcutsImplCtrlPE, (void* instance), (instance), (void*));

static std::string ElectronImplTag(const char* name, void* ptr) {
    std::string temp = name;
    temp += "##";
    temp += std::to_string((long long) ptr);
    return temp;
}

/*
    typedef void (__stdcall *UIExportTest_T)();

    void UIExportTest() {
        HINSTANCE implLibrary = GetModuleHandle(NULL);
        if (!implLibrary) {
            print("FAILED! IMPL_LIBRARY");
        }

        UIExportTest_T proc = (UIExportTest_T) GetProcAddress(implLibrary, "UIExportTest");
        if (!proc) {
            print("FAILED PROC");
        }

        proc();

        FreeLibrary(implLibrary);
    }
*/
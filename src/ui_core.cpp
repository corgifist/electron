#include "ui_core.h"

float UIExportTest() {
    print("Hello, Export!");
    return 3.14;
}

void UIBegin(const char* name, Electron::ElectronSignal signal, ImGuiWindowFlags flags) {
    if (signal == Electron::ElectronSignal_None) {
        ImGui::Begin(name, nullptr, flags);
    } else {
        bool pOpen = true;
        ImGui::Begin(name, &pOpen, flags);
        if (!pOpen) {
            if (signal == Electron::ElectronSignal_CloseWindow) {
                ImGui::End();
            }
            throw signal;
        }
    }
}

void UIBeginFlagless(const char* name, Electron::ElectronSignal signal) {
    UIBegin(name, signal, 0);
}

void UIText(const char* text) {
    ImGui::Text(text);
}

void UIEnd() {
    ImGui::End();
}

ImVec2 UICalcTextSize(const char* text) {
    return ImGui::CalcTextSize(text);
}

ImVec2 UIGetWindowSize() {
    return ImGui::GetWindowSize();
}

void UISetCursorPos(ImVec2 cursor) {
    ImGui::SetCursorPos(cursor);
}

void UISetCursorX(float x) {
    ImGui::SetCursorPosX(x);
}

void UISetCursorY(float y) {
    ImGui::SetCursorPosY(y);
}

bool UIBeginMainMenuBar() {
    return ImGui::BeginMainMenuBar();
}

void UIEndMainMenuBar() {
    ImGui::EndMainMenuBar();
}

bool UIBeginMenu(const char* menu) {
    return ImGui::BeginMenu(menu);
}

void UIEndMenu() {
    return ImGui::EndMenu();
}

bool UIMenuItem(const char* menu, const char* shortcut) {
    return ImGui::MenuItem(menu, shortcut);
}

bool UIMenuItemShortcutless(const char* menu) {
    return ImGui::MenuItem(menu);
}

bool UIBeginMenuBar() {
    return ImGui::BeginMenuBar();
}

void UIEndMenuBar() {
    ImGui::EndMenuBar();
}

void UIPushID(void* ptr) {
    ImGui::PushID(ptr);
}

void UIPopID() {
    ImGui::PopID();
}

void UIImage(GLuint texture, Electron::ElectronVector2f size) {
    ImGui::Image((void*)(intptr_t) texture, ImVec2{size.x, size.y});
}

void UISliderFloat(const char* label, float* value, float min, float max, const char* format, ImGuiSliderFlags flags) {
    ImGui::SliderFloat(label, value, min, max, format, flags);
}

void UISeparator() {
    ImGui::Separator();
}

void UISpacing() {
    ImGui::Spacing();
}

bool UICollapsingHeader(const char* name) {
    return ImGui::CollapsingHeader(name);
}

void UISetNextWindowSize(Electron::ElectronVector2f size, ImGuiCond_ cond) {
    ImGui::SetNextWindowSize(ImVec2{size.x, size.y}, cond);
}

void UIPushFont(ImFont* font) {
    ImGui::PushFont(font);
}

void UIPopFont() {
    ImGui::PopFont();
}

void ElectronImplThrow(Electron::ElectronSignal signal) {
    throw signal;
}

void ElectronImplDirectSignal(void* instance, Electron::ElectronSignal signal) {
    int ref;
    bool bRef;
    ((Electron::AppInstance*) instance)->ExecuteSignal(signal, ref, ref, bRef);
}

void GraphicsImplRequestRenderWithinRegion(void* instance, Electron::RenderRequestMetadata metadata) {
    ((Electron::AppInstance*) instance)->graphics.RequestRenderWithinRegion(metadata);
}

GLuint PixelBufferImplBuildGPUTexture(Electron::PixelBuffer& buffer) {
    return buffer.BuildGPUTexture();
}

void GraphicsImplCleanPreviewGPUTexture(void* instance) {
    ((Electron::AppInstance*) instance)->graphics.CleanPreviewGPUTexture();
}

void GraphicsImplBuildPreviewGPUTexture(void* instance) {
    ((Electron::AppInstance*) instance)->graphics.BuildPreviewGPUTexture();
}

GLuint GraphicsImplGetPreviewGPUTexture(void* instance) {
    return ((Electron::AppInstance*) instance)->graphics.renderBufferTexture;
}

void PixelBufferImplSetFiltering(int filtering) {
    Electron::PixelBuffer::filtering = filtering;
}

ImVec2 UIGetAvailZone() {
    return ImGui::GetContentRegionAvail();
}

bool UIBeginCombo(const char* label, const char* option) {
    return ImGui::BeginCombo(label, option);
}

void UIEndCombo() {
    ImGui::EndCombo();
}

bool UISelectable(const char* text, bool& selected) {
    return ImGui::Selectable(text, &selected);
}

void UISetItemFocusDefault() {
    ImGui::SetItemDefaultFocus();
}

void GraphicsImplResizeRenderBuffer(void* instance, int width, int height) {
    ((Electron::AppInstance*) instance)->graphics.ResizeRenderBuffer(width, height);
}

void FileDialogImplOpenDialog(const char* internalName, const char* title, const char* extensions, const char* initialPath) {
    ImGuiFileDialog::Instance()->OpenDialog(internalName, title, extensions, initialPath);
}

bool FileDialogImplDisplay(const char* internalName) {
    return ImGuiFileDialog::Instance()->Display(internalName);
}

bool FileDialogImplIsOK() {
    return ImGuiFileDialog::Instance()->IsOk();
}

const char* FileDialogImplGetFilePathName() {
    return ImGuiFileDialog::Instance()->GetFilePathName().c_str();
}

const char* FileDialogImplGetFilePath() {
    return ImGuiFileDialog::Instance()->GetCurrentFileName().c_str();
}

void FileDialogImplClose() {
    ImGuiFileDialog::Instance()->Close();
}

void UIInputField(const char* label, std::string* string, ImGuiInputTextFlags flags) {
    ImGui::InputText(label, string, flags);
}

void UIInputInt2(const char* label, int* ptr, ImGuiInputTextFlags flags) {
    ImGui::InputInt2(label, ptr, flags);
}

Electron::PixelBuffer GraphicsImplPixelBufferFromImage(void* instance, const char* filename) {
    return ((Electron::AppInstance*) instance)->graphics.CreateBufferFromImage(filename);
}

std::string ParentString(const char* string) {
    return std::string(string);
}

void UIInputColor3(const char* label, float* color, ImGuiColorEditFlags flags) {
    ImGui::ColorEdit3(label, color, flags);
}

void ShortcutsImplCtrlWR(void* instance) {
    SHORTCUT(Ctrl_W_R)();
}

void ShortcutsImplCtrlPO(void* instance, Electron::ProjectMap projectMap) {
    SHORTCUT(Ctrl_P_O)(projectMap);
}

void ShortcutsImplCtrlPE(void* instance) {
    SHORTCUT(Ctrl_P_E)();
}
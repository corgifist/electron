#include "ui_core.h"

#define HEADER_TARGET

HEADER_TARGET float UIExportTest() {
    print("Hello, Export!");
    return 3.14;
}

HEADER_TARGET void UIBegin(const char* name, Electron::ElectronSignal signal, ImGuiWindowFlags flags) {
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

HEADER_TARGET void UIBeginFlagless(const char* name, Electron::ElectronSignal signal) {
    UIBegin(name, signal, 0);
}

HEADER_TARGET void UIText(const char* text) {
    ImGui::Text(text);
}

HEADER_TARGET void UIEnd() {
    ImGui::End();
}

HEADER_TARGET ImVec2 UICalcTextSize(const char* text) {
    return ImGui::CalcTextSize(text);
}

HEADER_TARGET ImVec2 UIGetWindowSize() {
    return ImGui::GetWindowSize();
}

HEADER_TARGET void UISetCursorPos(ImVec2 cursor) {
    ImGui::SetCursorPos(cursor);
}

HEADER_TARGET void UISetCursorX(float x) {
    ImGui::SetCursorPosX(x);
}

HEADER_TARGET void UISetCursorY(float y) {
    ImGui::SetCursorPosY(y);
}

HEADER_TARGET bool UIBeginMainMenuBar() {
    return ImGui::BeginMainMenuBar();
}

HEADER_TARGET void UIEndMainMenuBar() {
    ImGui::EndMainMenuBar();
}

HEADER_TARGET bool UIBeginMenu(const char* menu) {
    return ImGui::BeginMenu(menu);
}

HEADER_TARGET void UIEndMenu() {
    return ImGui::EndMenu();
}

HEADER_TARGET bool UIMenuItem(const char* menu, const char* shortcut) {
    return ImGui::MenuItem(menu, shortcut);
}

HEADER_TARGET bool UIMenuItemShortcutless(const char* menu) {
    return ImGui::MenuItem(menu);
}

HEADER_TARGET bool UIBeginMenuBar() {
    return ImGui::BeginMenuBar();
}

HEADER_TARGET void UIEndMenuBar() {
    ImGui::EndMenuBar();
}

HEADER_TARGET void UIPushID(void* ptr) {
    ImGui::PushID(ptr);
}

HEADER_TARGET void UIPopID() {
    ImGui::PopID();
}

HEADER_TARGET void UIImage(GLuint texture, Electron::ElectronVector2f size) {
    ImGui::Image((void*)(intptr_t) texture, ImVec2{size.x, size.y});
}

HEADER_TARGET void UISliderFloat(const char* label, float* value, float min, float max, const char* format, ImGuiSliderFlags flags) {
    ImGui::SliderFloat(label, value, min, max, format, flags);
}

HEADER_TARGET void UISeparator() {
    ImGui::Separator();
}

HEADER_TARGET void UISpacing() {
    ImGui::Spacing();
}

HEADER_TARGET bool UICollapsingHeader(const char* name) {
    return ImGui::CollapsingHeader(name);
}

HEADER_TARGET void UISetNextWindowSize(Electron::ElectronVector2f size, ImGuiCond_ cond) {
    ImGui::SetNextWindowSize(ImVec2{size.x, size.y}, cond);
}

HEADER_TARGET void UIPushFont(ImFont* font) {
    ImGui::PushFont(font);
}

HEADER_TARGET void UIPopFont() {
    ImGui::PopFont();
}

HEADER_TARGET void ElectronImplThrow(Electron::ElectronSignal signal) {
    throw signal;
}

HEADER_TARGET void ElectronImplDirectSignal(void* instance, Electron::ElectronSignal signal) {
    int ref;
    bool bRef;
    ((Electron::AppInstance*) instance)->ExecuteSignal(signal, ref, ref, bRef);
}

HEADER_TARGET void GraphicsImplRequestRenderWithinRegion(void* instance, Electron::RenderRequestMetadata metadata) {
    ((Electron::AppInstance*) instance)->graphics.RequestRenderWithinRegion(metadata);
}

HEADER_TARGET GLuint PixelBufferImplBuildGPUTexture(Electron::PixelBuffer& buffer) {
    return buffer.BuildGPUTexture();
}

HEADER_TARGET void GraphicsImplCleanPreviewGPUTexture(void* instance) {
    ((Electron::AppInstance*) instance)->graphics.CleanPreviewGPUTexture();
}

HEADER_TARGET void GraphicsImplBuildPreviewGPUTexture(void* instance) {
    ((Electron::AppInstance*) instance)->graphics.BuildPreviewGPUTexture();
}

HEADER_TARGET GLuint GraphicsImplGetPreviewGPUTexture(void* instance) {
    return ((Electron::AppInstance*) instance)->graphics.renderBufferTexture;
}

HEADER_TARGET void PixelBufferImplSetFiltering(int filtering) {
    Electron::PixelBuffer::filtering = filtering;
}

HEADER_TARGET ImVec2 UIGetAvailZone() {
    return ImGui::GetContentRegionAvail();
}

HEADER_TARGET bool UIBeginCombo(const char* label, const char* option) {
    return ImGui::BeginCombo(label, option);
}

HEADER_TARGET void UIEndCombo() {
    ImGui::EndCombo();
}

HEADER_TARGET bool UISelectable(const char* text, bool& selected) {
    return ImGui::Selectable(text, &selected);
}

HEADER_TARGET void UISetItemFocusDefault() {
    ImGui::SetItemDefaultFocus();
}

HEADER_TARGET void GraphicsImplResizeRenderBuffer(void* instance, int width, int height) {
    ((Electron::AppInstance*) instance)->graphics.ResizeRenderBuffer(width, height);
}

HEADER_TARGET void FileDialogImplOpenDialog(const char* internalName, const char* title, const char* extensions, const char* initialPath) {
    ImGuiFileDialog::Instance()->OpenDialog(internalName, title, extensions, initialPath);
}

HEADER_TARGET bool FileDialogImplDisplay(const char* internalName) {
    return ImGuiFileDialog::Instance()->Display(internalName);
}

HEADER_TARGET bool FileDialogImplIsOK() {
    return ImGuiFileDialog::Instance()->IsOk();
}

HEADER_TARGET const char* FileDialogImplGetFilePathName() {
    return ImGuiFileDialog::Instance()->GetFilePathName().c_str();
}

HEADER_TARGET const char* FileDialogImplGetFilePath() {
    return ImGuiFileDialog::Instance()->GetCurrentFileName().c_str();
}

HEADER_TARGET void FileDialogImplClose() {
    ImGuiFileDialog::Instance()->Close();
}

HEADER_TARGET void UIInputField(const char* label, std::string* string, ImGuiInputTextFlags flags) {
    ImGui::InputText(label, string, flags);
}

HEADER_TARGET void UIInputInt2(const char* label, int* ptr, ImGuiInputTextFlags flags) {
    ImGui::InputInt2(label, ptr, flags);
}

HEADER_TARGET Electron::PixelBuffer GraphicsImplPixelBufferFromImage(void* instance, const char* filename) {
    return ((Electron::AppInstance*) instance)->graphics.CreateBufferFromImage(filename);
}

HEADER_TARGET std::string ParentString(const char* string) {
    return std::string(string);
}

HEADER_TARGET void UIInputColor3(const char* label, float* color, ImGuiColorEditFlags flags) {
    ImGui::ColorEdit3(label, color, flags);
}

HEADER_TARGET void UISetWindowSize(ImVec2 size, ImGuiCond cond) {
    ImGui::SetWindowSize(size, cond);
}

HEADER_TARGET void UISetWindowPos(ImVec2 pos, ImGuiCond cond) {
    ImGui::SetWindowPos(pos, cond);
}

HEADER_TARGET Electron::ElectronVector2f ElectronImplGetNativeWindowSize(void* instance) {
    return ((Electron::AppInstance*) instance)->GetNativeWindowSize();
}

HEADER_TARGET Electron::ElectronVector2f ElectronImplGetNativeWindowPos(void* instance) {
    return ((Electron::AppInstance*) instance)->GetNativeWindowPos();
}

HEADER_TARGET ImGuiDockNode* UIDockBuilderGetNode(ImGuiID id) {
    return ImGui::DockBuilderGetNode(id);
}

HEADER_TARGET ImGuiID UIGetWindowDockID() {
    return ImGui::GetWindowDockID();
}

HEADER_TARGET void UIDockBuilderSetNodeSize(ImGuiID id, ImVec2 size) {
    ImGui::DockBuilderSetNodeSize(id, size);
}

HEADER_TARGET ImGuiID UIDockSpaceOverViewport(const ImGuiViewport* viewport, ImGuiDockNodeFlags flags, const ImGuiWindowClass* window_class) {
    return ImGui::DockSpaceOverViewport(viewport, flags, window_class);
}

HEADER_TARGET void UISetNextWindowDockID(ImGuiID id, ImGuiCond cond) {
    ImGui::SetNextWindowDockID(id, cond);
}

HEADER_TARGET ImGuiID UIGetID(const char* name) {
    return ImGui::GetID(name);
}

HEADER_TARGET ImGuiID UIDockSpace(ImGuiID id, const ImVec2& size, ImGuiDockNodeFlags flag, const ImGuiWindowClass* window_class) {
    return ImGui::DockSpace(id, size, flag, window_class);
}

HEADER_TARGET ImGuiViewport* UIGetViewport() {
    return ImGui::GetMainViewport();
}

HEADER_TARGET bool UIBeginTabBar(const char* str_id, ImGuiTabBarFlags flags) {
    return ImGui::BeginTabBar(str_id, flags);
}

HEADER_TARGET void UIEndTabBar() {
    ImGui::EndTabBar();
}

HEADER_TARGET bool UIBeginTabItem(const char* str_id, bool* p_open, ImGuiTabItemFlags flags) {
    return ImGui::BeginTabItem(str_id, p_open, flags);
}

HEADER_TARGET void UIEndTabItem() {
    ImGui::EndTabItem();
}

HEADER_TARGET bool UIIsItemHovered(ImGuiHoveredFlags flags) {
    return ImGui::IsItemHovered(flags);
}

HEADER_TARGET void UIPushItemWidth(float width) {
    ImGui::PushItemWidth(width);
}

HEADER_TARGET void UIPopItemWidth() {
    ImGui::PopItemWidth();
}

HEADER_TARGET void UICheckbox(const char* label, bool* v) {
    ImGui::Checkbox(label, v);
}

HEADER_TARGET void UISameLine() {
    ImGui::SameLine();
}

HEADER_TARGET void UIInputInt(const char* label, int* ptr, int step, int step_fast, ImGuiInputTextFlags flags) {
    ImGui::InputInt(label, ptr, step, step_fast, flags);
}

HEADER_TARGET void UISetTooltip(const char* tooltip) {
    ImGui::SetTooltip(tooltip);
}

HEADER_TARGET Electron::PixelBuffer& GraphicsImplGetPreviewBufferByOutputType(void* instance) {
    return ((Electron::AppInstance*) instance)->graphics.GetPreviewBufferByOutputType();
}

HEADER_TARGET void ShortcutsImplCtrlWR(void* instance) {
    SHORTCUT(Ctrl_W_R)();
}

HEADER_TARGET void ShortcutsImplCtrlPO(void* instance, Electron::ProjectMap projectMap) {
    SHORTCUT(Ctrl_P_O)(projectMap);
}

HEADER_TARGET void ShortcutsImplCtrlPE(void* instance) {
    SHORTCUT(Ctrl_P_E)();
}
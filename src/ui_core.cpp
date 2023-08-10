#include "ui_core.h"

#ifdef WIN32
    #define HEADER_TARGET __declspec(dllexport)
#else
    #define HEADER_TARGET
#endif

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

HEADER_TARGET bool UIMenuItemEnhanced(const char* menu, const char* shortcut, bool enabled) {
    return ImGui::MenuItem(menu, shortcut, false, enabled);
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

HEADER_TARGET void UIImage(GLuint texture, ImVec2 size) {
    ImGui::Image((void*)(intptr_t) texture, size);
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

HEADER_TARGET void UIDummy(ImVec2 size) {
    ImGui::Dummy(size);
}

HEADER_TARGET void TextureUnionImplRebuildAssetData(Electron::TextureUnion* _union, Electron::GraphicsCore* owner) {
    _union->RebuildAssetData(owner);
}

HEADER_TARGET bool UICollapsingHeader(const char* name) {
    return ImGui::CollapsingHeader(name);
}

HEADER_TARGET void UISetNextWindowSize(ImVec2 size, ImGuiCond cond) {
    ImGui::SetNextWindowSize(size, cond);
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

HEADER_TARGET std::vector<float> GraphicsImplRequestRenderWithinRegion(void* instance, Electron::RenderRequestMetadata metadata) {
    return ((Electron::AppInstance*) instance)->graphics.RequestRenderWithinRegion(metadata);
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
    ImGuiFileDialog::Instance()->OpenDialog(internalName, title, extensions, initialPath, 1, nullptr, ImGuiFileDialogFlags_Modal);
}

HEADER_TARGET bool FileDialogImplDisplay(const char* internalName) {
    return ImGuiFileDialog::Instance()->Display(internalName, 32 | ImGuiWindowFlags_NoDocking);
}

HEADER_TARGET bool FileDialogImplIsOK() {
    return ImGuiFileDialog::Instance()->IsOk();
}

HEADER_TARGET std::string FileDialogImplGetFilePathName() {
    return ImGuiFileDialog::Instance()->GetFilePathName();
}

HEADER_TARGET std::string FileDialogImplGetFilePath() {
    return ImGuiFileDialog::Instance()->GetCurrentFileName();
}

HEADER_TARGET void FileDialogImplClose() {
    ImGuiFileDialog::Instance()->Close();
}

HEADER_TARGET bool UIInputField(const char* label, std::string* string, ImGuiInputTextFlags flags) {
    return ImGui::InputText(label, string, flags);
}

HEADER_TARGET std::string AssetManagerImplImportAsset(Electron::AssetRegistry* registry, std::string path) {
    return registry->ImportAsset(path);
}

HEADER_TARGET bool UICenteredButton(Electron::AppInstance* instance, const char* label) {
    return instance->ButtonCenteredOnLine(label);
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

HEADER_TARGET void UIIndent() {
    ImGui::Indent();
}

HEADER_TARGET void UIUnindent() {
    ImGui::Unindent();
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

HEADER_TARGET GLuint GraphicsImplGetPreviewBufferByOutputType(void* instance) {
    return ((Electron::AppInstance*) instance)->graphics.GetPreviewBufferByOutputType();
}

HEADER_TARGET bool UIBeginTooltip() {
    return ImGui::BeginTooltip();
}

HEADER_TARGET void UIEndTooltip() {
    ImGui::EndTooltip();
}

HEADER_TARGET void UISetNextWindowPos(ImVec2 size, ImGuiCond cond) {
    ImGui::SetNextWindowSize(size, cond);
}

HEADER_TARGET bool UIButton(const char* text) {
    return ImGui::Button(text);
}

HEADER_TARGET ImVec2 UIGetDisplaySize() {
    return ImGui::GetIO().DisplaySize;
}

HEADER_TARGET void UISetItemTooltip(const char* tooltip) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip(tooltip);
    }
}

HEADER_TARGET void RenderLayerImplRenderProperty(Electron::RenderLayer* layer, Electron::GeneralizedPropertyType type, Electron::json_t& property, std::string name) {
    layer->RenderProperty(type, property, name);
}

HEADER_TARGET Electron::json_t RenderLayerImplInterpolateProperty(Electron::RenderLayer* layer, Electron::json_t property) {
    return layer->InterpolateProperty(property);
}

HEADER_TARGET void PixelBufferImplSetPixel(Electron::PixelBuffer* buffer, int x, int y, Electron::Pixel pixel) {
    buffer->SetPixel(x, y, pixel);
}

HEADER_TARGET bool RectContains(Electron::Rect rect, Electron::Point point) {
    return rect.contains(point);
}

HEADER_TARGET void RenderLayerImplSortKeyframes(Electron::RenderLayer* layer, Electron::json_t& keyframes) {
    layer->SortKeyframes(keyframes);
}

HEADER_TARGET void RenderLayerImplRenderProperties(Electron::RenderLayer* layer) {
    layer->RenderProperties();
}

HEADER_TARGET void UIPushColor(ImGuiCol col, ImVec4 color) {
    ImGui::PushStyleColor(col, color);
}

HEADER_TARGET void UIPopColor() {
    ImGui::PopStyleColor();
}

HEADER_TARGET void UISetNextWindowViewport(ImGuiID viewport) {
    ImGui::SetNextWindowViewport(viewport);
}

HEADER_TARGET void UIPushStyleVarF(ImGuiStyleVar var, float f) {
    ImGui::PushStyleVar(var, f);
}

HEADER_TARGET void UIPushStyleVarV2(ImGuiStyleVar var, ImVec2 vec) {
    ImGui::PushStyleVar(var, vec);
}

HEADER_TARGET void UIPopStyleVar() {
    ImGui::PopStyleVar();
}

HEADER_TARGET void UIPopStyleVarIter(int count) {
    ImGui::PopStyleVar(count);
}

HEADER_TARGET void PixelBufferImplDestroyGPUTexture(GLuint texture) {
    Electron::PixelBuffer::DestroyGPUTexture(texture);
}

HEADER_TARGET void ShortcutsImplCtrlWL(void* instance) {
    SHORTCUT(Ctrl_W_L)();
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

HEADER_TARGET void ShortcutsImplCtrlWA(void* instance) {
    SHORTCUT(Ctrl_W_A)();
}

HEADER_TARGET std::string ClipGetText() {
    std::string value;
    clip::get_text(value);
    return value;
}

HEADER_TARGET void ClipSetText(std::string text) {
    clip::set_text(text);
}

HEADER_TARGET int CounterGetProjectConfiguration() {
    return Electron::UICounters::ProjectConfigurationCounter;
}

HEADER_TARGET int CounterGetRenderPreview() {
    return Electron::UICounters::RenderPreviewCounter;
}

HEADER_TARGET int CounterGetLayerProperties() {
    return Electron::UICounters::LayerPropertiesCounter;
}

HEADER_TARGET int CounterGetAssetManager() {
    return Electron::UICounters::AssetManagerCounter;
}

HEADER_TARGET Electron::Pixel PixelBufferImplGetPixel(Electron::PixelBuffer* pbo, int x, int y) {
    return pbo->GetPixel(x, y);
}

HEADER_TARGET float UIBeginDisabled() {
    float savedAlpha = ImGui::GetStyle().Alpha;
    ImGui::GetStyle().Alpha = 0.25f;
    return savedAlpha;
}

HEADER_TARGET void UIEndDisabled(float alpha) {
    ImGui::GetStyle().Alpha = alpha;
}

HEADER_TARGET void GraphicsImplRequestRenderBufferCleaningWithingRegion(Electron::AppInstance* instance, Electron::RenderRequestMetadata metadata) {
    instance->graphics.RequestRenderBufferCleaningWithinRegion(metadata);
}

HEADER_TARGET void UISliderInt(const char* label, int* v, int v_min, int v_max, const char* format, ImGuiSliderFlags flags) {
    ImGui::SliderInt(label, v, v_min, v_max, format, flags);
}

HEADER_TARGET GLuint GraphicsImplCompileComputeShader(Electron::GraphicsCore* graphics, std::string path) {
    return graphics->CompileComputeShader(path);
}

HEADER_TARGET void GraphicsImplUseShader(Electron::GraphicsCore* core, GLuint shader) {
    core->UseShader(shader);
}

HEADER_TARGET void GraphicsImplDispatchComputeShader(Electron::GraphicsCore* core, int grid_x, int grid_y, int grid_z) {
    core->DispatchComputeShader(grid_x, grid_y, grid_z);
}

HEADER_TARGET void GraphicsImplBindGPUTexture(Electron::GraphicsCore* core, GLuint texture, int unit) {
    core->BindGPUTexture(texture, unit);
}

HEADER_TARGET GLuint GraphicsImplGenerateGPUTexture(Electron::GraphicsCore* core, int width, int height, int unit) {
    return core->GenerateGPUTexture(width, height, unit);
}

HEADER_TARGET void GraphicsImplMemoryBarrier(Electron::GraphicsCore* core, GLbitfield barrier) {
    core->ComputeMemoryBarier(barrier);
}

HEADER_TARGET Electron::PixelBuffer GraphicsImplPBOFromGPUTexture(Electron::GraphicsCore* core, GLuint texture, int width, int height) {
    return core->PBOFromGPUTexture(texture, width, height);
}


HEADER_TARGET void GraphicsImplShaderSetUniformII(Electron::GraphicsCore* core, GLuint program, std::string name, int x, int y) {
    core->ShaderSetUniform(program, name, x, y);
}

HEADER_TARGET Electron::ResizableGPUTexture ResizableGPUTextureCreate(Electron::GraphicsCore* core, int width, int height) {
    return Electron::ResizableGPUTexture(core, width, height);
}

HEADER_TARGET void ResizableGPUTImplCheckForResize(Electron::ResizableGPUTexture* texture, Electron::RenderBuffer* pbo) {
    return texture->CheckForResize(pbo);
}

HEADER_TARGET void GraphicsImplCallCompositor(Electron::GraphicsCore* core, Electron::ResizableGPUTexture color, Electron::ResizableGPUTexture uv, Electron::ResizableGPUTexture depth) {
    core->CallCompositor(color, uv, depth);
}

HEADER_TARGET void GraphicsImplShaderSetUniformFF(Electron::GraphicsCore* core, GLuint program, std::string name, glm::vec2 vec2) {
    core->ShaderSetUniform(program, name, vec2);
}

HEADER_TARGET void GraphicsImplShaderSetUniformFFF(Electron::GraphicsCore* core, GLuint program, std::string name, glm::vec3 vec) {
    core->ShaderSetUniform(program, name, vec);
}

HEADER_TARGET void GraphicsImplShaderSetUniformF(Electron::GraphicsCore* core, GLuint program, std::string name, float f) {
    core->ShaderSetUniform(program, name, f);
}

HEADER_TARGET void GraphicsImplRequestTextureCollectionCleaning(Electron::GraphicsCore* core, GLuint color, GLuint uv, GLuint depth, int width, int height, Electron::RenderRequestMetadata metadata) {
    core->RequestTextureCollectionCleaning(color, uv, depth, width, height, metadata);
}
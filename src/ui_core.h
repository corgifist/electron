
#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS

#include "electron.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_stdlib.h"
#include "editor_core.h"
#include "graphics_core.h"
#include "ImGuiFileDialog.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/implot.h"

#if defined(WIN32) || defined(WIN64)
    #define UI_EXPORT __declspec(dllexport) __stdcall
#else
    #define UI_EXPORT
#endif
#define SHORTCUT(shortcut) ((Electron::AppInstance*) instance)->shortcuts. shortcut

extern "C" {


UI_EXPORT  float UIExportTest() ;
UI_EXPORT  void UIBegin(const char* name, Electron::ElectronSignal signal, ImGuiWindowFlags flags) ;
UI_EXPORT  void UIBeginFlagless(const char* name, Electron::ElectronSignal signal) ;
UI_EXPORT  void UIText(const char* text) ;
UI_EXPORT  void UIEnd() ;
UI_EXPORT  ImVec2 UICalcTextSize(const char* text) ;
UI_EXPORT  ImVec2 UIGetWindowSize() ;
UI_EXPORT  void UISetCursorPos(ImVec2 cursor) ;
UI_EXPORT  void UISetCursorX(float x) ;
UI_EXPORT  void UISetCursorY(float y) ;
UI_EXPORT  bool UIBeginMainMenuBar() ;
UI_EXPORT  void UIEndMainMenuBar() ;
UI_EXPORT  bool UIBeginMenu(const char* menu) ;
UI_EXPORT  void UIEndMenu() ;
UI_EXPORT  bool UIMenuItem(const char* menu, const char* shortcut) ;
UI_EXPORT  bool UIMenuItemEnhanced(const char* menu, const char* shortcut, bool enabled) ;
UI_EXPORT  bool UIMenuItemShortcutless(const char* menu) ;
UI_EXPORT  bool UIBeginMenuBar() ;
UI_EXPORT  void UIEndMenuBar() ;
UI_EXPORT  void UIPushID(void* ptr) ;
UI_EXPORT  void UIPopID() ;
UI_EXPORT  void UIImage(GLuint texture, ImVec2 size) ;
UI_EXPORT  void UISliderFloat(const char* label, float* value, float min, float max, const char* format, ImGuiSliderFlags flags) ;
UI_EXPORT  void UISeparator() ;
UI_EXPORT  void UISpacing() ;
UI_EXPORT  void UIDummy(ImVec2 size) ;
UI_EXPORT  void TextureUnionImplRebuildAssetData(Electron::TextureUnion* _union, Electron::GraphicsCore* owner) ;
UI_EXPORT  bool UICollapsingHeader(const char* name) ;
UI_EXPORT  void UISetNextWindowSize(ImVec2 size, ImGuiCond cond) ;
UI_EXPORT  void UIPushFont(ImFont* font) ;
UI_EXPORT  void UIPopFont() ;
UI_EXPORT  void ElectronImplThrow(Electron::ElectronSignal signal) ;
UI_EXPORT  void ElectronImplDirectSignal(void* instance, Electron::ElectronSignal signal) ;
UI_EXPORT  std::vector<float> GraphicsImplRequestRenderWithinRegion(void* instance, Electron::RenderRequestMetadata metadata) ;
UI_EXPORT  GLuint PixelBufferImplBuildGPUTexture(Electron::PixelBuffer& buffer) ;
UI_EXPORT  void GraphicsImplCleanPreviewGPUTexture(void* instance) ;
UI_EXPORT  void GraphicsImplBuildPreviewGPUTexture(void* instance) ;
UI_EXPORT  GLuint GraphicsImplGetPreviewGPUTexture(void* instance) ;
UI_EXPORT  void PixelBufferImplSetFiltering(int filtering) ;
UI_EXPORT  ImVec2 UIGetAvailZone() ;
UI_EXPORT  bool UIBeginCombo(const char* label, const char* option) ;
UI_EXPORT  void UIEndCombo() ;
UI_EXPORT  bool UISelectable(const char* text, bool& selected) ;
UI_EXPORT  void UISetItemFocusDefault() ;
UI_EXPORT  void GraphicsImplResizeRenderBuffer(void* instance, int width, int height) ;
UI_EXPORT  void FileDialogImplOpenDialog(const char* internalName, const char* title, const char* extensions, const char* initialPath) ;
UI_EXPORT  bool FileDialogImplDisplay(const char* internalName) ;
UI_EXPORT  bool FileDialogImplIsOK() ;
UI_EXPORT  std::string FileDialogImplGetFilePathName() ;
UI_EXPORT  std::string FileDialogImplGetFilePath() ;
UI_EXPORT  void FileDialogImplClose() ;
UI_EXPORT  bool UIInputField(const char* label, std::string* string, ImGuiInputTextFlags flags) ;
UI_EXPORT  std::string AssetManagerImplImportAsset(Electron::AssetRegistry* registry, std::string path) ;
UI_EXPORT  bool UICenteredButton(Electron::AppInstance* instance, const char* label) ;
UI_EXPORT  void UIInputInt2(const char* label, int* ptr, ImGuiInputTextFlags flags) ;
UI_EXPORT  Electron::PixelBuffer GraphicsImplPixelBufferFromImage(void* instance, const char* filename) ;
UI_EXPORT  std::string ParentString(const char* string) ;
UI_EXPORT  void UIInputColor3(const char* label, float* color, ImGuiColorEditFlags flags) ;
UI_EXPORT  void UISetWindowSize(ImVec2 size, ImGuiCond cond) ;
UI_EXPORT  void UISetWindowPos(ImVec2 pos, ImGuiCond cond) ;
UI_EXPORT  void UIIndent() ;
UI_EXPORT  void UIUnindent() ;
UI_EXPORT  Electron::ElectronVector2f ElectronImplGetNativeWindowSize(void* instance) ;
UI_EXPORT  Electron::ElectronVector2f ElectronImplGetNativeWindowPos(void* instance) ;
UI_EXPORT  ImGuiDockNode* UIDockBuilderGetNode(ImGuiID id) ;
UI_EXPORT  ImGuiID UIGetWindowDockID() ;
UI_EXPORT  void UIDockBuilderSetNodeSize(ImGuiID id, ImVec2 size) ;
UI_EXPORT  ImGuiID UIDockSpaceOverViewport(const ImGuiViewport* viewport, ImGuiDockNodeFlags flags, const ImGuiWindowClass* window_class) ;
UI_EXPORT  void UISetNextWindowDockID(ImGuiID id, ImGuiCond cond) ;
UI_EXPORT  ImGuiID UIGetID(const char* name) ;
UI_EXPORT  ImGuiID UIDockSpace(ImGuiID id, const ImVec2& size, ImGuiDockNodeFlags flag, const ImGuiWindowClass* window_class) ;
UI_EXPORT  ImGuiViewport* UIGetViewport() ;
UI_EXPORT  bool UIBeginTabBar(const char* str_id, ImGuiTabBarFlags flags) ;
UI_EXPORT  void UIEndTabBar() ;
UI_EXPORT  bool UIBeginTabItem(const char* str_id, bool* p_open, ImGuiTabItemFlags flags) ;
UI_EXPORT  void UIEndTabItem() ;
UI_EXPORT  bool UIIsItemHovered(ImGuiHoveredFlags flags) ;
UI_EXPORT  void UIPushItemWidth(float width) ;
UI_EXPORT  void UIPopItemWidth() ;
UI_EXPORT  void UICheckbox(const char* label, bool* v) ;
UI_EXPORT  void UISameLine() ;
UI_EXPORT  void UIInputInt(const char* label, int* ptr, int step, int step_fast, ImGuiInputTextFlags flags) ;
UI_EXPORT  void UISetTooltip(const char* tooltip) ;
UI_EXPORT  GLuint GraphicsImplGetPreviewBufferByOutputType(void* instance) ;
UI_EXPORT  bool UIBeginTooltip() ;
UI_EXPORT  void UIEndTooltip() ;
UI_EXPORT  void UISetNextWindowPos(ImVec2 size, ImGuiCond cond) ;
UI_EXPORT  bool UIButton(const char* text) ;
UI_EXPORT  ImVec2 UIGetDisplaySize() ;
UI_EXPORT  void UISetItemTooltip(const char* tooltip) ;
UI_EXPORT  void RenderLayerImplRenderProperty(Electron::RenderLayer* layer, Electron::GeneralizedPropertyType type, Electron::json_t& property, std::string name) ;
UI_EXPORT  Electron::json_t RenderLayerImplInterpolateProperty(Electron::RenderLayer* layer, Electron::json_t property) ;
UI_EXPORT  void PixelBufferImplSetPixel(Electron::PixelBuffer* buffer, int x, int y, Electron::Pixel pixel) ;
UI_EXPORT  bool RectContains(Electron::Rect rect, Electron::Point point) ;
UI_EXPORT  void RenderLayerImplSortKeyframes(Electron::RenderLayer* layer, Electron::json_t& keyframes) ;
UI_EXPORT  void RenderLayerImplRenderProperties(Electron::RenderLayer* layer) ;
UI_EXPORT  void UIPushColor(ImGuiCol col, ImVec4 color) ;
UI_EXPORT  void UIPopColor() ;
UI_EXPORT  void UISetNextWindowViewport(ImGuiID viewport) ;
UI_EXPORT  void UIPushStyleVarF(ImGuiStyleVar var, float f) ;
UI_EXPORT  void UIPushStyleVarV2(ImGuiStyleVar var, ImVec2 vec) ;
UI_EXPORT  void UIPopStyleVar() ;
UI_EXPORT  void UIPopStyleVarIter(int count) ;
UI_EXPORT  void PixelBufferImplDestroyGPUTexture(GLuint texture) ;
UI_EXPORT  void ShortcutsImplCtrlWL(void* instance) ;
UI_EXPORT  void ShortcutsImplCtrlWR(void* instance) ;
UI_EXPORT  void ShortcutsImplCtrlPO(void* instance, Electron::ProjectMap projectMap) ;
UI_EXPORT  void ShortcutsImplCtrlPE(void* instance) ;
UI_EXPORT  void ShortcutsImplCtrlWA(void* instance) ;
UI_EXPORT  std::string ClipGetText() ;
UI_EXPORT  void ClipSetText(std::string text) ;
UI_EXPORT  int CounterGetProjectConfiguration() ;
UI_EXPORT  int CounterGetRenderPreview() ;
UI_EXPORT  int CounterGetLayerProperties() ;
UI_EXPORT  int CounterGetAssetManager() ;
UI_EXPORT  Electron::Pixel PixelBufferImplGetPixel(Electron::PixelBuffer* pbo, int x, int y) ;
UI_EXPORT  float UIBeginDisabled() ;
UI_EXPORT  void UIEndDisabled(float alpha) ;
UI_EXPORT  void GraphicsImplRequestRenderBufferCleaningWithingRegion(Electron::AppInstance* instance, Electron::RenderRequestMetadata metadata) ;
UI_EXPORT  void UISliderInt(const char* label, int* v, int v_min, int v_max, const char* format, ImGuiSliderFlags flags) ;
UI_EXPORT  GLuint GraphicsImplCompileComputeShader(Electron::GraphicsCore* graphics, std::string path) ;
UI_EXPORT  void GraphicsImplUseShader(Electron::GraphicsCore* core, GLuint shader) ;
UI_EXPORT  void GraphicsImplDispatchComputeShader(Electron::GraphicsCore* core, int grid_x, int grid_y, int grid_z) ;
UI_EXPORT  void GraphicsImplBindGPUTexture(Electron::GraphicsCore* core, GLuint texture, int unit) ;
UI_EXPORT  GLuint GraphicsImplGenerateGPUTexture(Electron::GraphicsCore* core, int width, int height, int unit) ;
UI_EXPORT  void GraphicsImplMemoryBarrier(Electron::GraphicsCore* core, GLbitfield barrier) ;
UI_EXPORT  Electron::PixelBuffer GraphicsImplPBOFromGPUTexture(Electron::GraphicsCore* core, GLuint texture, int width, int height) ;
UI_EXPORT  void GraphicsImplShaderSetUniformII(Electron::GraphicsCore* core, GLuint program, std::string name, int x, int y) ;
UI_EXPORT  Electron::ResizableGPUTexture ResizableGPUTextureCreate(Electron::GraphicsCore* core, int width, int height) ;
UI_EXPORT  void ResizableGPUTImplCheckForResize(Electron::ResizableGPUTexture* texture, Electron::RenderBuffer* pbo) ;
UI_EXPORT  void GraphicsImplCallCompositor(Electron::GraphicsCore* core, Electron::ResizableGPUTexture color, Electron::ResizableGPUTexture uv, Electron::ResizableGPUTexture depth) ;
UI_EXPORT  void GraphicsImplShaderSetUniformFF(Electron::GraphicsCore* core, GLuint program, std::string name, glm::vec2 vec2) ;
UI_EXPORT  void GraphicsImplShaderSetUniformFFF(Electron::GraphicsCore* core, GLuint program, std::string name, glm::vec3 vec) ;
UI_EXPORT  void GraphicsImplShaderSetUniformF(Electron::GraphicsCore* core, GLuint program, std::string name, float f) ;
UI_EXPORT  void GraphicsImplShaderSetUniformI(Electron::GraphicsCore* core, GLuint program, std::string name, int f) ;
UI_EXPORT  void GraphicsImplRequestTextureCollectionCleaning(Electron::GraphicsCore* core, GLuint color, GLuint uv, GLuint depth, int width, int height, Electron::RenderRequestMetadata metadata) ;
UI_EXPORT  glm::vec2 TextureUnionImplGetDimensions(Electron::TextureUnion* un) ;
UI_EXPORT  void ShortcutsImplCtrlWT(Electron::AppInstance* instance) ;
UI_EXPORT  int CounterGetTimelineCounter() ;
UI_EXPORT  bool IOCtrl() ;
UI_EXPORT  bool IOKeyPressed(ImGuiKey key) ;
UI_EXPORT  ImVec2 UIGetCursorPos() ;
UI_EXPORT  float UIGetScrollX() ;
UI_EXPORT  float UIGetScrollY() ;
UI_EXPORT  void UISetMouseCursor(ImGuiMouseCursor cursor) ;
UI_EXPORT  ImGuiContext* UIGetCurrentContext() ;
UI_EXPORT  void DestroyContext() ;
UI_EXPORT  ImGuiIO& UIGetIO() ;
UI_EXPORT  ImGuiStyle& UIGetStyle() ;
UI_EXPORT  ImDrawData* UIGetDrawData() ;
UI_EXPORT  void UIShowDemoWindow() ;
UI_EXPORT  std::string UIGetVersion() ;
UI_EXPORT  bool UIBeginChild(const char* std, ImVec2 size, bool border, ImGuiWindowFlags flags) ;
UI_EXPORT  void UIEndChild() ;
UI_EXPORT  bool UIWindowIsAppearing() ;
UI_EXPORT  bool UIWindowCollapsed() ;
UI_EXPORT  bool UIWindowFocused(ImGuiFocusedFlags flags) ;
UI_EXPORT  bool UIWindowHovered(ImGuiHoveredFlags flags) ;
UI_EXPORT  ImDrawList* UIGetWindowDrawList() ;
UI_EXPORT  float UIGetWindowDpiScale() ;
UI_EXPORT  ImVec2 UIGetWindowPos() ;
UI_EXPORT  ImGuiViewport* UIGetWindowViewport() ;
UI_EXPORT  void UISetNextWindowSizeConstraints(ImVec2 size_min, ImVec2 size_max, ImGuiSizeCallback custom_callback, void* custom_data) ;
UI_EXPORT  void UISetNextWindowCollapsed(bool collapsed, ImGuiCond cond) ;
UI_EXPORT  void UISetNextWindowFocus() ;
UI_EXPORT  void UISetNextWindowScroll(ImVec2 scroll) ;
UI_EXPORT  DylibRegistry InternalGetDylibRegistry() ;
UI_EXPORT  void GraphicsImplAddRenderLayer(Electron::GraphicsCore* core, std::string path) ;
UI_EXPORT  void UISetNextWindowBgAlpha(float alpha) ;
UI_EXPORT  ImVec2 UIGetContentRegionAvail() ;
UI_EXPORT  ImVec2 UIGetContentRegionMax() ;
UI_EXPORT  ImVec2 UIGetWindowContentRegionMin() ;
UI_EXPORT  ImVec2 UIGetWindowContentRegionMax() ;
UI_EXPORT  void UISetScrollX(float x) ;
UI_EXPORT  void UISetScrollY(float y) ;
UI_EXPORT  float UIGetScrollMaxX() ;
UI_EXPORT  float UIGetScrollMaxY() ;
UI_EXPORT  void UISetScrollHereX(float x) ;
UI_EXPORT  void UISetScrollHereY(float y) ;
UI_EXPORT  void UISetScrollFromPosX(float x, float center) ;
UI_EXPORT  void UISetScrollFromPosY(float y, float center) ;
UI_EXPORT  void UIPushButtonRepeat(bool repeat) ;
UI_EXPORT  void UIPopButtonRepeat() ;
UI_EXPORT  void UISetNextItemWidth(float item_width) ;
UI_EXPORT  void UIPushTextWrapPos(float x) ;
UI_EXPORT  void UIPopTextWrapPos() ;
UI_EXPORT  float UIGetFontSize() ;
UI_EXPORT  ImFont* UIGetFont() ;
UI_EXPORT  ImU32 UIGetColorU32(ImVec4 color) ;
UI_EXPORT  void UIBeginGroup() ;
UI_EXPORT  void UIEndGroup() ;
UI_EXPORT  void BulletText(const char* text) ;
UI_EXPORT  bool UISmallButton(const char* label) ;
UI_EXPORT  void UIBullet() ;
UI_EXPORT  Electron::RenderLayer* GraphicsImplGetLayerByID(Electron::GraphicsCore* core, int id) ;
UI_EXPORT  void UIPlotLines(std::string label, std::vector<float> values, int values_offset, float min, float max) ;
UI_EXPORT  float UIGetTime() ;
UI_EXPORT  void UIOpenPopup(std::string popup, ImGuiPopupFlags flags) ;
UI_EXPORT  bool UIBeginPopup(std::string popup, ImGuiPopupFlags flags) ;
UI_EXPORT  void UIEndPopup() ;

}
#undef UI_EXPORT


#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS

#include "electron.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_stdlib.h"
#include "editor_core.h"
#include "graphics_core.h"
#include "ImGuiFileDialog.h"
#include "ImGui/imgui_internal.h"

#define UI_EXPORT __declspec(dllexport) __stdcall

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
UI_EXPORT  bool UIMenuItemShortcutless(const char* menu) ;
UI_EXPORT  bool UIBeginMenuBar() ;
UI_EXPORT  void UIEndMenuBar() ;
UI_EXPORT  void UIPushID(void* ptr) ;
UI_EXPORT  void UIPopID() ;
UI_EXPORT  void UIImage(GLuint texture, Electron::ElectronVector2f size) ;
UI_EXPORT  void UISliderFloat(const char* label, float* value, float min, float max, const char* format, ImGuiSliderFlags flags) ;
UI_EXPORT  void UISeparator() ;
UI_EXPORT  void UISpacing() ;
UI_EXPORT  bool UICollapsingHeader(const char* name) ;
UI_EXPORT  void UISetNextWindowSize(Electron::ElectronVector2f size, ImGuiCond_ cond) ;
UI_EXPORT  void UIPushFont(ImFont* font) ;
UI_EXPORT  void UIPopFont() ;
UI_EXPORT  void ElectronImplThrow(Electron::ElectronSignal signal) ;
UI_EXPORT  void ElectronImplDirectSignal(void* instance, Electron::ElectronSignal signal) ;
UI_EXPORT  void GraphicsImplRequestRenderWithinRegion(void* instance, Electron::RenderRequestMetadata metadata) ;
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
UI_EXPORT  const char* FileDialogImplGetFilePathName() ;
UI_EXPORT  const char* FileDialogImplGetFilePath() ;
UI_EXPORT  void FileDialogImplClose() ;
UI_EXPORT  void UIInputField(const char* label, std::string* string, ImGuiInputTextFlags flags) ;
UI_EXPORT  void UIInputInt2(const char* label, int* ptr, ImGuiInputTextFlags flags) ;
UI_EXPORT  Electron::PixelBuffer GraphicsImplPixelBufferFromImage(void* instance, const char* filename) ;
UI_EXPORT  std::string ParentString(const char* string) ;
UI_EXPORT  void UIInputColor3(const char* label, float* color, ImGuiColorEditFlags flags) ;
UI_EXPORT  void UISetWindowSize(ImVec2 size, ImGuiCond cond) ;
UI_EXPORT  void UISetWindowPos(ImVec2 pos, ImGuiCond cond) ;
UI_EXPORT  Electron::ElectronVector2f ElectronImplGetNativeWindowSize(void* instance) ;
UI_EXPORT  Electron::ElectronVector2f ElectronImplGetNativeWindowPos(void* instance) ;
UI_EXPORT  ImGuiDockNode* UIDockBuilderGetNode(ImGuiID id) ;
UI_EXPORT  ImGuiID UIGetWindowDockID() ;
UI_EXPORT  void UIDockBuilderSetNodeSize(ImGuiID id, ImVec2 size) ;
UI_EXPORT  ImGuiID UIDockSpaceOverViewport(const ImGuiViewport *viewport, ImGuiDockNodeFlags flags, const ImGuiWindowClass *window_class) ;
UI_EXPORT  void UISetNextWindowDockID(ImGuiID id, ImGuiCond cond) ;
UI_EXPORT  ImGuiID UIGetID(const char* name) ;
UI_EXPORT  ImGuiID UIDockSpace(ImGuiID id, const ImVec2 &size, ImGuiDockNodeFlags flag, const ImGuiWindowClass *window_class) ;
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
UI_EXPORT  void UISetTooltip(const char* tooltip) ;
UI_EXPORT  void ShortcutsImplCtrlWR(void* instance) ;
UI_EXPORT  void ShortcutsImplCtrlPO(void* instance, Electron::ProjectMap projectMap) ;
UI_EXPORT  void ShortcutsImplCtrlPE(void* instance) ;

}
#undef UI_EXPORT

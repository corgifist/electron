
#include "electron.h"
#include "ImGui/imgui_internal.h"

#define CSTR(str) (str).c_str()

#define IMPLEMENT_UI_WRAPPER(ui_proc_name, args_signature, call_signature, typedef_args, fn_type)     typedef fn_type (*ui_proc_name ## _T) typedef_args;     fn_type ui_proc_name args_signature {         ui_proc_name ## _T proc = IMPL_LIBRARY.get_function<fn_type typedef_args>(#ui_proc_name);         if (!proc) throw std::runtime_error(std::string("failed to load proc from impl library in ui wrapper ") + #ui_proc_name);         fn_type ret_val = proc call_signature;         return ret_val;     }

#define IMPLEMENT_UI_VOID_WRAPPER(ui_proc_name, args_signature, call_signature, typedef_args)     typedef void (*ui_proc_name ## _T) typedef_args;     void ui_proc_name args_signature {         ui_proc_name ## _T proc = IMPL_LIBRARY.get_function<void typedef_args>(#ui_proc_name);         if (!proc) throw std::runtime_error(std::string("failed to load proc from impl library in ui wrapper ") + #ui_proc_name);         proc call_signature;     }

static dylib IMPL_LIBRARY = dylib();

static std::string ElectronImplTag(const char* name, void* ptr) {
    std::string temp = name;
    temp += "##";
    temp += std::to_string((long long) ptr);
    return temp;
}

static float ui_lerp(float a, float b, float f)
{
    return a * (1.0 - f) + (b * f);
}



IMPLEMENT_UI_WRAPPER(UIExportTest, (), (), (), float)
IMPLEMENT_UI_VOID_WRAPPER(UIBegin, (const char* name,Electron::ElectronSignal signal,ImGuiWindowFlags flags), (name ,signal ,flags), (const char* ,Electron::ElectronSignal ,ImGuiWindowFlags))
IMPLEMENT_UI_VOID_WRAPPER(UIBeginFlagless, (const char* name,Electron::ElectronSignal signal), (name ,signal), (const char* ,Electron::ElectronSignal))
IMPLEMENT_UI_VOID_WRAPPER(UIText, (const char* text), (text), (const char*))
IMPLEMENT_UI_VOID_WRAPPER(UIEnd, (), (), ())
IMPLEMENT_UI_WRAPPER(UICalcTextSize, (const char* text), (text), (const char*), ImVec2)
IMPLEMENT_UI_WRAPPER(UIGetWindowSize, (), (), (), ImVec2)
IMPLEMENT_UI_VOID_WRAPPER(UISetCursorPos, (ImVec2 cursor), (cursor), (ImVec2))
IMPLEMENT_UI_VOID_WRAPPER(UISetCursorX, (float x), (x), (float))
IMPLEMENT_UI_VOID_WRAPPER(UISetCursorY, (float y), (y), (float))
IMPLEMENT_UI_WRAPPER(UIBeginMainMenuBar, (), (), (), bool)
IMPLEMENT_UI_VOID_WRAPPER(UIEndMainMenuBar, (), (), ())
IMPLEMENT_UI_WRAPPER(UIBeginMenu, (const char* menu), (menu), (const char*), bool)
IMPLEMENT_UI_VOID_WRAPPER(UIEndMenu, (), (), ())
IMPLEMENT_UI_WRAPPER(UIMenuItem, (const char* menu,const char* shortcut), (menu ,shortcut), (const char* ,const char*), bool)
IMPLEMENT_UI_WRAPPER(UIMenuItemEnhanced, (const char* menu,const char* shortcut,bool enabled), (menu ,shortcut ,enabled), (const char* ,const char* ,bool), bool)
IMPLEMENT_UI_WRAPPER(UIMenuItemShortcutless, (const char* menu), (menu), (const char*), bool)
IMPLEMENT_UI_WRAPPER(UIBeginMenuBar, (), (), (), bool)
IMPLEMENT_UI_VOID_WRAPPER(UIEndMenuBar, (), (), ())
IMPLEMENT_UI_VOID_WRAPPER(UIPushID, (void* ptr), (ptr), (void*))
IMPLEMENT_UI_VOID_WRAPPER(UIPopID, (), (), ())
IMPLEMENT_UI_VOID_WRAPPER(UIImage, (GLuint texture,ImVec2 size), (texture ,size), (GLuint ,ImVec2))
IMPLEMENT_UI_VOID_WRAPPER(UISliderFloat, (const char* label,float* value,float min,float max,const char* format,ImGuiSliderFlags flags), (label ,value ,min ,max ,format ,flags), (const char* ,float* ,float ,float ,const char* ,ImGuiSliderFlags))
IMPLEMENT_UI_VOID_WRAPPER(UISeparator, (), (), ())
IMPLEMENT_UI_VOID_WRAPPER(UISpacing, (), (), ())
IMPLEMENT_UI_VOID_WRAPPER(UIDummy, (ImVec2 size), (size), (ImVec2))
IMPLEMENT_UI_VOID_WRAPPER(TextureUnionImplRebuildAssetData, (Electron::TextureUnion* _union,Electron::GraphicsCore* owner), (_union ,owner), (Electron::TextureUnion* ,Electron::GraphicsCore*))
IMPLEMENT_UI_WRAPPER(UICollapsingHeader, (const char* name), (name), (const char*), bool)
IMPLEMENT_UI_VOID_WRAPPER(UISetNextWindowSize, (ImVec2 size,ImGuiCond cond), (size ,cond), (ImVec2 ,ImGuiCond))
IMPLEMENT_UI_VOID_WRAPPER(UIPushFont, (ImFont* font), (font), (ImFont*))
IMPLEMENT_UI_VOID_WRAPPER(UIPopFont, (), (), ())
IMPLEMENT_UI_VOID_WRAPPER(ElectronImplThrow, (Electron::ElectronSignal signal), (signal), (Electron::ElectronSignal))
IMPLEMENT_UI_VOID_WRAPPER(ElectronImplDirectSignal, (void* instance,Electron::ElectronSignal signal), (instance ,signal), (void* ,Electron::ElectronSignal))
IMPLEMENT_UI_WRAPPER(GraphicsImplRequestRenderWithinRegion, (void* instance,Electron::RenderRequestMetadata metadata), (instance ,metadata), (void* ,Electron::RenderRequestMetadata), std::vector<float>)
IMPLEMENT_UI_WRAPPER(PixelBufferImplBuildGPUTexture, (Electron::PixelBuffer& buffer), (buffer), (Electron::PixelBuffer&), GLuint)
IMPLEMENT_UI_VOID_WRAPPER(GraphicsImplCleanPreviewGPUTexture, (void* instance), (instance), (void*))
IMPLEMENT_UI_VOID_WRAPPER(GraphicsImplBuildPreviewGPUTexture, (void* instance), (instance), (void*))
IMPLEMENT_UI_WRAPPER(GraphicsImplGetPreviewGPUTexture, (void* instance), (instance), (void*), GLuint)
IMPLEMENT_UI_VOID_WRAPPER(PixelBufferImplSetFiltering, (int filtering), (filtering), (int))
IMPLEMENT_UI_WRAPPER(UIGetAvailZone, (), (), (), ImVec2)
IMPLEMENT_UI_WRAPPER(UIBeginCombo, (const char* label,const char* option), (label ,option), (const char* ,const char*), bool)
IMPLEMENT_UI_VOID_WRAPPER(UIEndCombo, (), (), ())
IMPLEMENT_UI_WRAPPER(UISelectable, (const char* text,bool& selected), (text ,selected), (const char* ,bool&), bool)
IMPLEMENT_UI_VOID_WRAPPER(UISetItemFocusDefault, (), (), ())
IMPLEMENT_UI_VOID_WRAPPER(GraphicsImplResizeRenderBuffer, (void* instance,int width,int height), (instance ,width ,height), (void* ,int ,int))
IMPLEMENT_UI_VOID_WRAPPER(FileDialogImplOpenDialog, (const char* internalName,const char* title,const char* extensions,const char* initialPath), (internalName ,title ,extensions ,initialPath), (const char* ,const char* ,const char* ,const char*))
IMPLEMENT_UI_WRAPPER(FileDialogImplDisplay, (const char* internalName), (internalName), (const char*), bool)
IMPLEMENT_UI_WRAPPER(FileDialogImplIsOK, (), (), (), bool)
IMPLEMENT_UI_WRAPPER(FileDialogImplGetFilePathName, (), (), (), std::string)
IMPLEMENT_UI_WRAPPER(FileDialogImplGetFilePath, (), (), (), std::string)
IMPLEMENT_UI_VOID_WRAPPER(FileDialogImplClose, (), (), ())
IMPLEMENT_UI_WRAPPER(UIInputField, (const char* label,std::string* string,ImGuiInputTextFlags flags), (label ,string ,flags), (const char* ,std::string* ,ImGuiInputTextFlags), bool)
IMPLEMENT_UI_WRAPPER(AssetManagerImplImportAsset, (Electron::AssetRegistry* registry,std::string path), (registry ,path), (Electron::AssetRegistry* ,std::string), std::string)
IMPLEMENT_UI_WRAPPER(UICenteredButton, (Electron::AppInstance* instance,const char* label), (instance ,label), (Electron::AppInstance* ,const char*), bool)
IMPLEMENT_UI_VOID_WRAPPER(UIInputInt2, (const char* label,int* ptr,ImGuiInputTextFlags flags), (label ,ptr ,flags), (const char* ,int* ,ImGuiInputTextFlags))
IMPLEMENT_UI_WRAPPER(GraphicsImplPixelBufferFromImage, (void* instance,const char* filename), (instance ,filename), (void* ,const char*), Electron::PixelBuffer)
IMPLEMENT_UI_WRAPPER(ParentString, (const char* string), (string), (const char*), std::string)
IMPLEMENT_UI_VOID_WRAPPER(UIInputColor3, (const char* label,float* color,ImGuiColorEditFlags flags), (label ,color ,flags), (const char* ,float* ,ImGuiColorEditFlags))
IMPLEMENT_UI_VOID_WRAPPER(UISetWindowSize, (ImVec2 size,ImGuiCond cond), (size ,cond), (ImVec2 ,ImGuiCond))
IMPLEMENT_UI_VOID_WRAPPER(UISetWindowPos, (ImVec2 pos,ImGuiCond cond), (pos ,cond), (ImVec2 ,ImGuiCond))
IMPLEMENT_UI_VOID_WRAPPER(UIIndent, (), (), ())
IMPLEMENT_UI_VOID_WRAPPER(UIUnindent, (), (), ())
IMPLEMENT_UI_WRAPPER(ElectronImplGetNativeWindowSize, (void* instance), (instance), (void*), Electron::ElectronVector2f)
IMPLEMENT_UI_WRAPPER(ElectronImplGetNativeWindowPos, (void* instance), (instance), (void*), Electron::ElectronVector2f)
IMPLEMENT_UI_WRAPPER(UIDockBuilderGetNode, (ImGuiID id), (id), (ImGuiID), ImGuiDockNode*)
IMPLEMENT_UI_WRAPPER(UIGetWindowDockID, (), (), (), ImGuiID)
IMPLEMENT_UI_VOID_WRAPPER(UIDockBuilderSetNodeSize, (ImGuiID id,ImVec2 size), (id ,size), (ImGuiID ,ImVec2))
IMPLEMENT_UI_WRAPPER(UIDockSpaceOverViewport, (const ImGuiViewport* viewport,ImGuiDockNodeFlags flags,const ImGuiWindowClass* window_class), (viewport ,flags ,window_class), (const ImGuiViewport* ,ImGuiDockNodeFlags ,const ImGuiWindowClass*), ImGuiID)
IMPLEMENT_UI_VOID_WRAPPER(UISetNextWindowDockID, (ImGuiID id,ImGuiCond cond), (id ,cond), (ImGuiID ,ImGuiCond))
IMPLEMENT_UI_WRAPPER(UIGetID, (const char* name), (name), (const char*), ImGuiID)
IMPLEMENT_UI_WRAPPER(UIDockSpace, (ImGuiID id,const ImVec2& size,ImGuiDockNodeFlags flag,const ImGuiWindowClass* window_class), (id ,size ,flag ,window_class), (ImGuiID ,const ImVec2& ,ImGuiDockNodeFlags ,const ImGuiWindowClass*), ImGuiID)
IMPLEMENT_UI_WRAPPER(UIGetViewport, (), (), (), ImGuiViewport*)
IMPLEMENT_UI_WRAPPER(UIBeginTabBar, (const char* str_id,ImGuiTabBarFlags flags), (str_id ,flags), (const char* ,ImGuiTabBarFlags), bool)
IMPLEMENT_UI_VOID_WRAPPER(UIEndTabBar, (), (), ())
IMPLEMENT_UI_WRAPPER(UIBeginTabItem, (const char* str_id,bool* p_open,ImGuiTabItemFlags flags), (str_id ,p_open ,flags), (const char* ,bool* ,ImGuiTabItemFlags), bool)
IMPLEMENT_UI_VOID_WRAPPER(UIEndTabItem, (), (), ())
IMPLEMENT_UI_WRAPPER(UIIsItemHovered, (ImGuiHoveredFlags flags), (flags), (ImGuiHoveredFlags), bool)
IMPLEMENT_UI_VOID_WRAPPER(UIPushItemWidth, (float width), (width), (float))
IMPLEMENT_UI_VOID_WRAPPER(UIPopItemWidth, (), (), ())
IMPLEMENT_UI_VOID_WRAPPER(UICheckbox, (const char* label,bool* v), (label ,v), (const char* ,bool*))
IMPLEMENT_UI_VOID_WRAPPER(UISameLine, (), (), ())
IMPLEMENT_UI_VOID_WRAPPER(UIInputInt, (const char* label,int* ptr,int step,int step_fast,ImGuiInputTextFlags flags), (label ,ptr ,step ,step_fast ,flags), (const char* ,int* ,int ,int ,ImGuiInputTextFlags))
IMPLEMENT_UI_VOID_WRAPPER(UISetTooltip, (const char* tooltip), (tooltip), (const char*))
IMPLEMENT_UI_WRAPPER(GraphicsImplGetPreviewBufferByOutputType, (void* instance), (instance), (void*), GLuint)
IMPLEMENT_UI_WRAPPER(UIBeginTooltip, (), (), (), bool)
IMPLEMENT_UI_VOID_WRAPPER(UIEndTooltip, (), (), ())
IMPLEMENT_UI_VOID_WRAPPER(UISetNextWindowPos, (ImVec2 size,ImGuiCond cond), (size ,cond), (ImVec2 ,ImGuiCond))
IMPLEMENT_UI_WRAPPER(UIButton, (const char* text), (text), (const char*), bool)
IMPLEMENT_UI_WRAPPER(UIGetDisplaySize, (), (), (), ImVec2)
IMPLEMENT_UI_VOID_WRAPPER(UISetItemTooltip, (const char* tooltip), (tooltip), (const char*))
IMPLEMENT_UI_VOID_WRAPPER(RenderLayerImplRenderProperty, (Electron::RenderLayer* layer,Electron::GeneralizedPropertyType type,Electron::json_t& property,std::string name), (layer ,type ,property ,name), (Electron::RenderLayer* ,Electron::GeneralizedPropertyType ,Electron::json_t& ,std::string))
IMPLEMENT_UI_WRAPPER(RenderLayerImplInterpolateProperty, (Electron::RenderLayer* layer,Electron::json_t property), (layer ,property), (Electron::RenderLayer* ,Electron::json_t), Electron::json_t)
IMPLEMENT_UI_VOID_WRAPPER(PixelBufferImplSetPixel, (Electron::PixelBuffer* buffer,int x,int y,Electron::Pixel pixel), (buffer ,x ,y ,pixel), (Electron::PixelBuffer* ,int ,int ,Electron::Pixel))
IMPLEMENT_UI_WRAPPER(RectContains, (Electron::Rect rect,Electron::Point point), (rect ,point), (Electron::Rect ,Electron::Point), bool)
IMPLEMENT_UI_VOID_WRAPPER(RenderLayerImplSortKeyframes, (Electron::RenderLayer* layer,Electron::json_t& keyframes), (layer ,keyframes), (Electron::RenderLayer* ,Electron::json_t&))
IMPLEMENT_UI_VOID_WRAPPER(RenderLayerImplRenderProperties, (Electron::RenderLayer* layer), (layer), (Electron::RenderLayer*))
IMPLEMENT_UI_VOID_WRAPPER(UIPushColor, (ImGuiCol col,ImVec4 color), (col ,color), (ImGuiCol ,ImVec4))
IMPLEMENT_UI_VOID_WRAPPER(UIPopColor, (), (), ())
IMPLEMENT_UI_VOID_WRAPPER(UISetNextWindowViewport, (ImGuiID viewport), (viewport), (ImGuiID))
IMPLEMENT_UI_VOID_WRAPPER(UIPushStyleVarF, (ImGuiStyleVar var,float f), (var ,f), (ImGuiStyleVar ,float))
IMPLEMENT_UI_VOID_WRAPPER(UIPushStyleVarV2, (ImGuiStyleVar var,ImVec2 vec), (var ,vec), (ImGuiStyleVar ,ImVec2))
IMPLEMENT_UI_VOID_WRAPPER(UIPopStyleVar, (), (), ())
IMPLEMENT_UI_VOID_WRAPPER(UIPopStyleVarIter, (int count), (count), (int))
IMPLEMENT_UI_VOID_WRAPPER(PixelBufferImplDestroyGPUTexture, (GLuint texture), (texture), (GLuint))
IMPLEMENT_UI_VOID_WRAPPER(ShortcutsImplCtrlWL, (void* instance), (instance), (void*))
IMPLEMENT_UI_VOID_WRAPPER(ShortcutsImplCtrlWR, (void* instance), (instance), (void*))
IMPLEMENT_UI_VOID_WRAPPER(ShortcutsImplCtrlPO, (void* instance,Electron::ProjectMap projectMap), (instance ,projectMap), (void* ,Electron::ProjectMap))
IMPLEMENT_UI_VOID_WRAPPER(ShortcutsImplCtrlPE, (void* instance), (instance), (void*))
IMPLEMENT_UI_VOID_WRAPPER(ShortcutsImplCtrlWA, (void* instance), (instance), (void*))
IMPLEMENT_UI_WRAPPER(ClipGetText, (), (), (), std::string)
IMPLEMENT_UI_VOID_WRAPPER(ClipSetText, (std::string text), (text), (std::string))
IMPLEMENT_UI_WRAPPER(CounterGetProjectConfiguration, (), (), (), int)
IMPLEMENT_UI_WRAPPER(CounterGetRenderPreview, (), (), (), int)
IMPLEMENT_UI_WRAPPER(CounterGetLayerProperties, (), (), (), int)
IMPLEMENT_UI_WRAPPER(CounterGetAssetManager, (), (), (), int)
IMPLEMENT_UI_WRAPPER(PixelBufferImplGetPixel, (Electron::PixelBuffer* pbo,int x,int y), (pbo ,x ,y), (Electron::PixelBuffer* ,int ,int), Electron::Pixel)
IMPLEMENT_UI_WRAPPER(UIBeginDisabled, (), (), (), float)
IMPLEMENT_UI_VOID_WRAPPER(UIEndDisabled, (float alpha), (alpha), (float))
IMPLEMENT_UI_VOID_WRAPPER(GraphicsImplRequestRenderBufferCleaningWithingRegion, (Electron::AppInstance* instance,Electron::RenderRequestMetadata metadata), (instance ,metadata), (Electron::AppInstance* ,Electron::RenderRequestMetadata))
IMPLEMENT_UI_VOID_WRAPPER(UISliderInt, (const char* label,int* v,int v_min,int v_max,const char* format,ImGuiSliderFlags flags), (label ,v ,v_min ,v_max ,format ,flags), (const char* ,int* ,int ,int ,const char* ,ImGuiSliderFlags))
IMPLEMENT_UI_WRAPPER(GraphicsImplCompileComputeShader, (Electron::GraphicsCore* graphics,std::string path), (graphics ,path), (Electron::GraphicsCore* ,std::string), GLuint)
IMPLEMENT_UI_VOID_WRAPPER(GraphicsImplUseShader, (Electron::GraphicsCore* core,GLuint shader), (core ,shader), (Electron::GraphicsCore* ,GLuint))
IMPLEMENT_UI_VOID_WRAPPER(GraphicsImplDispatchComputeShader, (Electron::GraphicsCore* core,int grid_x,int grid_y,int grid_z), (core ,grid_x ,grid_y ,grid_z), (Electron::GraphicsCore* ,int ,int ,int))
IMPLEMENT_UI_VOID_WRAPPER(GraphicsImplBindGPUTexture, (Electron::GraphicsCore* core,GLuint texture,int unit), (core ,texture ,unit), (Electron::GraphicsCore* ,GLuint ,int))
IMPLEMENT_UI_WRAPPER(GraphicsImplGenerateGPUTexture, (Electron::GraphicsCore* core,int width,int height,int unit), (core ,width ,height ,unit), (Electron::GraphicsCore* ,int ,int ,int), GLuint)
IMPLEMENT_UI_VOID_WRAPPER(GraphicsImplMemoryBarrier, (Electron::GraphicsCore* core,GLbitfield barrier), (core ,barrier), (Electron::GraphicsCore* ,GLbitfield))
IMPLEMENT_UI_WRAPPER(GraphicsImplPBOFromGPUTexture, (Electron::GraphicsCore* core,GLuint texture,int width,int height), (core ,texture ,width ,height), (Electron::GraphicsCore* ,GLuint ,int ,int), Electron::PixelBuffer)
IMPLEMENT_UI_VOID_WRAPPER(GraphicsImplShaderSetUniformII, (Electron::GraphicsCore* core,GLuint program,std::string name,int x,int y), (core ,program ,name ,x ,y), (Electron::GraphicsCore* ,GLuint ,std::string ,int ,int))
IMPLEMENT_UI_WRAPPER(ResizableGPUTextureCreate, (Electron::GraphicsCore* core,int width,int height), (core ,width ,height), (Electron::GraphicsCore* ,int ,int), Electron::ResizableGPUTexture)
IMPLEMENT_UI_VOID_WRAPPER(ResizableGPUTImplCheckForResize, (Electron::ResizableGPUTexture* texture,Electron::RenderBuffer* pbo), (texture ,pbo), (Electron::ResizableGPUTexture* ,Electron::RenderBuffer*))
IMPLEMENT_UI_VOID_WRAPPER(GraphicsImplCallCompositor, (Electron::GraphicsCore* core,Electron::ResizableGPUTexture color,Electron::ResizableGPUTexture uv,Electron::ResizableGPUTexture depth), (core ,color ,uv ,depth), (Electron::GraphicsCore* ,Electron::ResizableGPUTexture ,Electron::ResizableGPUTexture ,Electron::ResizableGPUTexture))
IMPLEMENT_UI_VOID_WRAPPER(GraphicsImplShaderSetUniformFF, (Electron::GraphicsCore* core,GLuint program,std::string name,glm::vec2 vec2), (core ,program ,name ,vec2), (Electron::GraphicsCore* ,GLuint ,std::string ,glm::vec2))
IMPLEMENT_UI_VOID_WRAPPER(GraphicsImplShaderSetUniformFFF, (Electron::GraphicsCore* core,GLuint program,std::string name,glm::vec3 vec), (core ,program ,name ,vec), (Electron::GraphicsCore* ,GLuint ,std::string ,glm::vec3))
IMPLEMENT_UI_VOID_WRAPPER(GraphicsImplShaderSetUniformF, (Electron::GraphicsCore* core,GLuint program,std::string name,float f), (core ,program ,name ,f), (Electron::GraphicsCore* ,GLuint ,std::string ,float))
IMPLEMENT_UI_VOID_WRAPPER(GraphicsImplShaderSetUniformI, (Electron::GraphicsCore* core,GLuint program,std::string name,int f), (core ,program ,name ,f), (Electron::GraphicsCore* ,GLuint ,std::string ,int))
IMPLEMENT_UI_VOID_WRAPPER(GraphicsImplRequestTextureCollectionCleaning, (Electron::GraphicsCore* core,GLuint color,GLuint uv,GLuint depth,int width,int height,Electron::RenderRequestMetadata metadata), (core ,color ,uv ,depth ,width ,height ,metadata), (Electron::GraphicsCore* ,GLuint ,GLuint ,GLuint ,int ,int ,Electron::RenderRequestMetadata))
IMPLEMENT_UI_WRAPPER(TextureUnionImplGetDimensions, (Electron::TextureUnion* un), (un), (Electron::TextureUnion*), glm::vec2)
IMPLEMENT_UI_VOID_WRAPPER(ShortcutsImplCtrlWT, (Electron::AppInstance* instance), (instance), (Electron::AppInstance*))
IMPLEMENT_UI_WRAPPER(CounterGetTimelineCounter, (), (), (), int)
IMPLEMENT_UI_WRAPPER(IOCtrl, (), (), (), bool)
IMPLEMENT_UI_WRAPPER(IOKeyPressed, (ImGuiKey key), (key), (ImGuiKey), bool)
IMPLEMENT_UI_WRAPPER(UIGetCursorPos, (), (), (), ImVec2)
IMPLEMENT_UI_WRAPPER(UIGetScrollX, (), (), (), float)
IMPLEMENT_UI_WRAPPER(UIGetScrollY, (), (), (), float)
IMPLEMENT_UI_VOID_WRAPPER(UISetMouseCursor, (ImGuiMouseCursor cursor), (cursor), (ImGuiMouseCursor))
IMPLEMENT_UI_WRAPPER(UIGetCurrentContext, (), (), (), ImGuiContext*)
IMPLEMENT_UI_VOID_WRAPPER(DestroyContext, (), (), ())
IMPLEMENT_UI_WRAPPER(UIGetIO, (), (), (), ImGuiIO&)
IMPLEMENT_UI_WRAPPER(UIGetStyle, (), (), (), ImGuiStyle&)
IMPLEMENT_UI_WRAPPER(UIGetDrawData, (), (), (), ImDrawData*)
IMPLEMENT_UI_VOID_WRAPPER(UIShowDemoWindow, (), (), ())
IMPLEMENT_UI_WRAPPER(UIGetVersion, (), (), (), std::string)
IMPLEMENT_UI_WRAPPER(UIBeginChild, (const char* std,ImVec2 size,bool border,ImGuiWindowFlags flags), (std ,size ,border ,flags), (const char* ,ImVec2 ,bool ,ImGuiWindowFlags), bool)
IMPLEMENT_UI_VOID_WRAPPER(UIEndChild, (), (), ())
IMPLEMENT_UI_WRAPPER(UIWindowIsAppearing, (), (), (), bool)
IMPLEMENT_UI_WRAPPER(UIWindowCollapsed, (), (), (), bool)
IMPLEMENT_UI_WRAPPER(UIWindowFocused, (ImGuiFocusedFlags flags), (flags), (ImGuiFocusedFlags), bool)
IMPLEMENT_UI_WRAPPER(UIWindowHovered, (ImGuiHoveredFlags flags), (flags), (ImGuiHoveredFlags), bool)
IMPLEMENT_UI_WRAPPER(UIGetWindowDrawList, (), (), (), ImDrawList*)
IMPLEMENT_UI_WRAPPER(UIGetWindowDpiScale, (), (), (), float)
IMPLEMENT_UI_WRAPPER(UIGetWindowPos, (), (), (), ImVec2)
IMPLEMENT_UI_WRAPPER(UIGetWindowViewport, (), (), (), ImGuiViewport*)
IMPLEMENT_UI_VOID_WRAPPER(UISetNextWindowSizeConstraints, (ImVec2 size_min,ImVec2 size_max,ImGuiSizeCallback custom_callback,void* custom_data), (size_min ,size_max ,custom_callback ,custom_data), (ImVec2 ,ImVec2 ,ImGuiSizeCallback ,void*))
IMPLEMENT_UI_VOID_WRAPPER(UISetNextWindowCollapsed, (bool collapsed,ImGuiCond cond), (collapsed ,cond), (bool ,ImGuiCond))
IMPLEMENT_UI_VOID_WRAPPER(UISetNextWindowFocus, (), (), ())
IMPLEMENT_UI_VOID_WRAPPER(UISetNextWindowScroll, (ImVec2 scroll), (scroll), (ImVec2))
IMPLEMENT_UI_WRAPPER(InternalGetDylibRegistry, (), (), (), DylibRegistry)
IMPLEMENT_UI_VOID_WRAPPER(GraphicsImplAddRenderLayer, (Electron::GraphicsCore* core,std::string path), (core ,path), (Electron::GraphicsCore* ,std::string))
IMPLEMENT_UI_VOID_WRAPPER(UISetNextWindowBgAlpha, (float alpha), (alpha), (float))
IMPLEMENT_UI_WRAPPER(UIGetContentRegionAvail, (), (), (), ImVec2)
IMPLEMENT_UI_WRAPPER(UIGetContentRegionMax, (), (), (), ImVec2)
IMPLEMENT_UI_WRAPPER(UIGetWindowContentRegionMin, (), (), (), ImVec2)
IMPLEMENT_UI_WRAPPER(UIGetWindowContentRegionMax, (), (), (), ImVec2)
IMPLEMENT_UI_VOID_WRAPPER(UISetScrollX, (float x), (x), (float))
IMPLEMENT_UI_VOID_WRAPPER(UISetScrollY, (float y), (y), (float))
IMPLEMENT_UI_WRAPPER(UIGetScrollMaxX, (), (), (), float)
IMPLEMENT_UI_WRAPPER(UIGetScrollMaxY, (), (), (), float)
IMPLEMENT_UI_VOID_WRAPPER(UISetScrollHereX, (float x), (x), (float))
IMPLEMENT_UI_VOID_WRAPPER(UISetScrollHereY, (float y), (y), (float))
IMPLEMENT_UI_VOID_WRAPPER(UISetScrollFromPosX, (float x,float center), (x ,center), (float ,float))
IMPLEMENT_UI_VOID_WRAPPER(UISetScrollFromPosY, (float y,float center), (y ,center), (float ,float))
IMPLEMENT_UI_VOID_WRAPPER(UIPushButtonRepeat, (bool repeat), (repeat), (bool))
IMPLEMENT_UI_VOID_WRAPPER(UIPopButtonRepeat, (), (), ())
IMPLEMENT_UI_VOID_WRAPPER(UISetNextItemWidth, (float item_width), (item_width), (float))
IMPLEMENT_UI_VOID_WRAPPER(UIPushTextWrapPos, (float x), (x), (float))
IMPLEMENT_UI_VOID_WRAPPER(UIPopTextWrapPos, (), (), ())
IMPLEMENT_UI_WRAPPER(UIGetFontSize, (), (), (), float)
IMPLEMENT_UI_WRAPPER(UIGetFont, (), (), (), ImFont*)
IMPLEMENT_UI_WRAPPER(UIGetColorU32, (ImVec4 color), (color), (ImVec4), ImU32)
IMPLEMENT_UI_VOID_WRAPPER(UIBeginGroup, (), (), ())
IMPLEMENT_UI_VOID_WRAPPER(UIEndGroup, (), (), ())
IMPLEMENT_UI_VOID_WRAPPER(BulletText, (const char* text), (text), (const char*))
IMPLEMENT_UI_WRAPPER(UISmallButton, (const char* label), (label), (const char*), bool)
IMPLEMENT_UI_VOID_WRAPPER(UIBullet, (), (), ())
IMPLEMENT_UI_WRAPPER(GraphicsImplGetLayerByID, (Electron::GraphicsCore* core,int id), (core ,id), (Electron::GraphicsCore* ,int), Electron::RenderLayer*)
IMPLEMENT_UI_VOID_WRAPPER(UIPlotLines, (std::string label,std::vector<float> values,int values_offset,float min,float max), (label ,values ,values_offset ,min ,max), (std::string ,std::vector<float> ,int ,float ,float))
IMPLEMENT_UI_WRAPPER(UIGetTime, (), (), (), float)

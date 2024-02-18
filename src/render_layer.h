#pragma once
#include "electron.h"
#include "libraries.h"
#include "shared.h"
#include "graphics_core.h"
#include "asset_core.h"
#include "ImGui/imgui.h"
#include "vram.h"


namespace Electron {
    
    class RenderLayer;
    typedef void (*Electron_LayerImplF)(RenderLayer*);
    typedef json_t (*Electron_PropertiesImplF)(RenderLayer*);
    typedef PipelineFrameBuffer (*Electron_FramebufferImplF)(RenderLayer*);
    typedef void (*Electron_MakeFramebufferResidentImplF)(RenderLayer*, bool);

    enum class GeneralizedPropertyType {
        Vec3, Vec2, Float, Color3
    };

    // Rendering unit of electron
    class RenderLayer {
    public:
        float beginFrame, endFrame, frameOffset; // Timing data
        std::string layerLibrary; // Name of implementation library (e.g. libsdf2d.so)
        json_t previousProperties; // Storing previous properties for LayerOnPropertiesChange event
        json_t properties; // Properties of the layer (keyframes, values) that can be saved to JSON
        json_t internalData; // JSON data that cannot be saved to file
        Electron_LayerImplF layerProcedure;
        Electron_LayerImplF destructionProcedure;
        Electron_LayerImplF onPlaybackChangeProcedure;
        Electron_LayerImplF onTimelineSeek;
        Electron_LayerImplF onPropertiesChange;
        Electron_LayerImplF initializationProcedure;
        Electron_LayerImplF propertiesProcedure;
        Electron_LayerImplF sortingProcedure;
        Electron_LayerImplF menuProcedure;
        Electron_PropertiesImplF previewPropertiesProcedure;
        Electron_FramebufferImplF framebufferProcedure;
        Electron_MakeFramebufferResidentImplF residentProcedure;
        bool initialized;
        std::string layerPublicName;
        float renderTime;
        std::vector<std::any> anyData; // Non-JSON data that cannot be saved to file
        glm::vec4 layerColor; // Timeline layer color
        std::string layerUsername; // Layer name given by user
        int id;
        bool visible;


        RenderLayer(std::string layerLibrary); 
        RenderLayer() {
            this->initialized = false;
        };

        ~RenderLayer();

        void FetchImplementation();

        void Render();
        void RenderProperty(GeneralizedPropertyType type, json_t& property, std::string propertyName);
        void RenderAssetProperty(json_t& property, std::string label, TextureUnionType type);
        void RenderProperties();

        void SortKeyframes(json_t& keyframes);
        json_t InterpolateProperty(json_t property, float time = -1.0f);
        json_t ExtractExactValue(json_t property);

        json_t GetPreviewProperties();

        Electron_LayerImplF TryGetLayerImplF(std::string key);
        Electron_PropertiesImplF TryGetPropertiesImplF(std::string key);
        Electron_FramebufferImplF TryGetFramebufferImplF(std::string key);
        Electron_MakeFramebufferResidentImplF TryGetMakeFramebufferResidentImplF(std::string key);

        void Destroy();
    };

    void LayerImplPlaceholder(Electron::RenderLayer* layer);
    json_t LayerPropertiesImplPlaceholder(Electron::RenderLayer* layer);
    PipelineFrameBuffer LayerFramebufferImplPlaceholder(RenderLayer* layer);
    void LayerMakeFramebufferResidentPlaceholder(RenderLayer* layer, bool);
}
#pragma once
#include "electron.h"
#include "libraries.h"
#include "shared.h"
#include "graphics_core.h"
#include "asset_registry.h"
#include "ImGui/imgui.h"


namespace Electron {
    
    class RenderLayer;
    typedef void (*Electron_LayerImplF)(RenderLayer*);

    enum class GeneralizedPropertyType {
        Vec3, Vec2, Float, Color3
    };

    // Rendering unit of electron
    class RenderLayer {
    public:
        int beginFrame, endFrame, frameOffset; // Timing data
        std::string layerLibrary; // Name of implementation library (e.g. libsdf2d.so)
        json_t previousProperties; // Storing previous properties for LayerOnPropertiesChange event
        json_t properties; // Properties of the layer (keyframes, values) that can be saved to JSON
        json_t internalData; // JSON data that cannot be saved to file
        json_t previewProperties; // No usage data
        Electron_LayerImplF layerProcedure;
        Electron_LayerImplF destructionProcedure;
        Electron_LayerImplF onPlaybackChangeProcedure;
        Electron_LayerImplF onTimelineSeek;
        Electron_LayerImplF onPropertiesChange;
        Electron_LayerImplF initializationProcedure;
        Electron_LayerImplF propertiesProcedure;
        Electron_LayerImplF sortingProcedure;
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
        void RenderProperty(GeneralizedPropertyType type, json_t& property, std::string propertyName, ImVec2 floatBoundaries = {0.0f, 0.0f});
        void RenderAssetProperty(json_t& property, std::string label, TextureUnionType type);
        void RenderProperties();

        void SortKeyframes(json_t& keyframes);
        json_t InterpolateProperty(json_t property);
        json_t ExtractExactValue(json_t property);

        Electron_LayerImplF TryGetLayerImplF(std::string key);

        void Destroy();
    };

    void LayerImplPlaceholder(Electron::RenderLayer* layer);
}
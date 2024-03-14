#pragma once
#include "electron.h"
#include "async_ffmpeg.h"
#include "pixel_buffer.h"
#include "cache.h"

#define IMPORT_EXTENSIONS ".*,.png,.jpg,.jpeg,.tga,.psd,.ogg,.mp3,.wav,.mp4,.wmv,.mov,.mkv"
#define VIDEO_CACHE_EXTENSION "jpg"

namespace Electron {
    enum class TextureUnionType {
        Texture, Audio, Video
    };

    // Holds ffprobe's result
    struct AudioMetadata {
        float audioLength;
        int bitrate;
        int sampleRate;
        std::string codecName;

        AudioMetadata(json_t probeJson);
        AudioMetadata() {}
    };

    struct PixelBufferMetadata {
        int width, height, channels;

        PixelBufferMetadata() {}
    };

    struct VideoMetadata {
        float duration;
        std::string codecName;

        int width, height;
        float framerate;

        VideoMetadata(json_t probeJson);
        VideoMetadata() {}
    };

    using InternalTextureUnion = std::variant<PixelBufferMetadata, AudioMetadata, VideoMetadata>;

    struct TextureUnion;

    struct AssetDecoder {
        GPUExtendedHandle texture;
        int width, height;
        stbi_uc* image;
        int lastLoadedFrame;
        int frame;

        AssetDecoder();

        GPUExtendedHandle GetGPUTexture(TextureUnion* asset);

        void Destroy();    
    };

    // Represents Texture/Audio/Video asset
    struct TextureUnion {
        TextureUnionType type;
        InternalTextureUnion as;
        std::string name;
        std::string path;
        std::string strType;
        std::string audioCacheCover;
        std::string ffprobeData, ffprobeJsonData;
        GPUExtendedHandle pboGpuTexture;
        float previewScale;
        glm::vec2 coverResolution;
        std::vector<std::string> linkedCache;
        int id;
        bool ready;
        bool invalid;

        TextureUnion() {
            this->pboGpuTexture = 0;
            this->previewScale = 1.0f;
            this->ready = true;
            this->invalid = false;
        }
        ~TextureUnion() {}

        // Can be interpreted as texture
        bool IsTextureCompatible() {
            return type == TextureUnionType::Texture || type == TextureUnionType::Video;
        }

        void RebuildAssetData();
        glm::vec2 GetDimensions();
        void DumpData();
        std::string GetIcon();
        static std::string GetIconByType(TextureUnionType type);
        void Destroy();
    }; 

    // Helper class that stores asset import logs
    struct AssetLoadInfo {
        TextureUnion result;
        std::string returnMessage;

        AssetLoadInfo(std::string msg) {
            this->returnMessage = msg;
        }

        AssetLoadInfo(const char* msg) {
            this->returnMessage = msg;
        }

        AssetLoadInfo() {}
    };

    struct FaultyAssetDescription {
        std::string path, name;
        TextureUnionType type;

        FaultyAssetDescription() {}
    };

    // All assets live here
    struct AssetCore {
        static std::vector<TextureUnion> assets;
        static std::vector<FaultyAssetDescription> faultyAssets;

        static bool ffmpegActive;
        static std::vector<AsyncFFMpegOperation> operations;

        AssetCore() {}

        static void LoadFromProject(json_t project);
        static void Clear();

        // Load asset and push it into registry
        static std::string ImportAsset(std::string path);
        // Load asset and get it's TextureUnion representation
        static AssetLoadInfo LoadAssetFromPath(std::string path);

        static TextureUnion* GetAsset(std::string id);

        static TextureUnionType TextureUnionTypeFromString(std::string type);

        static std::string StringFromTextureUnionType(TextureUnionType type);
    };
}
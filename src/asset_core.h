#pragma once
#include "electron.h"
#include "async_ffmpeg.h"
#include "pixel_buffer.h"
#include "cache.h"

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

    using InternalTextureUnion = std::variant<PixelBufferMetadata, AudioMetadata>;

    // Represents Texture/Audio/Video asset
    struct TextureUnion {
        TextureUnionType type;
        InternalTextureUnion as;
        std::string name;
        std::string path;
        std::string strType;
        std::string audioCacheCover;
        std::string ffprobeData, ffprobeJsonData;
        GLuint pboGpuTexture;
        bool invalid;
        float previewScale;
        glm::vec2 coverResolution;
        std::vector<std::string> linkedCache;
        int id;
        bool ready;

        TextureUnion() {
            this->previewScale = 1.0f;
            this->invalid = false;
            this->ready = true;
        }
        ~TextureUnion() {}

        // Can be interpreted as texture
        bool IsTextureCompatible() {
            return type == TextureUnionType::Texture || type == TextureUnionType::Audio;
        }

        void RebuildAssetData();
        glm::vec2 GetDimensions();
        void GenerateUVTexture();
        void DumpData();
        std::string GetIcon();
        static std::string GetIconByType(TextureUnionType type);
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
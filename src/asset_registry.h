#pragma once
#include "electron.h"
#include "async_ffmpeg.h"
#include "pixel_buffer.h"
#include "cache.h"

namespace Electron {
    enum class TextureUnionType {
        Texture, Audio
    };

    // Holds ffprobe's result
    struct AudioMetadata {
        float audioLength;

        AudioMetadata() {}
    };

    using InternalTextureUnion = std::variant<PixelBuffer, AudioMetadata>;

    // Represents Texture/Audio/Video asset
    struct TextureUnion {
        TextureUnionType type;
        InternalTextureUnion as;
        std::string name;
        std::string path;
        std::string strType;
        std::string audioCacheCover;
        std::string ffprobeData;
        GLuint pboGpuTexture;
        GLuint previousPboGpuTexture;
        bool invalid;
        float previewScale;
        glm::vec2 coverResolution;
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
            return type == TextureUnionType::Texture;
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
    struct AssetRegistry {
        std::vector<TextureUnion> assets;
        std::vector<FaultyAssetDescription> faultyAssets;

        bool ffmpegActive;
        std::vector<AsyncFFMpegOperation> operations;

        AssetRegistry() {
            this->ffmpegActive = false;
        }

        void LoadFromProject(json_t project);
        void Clear();

        // Load asset and push it into registry
        std::string ImportAsset(std::string path);
        // Load asset and get it's TextureUnion representation
        AssetLoadInfo LoadAssetFromPath(std::string path);

        static TextureUnionType TextureUnionTypeFromString(std::string type) {
            if (type == "Image") {
                return TextureUnionType::Texture;
            }
            if (type == "Audio") {
                return TextureUnionType::Audio;
            }
            return TextureUnionType::Texture;
        }

        static std::string StringFromTextureUnionType(TextureUnionType type) {
            switch (type) {
                case TextureUnionType::Texture: {
                    return "Image";
                }
                case TextureUnionType::Audio: {
                    return "Audio";
                }
            }
            return "Image";
        }
    };
}
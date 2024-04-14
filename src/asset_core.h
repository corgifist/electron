#pragma once
#include "electron.h"
#include "async_ffmpeg.h"
#include "pixel_buffer.h"
#include "cache.h"

#include "utils/video_reader.hpp"

#define IMPORT_EXTENSIONS ".*,.png,.jpg,.jpeg,.tga,.psd,.ogg,.mp3,.wav,.mp4,.m4a,.wmv,.mov,.mkv"
#define VIDEO_CACHE_EXTENSION "jpg"

#define VIDEO_BUFFERS_COUNT 2

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

        int linkedAudioAsset;

        VideoMetadata(json_t probeJson);
        VideoMetadata() {}
    };

    using InternalTextureUnion = std::variant<PixelBufferMetadata, AudioMetadata, VideoMetadata>;

    struct TextureUnion;

    enum class AssetDecoderCommandType {
        Seek, Decode
    };

    struct AssetDecoderCommand {
        AssetDecoderCommandType type;
        int64_t pts;
        uint8_t* image;

        AssetDecoderCommand(int64_t pts);
        AssetDecoderCommand(uint8_t* image);
    };

    struct AssetDecoder;

    // Decodes video, texture assets
    struct AssetDecoder {
        std::vector<GPUExtendedHandle> transferBuffers;
        std::vector<GPUExtendedHandle> imageHandles;
        std::vector<AssetDecoderCommand> commandBuffer;
        std::thread* decoderTask;
        bool terminateDecoderTask;
        bool decodingFinished;
        int transferBufferIndex;
        int width, height;
        int lastLoadedFrame, frame, id;
        uint8_t* image;
        bool readyToBePresented;
        std::mutex* commandBufferMutex;

        VideoReaderState vr;

        AssetDecoder();

        GPUExtendedHandle GetGPUTexture(TextureUnion* asset, GPUExtendedHandle context = 0);

        void LoadHandle(TextureUnion* asset);
        void UnloadHandles();
        bool AreHandlesLoaded();
        GPUExtendedHandle GetImageHandle(TextureUnion* asset);

        int GetCurrentKeyFrameForFrame(int frame, TextureUnion* asset, int keyframeOffset = -1);

        void Destroy();    
    };

    // Combines AssetDecoder and ManagedImageHandle classes
    struct ManagedAssetDecoder {
        AssetDecoder decoder;
        GPUExtendedHandle previewHandle;
        TextureUnion* loadedAsset;

        ManagedAssetDecoder() {
            this->previewHandle = 0;
            this->loadedAsset = nullptr;
        }

        GPUExtendedHandle GetImageHandle(TextureUnion* asset);
        GPUExtendedHandle GetGPUTexture(TextureUnion* asset, GPUExtendedHandle context = 0);
        void Update(TextureUnion* asset);
        bool IsDisposed();

        void Reinitialize(TextureUnion* asset);
        void Dispose();

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
        std::vector<int> videoKeyframes;
        int id;
        int linkedAudioAsset;
        bool ready;
        bool invalid;
        bool visible;

        TextureUnion() {
            this->pboGpuTexture = 0;
            this->previewScale = 1.0f;
            this->ready = true;
            this->invalid = false;
            this->visible = true;
            this->linkedAudioAsset = 0;
        }

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

        static void Destroy();

        // Load asset and push it into registry
        static AssetLoadInfo ImportAsset(std::string path);
        // Load asset and get it's TextureUnion representation
        static AssetLoadInfo LoadAssetFromPath(std::string path);

        static TextureUnion* GetAsset(std::string id);

        static TextureUnionType TextureUnionTypeFromString(std::string type);

        static std::string StringFromTextureUnionType(TextureUnionType type);
    };
}
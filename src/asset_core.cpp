#include "asset_core.h"

namespace Electron {

    std::vector<TextureUnion> AssetCore::assets;
    std::vector<FaultyAssetDescription> AssetCore::faultyAssets;
    bool AssetCore::ffmpegActive = false;
    std::vector<AsyncFFMpegOperation> AssetCore::operations;

    AudioMetadata::AudioMetadata(json_t data) {
        json_t format = data["format"];
        this->audioLength = std::stof(JSON_AS_TYPE(format["duration"], std::string));
        this->bitrate = std::stoi(JSON_AS_TYPE(format["bit_rate"], std::string)) / 1024;
        this->sampleRate = std::stoi(JSON_AS_TYPE(data["streams"].at(0)["sample_rate"], std::string));
        this->codecName = JSON_AS_TYPE(data["streams"].at(0)["codec_long_name"], std::string);
    }

    VideoMetadata::VideoMetadata(json_t data) {
        json_t format = data["format"];
        this->duration = std::stof(JSON_AS_TYPE(format["duration"], std::string));
        this->codecName = JSON_AS_TYPE(format["format_long_name"], std::string);
        json_t firstStream = data["streams"].at(0);
        this->width = JSON_AS_TYPE(firstStream["width"], int);
        this->height = JSON_AS_TYPE(firstStream["height"], int);
        auto frameRateParts = split_string(JSON_AS_TYPE(firstStream["r_frame_rate"], std::string), "/");
        this->framerate = std::stof(frameRateParts[0]) / std::stof(frameRateParts[1]);
    }

    AssetDecoder::AssetDecoder() {
        this->transferBuffer = 0;
        this->imageHandle = 0;
        this->image = nullptr;
        this->lastLoadedFrame = -1;
        this->frame = 0;
        this->id = 0;
    }

    GPUExtendedHandle AssetDecoder::GetGPUTexture(TextureUnion* asset) {
        if (asset->type == TextureUnionType::Texture || asset->type == TextureUnionType::Audio) {
            if (!id) id = seedrand();
            if (transferBuffer) DriverCore::DestroyGPUTexture(transferBuffer);
            return asset->pboGpuTexture;
        } else if (asset->type == TextureUnionType::Video) {
            if (!id) id = seedrand();
            auto video = std::get<VideoMetadata>(asset->as);
            if (transferBuffer == 0 || video.width != width || video.height != height) {
                this->width = video.width;
                this->height = video.height;
                UnloadHandles();
                DriverCore::DestroyGPUTexture(transferBuffer);
                transferBuffer = DriverCore::GenerateGPUTexture(width, height, true);
                this->id = seedrand();
            }
            if (lastLoadedFrame != frame) {
                lastLoadedFrame = frame;
                std::string framePath = string_format("%s/%i/%i.%s", Shared::project.path.c_str(), asset->id, frame + 1, VIDEO_CACHE_EXTENSION);
                if (std::filesystem::exists(framePath)) {
                    int loadedWidth, loadedHeight, loadedChannels;
                    loadedChannels = 4;
                    image = stbi_load(framePath.c_str(), &loadedWidth, &loadedHeight, &loadedChannels, 4);
                }
                if (image) DriverCore::UpdateTextureData(transferBuffer, width, height, image);
                if (image) stbi_image_free(image);
                if (image) image = nullptr;
            }
            return transferBuffer;
        }
        return asset->pboGpuTexture;
    }

    void AssetDecoder::Destroy() {
        UnloadHandles();
        DriverCore::DestroyGPUTexture(transferBuffer);
        if (image) stbi_image_free(image);
    }

    void AssetDecoder::LoadHandle(TextureUnion* asset) {
        if (imageHandle) {
            throw std::runtime_error("image handle leak detected!");
        }
        imageHandle = DriverCore::GetImageHandleUI(asset->type == TextureUnionType::Video ? transferBuffer : asset->pboGpuTexture);
    }

    void AssetDecoder::UnloadHandles() {
        DriverCore::DestroyImageHandleUI(imageHandle);
        imageHandle = 0;
    }

    bool AssetDecoder::AreHandlesLoaded() {
        return imageHandle;
    }

    GPUExtendedHandle AssetDecoder::GetImageHandle(TextureUnion* asset) {
        return imageHandle;
    }

    /* Managed Asset Decoder */

    GPUExtendedHandle ManagedAssetDecoder::GetGPUTexture(TextureUnion* asset) {
        return decoder.GetGPUTexture(asset);
    }

    GPUExtendedHandle ManagedAssetDecoder::GetImageHandle(TextureUnion* asset) {
        return decoder.GetImageHandle(asset);
    }

    void ManagedAssetDecoder::Update(TextureUnion* asset) {
        if (asset && !previewHandle) {
            this->loadedAsset = asset;
            previewHandle = DriverCore::GetImageHandleUI(asset->pboGpuTexture);
        }
        if (asset != this->loadedAsset) {
            this->loadedAsset = asset;
            DriverCore::DestroyImageHandleUI(previewHandle);
            previewHandle = (asset ? asset->pboGpuTexture : 0);
            if (!previewHandle) {
                decoder.UnloadHandles();
            }
        }
    }

    bool ManagedAssetDecoder::IsDisposed() {
        return !decoder.imageHandle;
    }

    void ManagedAssetDecoder::Reinitialize(TextureUnion* asset) {
        decoder.LoadHandle(asset);
    }

    void ManagedAssetDecoder::Dispose() {
        decoder.UnloadHandles();
    }

    void ManagedAssetDecoder::Destroy() {
        DriverCore::DestroyImageHandleUI(previewHandle);
        previewHandle = 0;
        decoder.Destroy();
    }

    void TextureUnion::RebuildAssetData() {
        std::string oldName = name;
        PixelBuffer::DestroyGPUTexture(pboGpuTexture);
        pboGpuTexture = 0;
        *this = AssetCore::LoadAssetFromPath(path).result;
        this->name = oldName;
    }

    glm::vec2 TextureUnion::GetDimensions() {
    switch (type) {
    case TextureUnionType::Texture: {
        PixelBufferMetadata pbo = std::get<PixelBufferMetadata>(as);
        return {pbo.width, pbo.height};
    }
    case TextureUnionType::Audio: {
        return coverResolution;
    }
    case TextureUnionType::Video: {
        VideoMetadata video = std::get<VideoMetadata>(as);
        return {video.width, video.height};
    }
    default: {
        return {0, 0};
    }
    }
}

std::string TextureUnion::GetIcon() {
    return TextureUnion::GetIconByType(type);
}

std::string TextureUnion::GetIconByType(TextureUnionType type) {
    switch (type) {
    case TextureUnionType::Texture: {
        return ICON_FA_IMAGE;
    }
    case TextureUnionType::Audio: {
        return ICON_FA_MUSIC;
    }
    case TextureUnionType::Video: {
        return ICON_FA_VIDEO;
    }
    }
    return ICON_FA_QUESTION;
}

void TextureUnion::DumpData() {
    print("== Asset Dump " << name << " ==");
    print("type: " << AssetCore::StringFromTextureUnionType(type));
    print("path: " << path);
}

void TextureUnion::Destroy() {
    if (type == TextureUnionType::Texture || type == TextureUnionType::Audio) {
        PixelBuffer::DestroyGPUTexture(pboGpuTexture);
    }
    for (auto& path : linkedCache) {
        if (std::filesystem::exists(path))
            std::filesystem::remove_all(path);
    }
}

void AssetCore::LoadFromProject(json_t project) {
    Clear();

    json_t AssetCore = project["AssetCore"];
    for (int i = 0; i < AssetCore.size(); i++) {
        json_t assetDescription = AssetCore.at(i);
        TextureUnionType type = TextureUnionTypeFromString(
            JSON_AS_TYPE(assetDescription["Type"], std::string));
        std::string resourcePath =
            JSON_AS_TYPE(assetDescription["Path"], std::string);
        std::string internalName =
            JSON_AS_TYPE(assetDescription["InternalName"], std::string);
        std::vector<std::string> linkedCache{};
        for (auto& link : assetDescription["LinkedCache"]) {
            linkedCache.push_back(JSON_AS_TYPE(link, std::string));
        }
        try {
        std::string audioCoverPath = "";
        if (type == TextureUnionType::Audio) {
            audioCoverPath = JSON_AS_TYPE(assetDescription["AudioCoverPath"], std::string);
        }
        std::string ffprobeData = JSON_AS_TYPE(assetDescription["FFProbeData"], std::string);
        std::string ffprobeJsonData = JSON_AS_TYPE(assetDescription["FFProbeJsonData"], std::string);
        glm::vec2 audioCoverResolution = {0, 0};
        if (type == TextureUnionType::Audio && audioCoverPath != "") {
            audioCoverResolution = {
                JSON_AS_TYPE(assetDescription["AudioCoverResolution"].at(0), float),
                JSON_AS_TYPE(assetDescription["AudioCoverResolution"].at(1), float)
            };
        }
        int id = JSON_AS_TYPE(assetDescription["ID"], int);

        AssetLoadInfo assetUnion = LoadAssetFromPath(resourcePath);
        assetUnion.result.ffprobeData = ffprobeData;
        assetUnion.result.ffprobeJsonData = ffprobeJsonData;
        assetUnion.result.linkedCache = linkedCache;
        if (assetUnion.returnMessage != "") {
            print(resourcePath << ": " << assetUnion.returnMessage);
            throw std::runtime_error("fauly asset detected!");
        }
        assetUnion.result.audioCacheCover = audioCoverPath;
        assetUnion.result.coverResolution = audioCoverResolution;
        if (assetUnion.result.type == TextureUnionType::Audio && audioCoverPath != "") {
            PixelBuffer pbo(audioCoverPath);
            PixelBufferMetadata metadata;
            metadata.width = pbo.width;
            metadata.height = pbo.height;
            metadata.channels = 4;
            assetUnion.result.as = metadata;
            assetUnion.result.pboGpuTexture = pbo.BuildGPUTexture();
        } else if (assetUnion.result.type == TextureUnionType::Video) {
            std::string firstFramePath = string_format("%s/%i/%i.%s", Shared::project.path.c_str(), id, 1, VIDEO_CACHE_EXTENSION);
            DUMP_VAR(firstFramePath);
            if (file_exists(firstFramePath)) {
                PixelBuffer firstFramePbo(firstFramePath);
                assetUnion.result.pboGpuTexture = firstFramePbo.BuildGPUTexture();
            }
        }
        if (assetUnion.result.type == TextureUnionType::Audio) 
            assetUnion.result.as = AudioMetadata(json_t::parse(ffprobeJsonData));
        if (assetUnion.result.type == TextureUnionType::Video)
            assetUnion.result.as = VideoMetadata(json_t::parse(ffprobeJsonData));
        assetUnion.result.name = internalName;
        assetUnion.result.id = id;
        assetUnion.result.ready = true;

        assets.push_back(assetUnion.result);

        print("[" << JSON_AS_TYPE(assetDescription["Type"], std::string)
                  << "] Loaded " << resourcePath);
        } catch (...) {
            FaultyAssetDescription desc;
            desc.name = internalName;
            desc.type = type;
            desc.path = resourcePath;
            faultyAssets.push_back(desc);
        }
    }
}

AssetLoadInfo
AssetCore::LoadAssetFromPath(std::string path) {
    std::string retMessage = "";
    bool invalid = false;
    if (!file_exists(path)) {
        invalid = true;
        retMessage = "File does not exist '" + path + "'";
    }
    TextureUnionType targetAssetType = static_cast<TextureUnionType>(0);
    std::string lowerPath = lowercase(path);
    if (hasEnding(lowerPath, ".png") || hasEnding(lowerPath, ".jpg") ||
        hasEnding(lowerPath, ".jpeg") || hasEnding(lowerPath, ".tga")) {
        targetAssetType = TextureUnionType::Texture;
    } else if (hasEnding(lowerPath, ".ogg") || hasEnding(lowerPath, ".mp3") || hasEnding(lowerPath, ".wav")) {
        targetAssetType = TextureUnionType::Audio;
    } else if (hasEnding(lowerPath, ".mp4") || hasEnding(lowerPath, ".wmv") || hasEnding(lowerPath, ".mkv") || hasEnding(lowerPath, ".mov")) {
        targetAssetType = TextureUnionType::Video;
    } else {
        retMessage = "Unsupported File Format '" + path + "'";
        invalid = true;
    }

    TextureUnion assetUnion{};
    assetUnion.type = targetAssetType;
    assetUnion.strType = StringFromTextureUnionType(targetAssetType);
    assetUnion.name = "New Asset";
    assetUnion.path = path;
    assetUnion.previewScale = 1.0f;
    if (!invalid) {
        switch (targetAssetType) {
        case TextureUnionType::Texture: {
            PixelBuffer pbo(path);
            PixelBufferMetadata metadata;
            metadata.width = pbo.width;
            metadata.height = pbo.height;
            metadata.channels = 4;
            assetUnion.as = metadata;
            assetUnion.pboGpuTexture =
                pbo.BuildGPUTexture();
            break;
        }
        case TextureUnionType::Audio: {
            assetUnion.as = AudioMetadata();
            break;
        }   
        case TextureUnionType::Video: {
            assetUnion.as = VideoMetadata();
            break;
        }

        default: {
            retMessage = "Editor is now unstable, restart now.";
        }
        }
    }

    AssetLoadInfo result{};
    result.result = assetUnion;
    result.returnMessage = retMessage;
    return result;
}


std::string AssetCore::ImportAsset(std::string path) {
    AssetLoadInfo loadInfo = LoadAssetFromPath(path);
    if (loadInfo.returnMessage != "")
        return loadInfo.returnMessage;
    loadInfo.result.id = seedrand();
    if (file_exists(path)) {
        std::string cachePath = string_format("%s/%i.%s", Shared::project.path.c_str(), loadInfo.result.id, std::filesystem::path(path).extension().c_str());
        std::filesystem::copy(path, cachePath);
        loadInfo.result.path = cachePath;
        path = cachePath;
    }
    assets.push_back(loadInfo.result);
    if (loadInfo.result.type == TextureUnionType::Texture) {
        // Texture FFProbing
        TextureUnion* tu = &assets[assets.size() - 1];
        operations.push_back(AsyncFFMpegOperation(
            [](OperationArgs_T& args) {
                TextureUnion* tu = std::any_cast<TextureUnion*>(args[0]);
                tu->ready = false;
                tu->ffprobeData = filterFFProbe(read_file(".ffprobe_data"));
                tu->ready = true;
            }, {tu}, string_format("ffprobe %s &> .ffprobe_data", tu->path.c_str()),
                string_format("%s %s '%s'", ICON_FA_INFO, ELECTRON_GET_LOCALIZATION("FFPROBE_GATHERING"), tu->path.c_str())
        ));
    }
    if (loadInfo.result.type == TextureUnionType::Audio) {
        TextureUnion* tu = &assets[assets.size() - 1];
        tu->as = AudioMetadata();
        tu->ready = false;
        std::string originalAudioPath = tu->path;

        // Gather FFProbe data and extract cover
        operations.push_back(AsyncFFMpegOperation(
            [](OperationArgs_T& args) {
                TextureUnion* tu = std::any_cast<TextureUnion*>(args[0]);
                try {
                std::vector<AsyncFFMpegOperation>* operations = 
                    std::any_cast<std::vector<AsyncFFMpegOperation>*>(args[1]);
                std::string originalAudioPath = std::any_cast<std::string>(args[2]);
                int cacheIndex = std::any_cast<int>(args[3]);
                std::string ffprobeData = filterFFProbe(read_file(".ffprobe_data"));
                std::string ffprobeJsonData = read_file(".ffprobe_json");
                tu->ffprobeData = ffprobeData;
                tu->ffprobeJsonData = ffprobeJsonData;
                tu->as = AudioMetadata(json_t::parse(ffprobeJsonData));
                if (tu->ffprobeData.find("attached pic") != std::string::npos) {
                    std::string coverPath = string_format("%s/%i.png", Shared::project.path.c_str(), Cache::GetCacheIndex());
                    tu->linkedCache.push_back(coverPath);
                    /* Perform async cover extraction */
                    operations->push_back(AsyncFFMpegOperation(
                        [](OperationArgs_T& args) {
                            std::string coverPath = std::any_cast<std::string>(args[0]);
                            TextureUnion* tu = std::any_cast<TextureUnion*>(args[1]);
                            tu->audioCacheCover = coverPath;
                            PixelBuffer coverBuffer = PixelBuffer(coverPath);
                            tu->pboGpuTexture = coverBuffer.BuildGPUTexture();
                            tu->coverResolution = {
                                coverBuffer.width, coverBuffer.height
                            };
                        }, {coverPath, tu}, string_format("ffmpeg -y -i '%s' '%s' >/dev/null", originalAudioPath.c_str(), coverPath.c_str()), 
                            string_format("%s %s '%s'", ICON_FA_IMAGE, ELECTRON_GET_LOCALIZATION("GATHERING_COVER"), tu->path.c_str())
                    ));
                } else {
                    PixelBuffer wavTex = PixelBuffer("misc/wav.png");
                    tu->audioCacheCover = "misc/wav.png";
                    tu->pboGpuTexture = wavTex.BuildGPUTexture();
                    tu->coverResolution = {
                        wavTex.width, wavTex.height
                    };
                }
                if (!hasEnding(tu->path, ".wav")) {
                    std::string formattedCachePath = string_format("%s/%i.wav", Shared::project.path.c_str(), Cache::GetCacheIndex());
                    tu->linkedCache.push_back(formattedCachePath);
                    operations->push_back(AsyncFFMpegOperation(
                        [](OperationArgs_T& coverArgs){
                            TextureUnion* tu = std::any_cast<TextureUnion*>(coverArgs[0]);
                            std::string formattedCachePath = std::any_cast<std::string>(coverArgs[1]);
                            tu->path = formattedCachePath;
                        }, {tu, formattedCachePath},
                        string_format("ffmpeg -y -i '%s' '%s'", originalAudioPath.c_str(), formattedCachePath.c_str()),
                        string_format("%s %s '%s'", ICON_FA_FILE_IMPORT, ELECTRON_GET_LOCALIZATION("GENERIC_IMPORTING"), tu->path.c_str())
                    ));
                }
                tu->ready = true;
                } catch (std::runtime_error& error) {
                    tu->invalid = true;
                    operations.push_back(
                        AsyncFFMpegOperation(
                            nullptr, {}, "sleep 5", 
                            string_format("%s %s '%s'", ICON_FA_TRIANGLE_EXCLAMATION, ELECTRON_GET_LOCALIZATION("CANNOT_IMPORT_ASSET"), tu->path.c_str())
                        )
                    );
                }
            }, {tu, &operations, originalAudioPath, Cache::GetCacheIndex()}, string_format("(ffprobe -v quiet -print_format json -show_format -show_streams '%s' &> .ffprobe_json) && (ffprobe '%s' &> .ffprobe_data)", tu->path.c_str(), tu->path.c_str()),
                string_format("%s %s '%s'", ICON_FA_INFO, ELECTRON_GET_LOCALIZATION("FFPROBE_GATHERING"), tu->path.c_str())
        ));
    } else if (loadInfo.result.type == TextureUnionType::Video) {
        TextureUnion* tu = &assets[assets.size() - 1];
        tu->linkedCache.resize(2);
        tu->as = VideoMetadata();
        tu->ready = false;
        std::string originalAudioPath = tu->path;
        operations.push_back(AsyncFFMpegOperation(
            [](OperationArgs_T& args) {
                TextureUnion* tu = std::any_cast<TextureUnion*>(args[0]);
                std::vector<AsyncFFMpegOperation>* operations = 
                    std::any_cast<std::vector<AsyncFFMpegOperation>*>(args[1]);
                std::string ffprobeData = filterFFProbe(read_file(".ffprobe_data"));
                std::string ffprobeJsonData = read_file(".ffprobe_json");
                tu->ffprobeData = ffprobeData;
                tu->ffprobeJsonData = ffprobeJsonData;
                tu->as = VideoMetadata(json_t::parse(ffprobeJsonData));
                std::string idPath = string_format("%s/%i", Shared::project.path.c_str(), tu->id);
                std::string wavPath = string_format("%s/audio.wav", idPath.c_str());
                if (!file_exists(idPath)) {
                    std::filesystem::create_directories(idPath);
                }
                operations->push_back(AsyncFFMpegOperation(
                    [](OperationArgs_T& args) {
                        TextureUnion* tu = std::any_cast<TextureUnion*>(args[0]);
                        std::string firstFramePath = string_format("%s/%i/%i.%s", Shared::project.path.c_str(), tu->id, 1, VIDEO_CACHE_EXTENSION);
                        if (file_exists(firstFramePath)) {
                            PixelBuffer firstFramePbo(firstFramePath);
                            tu->pboGpuTexture = firstFramePbo.BuildGPUTexture();
                        }
                        tu->ready = true;
                    }, {tu, operations}, 
                        string_format("ffmpeg -i '%s' '%s/%%d.%s'", tu->path.c_str(), idPath.c_str(), VIDEO_CACHE_EXTENSION),
                        string_format("%s %s '%s'", ICON_FA_INFO, ELECTRON_GET_LOCALIZATION("CACHING_VIDEO_FRAMES"), tu->path.c_str())
                ));
                operations->push_back(AsyncFFMpegOperation(
                    nullptr, {},
                    string_format("ffmpeg -i '%s' '%s'", tu->path.c_str(), wavPath.c_str()),
                    string_format("%s %s '%s'", ICON_FA_INFO, ELECTRON_GET_LOCALIZATION("CACHING_AUDIO"), tu->path.c_str())
                ));
                tu->linkedCache[0] = idPath;
                tu->linkedCache[1] = wavPath;
            }, {tu, &operations}, string_format("ffprobe -v quiet -print_format json -show_format -show_streams '%s' &> .ffprobe_json & ffprobe '%s' &> .ffprobe_data", tu->path.c_str(), tu->path.c_str()),
                string_format("%s %s '%s'", ICON_FA_INFO, ELECTRON_GET_LOCALIZATION("FFPROBE_GATHERING"), tu->path.c_str())
        ));
    }
    return loadInfo.returnMessage;
}

TextureUnion* AssetCore::GetAsset(std::string id) {
    int valueID = hexToInt(id);
    for (int i = 0; i < assets.size(); i++) {
        if (assets[i].id == valueID) {
                return &assets.at(i);
        }
    }
    return nullptr;
}

void AssetCore::Clear() { 
    assets.clear(); 
    faultyAssets.clear();
}

void AssetCore::Destroy() {
    for (auto& asset : assets) {
        asset.Destroy();
    }
    assets.clear();
}

TextureUnionType AssetCore::TextureUnionTypeFromString(std::string type) {
    if (type == "Image") {
        return TextureUnionType::Texture;
    }
    if (type == "Audio") {
        return TextureUnionType::Audio;
    }
    if (type == "Video") {
        return TextureUnionType::Video;
    }
    return TextureUnionType::Texture;
}
        
std::string AssetCore::StringFromTextureUnionType(TextureUnionType type) {
    switch (type) {
        case TextureUnionType::Texture: {
            return "Image";
        }
        case TextureUnionType::Audio: {
            return "Audio";
        }
        case TextureUnionType::Video: {
            return "Video";
        }
    }
    return "Image";
}
}
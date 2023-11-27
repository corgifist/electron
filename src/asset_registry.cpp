#include "asset_registry.h"

namespace Electron {
    void TextureUnion::RebuildAssetData() {
        std::string oldName = name;
        if (!file_exists(path)) {
            invalid = true;
            type = TextureUnionType::Texture;
            GenerateUVTexture();
            PixelBuffer::DestroyGPUTexture(pboGpuTexture);
            pboGpuTexture = std::get<PixelBuffer>(as).BuildGPUTexture();
            return;
        } else
            invalid = false;
        PixelBuffer::DestroyGPUTexture(pboGpuTexture);
        pboGpuTexture = 0;
        *this = Shared::assets->LoadAssetFromPath(path).result;
        this->name = oldName;
    }

    glm::vec2 TextureUnion::GetDimensions() {
    switch (type) {
    case TextureUnionType::Texture: {
        PixelBuffer pbo = std::get<PixelBuffer>(as);
        return {pbo.width, pbo.height};
    }
    case TextureUnionType::Audio: {
        return coverResolution;
    }
    default: {
        return {0, 0};
    }
    }
}

void TextureUnion::GenerateUVTexture() {
    type = TextureUnionType::Texture;
    as = PixelBuffer(256, 256);
    PixelBuffer &buffer = std::get<PixelBuffer>(as);
    for (int x = 0; x < buffer.width; x++) {
        for (int y = 0; y < buffer.height; y++) {
            buffer.SetPixel(x, y,
                            Pixel((float)x / (float)buffer.width,
                                  (float)y / (float)buffer.height, 0, 1));
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
    }
    return ICON_FA_QUESTION;
}

void AssetRegistry::LoadFromProject(json_t project) {
    Clear();

    json_t assetRegistry = project["AssetRegistry"];
    for (int i = 0; i < assetRegistry.size(); i++) {
        json_t assetDescription = assetRegistry.at(i);
        TextureUnionType type = TextureUnionTypeFromString(
            JSON_AS_TYPE(assetDescription["Type"], std::string));
        std::string resourcePath =
            JSON_AS_TYPE(assetDescription["Path"], std::string);
        std::string internalName =
            JSON_AS_TYPE(assetDescription["InternalName"], std::string);
        try {
        std::string audioCoverPath = "";
        glm::vec2 audioCoverResolution = {0, 0};
        if (type == TextureUnionType::Audio) {
            audioCoverPath = JSON_AS_TYPE(assetDescription["AudioCoverPath"], std::string);
            audioCoverResolution = {
                JSON_AS_TYPE(assetDescription["AudioCoverResolution"].at(0), float),
                JSON_AS_TYPE(assetDescription["AudioCoverResolution"].at(1), float)
            };
        }
        int id = JSON_AS_TYPE(assetDescription["ID"], int);

        AssetLoadInfo assetUnion = LoadAssetFromPath(resourcePath);
        if (assetUnion.returnMessage != "") {
            print(resourcePath << ": " << assetUnion.returnMessage);
            assetUnion.result.GenerateUVTexture();
        }
        assetUnion.result.audioCacheCover = audioCoverPath;
        assetUnion.result.coverResolution = audioCoverResolution;
        DUMP_VAR(assetUnion.result.audioCacheCover);
        if (assetUnion.result.type == TextureUnionType::Audio) {
            assetUnion.result.pboGpuTexture = PixelBuffer(audioCoverPath).BuildGPUTexture();
        }
        assetUnion.result.name = internalName;
        assetUnion.result.id = id;

        assets.push_back(assetUnion.result);

        print("[" << JSON_AS_TYPE(assetDescription["Type"], std::string)
                  << "] Loaded " << resourcePath);
        } catch (std::runtime_error ex) {
            FaultyAssetDescription desc;
            desc.name = internalName;
            desc.type = type;
            desc.path = resourcePath;
            faultyAssets.push_back(desc);
        }
    }
}

AssetLoadInfo
AssetRegistry::LoadAssetFromPath(std::string path) {
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
    assetUnion.invalid = invalid;
    if (!invalid) {
        switch (targetAssetType) {
        case TextureUnionType::Texture: {
            assetUnion.as = PixelBuffer(path);
            assetUnion.pboGpuTexture =
                std::get<PixelBuffer>(assetUnion.as).BuildGPUTexture();
            break;
        }
        case TextureUnionType::Audio: {
            AudioMetadata metadata{};
            system(string_format("ffprobe %s &> .ffprobe_data", assetUnion.path.c_str()).c_str());
            metadata.probe = read_file(".ffprobe_data");
            std::vector<std::string> probeLines = split_string(metadata.probe, "\n");
            for (auto& line : probeLines) {
                line = trim_copy(line);
                if (hasBegining(line, "Duration: ")) {
                    int hours, minutes, seconds;
                    sscanf(line.c_str(), "Duration: %i:%i:%i.%*c", &hours, &minutes, &seconds);
                    metadata.audioLength = seconds + (minutes * 60) + (hours * 3600);
                }
            }
            assetUnion.as = metadata;
            break;
        }   

        default: {
            retMessage = "Editor is now unstable, restart now.";
        }
        }
    } else {
        assetUnion.type = TextureUnionType::Texture;
        assetUnion.strType = "Image";
        assetUnion.GenerateUVTexture();
        assetUnion.pboGpuTexture =
            std::get<PixelBuffer>(assetUnion.as).BuildGPUTexture();
    }

    AssetLoadInfo result{};
    result.result = assetUnion;
    result.returnMessage = retMessage;
    return result;
}


std::string AssetRegistry::ImportAsset(std::string path) {
    AssetLoadInfo loadInfo = LoadAssetFromPath(path);
    if (loadInfo.returnMessage != "")
        return loadInfo.returnMessage;
    loadInfo.result.id = seedrand();
    assets.push_back(loadInfo.result);
    if (loadInfo.result.type == TextureUnionType::Audio) {
        TextureUnion* tu = &assets[assets.size() - 1];
        std::string originalAudioPath = tu->path;
        if (!hasEnding(tu->path, ".ogg")) {
            std::string formattedCachePath = string_format("cache/%i.ogg", Cache::GetCacheIndex());
            // PushAsyncOperation(string_format("ffmpeg -i %s %s", tu.path.c_str(), formattedCachePath.c_str()), nullptr, {});
            operations.push_back(AsyncFFMpegOperation(
                nullptr, {},
                {"ffmpeg", "-i", tu->path, formattedCachePath}
            ));
            tu->path = formattedCachePath;
        }
        std::string coverPath = string_format("cache/%i.png", Cache::GetCacheIndex());
        /* Perform async cover extraction*/
        operations.push_back(AsyncFFMpegOperation(
            [](OperationArgs_T& args) {
                std::string coverPath = std::any_cast<std::string>(args[0]);
                TextureUnion* tu = std::any_cast<TextureUnion*>(args[1]);
                PixelBuffer coverBuffer = PixelBuffer(coverPath);
                tu->audioCacheCover = coverPath;
                tu->pboGpuTexture = coverBuffer.BuildGPUTexture();
                tu->coverResolution = {
                    coverBuffer.width, coverBuffer.height
                };
            }, {coverPath, tu}, {"ffmpeg", "-i", originalAudioPath, coverPath}
        ));
    }
    return loadInfo.returnMessage;
}

void AssetRegistry::Clear() { 
    this->assets.clear(); 
    faultyAssets.clear();
}
}
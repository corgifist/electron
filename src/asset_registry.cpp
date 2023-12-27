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
        if (type == TextureUnionType::Audio) {
            audioCoverPath = JSON_AS_TYPE(assetDescription["AudioCoverPath"], std::string);
        }
        std::string ffprobeData = JSON_AS_TYPE(assetDescription["FFProbeData"], std::string);
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
        if (assetUnion.returnMessage != "") {
            print(resourcePath << ": " << assetUnion.returnMessage);
            assetUnion.result.GenerateUVTexture();
        }
        assetUnion.result.audioCacheCover = audioCoverPath;
        assetUnion.result.coverResolution = audioCoverResolution;
        DUMP_VAR(assetUnion.result.audioCacheCover);
        if (assetUnion.result.type == TextureUnionType::Audio && audioCoverPath != "") {
            assetUnion.result.pboGpuTexture = PixelBuffer(audioCoverPath).BuildGPUTexture();
        }
        if (assetUnion.result.type == TextureUnionType::Audio) {
            AudioMetadata metadata = std::get<AudioMetadata>(assetUnion.result.as);
            assetUnion.result.ffprobeData = JSON_AS_TYPE(assetDescription["FFProbeData"], std::string);
            std::vector<std::string> probeLines = split_string(assetUnion.result.ffprobeData, "\n");

            for (auto& line : probeLines) {
                line = trim_copy(line);
                if (hasBegining(line, "Duration: ")) {
                    int hours, minutes;
                    float seconds;
                    sscanf(line.c_str(), "Duration: %i:%i:%f%*c", &hours, &minutes, &seconds);
                    metadata.audioLength = seconds + (minutes * 60) + (hours * 3600);
                }
            }
            assetUnion.result.as = metadata;
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
            assetUnion.as = AudioMetadata();
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
                std::vector<AsyncFFMpegOperation>* operations = 
                    std::any_cast<std::vector<AsyncFFMpegOperation>*>(args[1]);
                std::string originalAudioPath = std::any_cast<std::string>(args[2]);
                int cacheIndex = std::any_cast<int>(args[3]);
                std::string ffprobe = filterFFProbe(read_file(".ffprobe_data"));
                AudioMetadata metadata{};
                tu->ffprobeData = ffprobe;
                std::vector<std::string> probeLines = split_string(tu->ffprobeData, "\n");
                for (auto& line : probeLines) {
                    line = trim_copy(line);
                    if (hasBegining(line, "Duration: ")) {
                        int hours, minutes;
                        float seconds;
                        sscanf(line.c_str(), "Duration: %i:%i:%f%*c", &hours, &minutes, &seconds);
                        metadata.audioLength = seconds + (minutes * 60.0f) + (hours * 3600.0f);
                    }
                }
                tu->as = metadata;
                if (tu->ffprobeData.find("attached pic") != std::string::npos) {
                    std::string coverPath = string_format("cache/%i.png", cacheIndex);
                    /* Perform async cover extraction */
                    operations->push_back(AsyncFFMpegOperation(
                        [](OperationArgs_T& args) {
                            std::string coverPath = std::any_cast<std::string>(args[0]);
                            TextureUnion* tu = std::any_cast<TextureUnion*>(args[1]);
                            PixelBuffer coverBuffer = PixelBuffer(coverPath);
                            tu->audioCacheCover = coverPath;
                            tu->pboGpuTexture = coverBuffer.BuildGPUTexture();
                            tu->coverResolution = {
                                coverBuffer.width, coverBuffer.height
                            };
                        }, {coverPath, tu}, string_format("ffmpeg -y -i %s %s &>/dev/null", originalAudioPath.c_str(), coverPath.c_str()), 
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
                    std::string formattedCachePath = string_format("cache/%i.wav", Cache::GetCacheIndex());
                    operations->push_back(AsyncFFMpegOperation(
                        nullptr, {},
                        string_format("ffmpeg -y -i %s %s &>/dev/null", tu->path.c_str(), formattedCachePath.c_str()),
                        string_format("%s %s '%s'", ICON_FA_FILE_IMPORT, ELECTRON_GET_LOCALIZATION("GENERIC_IMPORTING"), tu->path.c_str())
                    ));
                    tu->path = formattedCachePath;
                }
                tu->as = metadata;
                tu->ready = true;
            }, {tu, &operations, originalAudioPath, Cache::GetCacheIndex()}, string_format("ffprobe %s &> .ffprobe_data", tu->path.c_str()),
                string_format("%s %s '%s'", ICON_FA_INFO, ELECTRON_GET_LOCALIZATION("FFPROBE_GATHERING"), tu->path.c_str())
        ));
    }
    
    // Quickly update editor, so cache index will always be fresh
    Servers::AsyncWriterRequest({
        {"action", "write"},
        {"path", "misc/config.json"},
        {"content", Shared::configMap.dump()}
    });
    return loadInfo.returnMessage;
}

void AssetRegistry::Clear() { 
    this->assets.clear(); 
    faultyAssets.clear();
}
}
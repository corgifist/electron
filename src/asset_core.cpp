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
        json_t firstStream = {};
        for (auto& stream : data["streams"]) {
            if (stream["codec_type"] == "video") {
                firstStream = stream;
                break;
            }
        }
        this->width = JSON_AS_TYPE(firstStream["width"], int);
        this->height = JSON_AS_TYPE(firstStream["height"], int);
        auto frameRateParts = split_string(JSON_AS_TYPE(firstStream["r_frame_rate"], std::string), "/");
        this->framerate = std::stof(frameRateParts[0]) / std::stof(frameRateParts[1]);

        this->linkedAudioAsset = 0;
    }

    AssetDecoderCommand::AssetDecoderCommand(int64_t pts) {
        this->type = AssetDecoderCommandType::Seek;
        this->pts = pts;
    }

    AssetDecoderCommand::AssetDecoderCommand(uint8_t* image) {
        this->type = AssetDecoderCommandType::Decode;
        this->image = image;
    }

    AssetDecoder::AssetDecoder() {
        this->transferBuffers = {};
        this->imageHandles = {};
        this->lastLoadedFrame = -1;
        this->transferBufferIndex = 0;
        this->frame = 0;
        this->id = 0;
        this->image = nullptr;
        this->decoderTask = nullptr;
        this->terminateDecoderTask = false;
        this->decodingFinished = false;
    }

    GPUExtendedHandle AssetDecoder::GetGPUTexture(TextureUnion* asset) {
        if (!asset->ready) return 0;
        if (asset->type == TextureUnionType::Texture || asset->type == TextureUnionType::Audio) {
            if (!id) id = seedrand();
            Destroy();
            return asset->pboGpuTexture;
        } else if (asset->type == TextureUnionType::Video) {
            if (!id) id = seedrand();
            auto video = std::get<VideoMetadata>(asset->as);
            if (transferBuffers.size() == 0 || video.width != width || video.height != height) {
                this->width = video.width;
                this->height = video.height;
                Destroy();
                image = new uint8_t[width * height * 4];

                video_reader_open(&vr, asset->path.c_str(), Shared::deviceName.c_str(), !Shared::configMap["UseVideoGPUAcceleration"]);

                decoderTask = new std::thread([](AssetDecoder* decoder) {
                    while (true) {
                        if (decoder->terminateDecoderTask) break;
                        while (decoder->decodingFinished) {
                            if (decoder->terminateDecoderTask) break;
                        }
                        if (decoder->terminateDecoderTask) break;
                        auto commandBuffer = decoder->commandBuffer;
                        decoder->commandBuffer.clear();
                        for (auto& command : commandBuffer) {
                            if (decoder->terminateDecoderTask) break;
                            if (command.type == AssetDecoderCommandType::Decode) {
                                video_reader_read_frame(&decoder->vr, command.image, nullptr);
                            } else if (command.type == AssetDecoderCommandType::Seek) {
                                video_reader_seek_frame(&decoder->vr, command.pts);
                            }
                        }
                        if (decoder->terminateDecoderTask) break;
                        decoder->decodingFinished = true;
                    }
                }, this);

                for (int i = 0; i < DriverCore::FramesInFlightCount(); i++) {
                    transferBuffers.push_back(DriverCore::GenerateGPUTexture(width, height, true));
                    float blackPtr[4] = {
                        0, 0, 0, 1
                    };
                    DriverCore::ClearTextureImage(transferBuffers[i], 0, blackPtr);
                }
                this->id = seedrand();
            }
            if (frame == 0) {
                lastLoadedFrame = 0;
                commandBuffer.push_back(AssetDecoderCommand((int64_t) 0));
                return asset->pboGpuTexture;
            }
            if (lastLoadedFrame != frame) {
                int frameDifference = frame - lastLoadedFrame;
                int reservedLastLoadedFrame = lastLoadedFrame;
                lastLoadedFrame = frame;

                int currentKeyframe = GetCurrentKeyFrameForFrame(frame, asset);
                int lastKeyframe = GetCurrentKeyFrameForFrame(reservedLastLoadedFrame, asset);

                bool hardcoreDecode = false;

                if (frameDifference < 0 && frameDifference != 1) {
                    hardcoreDecode = true;
                } else if (frameDifference > 1) {
                    if (currentKeyframe != lastKeyframe) {
                        hardcoreDecode = true;
                    } else {
                        frameDifference--;
                        while (frameDifference-- > 0) {
                            commandBuffer.push_back(AssetDecoderCommand(nullptr));
                        }
                    }
                }

                if (hardcoreDecode) {
                    commandBuffer.clear();
                    int64_t currentKeyframePts = int64_t((double) currentKeyframe / video.framerate) * (double) vr.time_base.den / (double) vr.time_base.num;
                    commandBuffer.push_back(AssetDecoderCommand(currentKeyframePts));
                    int framesToDecode = frame - currentKeyframe - 1;
                    while (framesToDecode-- > 0) {
                        commandBuffer.push_back(AssetDecoderCommand(nullptr));
                    }
                }

                commandBuffer.push_back(AssetDecoderCommand(image));
                if (decodingFinished) transferBufferIndex = (transferBufferIndex + 1) % DriverCore::FramesInFlightCount();
                if (decodingFinished) DriverCore::UpdateTextureData(transferBuffers[transferBufferIndex], width, height, image);
                if (decodingFinished) decodingFinished = false;
                return frame == 0 ? asset->pboGpuTexture : transferBuffers[transferBufferIndex];
            }
            if (commandBuffer.size() != 0) {
                decodingFinished = false;
            }
            return transferBuffers[transferBufferIndex];
        }
        return asset->pboGpuTexture;
    }

    int AssetDecoder::GetCurrentKeyFrameForFrame(int frame, TextureUnion* asset, int offset) {
        if (frame < 0) return 0;
        for (int i = 0; i < asset->videoKeyframes.size(); i++) {
            int keyframe = asset->videoKeyframes[i];
            if (frame == keyframe) return keyframe;
            if (keyframe > frame) {
                return asset->videoKeyframes[glm::clamp(i - 1, 0, (int) asset->videoKeyframes.size() + offset)];
            }
        }
        return 0;
    }

    void AssetDecoder::Destroy() {
        id = 0;
        UnloadHandles();
        for (auto& transferBuffer : transferBuffers) {
            DriverCore::DestroyGPUTexture(transferBuffer);
        }
        transferBuffers.clear();
        if (decoderTask) {
            terminateDecoderTask = true;
            decoderTask->join();
            delete decoderTask;
            decoderTask = nullptr;
        }
        if (vr.sws_scaler_ctx) {
            video_reader_close(&vr);
        }
        terminateDecoderTask = false;
        delete image;
        image = nullptr;
    }

    void AssetDecoder::LoadHandle(TextureUnion* asset) {
        if (imageHandles.size() != 0) {
            throw std::runtime_error("image handle leak detected!");
        }
        if (!asset->ready) return;
        if (asset->type == TextureUnionType::Texture || asset->type == TextureUnionType::Audio) {
            imageHandles.push_back(DriverCore::GetImageHandleUI(asset->pboGpuTexture));
        } else {
            for (auto& transferBuffer : transferBuffers) {
                imageHandles.push_back(DriverCore::GetImageHandleUI(transferBuffer));
            }
            imageHandles.push_back(DriverCore::GetImageHandleUI(asset->pboGpuTexture));
        }
    }

    void AssetDecoder::UnloadHandles() {
        for (auto& imageHandle : imageHandles) {
            DriverCore::DestroyImageHandleUI(imageHandle);
        }
        imageHandles.clear();
    }

    bool AssetDecoder::AreHandlesLoaded() {
        return imageHandles.size() != 0;
    }

    GPUExtendedHandle AssetDecoder::GetImageHandle(TextureUnion* asset) {
        if (asset->type == TextureUnionType::Video && lastLoadedFrame == 0) {
            return imageHandles[3];
        } 
        return asset->type == TextureUnionType::Texture ? imageHandles[0] : imageHandles[transferBufferIndex];
    }

    /* Managed Asset Decoder */

    GPUExtendedHandle ManagedAssetDecoder::GetGPUTexture(TextureUnion* asset) {
        if (!asset) return 0;
        return decoder.GetGPUTexture(asset);
    }

    GPUExtendedHandle ManagedAssetDecoder::GetImageHandle(TextureUnion* asset) {
        if (!asset) return 0;
        return decoder.GetImageHandle(asset);
    }

    void ManagedAssetDecoder::Update(TextureUnion* asset) {
        if (!asset) return;
        if (!asset->ready) return;
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
        return !decoder.AreHandlesLoaded();
    }

    void ManagedAssetDecoder::Reinitialize(TextureUnion* asset) {
        if (!asset) return;
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

    if (linkedAudioAsset != 0) {
        int assetIndex = 0;
        for (auto& asset : AssetCore::assets) {
            if (asset.id == linkedAudioAsset) {
                break;
            }
            assetIndex++;
        }
        AssetCore::assets.erase(AssetCore::assets.begin() + assetIndex);
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
        assetUnion.result.videoKeyframes = JSON_AS_TYPE(assetDescription["VideoKeyframes"], std::vector<int>);
        if (assetUnion.returnMessage != "") {
            print(resourcePath << ": " << assetUnion.returnMessage);
            throw std::runtime_error("fauly asset detected!");
        }
        assetUnion.result.audioCacheCover = audioCoverPath;
        assetUnion.result.coverResolution = audioCoverResolution;
        assetUnion.result.linkedAudioAsset = assetDescription["LinkedAudioAsset"];
        assetUnion.result.visible = assetDescription["Visible"];
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
    } else if (hasEnding(lowerPath, ".ogg") || hasEnding(lowerPath, ".mp3") || hasEnding(lowerPath, ".wav") || hasEnding(lowerPath, ".m4a")) {
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


AssetLoadInfo AssetCore::ImportAsset(std::string path) {
    AssetLoadInfo loadInfo = LoadAssetFromPath(path);
    if (loadInfo.returnMessage != "")
        return loadInfo.returnMessage;
    loadInfo.result.id = Cache::GetCacheIndex();
    if (file_exists(path)) {
        std::string cachePath = string_format("%s/%i%s", Shared::project.path.c_str(), loadInfo.result.id, std::filesystem::path(path).extension().c_str());
        if (file_exists(cachePath)) {
            std::filesystem::remove(cachePath);
        }
        std::filesystem::copy(path, cachePath);
        loadInfo.result.path = cachePath;
        path = cachePath;
        loadInfo.result.linkedCache.push_back(cachePath);
    }
    assets.push_back(loadInfo.result);
    if (loadInfo.result.type == TextureUnionType::Texture) {
        // Texture FFProbing
        TextureUnion* tu = &assets[assets.size() - 1];
        operations.push_back(AsyncFFMpegOperation(
            [](OperationArgs_T& args) {
                TextureUnion* tu = std::any_cast<TextureUnion*>(args[0]);
                tu->ffprobeData = filterFFProbe(read_file(".ffprobe_data"));
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
                            tu->linkedCache.push_back(formattedCachePath);
                        }, {tu, formattedCachePath},
                        string_format("ffmpeg -y -i '%s' '%s'", originalAudioPath.c_str(), formattedCachePath.c_str()),
                        string_format("%s %s '%s'", ICON_FA_FILE_IMPORT, ELECTRON_GET_LOCALIZATION("GENERIC_IMPORTING"), tu->path.c_str())
                    ));
                    tu->linkedCache.push_back(formattedCachePath);
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
            }, {tu, &operations, originalAudioPath, Cache::GetCacheIndex()}, string_format("(ffprobe -v quiet -print_format json -show_format -show_streams '%s' &> .ffprobe_json) ; (ffprobe '%s' &> .ffprobe_data)", tu->path.c_str(), tu->path.c_str()),
                string_format("%s %s '%s'", ICON_FA_INFO, ELECTRON_GET_LOCALIZATION("FFPROBE_GATHERING"), tu->path.c_str())
        ));
    } else if (loadInfo.result.type == TextureUnionType::Video) {
        TextureUnion* tu = &assets[assets.size() - 1];
        tu->as = VideoMetadata();
        tu->ready = false;
        std::string originalAudioPath = tu->path;
        operations.push_back(AsyncFFMpegOperation(
            [](OperationArgs_T& args) {
                TextureUnion* tu = std::any_cast<TextureUnion*>(args[0]);
                std::vector<AsyncFFMpegOperation>* operations = 
                    std::any_cast<std::vector<AsyncFFMpegOperation>*>(args[1]);
                std::string path = std::any_cast<std::string>(args[2]);
                std::string ffprobeData = filterFFProbe(read_file(".ffprobe_data"));
                std::string ffprobeJsonData = read_file(".ffprobe_json");
                tu->ffprobeData = ffprobeData;
                tu->ffprobeJsonData = ffprobeJsonData;
                tu->as = VideoMetadata(json_t::parse(ffprobeJsonData));
                VideoMetadata video = std::get<VideoMetadata>(tu->as);
                std::string idPath = string_format("%s/%i", Shared::project.path.c_str(), tu->id);
                std::string wavPath = string_format("%s/audio.wav", idPath.c_str());
                if (!file_exists(idPath)) {
                    std::filesystem::create_directories(idPath);
                }
                operations->push_back(AsyncFFMpegOperation(
                    [](OperationArgs_T& args) {
                        TextureUnion* tu = std::any_cast<TextureUnion*>(args[0]);
                        std::vector<AsyncFFMpegOperation>* operations = 
                            std::any_cast<std::vector<AsyncFFMpegOperation>*>(args[1]);
                        std::string idPath = std::any_cast<std::string>(args[2]);
                        std::string ffprobeKeyframes = read_file(".ffprobe_keyframes");
                        std::vector<std::string> keyframeLines = split_string(ffprobeKeyframes, "\n");
                        VideoMetadata videoMetadata = std::get<VideoMetadata>(tu->as);
                        std::vector<int> keyframesList = {};
                        for (auto& keyframeStr : keyframeLines) {
                            if (keyframeStr.empty()) continue;
                            keyframesList.push_back(int(std::stof(keyframeStr) * videoMetadata.framerate));
                        }
                        tu->videoKeyframes = keyframesList;
                        VideoMetadata video = std::get<VideoMetadata>(tu->as);
                        operations->push_back(AsyncFFMpegOperation(
                            [](OperationArgs_T& args) {
                                TextureUnion* tu = std::any_cast<TextureUnion*>(args[0]);
                                std::string idPath = std::any_cast<std::string>(args[1]);
                                PixelBuffer preview = PixelBuffer(idPath + "/1.jpg");
                                tu->pboGpuTexture = preview.BuildGPUTexture();
                            }, {tu, idPath}, 
                            string_format("ffmpeg -i '%s' -vf \"select=eq(pict_type\\,I)*gt(n\\,%i)\" -q:v 3 %s/1.jpg", tu->path.c_str(), int(std::ceil(video.duration * video.framerate)) / 2, idPath.c_str()),
                            string_format("%s %s '%s'", ICON_FA_VIDEO, ELECTRON_GET_LOCALIZATION("GENERATING_VIDEO_PREVIEW"), tu->path.c_str())
                        ));
                    }, {tu, operations, idPath}, 
                    string_format("(ffprobe -loglevel error -select_streams v:0 -show_entries packet=pts_time,flags -of csv=print_section=0 '%s' | awk -F',' '/K/ {print $1}') &> .ffprobe_keyframes", tu->path.c_str()),
                    string_format("%s %s '%s'", ICON_FA_INFO, ELECTRON_GET_LOCALIZATION("GATHERING_KEYFRAMES_LIST"), tu->path.c_str())));
                
                operations->push_back(AsyncFFMpegOperation(
                    [](OperationArgs_T& args) {
                        TextureUnion* tu = std::any_cast<TextureUnion*>(args[0]);
                        int videoAssetID = tu->id;
                        std::string wavPath = std::any_cast<std::string>(args[1]);
                        auto linkedAudioAssetInstance = AssetCore::ImportAsset(wavPath);
                        tu = AssetCore::GetAsset(intToHex(videoAssetID));
                        TextureUnion* linkedAudioAsset = AssetCore::GetAsset(intToHex(linkedAudioAssetInstance.result.id));
                        tu->linkedAudioAsset = linkedAudioAssetInstance.result.id;
                        linkedAudioAsset->visible = false;
                        tu->ready = true;
                    }, {tu, wavPath},
                    string_format("ffmpeg -y -i '%s' '%s'", tu->path.c_str(), wavPath.c_str()),
                    string_format("%s %s '%s'", ICON_FA_INFO, ELECTRON_GET_LOCALIZATION("CACHING_AUDIO"), tu->path.c_str())
                ));
                tu->linkedCache.push_back(idPath);
                tu->linkedCache.push_back(wavPath);
            }, {tu, &operations, path}, string_format("(ffprobe -v quiet -print_format json -show_format -show_streams '%s' &> .ffprobe_json) ; (ffprobe '%s' &> .ffprobe_data)", tu->path.c_str(), tu->path.c_str()),
                string_format("%s %s '%s'", ICON_FA_INFO, ELECTRON_GET_LOCALIZATION("FFPROBE_GATHERING"), tu->path.c_str())
        ));
    }
    return loadInfo;
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
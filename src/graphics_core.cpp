#include "graphics_core.h"
#include "electron_image.h"

int Electron::PixelBuffer::filtering = GL_LINEAR;

static std::unordered_map<std::string, dylib> dylibRegistry{};

static GLuint basic_compute = 0;
static GLuint compositor_compute = 0;

Electron::PixelBuffer::PixelBuffer(int width, int height) {
    this->pixels = std::vector<Pixel>(width * height);
    this->width = width;
    this->height = height;
}

Electron::PixelBuffer::PixelBuffer(std::vector<Pixel> pixels, int width, int height) {
    this->pixels = pixels;
    this->width = width;
    this->height = height;
}

void Electron::PixelBuffer::SetPixel(int x, int y, Pixel pixel) {
    if (x >= this->width || y >= this->height) return; // discard out of bounds pixels
    if (0 > this->width || 0 > this->height) return;
    this->pixels[x + y * this->width] = pixel;
}

Electron::Pixel Electron::PixelBuffer::GetPixel(int x, int y) {
    return this->pixels[x + y * this->width];
}

GLuint Electron::PixelBuffer::BuildGPUTexture() {
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    std::vector<float> textureConversion(this->width * this->height * 4);
    for (int x = 0; x < this->width; x++) {
        for (int y = 0; y < this->height; y++) {
            int index = (x + y * width) * 4;
            Pixel pixel = GetPixel(x, y);
            textureConversion[index + 0] = pixel.r;
            textureConversion[index + 1] = pixel.g;
            textureConversion[index + 2] = pixel.b;
            textureConversion[index + 3] = pixel.a;
        }
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, textureConversion.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtering);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filtering);

    return id;
}

void Electron::PixelBuffer::FillColor(Pixel pixel, RenderRequestMetadata metadata) {
    for (int x = metadata.beginX; x < metadata.endX; x++) {
        for (int y = metadata.beginY; y < metadata.endY; y++) {
            SetPixel(x, y, pixel);
        }
    }
}

void Electron::PixelBuffer::DestroyGPUTexture(GLuint texture) {
    glDeleteTextures(1, &texture);
}

Electron::RenderBuffer::RenderBuffer(GraphicsCore* core, int width, int height) {
    this->colorBuffer = core->GenerateGPUTexture(width, height, 0);
    this->uvBuffer = core->GenerateGPUTexture(width, height, 1);
    this->depthBuffer = core->GenerateGPUTexture(width, height, 2);

    this->width = width;
    this->height = height;
}

Electron::RenderBuffer::~RenderBuffer() {
}

void Electron::TextureUnion::RebuildAssetData(GraphicsCore* owner) {
    if (!file_exists(path)) {
        invalid = true;
        type = TextureUnionType::Texture;
        GenerateUVTexture();
        PixelBuffer::DestroyGPUTexture(pboGpuTexture);
        pboGpuTexture = std::get<PixelBuffer>(as).BuildGPUTexture();
        return;
    } else invalid = false;
    PixelBuffer::DestroyGPUTexture(pboGpuTexture);
    pboGpuTexture = 0;
    *this = assetOwner->LoadAssetFromPath(path).result;
}

glm::vec2 Electron::TextureUnion::GetDimensions() {
    switch (type) {
        case TextureUnionType::Texture: {
            PixelBuffer pbo = std::get<PixelBuffer>(as);
            return {pbo.width, pbo.height};
        }
        default: {
            return {0, 0};
        }
    }
}

void Electron::TextureUnion::GenerateUVTexture() {
    type = TextureUnionType::Texture;
    as = PixelBuffer(256, 256);
    PixelBuffer& buffer = std::get<PixelBuffer>(as);
    for (int x = 0; x < buffer.width; x++) {
        for (int y = 0; y < buffer.height; y++) {
            buffer.SetPixel(x, y, Pixel((float) x / (float) buffer.width, (float) y / (float) buffer.height, 0, 1));
        }
    }
}


void Electron::AssetRegistry::LoadFromProject(json_t project) {
    Clear();

    json_t assetRegistry = project["AssetRegistry"];
    for (int i = 0; i < assetRegistry.size(); i++) {
        json_t assetDescription = assetRegistry.at(i);
        TextureUnionType type = TextureUnionTypeFromString(JSON_AS_TYPE(assetDescription["Type"], std::string));
        std::string resourcePath = JSON_AS_TYPE(assetDescription["Path"], std::string);
        std::string internalName = JSON_AS_TYPE(assetDescription["InternalName"], std::string);
        int id = JSON_AS_TYPE(assetDescription["ID"], int);

        AssetLoadInfo assetUnion = LoadAssetFromPath(resourcePath);
        if (assetUnion.returnMessage != "") {
            print(resourcePath << ": " << assetUnion.returnMessage);
            assetUnion.result.GenerateUVTexture();
        }
        assetUnion.result.name = internalName;
        assetUnion.result.id = id;

        assets.push_back(assetUnion.result);

        print("[" << JSON_AS_TYPE(assetDescription["Type"], std::string) << "] Loaded " << resourcePath);
    }
}

Electron::AssetLoadInfo Electron::AssetRegistry::LoadAssetFromPath(std::string path) {
    std::string retMessage = "";
    bool invalid = false;
    if (!file_exists(path)) {
        invalid = true;
        retMessage = "File does not exist '" + path + "'";
    }
    TextureUnionType targetAssetType = static_cast<TextureUnionType>(0);
    std::string lowerPath = lowercase(path);
    if (hasEnding(lowerPath, ".png") || hasEnding(lowerPath, ".jpg") || hasEnding(lowerPath, ".jpeg") || hasEnding(lowerPath, ".tga")) {
        targetAssetType = TextureUnionType::Texture;
    } else {
        retMessage = "Unsupported file format '" + path + "'";
        invalid = true;
    }

    TextureUnion assetUnion{};
    assetUnion.type = targetAssetType;
    assetUnion.strType = StringFromTextureUnionType(targetAssetType);
    assetUnion.name = "New asset";
    assetUnion.path = path;
    assetUnion.previewScale = 1.0f;
    assetUnion.assetOwner = this;
    assetUnion.invalid = invalid;
    if (!invalid) {
        switch (targetAssetType) {
        case TextureUnionType::Texture: {
            assetUnion.as = owner->CreateBufferFromImage(path.c_str());
            assetUnion.pboGpuTexture = std::get<PixelBuffer>(assetUnion.as).BuildGPUTexture();
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
        assetUnion.pboGpuTexture = std::get<PixelBuffer>(assetUnion.as).BuildGPUTexture();
    }

    AssetLoadInfo result{};
    result.result = assetUnion;
    result.returnMessage = "";
    return result;
}

std::string Electron::AssetRegistry::ImportAsset(std::string path) {
    AssetLoadInfo loadInfo = LoadAssetFromPath(path);
    if (loadInfo.returnMessage != "") return loadInfo.returnMessage;
    loadInfo.result.id = seedrand();
    assets.push_back(loadInfo.result);
    return loadInfo.returnMessage;
}

void Electron::AssetRegistry::Clear() {
    this->assets.clear();
}

Electron::RenderLayer::RenderLayer(std::string layerLibrary) {
    this->layerLibrary = layerLibrary;
    print("Loading dylib " + layerLibrary);

    this->beginFrame = 0;
    this->endFrame = 0;
    this->frameOffset = 0;

    dylib* implementation = nullptr;
    if (dylibRegistry.find(layerLibrary) == dylibRegistry.end()) {
        dylibRegistry[layerLibrary] = dylib("layers", layerLibrary);
    }   
    implementation = &dylibRegistry[layerLibrary];
    this->layerProcedure = implementation->get_function<void(RenderLayer*, RenderRequestMetadata)>("LayerRender");
    this->propertiesProcedure = implementation->get_function<void(RenderLayer*)>("LayerPropertiesRender");
    this->initializationProcedure = implementation->get_function<void(RenderLayer*)>("LayerInitialize");
    this->sortingProcedure = implementation->get_function<void(RenderLayer*)>("LayerSortKeyframes");
    this->layerPublicName = implementation->get_variable<std::string>("LayerName");
    this->layerColor = implementation->get_variable<glm::vec4>("LayerTimelineColor");
    if (!layerProcedure) throw std::runtime_error("bad layer procedure!");

    initializationProcedure(this);
    initialized = true;

}

void Electron::RenderLayer::Render(GraphicsCore* graphics, RenderRequestMetadata metadata) {
    this->graphicsOwner = graphics;
    if (std::clamp((int) graphics->renderFrame, beginFrame, endFrame) == (int) graphics->renderFrame)
        layerProcedure(this, metadata);
}

void Electron::RenderLayer::SortKeyframes(json_t& keyframes) {
    for (int step = 0; step < keyframes.size() - 1; ++step) {
        for (int i = 1; i < keyframes.size() - step - 1; ++i) {
            if (keyframes.at(i).at(0) > keyframes.at(i + 1).at(0)) {
                json_t temp = keyframes.at(i);
                keyframes.at(i) = keyframes.at(i + 1);
                keyframes.at(i + 1) = temp;
            }
        }
    }
}

void Electron::RenderLayer::RenderProperties() {
    propertiesProcedure(this);
}

void Electron::RenderLayer::RenderProperty(GeneralizedPropertyType type, json_t& property, std::string propertyName) {
#ifndef ELECTRON_IMPLEMENTATION_MODE
    ImVec2 windowSize = ImGui::GetWindowSize();
    float inputFieldDivider = 8;
    if (ImGui::CollapsingHeader(propertyName.c_str())) {
        if (ImGui::Button((std::string("Add keyframe") + "##" + propertyName).c_str())) {
            float currentViewTime = graphicsOwner->renderFrame - beginFrame;
            json_t addedKeyframe = {currentViewTime};
            int typeSize = 1;
            switch (type) {
                case GeneralizedPropertyType::Vec2: {
                    typeSize = 2;
                    break;
                }
                case GeneralizedPropertyType::Color3:
                case GeneralizedPropertyType::Vec3: {
                    typeSize = 3;
                    break;
                }
                case GeneralizedPropertyType::Float: {
                    typeSize = 1;
                    break;
                }
            }
            for (int i = 0; i < typeSize; i++) {
                addedKeyframe.push_back(0.0f);
            }
            property.push_back(addedKeyframe);
        }
        ImGui::Text("Keyframes:");
        for (int i = 1; i < property.size(); i++) {
            bool breakLoop = false;
            switch (type) {
                case GeneralizedPropertyType::Color3:
                case GeneralizedPropertyType::Vec2:
                case GeneralizedPropertyType::Vec3: {
                    float fKey = JSON_AS_TYPE(property.at(i).at(0), float);
                    std::vector<float> vectorProperty = {};
                    vectorProperty.push_back(JSON_AS_TYPE(property.at(i).at(1), float));
                    vectorProperty.push_back(JSON_AS_TYPE(property.at(i).at(2), float));
                    if (type == GeneralizedPropertyType::Vec3 || type == GeneralizedPropertyType::Color3) {
                        vectorProperty.push_back(JSON_AS_TYPE(property.at(i).at(3), float));
                    }
                    float* fProperty = vectorProperty.data();
                    ImGui::PushItemWidth(30);
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{1, 0, 0, 1});
                    
                            if (ImGui::Button(("Delete##" + propertyName + std::to_string(i) + "KeyframeDelete").c_str())) {
                                property.erase(property.begin() + i);
                                breakLoop = true;
                                ImGui::PopStyleColor();
                                ImGui::PopItemWidth();
                                break;
                            }
                    
                        ImGui::PopStyleColor();
                    ImGui::PopItemWidth();
                    ImGui::SameLine();


                    ImGui::PushItemWidth(windowSize.x / inputFieldDivider);
                        ImGui::InputFloat(("##" + propertyName + std::to_string(i) + "0").c_str(), &fKey, 0.0f, 0.0f, "%0.0f", ImGuiInputTextFlags_EnterReturnsTrue);
                    ImGui::PopItemWidth();
                    ImGui::SameLine();
                    if (type == GeneralizedPropertyType::Vec2)
                        ImGui::InputFloat2((std::string("Keyframe ") + std::to_string(i - 1) + "##" + propertyName + std::to_string(i)).c_str(), fProperty, "%0.3f", 0);
                    else if (type == GeneralizedPropertyType::Color3)
                        ImGui::ColorEdit3((std::string("Keyframe ") + std::to_string(i - 1) + "##" + propertyName + std::to_string(i)).c_str(), fProperty, 0);
                    else
                        ImGui::InputFloat3((std::string("Keyframe ") + std::to_string(i - 1) + "##" + propertyName + std::to_string(i)).c_str(), fProperty, "%0.3f", 0);


                    property.at(i).at(0) = fKey;
                    property.at(i).at(1) = fProperty[0];
                    property.at(i).at(2) = fProperty[1];
                    if (GeneralizedPropertyType::Vec3 == type || GeneralizedPropertyType::Color3 == type) {
                        property.at(i).at(3) = fProperty[2];
                    }
                    break;
                }
                case GeneralizedPropertyType::Float: {
                    float fKey = JSON_AS_TYPE(property.at(i).at(0), float);
                    float fValue = JSON_AS_TYPE(property.at(i).at(1), float);

                    ImGui::PushItemWidth(30);
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{1, 0, 0, 1});
                    
                            if (ImGui::Button(("Delete##" + propertyName + std::to_string(i) + "KeyframeDelete").c_str())) {
                                property.erase(property.begin() + i);
                                breakLoop = true;
                                ImGui::PopStyleColor();
                                ImGui::PopItemWidth();
                                break;
                            }
                    
                        ImGui::PopStyleColor();
                    ImGui::PopItemWidth();
                    ImGui::SameLine();
                    ImGui::SameLine();

                    ImGui::PushItemWidth(windowSize.x / inputFieldDivider);
                        ImGui::InputFloat(("##" + propertyName + std::to_string(i) + "0").c_str(), &fKey, 0.0f, 0.0f, "%0.0f", ImGuiInputTextFlags_EnterReturnsTrue);
                    ImGui::PopItemWidth();
                    ImGui::SameLine();
                    ImGui::InputFloat((std::string("Keyframe ") + std::to_string(i - 1) + "##" + propertyName + std::to_string(i)).c_str(), &fValue, 0, 0, "%0.2f", 0);

                    property.at(i).at(0) = fKey;
                    property.at(i).at(1) = fValue;
                    break;
                }
            }
            if (breakLoop) break;
        }
    }
#endif
}



Electron::json_t Electron::RenderLayer::InterpolateProperty(json_t keyframes) {
    int targetKeyframeIndex = -1;
    int keyframesLength = keyframes.size();
    int renderViewTime = graphicsOwner->renderFrame - beginFrame;
    GeneralizedPropertyType propertyType = keyframes.at(0);
    
    for (int i = 1; i < keyframesLength; i++) {
        int keyframeTimestamp = JSON_AS_TYPE(keyframes.at(i).at(0), int);
        if (renderViewTime <= keyframeTimestamp) {
            targetKeyframeIndex = i;
            break;
        }
    }

    // Make sure to keep last keyframe's value
    if (targetKeyframeIndex == -1) {
        return ExtractExactValue(keyframes.at(keyframesLength - 1));
    }

    // Set initial transformation position
    if (targetKeyframeIndex == 1) {
        return ExtractExactValue(keyframes.at(1));
    } 

    json_t beginKeyframeValue = ExtractExactValue(keyframes.at(targetKeyframeIndex - 1));
    json_t endKeyframeValue = ExtractExactValue(keyframes.at(targetKeyframeIndex));
    json_t interpolatedValue = {};

    float keyframeFrame = keyframes.at(targetKeyframeIndex).at(0);
    float interpolationPercentage = 0;
    if (targetKeyframeIndex == 1) {
        interpolationPercentage = renderViewTime / keyframeFrame;
    } else {
        float previousFrame = keyframes.at(targetKeyframeIndex - 1).at(0);
        interpolationPercentage = (renderViewTime - previousFrame) / (keyframeFrame - previousFrame);
    }

    switch (propertyType) {
        case GeneralizedPropertyType::Float: {
            interpolatedValue.push_back(glm::mix(beginKeyframeValue.at(0), endKeyframeValue.at(0), interpolationPercentage));
            break;
        }
        // Vector interpolation
        case GeneralizedPropertyType::Vec2:
        case GeneralizedPropertyType::Vec3:
        case GeneralizedPropertyType::Color3: {
            if (beginKeyframeValue.size() != endKeyframeValue.size()) {
                throw std::runtime_error("malformed interpolation targets (vector size mismatch)");
            }
            for (int i = 0; i < beginKeyframeValue.size(); i++) {
                float beginFloat = beginKeyframeValue.at(i);
                float endFloat = endKeyframeValue.at(i);
                interpolatedValue.push_back(glm::mix(beginFloat, endFloat, interpolationPercentage));
            }
            break;
        }
    }

    return interpolatedValue;
}

Electron::json_t Electron::RenderLayer::ExtractExactValue(json_t property) {
    std::vector<json_t> acc{};
    for (int i = 1; i < property.size(); i++) {
        acc.push_back(property.at(i));
    }
    return acc;
}

Electron::GraphicsCore::GraphicsCore() {
    this->previousRenderBufferTexture = -1;
    this->renderBufferTexture = -1;

    this->renderFrame = 0;
    this->renderFramerate = 60; // default render framerate
    this->renderLength = 0;

    this->outputBufferType = PreviewOutputBufferType_Color;
}

void Electron::GraphicsCore::ResizeRenderBuffer(int width, int height) {
    this->renderBuffer = RenderBuffer(this, width, height);
}

Electron::ResizableGPUTexture::ResizableGPUTexture(GraphicsCore* core, int width, int height) {
    this->core = core;
    this->width = width;
    this->height = height;
    this->texture = core->GenerateGPUTexture(width, height, 0);
}

void Electron::ResizableGPUTexture::CheckForResize(RenderBuffer* pbo) {
    if (width != pbo->width || height != pbo->height) {
        this->width = pbo->width;
        this->height = pbo->height;
        PixelBuffer::DestroyGPUTexture(texture);
        this->texture = core->GenerateGPUTexture(width, height, 0);
    }
}


void Electron::GraphicsCore::RequestRenderBufferCleaningWithinRegion(RenderRequestMetadata metadata) {
    /* this->renderBuffer.color.FillColor(Pixel(metadata.backgroundColor[0], metadata.backgroundColor[1], metadata.backgroundColor[2], 1), metadata);
    this->renderBuffer.depth.FillColor(Pixel(MAX_DEPTH, 0, 0, 1), metadata);
    this->renderBuffer.uv.FillColor(Pixel(0, 0, 0, 1), metadata);
    */

   RequestTextureCollectionCleaning(renderBuffer.colorBuffer, renderBuffer.uvBuffer, renderBuffer.depthBuffer, renderBuffer.width, renderBuffer.height, metadata);
}

void Electron::GraphicsCore::RequestTextureCollectionCleaning(GLuint color, GLuint uv, GLuint depth, int width, int height, RenderRequestMetadata metadata) {
    if (basic_compute == 0) {
        basic_compute = CompileComputeShader("basic.compute");
    }

    UseShader(basic_compute);
    BindGPUTexture(color, 0);
    BindGPUTexture(uv, 1);
    BindGPUTexture(depth, 2);

    ShaderSetUniform(basic_compute, "backgroundColor", glm::vec3(metadata.backgroundColor[0], metadata.backgroundColor[1], metadata.backgroundColor[2]));
    ShaderSetUniform(basic_compute, "maxDepth", (float) MAX_DEPTH);
    DispatchComputeShader(std::ceil(renderBuffer.width / 8), std::ceil(renderBuffer.height / 4), 1);
    ComputeMemoryBarier(GL_ALL_BARRIER_BITS);
}

std::vector<float> Electron::GraphicsCore::RequestRenderWithinRegion(RenderRequestMetadata metadata) {
    std::vector<float> renderTimes(layers.size());
    int layerIndex = 0;
    for (auto& layer : layers) {
        float first = glfwGetTime();
        layer.Render(this, metadata);
        renderTimes[layerIndex] = (glfwGetTime() - first);
        layerIndex++;
    }
    return renderTimes;
}

void Electron::GraphicsCore::CleanPreviewGPUTexture() {
}

void Electron::GraphicsCore::BuildPreviewGPUTexture() {
    renderBufferTexture = GetPreviewBufferByOutputType();
}

GLuint Electron::GraphicsCore::GetPreviewBufferByOutputType() {
    switch (outputBufferType) {
        case PreviewOutputBufferType_Color: return renderBuffer.colorBuffer;
        case PreviewOutputBufferType_Depth: return renderBuffer.depthBuffer;
        case PreviewOutputBufferType_UV: return renderBuffer.uvBuffer;
    }
    return renderBuffer.uvBuffer;
}

GLuint Electron::GraphicsCore::CompileComputeShader(std::string path) {
    std::string computeShaderCode = read_file("compute/" + path);
    const char* c_code = computeShaderCode.c_str();

    GLuint computeShader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(computeShader, 1, &c_code, NULL);
    glCompileShader(computeShader);

    GLint compiled = 0;
    glGetShaderiv(computeShader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE) {
        GLint maxLength = 0;
        glGetShaderiv(computeShader, GL_INFO_LOG_LENGTH, &maxLength);

        std::vector<GLchar> errorLog(maxLength);
        glGetShaderInfoLog(computeShader, maxLength, &maxLength, &errorLog[0]);
        print(errorLog.data());

        glDeleteShader(computeShader);

        throw std::runtime_error("error while compiling compute shader!");
        return -1;
    }
    
    GLuint computeProgram = glCreateProgram();
    glAttachShader(computeProgram, computeShader);
    glLinkProgram(computeProgram);

    GLint isLinked = 0;
    glGetProgramiv(computeProgram, GL_LINK_STATUS, &isLinked);
    if (isLinked == GL_FALSE) {
	    GLint maxLength = 0;
	    glGetProgramiv(computeProgram, GL_INFO_LOG_LENGTH, &maxLength);

	    std::vector<GLchar> infoLog(maxLength);
	    glGetProgramInfoLog(computeProgram, maxLength, &maxLength, &infoLog[0]);

	    glDeleteProgram(computeProgram);

        throw std::runtime_error("error while linking compute shader!");

	    return -1;
    }

    glDeleteShader(computeShader);

    return computeProgram;
}

void Electron::GraphicsCore::UseShader(GLuint shader) {
    glUseProgram(shader);
}

void Electron::GraphicsCore::DispatchComputeShader(int grid_x, int grid_y, int grid_z) {
    glDispatchCompute(grid_x, grid_y, grid_z);
}

void Electron::GraphicsCore::ComputeMemoryBarier(GLbitfield barrier) {
    glMemoryBarrier(barrier);
}

GLuint Electron::GraphicsCore::GenerateGPUTexture(int width, int height, int unit) {
    unsigned int texture;

	glGenTextures(1, &texture);
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);

    return texture;
}

void Electron::GraphicsCore::BindGPUTexture(GLuint texture, int unit) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBindImageTexture(unit, texture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
}

void Electron::GraphicsCore::ShaderSetUniform(GLuint program, std::string name, int x, int y) {
    glUniform2i(glGetUniformLocation(program, name.c_str()), x, y);
}

void Electron::GraphicsCore::ShaderSetUniform(GLuint program, std::string name, glm::vec3 vec) {
    glUniform3f(glGetUniformLocation(program, name.c_str()), vec.x, vec.y, vec.z);
}

void Electron::GraphicsCore::ShaderSetUniform(GLuint program, std::string name, float f) {
    glUniform1f(glGetUniformLocation(program, name.c_str()), f);
}

void Electron::GraphicsCore::ShaderSetUniform(GLuint program, std::string name, glm::vec2 vec) {
    glUniform2f(glGetUniformLocation(program, name.c_str()), vec.x, vec.y);
}

void Electron::GraphicsCore::ShaderSetUniform(GLuint program, std::string name, int x) {
    glUniform1i(glGetUniformLocation(program, name.c_str()), x);
}

Electron::PixelBuffer Electron::GraphicsCore::PBOFromGPUTexture(GLuint texture, int width, int height) {
    glFlush();
    PixelBuffer pbo(width, height);

    std::vector<float> pixels(width * height * 4);
    glBindTexture(GL_TEXTURE_2D, texture);
    glGetnTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, width * height * 4 * sizeof(float), pixels.data());

    int pixelIndex = 0;
    for (int i = 0; i < width * height * 4; i += 4) {
        pbo.pixels[pixelIndex].r = pixels[i + 0];
        pbo.pixels[pixelIndex].g = pixels[i + 1];
        pbo.pixels[pixelIndex].b = pixels[i + 2];
        pbo.pixels[pixelIndex].a = pixels[i + 3];

        pixelIndex++;
    }

    return pbo;
}

void Electron::GraphicsCore::CallCompositor(ResizableGPUTexture color, ResizableGPUTexture uv, ResizableGPUTexture depth) {

    if (compositor_compute == 0) {
        compositor_compute = CompileComputeShader("compositor.compute");
    }

    UseShader(compositor_compute);
    BindGPUTexture(renderBuffer.colorBuffer, 0);
    BindGPUTexture(renderBuffer.uvBuffer, 1);
    BindGPUTexture(renderBuffer.depthBuffer, 2);

    BindGPUTexture(color.texture, 3);
    BindGPUTexture(uv.texture, 4);
    BindGPUTexture(depth.texture, 5);

    DispatchComputeShader(std::ceil(renderBuffer.width / 8), std::ceil(renderBuffer.height / 4), 1);
    ComputeMemoryBarier(GL_ALL_BARRIER_BITS);
}

Electron::PixelBuffer Electron::GraphicsCore::CreateBufferFromImage(const char* filename) {
    int width, height, channels;
    if (!file_exists(filename)) {
        throw std::runtime_error("image " + std::string(filename) + " is not found");
    }
    stbi_uc* image = stbi_load(filename, &width, &height, &channels, 0);

    std::vector<stbi_uc> vector_image = std::vector<stbi_uc>(image, image + width * height * channels);

    std::vector<Pixel> pixelBuffer{};
    for (int i = 0; i < vector_image.size(); i += channels) {
        pixelBuffer.push_back({
            vector_image[i + 0] / 255.0f,
            vector_image[i + 1] / 255.0f,
            vector_image[i + 2] / 255.0f,
            channels == 4 ? vector_image[i + 3] / 255.0f : 1.0f
        });
    }

    stbi_image_free(image);

    return Electron::PixelBuffer(pixelBuffer, width, height);
}
#include "editor_core.h"
#include "app.h"
#define CSTR(x) ((x).c_str())

using namespace Electron;
using namespace glm;

#define SDF_2D_ACCEPT_THRESHOLD 0.0

extern "C" {

    ELECTRON_EXPORT std::string LayerName = "SDF2D";
    ELECTRON_EXPORT glm::vec4 LayerTimelineColor = {0.58, 0.576, 1, 1};
    ELECTRON_EXPORT json_t LayerPreviewProperties = {
        "Position", "Size", "Color", "Angle"
    };
    GLuint sdf2d_compute = UNDEFINED;
    GLuint sdf2d_vao = UNDEFINED;

    mat2 rotationMatrix(float angle) {
	    angle *= 3.1415f / 180.0f;
        float s=glm::sin(angle), c=glm::cos(angle);
        return mat2( c, -s, s, c );
    }

    vec2 rotate(vec2 uv, float th) {
        return mat2(cos(th), sin(th), -sin(th), cos(th)) * uv;
    }

    char const* gl_error_string(GLenum const err) noexcept
{
  switch (err)
  {
    // opengl 2 errors (8)
    case GL_NO_ERROR:
      return "GL_NO_ERROR";

    case GL_INVALID_ENUM:
      return "GL_INVALID_ENUM";

    case GL_INVALID_VALUE:
      return "GL_INVALID_VALUE";

    case GL_INVALID_OPERATION:
      return "GL_INVALID_OPERATION";

    case GL_OUT_OF_MEMORY:
      return "GL_OUT_OF_MEMORY";

    // opengl 3 errors (1)
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      return "GL_INVALID_FRAMEBUFFER_OPERATION";

    // gles 2, 3 and gl 4 error are handled by the switch above
    default:
      assert(!"unknown error");
      return nullptr;
  }
}

    ELECTRON_EXPORT void LayerInitialize(RenderLayer* owner) {
        owner->properties["Position"] = {
            GeneralizedPropertyType::Vec2,
            {0, 0, 0}
        };
        owner->properties["Size"] = {
            GeneralizedPropertyType::Vec2,
            {0, 0.5f, 0.5f},
        };
        owner->properties["Color"] = {
            GeneralizedPropertyType::Color3,
            {0, 1, 1, 1}
        };

        owner->properties["Angle"] = {
            GeneralizedPropertyType::Float,
            {0, 0}
        };

        owner->properties["TextureID"] = "";
        owner->properties["EnableTexturing"] = true;

        owner->anyData.resize(1);
        owner->anyData[0] = PipelineFrameBuffer(Shared::graphics->renderBuffer.width, Shared::graphics->renderBuffer.height);

        if (sdf2d_compute == UNDEFINED) {
            sdf2d_compute = GraphicsCore::CompilePipelineShader("sdf2d.pipeline");
        }

        if (sdf2d_vao == UNDEFINED) {
            sdf2d_vao = GraphicsCore::GenerateVAO(fsQuadVertices, fsQuadUV);
        }
    }

    ELECTRON_EXPORT void LayerRender(RenderLayer* owner) {
        PipelineFrameBuffer* pbo = &Shared::graphics->renderBuffer;
        GraphicsCore* core = Shared::graphics;


        PipelineFrameBuffer frb = std::any_cast<PipelineFrameBuffer>(owner->anyData[0]);
        if (frb.width != pbo->width || frb.height != pbo->height) {
            frb.Destroy();
            frb = PipelineFrameBuffer(pbo->width, pbo->height);
        }
        
        owner->anyData[0] = frb;
        frb = std::any_cast<PipelineFrameBuffer>(owner->anyData[0]); // update sdf2d_fbo (fixes some small issues)
        Shared::graphics->RequestTextureCollectionCleaning(frb, 0.0f);

        auto position = vec2();
        auto size = vec2();
        auto color = vec3();
        auto angle = 0.0f; {
            auto positionVector = JSON_AS_TYPE(owner->InterpolateProperty(owner->properties["Position"]), std::vector<float>);
            auto sizeVector = JSON_AS_TYPE(owner->InterpolateProperty(owner->properties["Size"]), std::vector<float>);
            auto colorVector = JSON_AS_TYPE(owner->InterpolateProperty(owner->properties["Color"]), std::vector<float>);
            auto angleFloat = JSON_AS_TYPE(owner->InterpolateProperty(owner->properties["Angle"]), std::vector<float>);
            position = vec2(positionVector[0], positionVector[1]);
            size = vec2(sizeVector[0], sizeVector[1]); 
            color = vec3(colorVector[0], colorVector[1], colorVector[2]);
            angle = angleFloat[0];
        }

        bool texturingEnabled = JSON_AS_TYPE(owner->properties["EnableTexturing"], bool);
        std::string textureID = JSON_AS_TYPE(owner->properties["TextureID"], std::string);
        TextureUnion* asset = nullptr;
        auto& assets = Shared::assets->assets;
        for (int i = 0; i < assets.size(); i++) {
            if (intToHex(assets.at(i).id) == textureID) {
                if (assets.at(i).IsTextureCompatible())
                    asset = &assets.at(i);
            }
        }

        bool canTexture = (asset != nullptr && texturingEnabled);

        frb.Bind();
        GraphicsCore::UseShader(sdf2d_compute);
        mat4 transform = glm::identity<mat3>();
        transform = glm::scale(transform, vec3(size, 1.0f));
        transform = glm::rotate(transform, glm::radians(angle), vec3(0, 0, 1));
        transform = glm::translate(transform, vec3(position, 0.0f));
        float aspect = (float) frb.width / (float) frb.height;
        mat4 projection = ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);
        GraphicsCore::ShaderSetUniform(sdf2d_compute, "uColor", color);
        GraphicsCore::ShaderSetUniform(sdf2d_compute, "uTransform", transform);
        GraphicsCore::ShaderSetUniform(sdf2d_compute, "uProjection", projection);
        if (canTexture) {
            GraphicsCore::BindGPUTexture(asset->pboGpuTexture, sdf2d_compute, 0, "uTexture");
        }
        GraphicsCore::ShaderSetUniform(sdf2d_compute, "uCanTexture", (int) canTexture);
        glBindVertexArray(sdf2d_vao);
        glDrawArrays(GL_TRIANGLES, 0, fsQuadVertices.size() / 2);
        frb.Unbind();

        Shared::graphics->CallCompositor(frb);
    }

    ELECTRON_EXPORT void LayerPropertiesRender(RenderLayer* layer) {
        AppInstance* instance = Shared::app;
        PipelineFrameBuffer* pbo = &Shared::graphics->renderBuffer;

        bool texturingEnabled = JSON_AS_TYPE(layer->properties["EnableTexturing"], bool);
        ImGui::Checkbox("Enable texturing", &texturingEnabled);
        layer->properties["EnableTexturing"] = texturingEnabled;
        ImGui::Separator();
        json_t& position = layer->properties["Position"];
        layer->RenderProperty(GeneralizedPropertyType::Vec2, position, "Position");

        json_t& size = layer->properties["Size"];
        layer->RenderProperty(GeneralizedPropertyType::Vec2, size, "Size");

        json_t& color = layer->properties["Color"];
        layer->RenderProperty(GeneralizedPropertyType::Color3, color, "Color");

        json_t& angle = layer->properties["Angle"];
        layer->RenderProperty(GeneralizedPropertyType::Float, angle, "Angle");

        json_t& textureID = layer->properties["TextureID"];
        layer->RenderAssetProperty(textureID, "Texturing", TextureUnionType::Texture);
    }

    ELECTRON_EXPORT void LayerSortKeyframes(RenderLayer* layer) {
        json_t& position = layer->properties["Position"];
        layer->SortKeyframes(position);

        json_t& size = layer->properties["Size"];
        layer->SortKeyframes(size);

        json_t& color = layer->properties["Color"];
        layer->SortKeyframes(color);

        json_t& angle = layer->properties["Angle"];
        layer->SortKeyframes(angle);
    }

    ELECTRON_EXPORT void LayerDestroy(RenderLayer* layer) {
        PipelineFrameBuffer rbo = std::any_cast<PipelineFrameBuffer>(layer->anyData[0]);
        rbo.Destroy();
    }
}
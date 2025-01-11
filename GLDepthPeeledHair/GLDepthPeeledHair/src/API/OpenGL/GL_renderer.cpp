#include "GL_renderer.h"
#include "GL_backend.h"
#include "GL_util.hpp"
#include "Types/GL_detachedMesh.hpp"
#include "Types/GL_frameBuffer.hpp"
#include "Types/GL_pbo.hpp"
#include "Types/GL_shader.h"
#include "../Managers/AssetManager.h"
#include "../Camera.h"
#include "../Scene.hpp"
#include "../Input.h"
#include "../Util.hpp"
#include <glm/gtx/matrix_decompose.hpp>
#include "../Hardcoded.hpp"
#include "../GameObject.h"
#include "../Audio.h"
#include "../FlipbookObject.h"

namespace OpenGLRenderer {

    struct Shaders {
        Shader solidColor;
        Shader textured;
        Shader water;
        Shader hairDepthOnly;
        Shader hairDepthOnlyPeeled;
        Shader flipbook;
        ComputeShader computeTest;
        ComputeShader debugOutput;
        ComputeShader hairLayerComposite;
        ComputeShader generateMipmap;
    } g_shaders;

    struct FrameBuffers {
        GLFrameBuffer main;
        GLFrameBuffer hair;
        GLFrameBuffer waterReflection;
        GLFrameBuffer waterRefraction;
    } g_frameBuffers;

    void DrawScene(Shader& shader, bool renderHair);
    void RenderLighting();
    void RenderWater();
    void RenderDebug();
    void RenderHair();
    void FlipBookPass();
    void FlipBookPassOLD();
    void GenerateMipmaps(OpenGLTexture& glTexture);

    void Init() {
        g_frameBuffers.main.Create("Main", 1920, 1080);
        g_frameBuffers.main.CreateAttachment("Color", GL_RGBA8);
        g_frameBuffers.main.CreateAttachment("Position", GL_RGBA16F);
        g_frameBuffers.main.CreateAttachment("UnderWaterColor", GL_RGBA8);
        g_frameBuffers.main.CreateDepthAttachment(GL_DEPTH32F_STENCIL8);

        float hairDownscaleRatio = 1.0f;
        g_frameBuffers.hair.Create("Hair", g_frameBuffers.main.GetWidth() * hairDownscaleRatio, g_frameBuffers.main.GetHeight() * hairDownscaleRatio);
        g_frameBuffers.hair.CreateDepthAttachment(GL_DEPTH32F_STENCIL8);
        g_frameBuffers.hair.CreateAttachment("Color", GL_RGBA8);
        g_frameBuffers.hair.CreateAttachment("ViewspaceDepth", GL_R32F);
        g_frameBuffers.hair.CreateAttachment("ViewspaceDepthPrevious", GL_R32F);
        g_frameBuffers.hair.CreateAttachment("Composite", GL_RGBA8);

        g_frameBuffers.waterReflection.Create("Water", g_frameBuffers.main.GetWidth(), g_frameBuffers.main.GetHeight());
        g_frameBuffers.waterReflection.CreateAttachment("Color", GL_RGBA8);
        g_frameBuffers.waterReflection.CreateDepthAttachment(GL_DEPTH32F_STENCIL8);

        g_frameBuffers.waterRefraction.Create("Water", g_frameBuffers.main.GetWidth(), g_frameBuffers.main.GetHeight());
        g_frameBuffers.waterRefraction.CreateAttachment("Color", GL_RGBA8);
        g_frameBuffers.waterRefraction.CreateDepthAttachment(GL_DEPTH32F_STENCIL8);

        LoadShaders();

    }

    void GenerateNextMipmap() {        
        static bool allMipmapsGenerated = false;
        if (allMipmapsGenerated) {
            return;
        }
        // Generate next mipmap
        for (int i = 0; i < AssetManager::GetTextureCount(); i++) {
            Texture* texture = AssetManager::GetTextureByIndex(i);
            if (texture->GetBakingState() == BakingState::BAKE_COMPLETE && texture->GetMipmapState() == MipmapState::AWAITING_MIPMAP_GENERATION) {
                texture->SetMipmapState(MipmapState::MIPMAPS_GENERATED);
                GenerateMipmaps(texture->GetGLTexture());
                return;
            }
        }
        // All done?
        allMipmapsGenerated = true;
        for (int i = 0; i < AssetManager::GetTextureCount(); i++) {
            Texture* texture = AssetManager::GetTextureByIndex(i);
            if (texture->GetMipmapState() == MipmapState::AWAITING_MIPMAP_GENERATION) {
                allMipmapsGenerated = false;
                return;
            }
        }
    }

    void RenderFrame() {

        GenerateNextMipmap();

        g_frameBuffers.main.Bind();
        g_frameBuffers.main.SetViewport();
        g_frameBuffers.main.DrawBuffers({ "Color", "Position", "UnderWaterColor" });
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        RenderLighting();
        //FlipBookPass();
        RenderHair();
        RenderWater();
        RenderDebug();

        int width, height;
        glfwGetWindowSize(OpenGLBackend::GetWindowPtr(), &width, &height);

        const float waterHeight = Hardcoded::roomY + Hardcoded::waterHeight;
        GLint finalImageTexture = (Camera::GetViewPos().y > waterHeight)
            ? g_frameBuffers.main.GetColorAttachmentHandleByName("Color")
            : g_frameBuffers.main.GetColorAttachmentHandleByName("UnderWaterColor");

        g_shaders.debugOutput.Use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_frameBuffers.hair.GetColorAttachmentHandleByName("Composite"));
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, finalImageTexture);
        glBindImageTexture(0, g_frameBuffers.main.GetColorAttachmentHandleByName("Color"), 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
        glDispatchCompute((g_frameBuffers.main.GetWidth() + 7) / 8, (g_frameBuffers.main.GetHeight() + 7) / 8, 1);

        g_frameBuffers.main.BlitToDefaultFrameBuffer("Color", 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        glfwSwapBuffers(OpenGLBackend::GetWindowPtr());
        glfwPollEvents();
    }

    void CopyDepthBuffer(GLFrameBuffer& srcFrameBuffer, GLFrameBuffer& dstFrameBuffer) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFrameBuffer.GetHandle());
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFrameBuffer.GetHandle());
        glBlitFramebuffer(0, 0, srcFrameBuffer.GetWidth(), srcFrameBuffer.GetHeight(), 0, 0, dstFrameBuffer.GetWidth(), dstFrameBuffer.GetHeight(), GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    }

    void CopyColorBuffer(GLFrameBuffer& srcFrameBuffer, GLFrameBuffer& dstFrameBuffer, const char* srcAttachmentName, const char* dstAttachmentName) {
        GLenum srcAttachmentSlot = srcFrameBuffer.GetColorAttachmentSlotByName(srcAttachmentName);
        GLenum dstAttachmentSlot = dstFrameBuffer.GetColorAttachmentSlotByName(dstAttachmentName);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFrameBuffer.GetHandle());
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFrameBuffer.GetHandle());
        glReadBuffer(srcAttachmentSlot);
        glDrawBuffer(dstAttachmentSlot);
        glBlitFramebuffer( 0, 0, srcFrameBuffer.GetWidth(), srcFrameBuffer.GetHeight(), 0, 0, dstFrameBuffer.GetWidth(), dstFrameBuffer.GetHeight(), GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    }

    void DrawScene(Shader& shader, bool renderHair) {
        // Non blended
        for (GameObject& gameObject : Scene::g_gameObjects) {
            if (gameObject.m_model) {
                shader.SetMat4("model", gameObject.m_transform.to_mat4());
                for (int i = 0; i < gameObject.m_model->GetMeshCount(); i++) {
                    OpenGLDetachedMesh* mesh = &MeshManager::gMeshes[gameObject.m_model->GetMeshIndices()[i]];
                    BlendingMode& blendingMode = gameObject.m_meshBlendingModes[i];
                    if (blendingMode == BlendingMode::NONE) {
                        Material* material = AssetManager::GetMaterialByIndex(gameObject.m_meshMaterialIndices[i]);
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, AssetManager::GetTextureByIndex(material->m_basecolor)->GetGLTexture().GetHandle());
                        //glBindTexture(GL_TEXTURE_2D, AssetManager::GetTextureByName("blood_pos4")->GetGLTexture().GetHandle());
                        //glBindTexture(GL_TEXTURE_2D, AssetManager::GetTextureByName("blood_norm9")->GetGLTexture().GetHandle());
                        glActiveTexture(GL_TEXTURE1);
                        glBindTexture(GL_TEXTURE_2D, AssetManager::GetTextureByIndex(material->m_normal)->GetGLTexture().GetHandle());
                        glActiveTexture(GL_TEXTURE2);
                        glBindTexture(GL_TEXTURE_2D, AssetManager::GetTextureByIndex(material->m_rma)->GetGLTexture().GetHandle());
                        glActiveTexture(GL_TEXTURE3);
                        glBindTexture(GL_TEXTURE_2D, AssetManager::GetTextureByIndex(material->m_sss)->GetGLTexture().GetHandle());
                        glBindVertexArray(mesh->GetVAO());
                        glDrawElements(GL_TRIANGLES, mesh->GetIndexCount(), GL_UNSIGNED_INT, 0);
                    }
                }
            }
        }    
        // Blended
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_CULL_FACE);
        glDepthMask(GL_FALSE);
        for (GameObject& gameObject : Scene::g_gameObjects) {
            if (gameObject.m_model) {
                shader.SetMat4("model", gameObject.m_transform.to_mat4());
                for (int i = 0; i < gameObject.m_model->GetMeshCount(); i++) {
                    OpenGLDetachedMesh* mesh = &MeshManager::gMeshes[gameObject.m_model->GetMeshIndices()[i]];
                    BlendingMode& blendingMode = gameObject.m_meshBlendingModes[i];
                    if (blendingMode == BlendingMode::BLENDED) {
                        Material* material = AssetManager::GetMaterialByIndex(gameObject.m_meshMaterialIndices[i]);
                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, AssetManager::GetTextureByIndex(material->m_basecolor)->GetGLTexture().GetHandle());
                        glActiveTexture(GL_TEXTURE1);
                        glBindTexture(GL_TEXTURE_2D, AssetManager::GetTextureByIndex(material->m_normal)->GetGLTexture().GetHandle());
                        glActiveTexture(GL_TEXTURE2);
                        glBindTexture(GL_TEXTURE_2D, AssetManager::GetTextureByIndex(material->m_rma)->GetGLTexture().GetHandle());
                        glBindVertexArray(mesh->GetVAO());
                        glDrawElements(GL_TRIANGLES, mesh->GetIndexCount(), GL_UNSIGNED_INT, 0);
                    }
                }
            }
        }
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
    }

    void FlipBookPass() {
        
        GLFrameBuffer frameBuffer = g_frameBuffers.main;
        frameBuffer.Bind();
        frameBuffer.SetViewport();
        frameBuffer.DrawBuffers({ "Color" });

        Shader& shader = g_shaders.flipbook;
        shader.Use();
        shader.SetMat4("projection", Camera::GetProjectionMatrix());
        shader.SetMat4("view", Camera::GetViewMatrix());

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);

        float deltaTime = 1.0f / 6.0f;

        for (FlipbookObject& flipbookObject : Scene::m_flipbookObjects) {

            if (!flipbookObject.IsBillboard()) {
                shader.SetMat4("model", flipbookObject.GetModelMatrix());
            }
            else {
                shader.SetMat4("model", flipbookObject.GetBillboardModelMatrix(Camera::GetForward(), Camera::GetRight(), Camera::GetUp()));
            }
            shader.SetFloat("mixFactor", flipbookObject.GetMixFactor());
            shader.SetInt("index", flipbookObject.GetFrameIndex());
            shader.SetInt("indexNext", flipbookObject.GetNextFrameIndex());
            FlipbookTexture* flipbookTexture = AssetManager::GetFlipbookByName(flipbookObject.GetTextureName());
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D_ARRAY, flipbookTexture->m_arrayHandle);

            uint32_t modelIndex = AssetManager::GetModelIndexByName("Quads");
            Model* model = AssetManager::GetModelByIndex(modelIndex);
            uint32_t meshIndex = model->GetMeshIndices()[1]; // FIX THIS!!!!!!!!!!!!
            OpenGLDetachedMesh* mesh = MeshManager::GetMeshByIndex(meshIndex);
            glBindVertexArray(mesh->GetVAO());
            glDrawElements(GL_TRIANGLES, mesh->GetIndexCount(), GL_UNSIGNED_INT, 0);
        }

        // Cleanup
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);
    }









    void FlipBookPassOLD() {

        static int index = 0;
        static int indexNext = 1;
        static float time = 0;
        static float deltaTime = 1.0f / 60.0f;
        static float animationSpeed = 3.5f;
        static bool play = true;
        static bool loop = true;
        static bool billboard = true;

        if (Input::KeyPressed(HELL_KEY_1)) {
            Audio::PlayAudio("UI_Select.wav", 1.0f);
            play = false;
        }
        if (Input::KeyPressed(HELL_KEY_2)) {
            Audio::PlayAudio("UI_Select.wav", 1.0f);
            time = 0;
            play = true;
            loop = true;
        }
        if (Input::KeyPressed(HELL_KEY_3)) {
            Audio::PlayAudio("UI_Select.wav", 1.0f);
            time = 0;
            play = true;
            loop = false;
        }
        if (Input::KeyPressed(HELL_KEY_4)) {
            Audio::PlayAudio("UI_Select.wav", 1.0f);
            billboard = !billboard;
        }
        if (!play) {
            return;
        }

        Transform transform;
        transform.position = glm::vec3(3.5, 0.0f, 6.5f);
        transform.rotation.y = 3.14159265359f;


        DrawPoint(transform.position, GREEN);


        static bool applyScale = false;

        if (Input::KeyPressed(HELL_KEY_SPACE)) {
            applyScale = !applyScale;
        }

        Transform scaleTransform;
        if (applyScale) {
            scaleTransform.scale = glm::vec3(0.5f);
        }

        glm::mat4 billboardMatrix = glm::mat4(1.0f);
        billboardMatrix[0] = glm::vec4(Camera::GetRight(), 0.0f);
        billboardMatrix[1] = glm::vec4(Camera::GetUp(), 0.0f);
        billboardMatrix[2] = glm::vec4(Camera::GetForward(), 0.0f);
        billboardMatrix[3] = glm::vec4(transform.position, 1.0f);

        glm::mat4 modelMatrix = billboard ? billboardMatrix : transform.to_mat4();
        modelMatrix = modelMatrix * scaleTransform.to_mat4();

        FlipbookTexture* flipbookTexture = AssetManager::GetFlipbookTextureByIndex(0);

        int frameCount = flipbookTexture->m_frameCount;
        float frameDuration = 1.0f / animationSpeed;
        float totalAnimationTime = frameCount * frameDuration; 
        if (!loop) {
            time = std::min(time + deltaTime, totalAnimationTime);
        }
        else {
            time += deltaTime;
            if (time >= totalAnimationTime) {
                time = fmod(time, totalAnimationTime);
            }
        }
        float frameTime = time / frameDuration;
        float mixFactor = fmod(frameTime, 1.0f);
        if (!loop && time >= totalAnimationTime) {
            index = frameCount - 1;
            indexNext = index;
        }
        else {
            index = static_cast<int>(floor(frameTime)) % frameCount;
            indexNext = loop ? (index + 1) % frameCount : std::min(index + 1, frameCount - 1);
        }
        bool animationComplete = !loop && time >= totalAnimationTime;
        
        GLFrameBuffer frameBuffer = g_frameBuffers.main;
        frameBuffer.Bind();
        frameBuffer.SetViewport();
        frameBuffer.DrawBuffers({ "Color" });

        Shader& shader = g_shaders.flipbook;
        shader.Use();
        shader.SetMat4("projection", Camera::GetProjectionMatrix());
        shader.SetMat4("view", Camera::GetViewMatrix());
        shader.SetMat4("model", modelMatrix);
        shader.SetFloat("mixFactor", mixFactor);
        shader.SetInt("index", index);
        shader.SetInt("indexNext", indexNext);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, flipbookTexture->m_arrayHandle);

        uint32_t modelIndex = AssetManager::GetModelIndexByName("Quads");
        Model* model = AssetManager::GetModelByIndex(modelIndex);
        uint32_t meshIndex = model->GetMeshIndices()[1];
        OpenGLDetachedMesh* mesh = MeshManager::GetMeshByIndex(meshIndex);
        glBindVertexArray(mesh->GetVAO());
        glDrawElements(GL_TRIANGLES, mesh->GetIndexCount(), GL_UNSIGNED_INT, 0);

        // Cleanup
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glEnable(GL_CULL_FACE);
    }

    void RenderLighting() {
        const float waterHeight = Hardcoded::roomY + Hardcoded::waterHeight;
        static float time = 0;
        time += 1.0f / 60.0f;

        g_frameBuffers.main.Bind();
        g_frameBuffers.main.SetViewport();
        g_frameBuffers.main.DrawBuffers({ "Color", "Position" });

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        g_shaders.textured.Use();
        g_shaders.textured.SetMat4("projection", Camera::GetProjectionMatrix());
        g_shaders.textured.SetMat4("view", Camera::GetViewMatrix());
        g_shaders.textured.SetMat4("model", glm::mat4(1));
        g_shaders.textured.SetVec3("viewPos", Camera::GetViewPos());
        g_shaders.textured.SetFloat("time", time);
        g_shaders.textured.SetFloat("viewportWidth", g_frameBuffers.hair.GetWidth());
        g_shaders.textured.SetFloat("viewportHeight", g_frameBuffers.hair.GetHeight());
        DrawScene(g_shaders.textured, false);

        g_frameBuffers.waterReflection.Bind();
        g_frameBuffers.waterReflection.SetViewport();
        g_frameBuffers.waterReflection.DrawBuffers({ "Color" });

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Transform cameraTransform;
        cameraTransform.position = Camera::GetViewPos();
        cameraTransform.rotation = Camera::GetViewRotation();
        cameraTransform.scale = glm::vec3(1);

        float reflectionDistance = 2 * (Camera::GetViewPos().y - waterHeight);
        Transform reflectionCameraTransform = cameraTransform;// Camera::GetTransform();
        reflectionCameraTransform.position.y -= reflectionDistance;
        reflectionCameraTransform.rotation.x *= -1;
        glm::mat4 reflectionViewMatrix = glm::inverse(reflectionCameraTransform.to_mat4());

        glEnable(GL_CLIP_DISTANCE0);

        g_shaders.textured.Use();
        g_shaders.textured.SetMat4("projection", Camera::GetProjectionMatrix());
        g_shaders.textured.SetMat4("view", reflectionViewMatrix);
        g_shaders.textured.SetMat4("model", glm::mat4(1));
        g_shaders.textured.SetVec3("viewPos", Camera::GetViewPos());
        g_shaders.textured.SetVec4("clippingPlane", glm::vec4(0, 1, 0, -waterHeight));
        DrawScene(g_shaders.textured, true);

        g_frameBuffers.waterRefraction.Bind();
        g_frameBuffers.waterRefraction.SetViewport();
        g_frameBuffers.waterRefraction.DrawBuffers({ "Color" });
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        g_shaders.textured.Use();
        g_shaders.textured.SetMat4("projection", Camera::GetProjectionMatrix());
        g_shaders.textured.SetMat4("view", Camera::GetViewMatrix());
        g_shaders.textured.SetMat4("model", glm::mat4(1));
        g_shaders.textured.SetVec3("viewPos", Camera::GetViewPos());
        g_shaders.textured.SetVec4("clippingPlane", glm::vec4(0, -1, 0, waterHeight));
        DrawScene(g_shaders.textured, true);
        glDisable(GL_CLIP_DISTANCE0);
    }


    struct HairRenderItem {
        OpenGLDetachedMesh* mesh;
        Material* material;
        glm::mat4 modelMatrix;
    };

    void RenderHairLayer(std::vector<HairRenderItem>& renderItems, int peelCount) {
        g_frameBuffers.hair.Bind();
        g_frameBuffers.hair.ClearAttachment("ViewspaceDepthPrevious", 1, 1, 1, 1);
        for (int i = 0; i < peelCount; i++) {
            // Viewspace depth pass
            CopyDepthBuffer(g_frameBuffers.main, g_frameBuffers.hair);
            g_frameBuffers.hair.Bind();
            g_frameBuffers.hair.ClearAttachment("ViewspaceDepth", 0, 0, 0, 0);
            g_frameBuffers.hair.DrawBuffer("ViewspaceDepth");
            glDepthFunc(GL_LESS);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, g_frameBuffers.hair.GetColorAttachmentHandleByName("ViewspaceDepthPrevious"));
            Shader* shader = &g_shaders.hairDepthOnlyPeeled;
            shader->Use();
            shader->SetMat4("projection", Camera::GetProjectionMatrix());
            shader->SetMat4("view", Camera::GetViewMatrix());
            shader->SetFloat("nearPlane", NEAR_PLANE);
            shader->SetFloat("farPlane", FAR_PLANE);
            shader->SetFloat("viewportWidth", g_frameBuffers.hair.GetWidth());
            shader->SetFloat("viewportHeight", g_frameBuffers.hair.GetHeight());
            for (HairRenderItem& renderItem : renderItems) {
                shader->SetMat4("model", renderItem.modelMatrix);
                glBindVertexArray(renderItem.mesh->GetVAO());
                glDrawElements(GL_TRIANGLES, renderItem.mesh->GetIndexCount(), GL_UNSIGNED_INT, 0);
            }
            // Color pass
            glDepthFunc(GL_EQUAL);
            g_frameBuffers.hair.Bind();
            g_frameBuffers.hair.ClearAttachment("Color", 0, 0, 0, 0);
            g_frameBuffers.hair.DrawBuffer("Color");
            shader = &g_shaders.textured;
            shader->Use();
            shader->SetMat4("projection", Camera::GetProjectionMatrix());
            shader->SetMat4("view", Camera::GetViewMatrix());
            for (HairRenderItem& renderItem : renderItems) {
                shader->SetMat4("model", renderItem.modelMatrix);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, AssetManager::GetTextureByIndex(renderItem.material->m_basecolor)->GetGLTexture().GetHandle());
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, AssetManager::GetTextureByIndex(renderItem.material->m_normal)->GetGLTexture().GetHandle());
                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_2D, AssetManager::GetTextureByIndex(renderItem.material->m_rma)->GetGLTexture().GetHandle());
                glBindVertexArray(renderItem.mesh->GetVAO());
                glDrawElements(GL_TRIANGLES, renderItem.mesh->GetIndexCount(), GL_UNSIGNED_INT, 0);
            }
            // TODO!: when you port this you can output previous viewspace depth in the pass above
            CopyColorBuffer(g_frameBuffers.hair, g_frameBuffers.hair, "ViewspaceDepth", "ViewspaceDepthPrevious");

            // Composite
            g_shaders.hairLayerComposite.Use();
            glBindImageTexture(0, g_frameBuffers.hair.GetColorAttachmentHandleByName("Color"), 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
            glBindImageTexture(1, g_frameBuffers.hair.GetColorAttachmentHandleByName("Composite"), 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
            glDispatchCompute((g_frameBuffers.hair.GetWidth() + 7) / 8, (g_frameBuffers.hair.GetHeight() + 7) / 8, 1);
        }
    }

    void RenderHair() {
        static int peelCount = 3;
        if (Input::KeyPressed(HELL_KEY_E) && peelCount < 7) {
            Audio::PlayAudio("UI_Select.wav", 1.0f);
            peelCount++;
            std::cout << "Depth peel layer count: " << peelCount << "\n";
        }
        if (Input::KeyPressed(HELL_KEY_Q) && peelCount > 0) {
            Audio::PlayAudio("UI_Select.wav", 1.0f);
            peelCount--;
            std::cout << "Depth peel layer count: " << peelCount << "\n";
        }

        // Setup state
        Shader* shader = &g_shaders.textured;
        shader->Use();
        shader->SetBool("isHair", true);
        g_frameBuffers.hair.Bind();
        g_frameBuffers.hair.ClearAttachment("Composite", 0, 0, 0, 0);
        g_frameBuffers.hair.SetViewport();
        glEnable(GL_CULL_FACE);
        glDisable(GL_BLEND);

        // Top layer
        std::vector<HairRenderItem> renderItems;
        for (GameObject& gameObject : Scene::g_gameObjects) {
            if (gameObject.m_model) {
                g_shaders.hairDepthOnly.SetMat4("model", gameObject.m_transform.to_mat4());
                for (int i = 0; i < gameObject.m_model->GetMeshCount(); i++) {
                    OpenGLDetachedMesh* mesh = &MeshManager::gMeshes[gameObject.m_model->GetMeshIndices()[i]];
                    BlendingMode& blendingMode = gameObject.m_meshBlendingModes[i];
                    if (blendingMode == BlendingMode::HAIR_TOP_LAYER) {
                        HairRenderItem& renderItem = renderItems.emplace_back();
                        renderItem.modelMatrix = gameObject.m_transform.to_mat4();
                        renderItem.material = AssetManager::GetMaterialByIndex(gameObject.m_meshMaterialIndices[i]);
                        renderItem.mesh = &MeshManager::gMeshes[gameObject.m_model->GetMeshIndices()[i]];
                    }
                }
            }
        }
        RenderHairLayer(renderItems, peelCount);

        // Bottom layer
        renderItems.clear();
        for (GameObject& gameObject : Scene::g_gameObjects) {
            if (gameObject.m_model) {
                g_shaders.hairDepthOnly.SetMat4("model", gameObject.m_transform.to_mat4());
                for (int i = 0; i < gameObject.m_model->GetMeshCount(); i++) {
                    OpenGLDetachedMesh* mesh = &MeshManager::gMeshes[gameObject.m_model->GetMeshIndices()[i]];
                    BlendingMode& blendingMode = gameObject.m_meshBlendingModes[i];
                    if (blendingMode == BlendingMode::HAIR_UNDER_LAYER) {
                        HairRenderItem& renderItem = renderItems.emplace_back();
                        renderItem.modelMatrix = gameObject.m_transform.to_mat4();
                        renderItem.material = AssetManager::GetMaterialByIndex(gameObject.m_meshMaterialIndices[i]);
                        renderItem.mesh = &MeshManager::gMeshes[gameObject.m_model->GetMeshIndices()[i]];
                    }
                }
            }
        }
        RenderHairLayer(renderItems, peelCount);

        // Cleanup
        shader->Use();
        shader->SetBool("isHair", false);
        g_frameBuffers.main.SetViewport();
        glDepthFunc(GL_LESS);
    }


    void RenderWater() {
        static float time = 0;
        static float deltaTime = 0;
        static double lastTime = glfwGetTime();
        double currentTime = glfwGetTime();
        deltaTime = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;
        time += deltaTime;
        g_frameBuffers.main.Bind();
        g_frameBuffers.main.SetViewport();
        glDrawBuffer(g_frameBuffers.main.GetColorAttachmentSlotByName("Color"));
        g_shaders.water.Use();
        g_shaders.water.SetMat4("projection", Camera::GetProjectionMatrix());
        g_shaders.water.SetMat4("view", Camera::GetViewMatrix());
        g_shaders.water.SetMat4("model", glm::mat4(1));
        g_shaders.water.SetVec3("viewPos", Camera::GetViewPos());
        g_shaders.water.SetFloat("viewportWidth", g_frameBuffers.waterReflection.GetWidth());
        g_shaders.water.SetFloat("viewportHeight", g_frameBuffers.waterReflection.GetHeight());
        g_shaders.water.SetFloat("time", time);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_frameBuffers.waterReflection.GetColorAttachmentHandleByName("Color"));
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, g_frameBuffers.waterRefraction.GetColorAttachmentHandleByName("Color"));
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, AssetManager::GetTextureByName("WaterDUDV")->GetGLTexture().GetHandle());
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, AssetManager::GetTextureByName("WaterNormals")->GetGLTexture().GetHandle());

        OpenGLDetachedMesh* mesh = MeshManager::GetDetachedMeshByName("Water");
        if (mesh) {
            glBindVertexArray(mesh->GetVAO());
            glDrawElements(GL_TRIANGLES, mesh->GetIndexCount(), GL_UNSIGNED_INT, 0);
        }
        const float waterHeight = Hardcoded::roomY + Hardcoded::waterHeight;
        g_shaders.computeTest.Use();
        g_shaders.computeTest.SetFloat("time", time);
        g_shaders.computeTest.SetFloat("viewportWidth", g_frameBuffers.main.GetWidth());
        g_shaders.computeTest.SetFloat("viewportHeight", g_frameBuffers.main.GetHeight());
        g_shaders.computeTest.SetFloat("waterHeight", waterHeight);
        g_shaders.computeTest.SetVec3("viewPos", Camera::GetViewPos());
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_frameBuffers.main.GetColorAttachmentHandleByName("Color"));
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, g_frameBuffers.main.GetColorAttachmentHandleByName("Position"));
        glBindImageTexture(2, g_frameBuffers.main.GetColorAttachmentHandleByName("UnderWaterColor"), 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
        glDispatchCompute(g_frameBuffers.main.GetWidth() / 8, g_frameBuffers.main.GetHeight() / 8, 1);
    }

    void RenderDebug() {
        //for (GameObject& gameObject : Scene::gGameObjects) {
        //    for (int i = 0; i < gameObject.m_model->GetMeshCount(); i++) {
        //        OpenGLDetachedMesh* mesh = &MeshManager::gMeshes[gameObject.m_model->GetMeshIndices()[i]];
        //        AABB aabb(mesh->aabbMin, mesh->aabbMax);
        //        DrawAABB(aabb, YELLOW, gameObject.m_transform.to_mat4());
        //    }
        //    AABB aabb(gameObject.m_model->m_aabbMin, gameObject.m_model->m_aabbMax);
        //    DrawAABB(aabb, RED, gameObject.m_transform.to_mat4());
        //}

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glPointSize(8.0f);
        glDisable(GL_DEPTH_TEST);
        g_shaders.solidColor.Use();
        g_shaders.solidColor.SetMat4("projection", Camera::GetProjectionMatrix());
        g_shaders.solidColor.SetMat4("view", Camera::GetViewMatrix());
        g_shaders.solidColor.SetMat4("model", glm::mat4(1));
        // Draw lines
        UpdateDebugLinesMesh();
        if (g_debugLinesMesh.GetIndexCount() > 0) {
            glBindVertexArray(g_debugLinesMesh.GetVAO());
            glDrawElements(GL_LINES, g_debugLinesMesh.GetIndexCount(), GL_UNSIGNED_INT, 0);
        }
        // Draw points
        UpdateDebugPointsMesh();
        if (g_debugPointsMesh.GetIndexCount() > 0) {
            glBindVertexArray(g_debugPointsMesh.GetVAO());
            glDrawElements(GL_POINTS, g_debugPointsMesh.GetIndexCount(), GL_UNSIGNED_INT, 0);
        }
    }

    void GenerateMipmaps(OpenGLTexture& glTexture) {
        g_shaders.generateMipmap.Use();
        glBindTexture(GL_TEXTURE_2D, glTexture.GetHandle());
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
        return;

        //for (int level = 0; level < glTexture.m_levels - 1; ++level) {
        //    int srcWidth = std::max(1, glTexture.m_width >> level);
        //    int srcHeight = std::max(1, glTexture.m_height >> level);
        //    int dstWidth = std::max(1, glTexture.m_width >> (level + 1));
        //    int dstHeight = std::max(1, glTexture.m_height >> (level + 1));
        //    glBindImageTexture(0, glTexture.GetID(), level, GL_FALSE, 0, GL_READ_ONLY, glTexture.m_format);
        //    glBindImageTexture(1, glTexture.GetID(), level + 1, GL_FALSE, 0, GL_WRITE_ONLY, glTexture.m_format);
        //    int groupX = (dstWidth + 15) / 16;
        //    int groupY = (dstHeight + 15) / 16;
        //    glDispatchCompute(groupX, groupY, 1);
        //    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        //}
        //glUseProgram(0);
        //std::cout << "Generated mipmap " << glTexture.GetFilename() << " " << OpenGLUtil::GetGLFormatString(glTexture.m_format) << "\n";
    }

    void LoadShaders() {        
        g_shaders.generateMipmap.Load("res/shaders/gl_generate_mipmap.comp");
        g_shaders.debugOutput.Load("res/shaders/debug_output.comp");
        g_shaders.hairLayerComposite.Load("res/shaders/hair_layer_composite.comp");
        g_shaders.computeTest.Load("res/shaders/compute_test.comp");
        g_shaders.water.Load("water.vert", "water.frag");
        g_shaders.solidColor.Load("solid_color.vert", "solid_color.frag");
        g_shaders.hairDepthOnly.Load("hair_depth_only.vert", "hair_depth_only.frag");
        g_shaders.hairDepthOnlyPeeled.Load("hair_depth_only_peeled.vert", "hair_depth_only_peeled.frag");
        g_shaders.textured.Load("textured.vert", "textured.frag");
        g_shaders.flipbook.Load("gl_flipbook.vert", "gl_flipbook.frag");
    }
}
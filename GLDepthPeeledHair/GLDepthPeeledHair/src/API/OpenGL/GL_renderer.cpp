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

namespace OpenGLRenderer {

    struct Shaders {
        Shader solidColor;
        Shader lighting;
        Shader water;
        Shader hairDepthPeel;
        ComputeShader computeTest;
        ComputeShader hairfinalComposite;
        ComputeShader hairLayerComposite;
    } g_shaders;

    struct FrameBuffers {
        GLFrameBuffer main;
        GLFrameBuffer hair;
    } g_frameBuffers;

    struct RenderItem {
        OpenGLDetachedMesh* mesh;
        Material* material;
        glm::mat4 modelMatrix;
    };

    void DrawScene(Shader& shader, bool renderHair);
    void RenderLighting();
    void RenderDebug();
    void RenderHair();
    void GenerateMipmaps(OpenGLTexture& glTexture); 
    void RenderHairLayer(std::vector<RenderItem>& renderItems, int peelCount);

    void Init() {
        g_frameBuffers.main.Create("Main", 1920, 1080);
        g_frameBuffers.main.CreateAttachment("Color", GL_RGBA8);
        g_frameBuffers.main.CreateDepthAttachment(GL_DEPTH32F_STENCIL8);

        float hairDownscaleRatio = 1.0f;
        g_frameBuffers.hair.Create("Hair", g_frameBuffers.main.GetWidth() * hairDownscaleRatio, g_frameBuffers.main.GetHeight() * hairDownscaleRatio);
        g_frameBuffers.hair.CreateDepthAttachment(GL_DEPTH32F_STENCIL8);
        g_frameBuffers.hair.CreateAttachment("Color", GL_RGBA8);
        g_frameBuffers.hair.CreateAttachment("ViewspaceDepth", GL_R32F);
        g_frameBuffers.hair.CreateAttachment("ViewspaceDepthPrevious", GL_R32F);
        g_frameBuffers.hair.CreateAttachment("Composite", GL_RGBA8);
        LoadShaders();
    }

    void RenderFrame() {

        g_frameBuffers.main.Bind();
        g_frameBuffers.main.SetViewport();
        g_frameBuffers.main.DrawBuffers({ "Color" });
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        RenderLighting();
        RenderHair();
        RenderDebug();

        int width, height;
        glfwGetWindowSize(OpenGLBackend::GetWindowPtr(), &width, &height);

        GLFrameBuffer& mainFrameBuffer = g_frameBuffers.main;
        GLFrameBuffer& hairFrameBuffer = g_frameBuffers.hair;

        g_shaders.hairfinalComposite.Use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, hairFrameBuffer.GetColorAttachmentHandleByName("Composite"));
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, mainFrameBuffer.GetColorAttachmentHandleByName("Color"));
        glBindImageTexture(0, mainFrameBuffer.GetColorAttachmentHandleByName("Color"), 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
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
        glBlitFramebuffer(0, 0, srcFrameBuffer.GetWidth(), srcFrameBuffer.GetHeight(), 0, 0, dstFrameBuffer.GetWidth(), dstFrameBuffer.GetHeight(), GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    }

    void DrawScene(Shader& shader, bool renderHair) {
        // Non blended
        for (GameObject& gameObject : Scene::g_gameObjects) {
            if (gameObject.m_model) {
                shader.SetMat4("model", gameObject.m_transform.to_mat4());
                for (int i = 0; i < gameObject.m_model->GetMeshCount(); i++) {
                    BlendingMode& blendingMode = gameObject.m_meshBlendingModes[i];
                    if (blendingMode == BlendingMode::NONE) {
                        Material* material = AssetManager::GetMaterialByIndex(gameObject.m_meshMaterialIndices[i]);
                        if (material && material->m_basecolor != -1) {
                            glActiveTexture(GL_TEXTURE0);
                            glBindTexture(GL_TEXTURE_2D, AssetManager::GetTextureByIndex(material->m_basecolor)->GetGLTexture().GetHandle());
                        }
                        if (material && material->m_normal != -1) {
                            glActiveTexture(GL_TEXTURE1);
                            glBindTexture(GL_TEXTURE_2D, AssetManager::GetTextureByIndex(material->m_normal)->GetGLTexture().GetHandle());
                        }
                        if (material && material->m_rma != -1) {
                            glActiveTexture(GL_TEXTURE2);
                            glBindTexture(GL_TEXTURE_2D, AssetManager::GetTextureByIndex(material->m_rma)->GetGLTexture().GetHandle());
                        }
                        OpenGLDetachedMesh* mesh = AssetManager::GetMeshByIndex(gameObject.m_model->GetMeshIndices()[i]);
                        if (mesh) {
                            glBindVertexArray(mesh->GetVAO());
                            glDrawElements(GL_TRIANGLES, mesh->GetIndexCount(), GL_UNSIGNED_INT, 0);
                        }
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
                    BlendingMode& blendingMode = gameObject.m_meshBlendingModes[i];
                    if (blendingMode == BlendingMode::BLENDED) {
                        Material* material = AssetManager::GetMaterialByIndex(gameObject.m_meshMaterialIndices[i]);
                        if (material && material->m_basecolor != -1) {
                            glActiveTexture(GL_TEXTURE0);
                            glBindTexture(GL_TEXTURE_2D, AssetManager::GetTextureByIndex(material->m_basecolor)->GetGLTexture().GetHandle());
                        }
                        if (material && material->m_normal != -1) {
                            glActiveTexture(GL_TEXTURE1);
                            glBindTexture(GL_TEXTURE_2D, AssetManager::GetTextureByIndex(material->m_normal)->GetGLTexture().GetHandle());
                        }
                        if (material && material->m_rma != -1) {
                            glActiveTexture(GL_TEXTURE2);
                            glBindTexture(GL_TEXTURE_2D, AssetManager::GetTextureByIndex(material->m_rma)->GetGLTexture().GetHandle());
                        }
                        OpenGLDetachedMesh* mesh = AssetManager::GetMeshByIndex(gameObject.m_model->GetMeshIndices()[i]);
                        if (mesh) {
                            glBindVertexArray(mesh->GetVAO());
                            glDrawElements(GL_TRIANGLES, mesh->GetIndexCount(), GL_UNSIGNED_INT, 0);
                        }
                    }
                }
            }
        }
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
    }


    void RenderLighting() {
        const float waterHeight = Hardcoded::roomY + Hardcoded::waterHeight;
        static float time = 0;
        time += 1.0f / 60.0f;

        g_frameBuffers.main.Bind();
        g_frameBuffers.main.SetViewport();
        g_frameBuffers.main.DrawBuffers({ "Color" });

        glEnable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        g_shaders.lighting.Use();
        g_shaders.lighting.SetMat4("projection", Camera::GetProjectionMatrix());
        g_shaders.lighting.SetMat4("view", Camera::GetViewMatrix());
        g_shaders.lighting.SetMat4("model", glm::mat4(1));
        g_shaders.lighting.SetVec3("viewPos", Camera::GetViewPos());
        g_shaders.lighting.SetFloat("time", time);
        g_shaders.lighting.SetFloat("viewportWidth", g_frameBuffers.hair.GetWidth());
        g_shaders.lighting.SetFloat("viewportHeight", g_frameBuffers.hair.GetHeight());
        DrawScene(g_shaders.lighting, false);
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
        Shader* shader = &g_shaders.lighting;
        shader->Use();
        shader->SetBool("isHair", true);
        g_frameBuffers.hair.Bind();
        g_frameBuffers.hair.ClearAttachment("Composite", 0, 0, 0, 0);
        g_frameBuffers.hair.SetViewport();
        glEnable(GL_CULL_FACE);
        glDisable(GL_BLEND);

        // Top layer
        std::vector<RenderItem> renderItems;
        for (GameObject& gameObject : Scene::g_gameObjects) {
            if (gameObject.m_model) {
                g_shaders.hairDepthPeel.SetMat4("model", gameObject.m_transform.to_mat4());
                for (int i = 0; i < gameObject.m_model->GetMeshCount(); i++) {
                    OpenGLDetachedMesh* mesh = AssetManager::GetMeshByIndex(gameObject.m_model->GetMeshIndices()[i]);
                    BlendingMode& blendingMode = gameObject.m_meshBlendingModes[i];
                    if (blendingMode == BlendingMode::HAIR_TOP_LAYER) {
                        RenderItem& renderItem = renderItems.emplace_back();
                        renderItem.modelMatrix = gameObject.m_transform.to_mat4();
                        renderItem.material = AssetManager::GetMaterialByIndex(gameObject.m_meshMaterialIndices[i]);
                        renderItem.mesh = mesh;
                    }
                }
            }
        }
        RenderHairLayer(renderItems, peelCount);

        // Bottom layer
        renderItems.clear();
        for (GameObject& gameObject : Scene::g_gameObjects) {
            if (gameObject.m_model) {
                g_shaders.hairDepthPeel.SetMat4("model", gameObject.m_transform.to_mat4());
                for (int i = 0; i < gameObject.m_model->GetMeshCount(); i++) {
                    OpenGLDetachedMesh* mesh = AssetManager::GetMeshByIndex(gameObject.m_model->GetMeshIndices()[i]);
                    BlendingMode& blendingMode = gameObject.m_meshBlendingModes[i];
                    if (blendingMode == BlendingMode::HAIR_UNDER_LAYER) {
                        RenderItem& renderItem = renderItems.emplace_back();
                        renderItem.modelMatrix = gameObject.m_transform.to_mat4();
                        renderItem.material = AssetManager::GetMaterialByIndex(gameObject.m_meshMaterialIndices[i]);
                        renderItem.mesh = mesh;
                    }
                }
            }
        }
        RenderHairLayer(renderItems, peelCount);


        GLFrameBuffer& mainFrameBuffer = g_frameBuffers.main;
        GLFrameBuffer& hairFrameBuffer = g_frameBuffers.hair;

        g_shaders.hairfinalComposite.Use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, hairFrameBuffer.GetColorAttachmentHandleByName("Composite"));
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, mainFrameBuffer.GetColorAttachmentHandleByName("Color"));
        glBindImageTexture(0, mainFrameBuffer.GetColorAttachmentHandleByName("Color"), 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);
        glDispatchCompute((g_frameBuffers.main.GetWidth() + 7) / 8, (g_frameBuffers.main.GetHeight() + 7) / 8, 1);

        // Cleanup
        shader->Use();
        shader->SetBool("isHair", false);
        g_frameBuffers.main.SetViewport();
        glDepthFunc(GL_LESS);
    }

    void RenderHairLayer(std::vector<RenderItem>& renderItems, int peelCount) {
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
            Shader* shader = &g_shaders.hairDepthPeel;
            shader->Use();
            shader->SetMat4("projection", Camera::GetProjectionMatrix());
            shader->SetMat4("view", Camera::GetViewMatrix());
            shader->SetFloat("nearPlane", NEAR_PLANE);
            shader->SetFloat("farPlane", FAR_PLANE);
            shader->SetFloat("viewportWidth", g_frameBuffers.hair.GetWidth());
            shader->SetFloat("viewportHeight", g_frameBuffers.hair.GetHeight());
            for (RenderItem& renderItem : renderItems) {
                shader->SetMat4("model", renderItem.modelMatrix);
                glBindVertexArray(renderItem.mesh->GetVAO());
                glDrawElements(GL_TRIANGLES, renderItem.mesh->GetIndexCount(), GL_UNSIGNED_INT, 0);
            }
            // Color pass
            glDepthFunc(GL_EQUAL);
            g_frameBuffers.hair.Bind();
            g_frameBuffers.hair.ClearAttachment("Color", 0, 0, 0, 0);
            g_frameBuffers.hair.DrawBuffer("Color");
            shader = &g_shaders.lighting;
            shader->Use();
            shader->SetMat4("projection", Camera::GetProjectionMatrix());
            shader->SetMat4("view", Camera::GetViewMatrix());
            for (RenderItem& renderItem : renderItems) {
                shader->SetMat4("model", renderItem.modelMatrix);
                Material* material = renderItem.material;
                if (material && material->m_basecolor != -1) {
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, AssetManager::GetTextureByIndex(material->m_basecolor)->GetGLTexture().GetHandle());
                }
                if (material && material->m_normal != -1) {
                    glActiveTexture(GL_TEXTURE1);
                    glBindTexture(GL_TEXTURE_2D, AssetManager::GetTextureByIndex(material->m_normal)->GetGLTexture().GetHandle());
                }
                if (material && material->m_rma != -1) {
                    glActiveTexture(GL_TEXTURE2);
                    glBindTexture(GL_TEXTURE_2D, AssetManager::GetTextureByIndex(material->m_rma)->GetGLTexture().GetHandle());
                }
                OpenGLDetachedMesh* mesh = renderItem.mesh;
                if (mesh) {
                    glBindVertexArray(renderItem.mesh->GetVAO());
                    glDrawElements(GL_TRIANGLES, renderItem.mesh->GetIndexCount(), GL_UNSIGNED_INT, 0);
                }
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

    void RenderDebug() {
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
        return;
        glBindTexture(GL_TEXTURE_2D, glTexture.GetHandle());
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void LoadShaders() {
        g_shaders.hairfinalComposite.Load("res/shaders/gl_hair_final_composite.comp");
        g_shaders.hairLayerComposite.Load("res/shaders/gl_hair_layer_composite.comp");
        g_shaders.solidColor.Load("gl_solid_color.vert", "gl_solid_color.frag");
        g_shaders.hairDepthPeel.Load("gl_hair_depth_peel.vert", "gl_hair_depth_peel.frag");
        g_shaders.lighting.Load("gl_lighting.vert", "gl_lighting.frag");
    }
}
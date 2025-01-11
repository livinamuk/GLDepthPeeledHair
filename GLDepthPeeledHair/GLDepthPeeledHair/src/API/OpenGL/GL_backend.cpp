#pragma once
#include "GL_backend.h"
#include <string>
#include <iostream>
#include <vector>
#include "GL_util.hpp"

namespace OpenGLBackend {

    GLFWwindow* g_window;
    WindowedMode g_windowedMode;
    GLFWmonitor* g_monitor;
    const GLFWvidmode* g_mode;
    GLuint g_currentWindowWidth = 0;
    GLuint g_currentWindowHeight = 0;
    GLuint g_windowedWidth = 0;
    GLuint g_windowedHeight = 0;
    GLuint g_fullscreenWidth;
    GLuint g_fullscreenHeight;

    // PBO texture loading
    const size_t MAX_TEXTURE_WIDTH = 4096;
    const size_t MAX_TEXTURE_HEIGHT = 4096;
    const size_t MAX_CHANNEL_COUNT = 4;
    const size_t MAX_DATA_SIZE = MAX_TEXTURE_WIDTH * MAX_TEXTURE_HEIGHT * MAX_CHANNEL_COUNT;
    std::vector<PBO> g_textureUploadPBOs;

    GLFWwindow* GetWindowPtr() {
        return g_window;
    }

    void Init(int width, int height, std::string title) {
        glfwInit();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
        g_monitor = glfwGetPrimaryMonitor();
        g_mode = glfwGetVideoMode(g_monitor);
        glfwWindowHint(GLFW_RED_BITS, g_mode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, g_mode->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, g_mode->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, g_mode->refreshRate);
        g_fullscreenWidth = g_mode->width;
        g_fullscreenHeight = g_mode->height;
        g_windowedWidth = width;
        g_windowedHeight = height;
        g_window = glfwCreateWindow(width, height, title.c_str(), NULL, NULL);
        if (g_window == NULL) {
            std::cout << "GLFW window failed creation\n";
            glfwTerminate();
            return;
        }
        glfwMakeContextCurrent(g_window);
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            std::cout << "GLAD failed init\n";
            return;
        }
        glfwSetInputMode(g_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        ToggleFullscreen();
        ToggleFullscreen();

        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        SwapBuffersPollEvents();

        for (int i = 0; i < 4; ++i) {
            PBO& pbo = g_textureUploadPBOs.emplace_back();
            pbo.Init(MAX_DATA_SIZE);
        }
        return;
    }

    void SetWindowedMode(const WindowedMode& windowedMode) {
        if (windowedMode == WindowedMode::WINDOWED) {
            g_currentWindowWidth = g_windowedWidth;
            g_currentWindowHeight = g_windowedHeight;
            glfwSetWindowMonitor(g_window, nullptr, 0, 0, g_windowedWidth, g_windowedHeight, g_mode->refreshRate);
            glfwSetWindowPos(g_window, 0, 0);
            glfwSwapInterval(0);
        }
        else if (windowedMode == WindowedMode::FULLSCREEN) {
            g_currentWindowWidth = g_fullscreenWidth;
            g_currentWindowHeight = g_fullscreenHeight;
            glfwSetWindowMonitor(g_window, g_monitor, 0, 0, g_fullscreenWidth, g_fullscreenHeight, g_mode->refreshRate); 
            glfwSwapInterval(1);
        }
        g_windowedMode = windowedMode;
    }

    void ToggleFullscreen() {
        if (g_windowedMode == WindowedMode::WINDOWED) {
            SetWindowedMode(WindowedMode::FULLSCREEN);
        }
        else {
            SetWindowedMode(WindowedMode::WINDOWED);
        }
    }

    void ForceCloseWindow() {
        glfwSetWindowShouldClose(g_window, true);
    }

    double GetMouseX() {
        double x, y;
        glfwGetCursorPos(g_window, &x, &y);
        return x;
    }

    double GetMouseY() {
        double x, y;
        glfwGetCursorPos(g_window, &x, &y);
        return y;
    }

    int GetWindowWidth() {
        int width, height;
        glfwGetWindowSize(g_window, &width, &height);
        return width;
    }

    int GetWindowHeight() {
        int width, height;
        glfwGetWindowSize(g_window, &width, &height);
        return height;
    }

    void SwapBuffersPollEvents() {
        glfwSwapBuffers(g_window);
        glfwPollEvents();
    }

    bool WindowIsOpen() {
        return !glfwWindowShouldClose(g_window);
    }

    void ImmediateBake(Texture& texture) {
        OpenGLTexture& glTexture = texture.GetGLTexture();
        glBindTexture(GL_TEXTURE_2D, glTexture.GetHandle());
        if (texture.GetImageDataType() == ImageDataType::UNCOMPRESSED) {
            // Hack to make Resident Evil font look okay when scaled
            if (texture.GetFileName().substr(0, 4) == "char") {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            }

            //std::cout << "Immediate bake: " << texture.GetFileName() << "\n";
            //std::cout << "texture.GetWidth(): " << texture.GetWidth() << "\n";
            //std::cout << "texture.GetHeight(): " << texture.GetHeight() << "\n";
            //std::cout << "glTexture.GetDataSize(): " << glTexture.GetDataSize() << "\n";
            //std::cout << "glTexture.GetFormat(): " << OpenGLUtil::GetGLFormatAsString(glTexture.GetFormat()) << "\n";
            //std::cout << "glTexture.GetInternalFormat(): " << OpenGLUtil::GetGLInternalFormatAsString(glTexture.GetInternalFormat()) << "\n";

            //glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, glTexture.GetWidth(), glTexture.GetHeight(), glTexture.GetFormat(), GL_UNSIGNED_BYTE, glTexture.GetData());
            glTexImage2D(GL_TEXTURE_2D, 0, glTexture.GetInternalFormat(), glTexture.GetWidth(), glTexture.GetHeight(), 0, glTexture.GetFormat(), GL_UNSIGNED_BYTE, glTexture.GetData());
        }
        if (texture.GetImageDataType() == ImageDataType::COMPRESSED) {
            std::cout << "You need to implement immediate baking for compressed texture!\n";
            //glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, glTexture.GetWidth(), glTexture.GetHeight(), glTexture.GetInternalFormat(), glTexture.GetDataSize(), nullptr);
        }
        if (texture.GetImageDataType() == ImageDataType::EXR) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16, glTexture.GetWidth(), glTexture.GetHeight(), 0, GL_RGBA, GL_FLOAT, glTexture.GetData());
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        texture.SetBakingState(BakingState::BAKE_COMPLETE);
    }

    void BakeNextAwaitingTexture(std::vector<Texture>& textures) {
        // Update pbo states
        for (PBO& pbo : g_textureUploadPBOs) {
            pbo.UpdateState();
        }
        // If any have completed, mark their corresponding textured as baked
        for (PBO& pbo : g_textureUploadPBOs) {
            uint32_t textureIndex = pbo.GetCustomValue();
            if (pbo.IsSyncComplete() && textureIndex != -1) {
                if (textures[textureIndex].GetBakingState() == BakingState::BAKING_IN_PROGRESS) {
                    textures[textureIndex].SetBakingState(BakingState::BAKE_COMPLETE);
                    std::cout << "PBO " << pbo.GetHandle() << " BAKE COMPLETE: " << textures[textureIndex].GetFileInfo().name << "\n";
                }
                pbo.SetCustomValue(-1);
            }
        }
        // Bake next unbaked texture
        for (int textureIndex = 0; textureIndex < textures.size(); textureIndex++) {
            Texture& texture = textures[textureIndex];
            if (texture.GetLoadingState() == LoadingState::LOADING_COMPLETE && texture.GetBakingState() == BakingState::AWAITING_BAKE) {

                // Get next free PBO
                PBO* pbo = nullptr;
                for (PBO& queryPbo : g_textureUploadPBOs) {
                    if (queryPbo.IsSyncComplete()) {
                        pbo = &queryPbo;
                        break;
                    }
                }
                // Return early if no free pbos
                if (!pbo) {
                    std::cout << "OpenGLBackend::UploadTexture() return early because there were no pbos avaliable!\n";
                    return;
                }
                texture.SetBakingState(BakingState::BAKING_IN_PROGRESS);
                texture.SetMipmapState(MipmapState::AWAITING_MIPMAP_GENERATION);

                OpenGLTexture& glTexture = texture.GetGLTexture();
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo->GetHandle());
                std::memcpy(pbo->GetPersistentBuffer(), glTexture.GetData(), glTexture.GetDataSize());
                glBindTexture(GL_TEXTURE_2D, glTexture.GetHandle());

                if (texture.GetImageDataType() == ImageDataType::UNCOMPRESSED) {
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, glTexture.GetWidth(), glTexture.GetHeight(), glTexture.GetFormat(), GL_UNSIGNED_BYTE, nullptr);
                    //std::cout << "glTexture.GetWidth(): " << glTexture.GetWidth() << "\n";
                    //std::cout << "glTexture.GetHeight(): " << glTexture.GetHeight() << "\n";
                    //std::cout << "glTexture.GetFormat(): " << glTexture.GetFormat() << "\n";
                    //std::cout << "glTexture.GetChannelCount(): " << glTexture.GetChannelCount() << "\n\n";

                }
                if (texture.GetImageDataType() == ImageDataType::COMPRESSED) {
                    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, glTexture.GetWidth(), glTexture.GetHeight(), glTexture.GetInternalFormat(), glTexture.GetDataSize(), nullptr);
                }
                if (texture.GetImageDataType() == ImageDataType::EXR) {
                    //glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, glTexture.GetWidth(), glTexture.GetHeight(), glTexture.GetFormat(), GL_FLOAT, nullptr);
                }
                pbo->SyncStart();
                pbo->SetCustomValue(textureIndex);

                std::cout << "PBO " << pbo->GetHandle() << " SYNC STARTED:  " << texture.GetFileName() << "\n";
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);             
                return;
            }
        }
    }
}
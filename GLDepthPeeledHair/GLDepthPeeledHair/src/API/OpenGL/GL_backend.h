#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>
#include <iostream>
#include "Types/GL_pbo.hpp"
#include "Types/GL_texture.h"
#include "../../Texture.h"

enum class WindowedMode { WINDOWED, FULLSCREEN };

namespace OpenGLBackend {

    void Init(int width, int height, std::string title);
    void ToggleFullscreen();
    void SetWindowedMode(const WindowedMode& windowedMode);
    void ToggleFullscreen();
    void ForceCloseWindow();
    double GetMouseX();
    double GetMouseY();
    int GetWindowWidth();
    int GetWindowHeight();
    void SwapBuffersPollEvents();
    bool WindowIsOpen();
    GLFWwindow* GetWindowPtr();

    // Texture uploading
    void ImmediateBake(Texture& texture);
    void BakeNextAwaitingTexture(std::vector<Texture>& textures);
}
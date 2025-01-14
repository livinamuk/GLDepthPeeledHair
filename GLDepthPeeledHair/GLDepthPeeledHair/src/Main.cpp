#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "Types.h"
#include "Managers/AssetManager.h"
#include "Audio.h"
#include "Input.h"
#include "Scene.hpp"
#include "Camera.h"
#include "File/File.h"
#include "Tools/ImageTools.h"
#include "API/OpenGL/GL_backend.h"
#include "API/OpenGL/GL_renderer.h"
#include "TextBlitting/Textblitter.h"

void Init(int width, int height, std::string title) {
    OpenGLBackend::Init(width, height, title);
    AssetManager::Init();
    TextBlitter::Init();
    Audio::Init();
    Scene::Init();
    Input::Init(OpenGLBackend::GetWindowPtr());
    Camera::Init(OpenGLBackend::GetWindowPtr());
    Scene::CreateGameObjects();
    OpenGLRenderer::Init();
}

void Update() {
    static float deltaTime = 0;
    static double lastTime = glfwGetTime();
    double currentTime = glfwGetTime();
    deltaTime = static_cast<float>(currentTime - lastTime);
    lastTime = currentTime;
    OpenGLBackend::UpdateTextureBaking();
    Scene::SetMaterials();
    AssetManager::Update();
    TextBlitter::Update();
    Input::Update();
    Camera::Update(deltaTime);
    Audio::Update();
    Scene::Update(deltaTime);
    if (Input::KeyPressed(HELL_KEY_ESCAPE)) {
        glfwSetWindowShouldClose(OpenGLBackend::GetWindowPtr(), true);
    }
    if (Input::KeyPressed(HELL_KEY_F)) {
        OpenGLBackend::ToggleFullscreen();
    }
    if (Input::KeyPressed(HELL_KEY_H)) {
        OpenGLRenderer::LoadShaders();
    }
}

void Render() {
    OpenGLRenderer::RenderFrame();
}

int main() {
    Init(1920, 1080, "GL Depth Peeling");
    while (OpenGLBackend::WindowIsOpen()) {
        Update();
        Render();
    }
    glfwTerminate();
    return 0;
}
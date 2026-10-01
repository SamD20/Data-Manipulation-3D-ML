#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include "app_state.h"
#include "editor.h"
#include "renderer.h"

#include <GLFW/glfw3.h>
#include <GL/gl.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl2.h>
#include <imgui.h>

#include <algorithm>
#include <iostream>

namespace {

GLFWwindow* createWindow() {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    return glfwCreateWindow(1100, 700, "OpenGL Data App", nullptr, nullptr);
}

void initializeImGui(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL2_Init();
}

void shutdownImGui() {
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void runApplication(GLFWwindow* window, app::AppState& state) {
    double previousFrameTime = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        app::drawEditorPanel(state);

        const double currentFrameTime = glfwGetTime();
        const float deltaTime = std::min(static_cast<float>(currentFrameTime - previousFrameTime), 0.1f);
        previousFrameTime = currentFrameTime;
        app::updateCameraMovement(window, state, deltaTime);

        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        app::renderScene(state, width, height);

        ImGui::Render();
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
}

}  // namespace

int main() {
    if (!glfwInit()) {
        std::cerr << "GLFW initialization failed\n";
        return 1;
    }

    GLFWwindow* window = createWindow();
    if (!window) {
        glfwTerminate();
        return 1;
    }

#ifdef _WIN32
    const HRESULT comStatus = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool uninitializeCom = SUCCEEDED(comStatus);
#endif

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    app::AppState state;
    app::initializeHistory(state);
    glfwSetWindowUserPointer(window, &state);
    app::installEditorCallbacks(window);
    initializeImGui(window);
    app::initializeRenderer();
    runApplication(window, state);

    shutdownImGui();
    for (const app::ImageLayer& image : state.imageLayers) {
        if (image.texture != 0) glDeleteTextures(1, &image.texture);
    }
    glfwDestroyWindow(window);
    glfwTerminate();

#ifdef _WIN32
    if (uninitializeCom) CoUninitialize();
#endif
    return 0;
}
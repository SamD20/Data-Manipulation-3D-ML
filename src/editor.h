#pragma once

#include "app_state.h"

namespace app {

void installEditorCallbacks(GLFWwindow* window);
void drawEditorPanel(AppState& state);
void updateCameraMovement(GLFWwindow* window, AppState& state, float deltaTime);

}  // namespace app
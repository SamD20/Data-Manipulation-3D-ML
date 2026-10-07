#include "editor.h"
#include "app_state.h"
#include "file_io.h"
#include "depth_extractor.h"
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace app {
namespace {

void onMouseButton(GLFWwindow* window, int button, int action, int) {
    auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        if (ImGui::GetIO().WantCaptureMouse) {
            state->orbiting = false;
            return;
        }
        state->orbiting = true;
        glfwGetCursorPos(window, &state->lastMouseX, &state->lastMouseY);
        state->selectedDataPoint = hitDataPoint(*state, window, state->lastMouseX, state->lastMouseY);
        state->selectedImage = -1;
    } else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        state->orbiting = false;
    }
}

void onCursorMoved(GLFWwindow* window, double x, double y) {
    auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (!state->orbiting) return;
    state->yaw += static_cast<float>(x - state->lastMouseX) * 0.35f;
    state->pitch += static_cast<float>(y - state->lastMouseY) * 0.35f;
    state->pitch = std::clamp(state->pitch, -89.0f, 89.0f);
    state->lastMouseX = x;
    state->lastMouseY = y;
}

void onScroll(GLFWwindow* window, double, double amount) {
    if (ImGui::GetIO().WantCaptureMouse) return;
    auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    state->distance = std::clamp(state->distance - static_cast<float>(amount) * 0.5f, 0.005f, 20.0f);
}

void onKeyboard(GLFWwindow* window, int key, int, int action, int) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;
    if (ImGui::GetIO().WantCaptureKeyboard) return;
    auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, GLFW_TRUE);
    if (state->selectedDataPoint < 0 || state->selectedDataPoint >= static_cast<int>(state->dataPoints.size())) return;

    DataPoint& point = state->dataPoints[state->selectedDataPoint];
    bool moved = true;
    if (key == GLFW_KEY_LEFT) point.x -= voxelSpacingWorldUnits;
    else if (key == GLFW_KEY_RIGHT) point.x += voxelSpacingWorldUnits;
    else if (key == GLFW_KEY_DOWN) point.z += voxelSpacingWorldUnits;
    else if (key == GLFW_KEY_UP) point.z -= voxelSpacingWorldUnits;
    else if (key == GLFW_KEY_PAGE_UP) point.y += voxelSpacingWorldUnits;
    else if (key == GLFW_KEY_PAGE_DOWN) point.y -= voxelSpacingWorldUnits;
    else moved = false;
    if (moved) commitHistory(*state, "Move Data Point");
}

}  // namespace

void installEditorCallbacks(GLFWwindow* window) {
    glfwSetMouseButtonCallback(window, onMouseButton);
    glfwSetCursorPosCallback(window, onCursorMoved);
    glfwSetScrollCallback(window, onScroll);
    glfwSetKeyCallback(window, onKeyboard);
}

void drawEditorPanel(AppState& state) {
    ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 500.0f), ImGuiCond_FirstUseEver);
    const float maxWindowSize = std::numeric_limits<float>::max();
    ImGui::SetNextWindowSizeConstraints(ImVec2(300.0f, 340.0f), ImVec2(maxWindowSize, maxWindowSize));
    ImGui::Begin("Menu");
    if (ImGui::Button("Open Project")) {
        static const char filter[] = "OBJ Projects (*.obj)\0*.obj\0All Files (*.*)\0*.*\0\0";
        const std::string path = choosePath(false, filter, "obj");
        if (!path.empty() && loadObj(state, path)) commitHistory(state, "Open Data Point project");
    }

    if (ImGui::BeginTabBar("MenuTabs")) {
        if (ImGui::BeginTabItem("Layers")) {
            if (ImGui::Button("Import TIFF Stack")) {
                static const char filter[] = "TIFF Images (*.tif;*.tiff)\0*.tif;*.tiff\0All Files (*.*)\0*.*\0\0";
                const std::vector<std::string> paths = chooseMultiplePaths(filter);
                for (const std::string& path : paths) {
                    if (!loadTiff(state, path)) break;
                    commitHistory(state, "Import TIFF layer");
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Add Data Point")) {
                state.dataPoints.emplace_back();
                state.selectedDataPoint = static_cast<int>(state.dataPoints.size()) - 1;
                state.selectedImage = -1;
                commitHistory(state, "Add Data Point");
            }
            ImGui::Text("TIFF slices: %d", static_cast<int>(state.activeImageCount));
            ImGui::BeginChild("LayerList", ImVec2(0.0f, 190.0f), true);
            for (int index = 0; index < static_cast<int>(state.activeImageCount); ++index) {
                const ImageLayer& image = state.imageLayers[index];
                const std::string label = "TIFF " + std::to_string(index + 1) + ": " + image.name;
                if (ImGui::Selectable(label.c_str(), state.selectedImage == index)) {
                    state.selectedImage = index;
                    state.selectedDataPoint = -1;
                }
            }
            for (int index = 0; index < static_cast<int>(state.dataPoints.size()); ++index) {
                const std::string label = "Data Point " + std::to_string(index + 1);
                if (ImGui::Selectable(label.c_str(), state.selectedImage == -1 && state.selectedDataPoint == index)) {
                    state.selectedImage = -1;
                    state.selectedDataPoint = index;
                }
            }
            ImGui::EndChild();

            if (state.selectedImage >= 0 && state.selectedImage < static_cast<int>(state.activeImageCount)) {
                ImGui::Text("Selected TIFF layer %d", state.selectedImage + 1);
            } else if (state.selectedDataPoint >= 0 && state.selectedDataPoint < static_cast<int>(state.dataPoints.size())) {
                ImGui::Text("Selected Data Point %d", state.selectedDataPoint + 1);
            } else {
                ImGui::TextUnformatted("Select a layer to edit it.");
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Transform")) {
            if (state.selectedImage >= 0 && state.selectedImage < static_cast<int>(state.activeImageCount)) {
                ImageLayer& image = state.imageLayers[state.selectedImage];
                ImGui::Text("TIFF Layer %d: %s", state.selectedImage + 1, image.name.c_str());
                bool changed = ImGui::Checkbox("Visible", &image.visible);
                float position[] = {image.x / voxelSpacingWorldUnits,
                                    image.y / voxelSpacingWorldUnits,
                                    image.z / voxelSpacingWorldUnits};
                if (ImGui::InputFloat3("Position (voxels X, Y, Z)", position, "%.1f")) {
                    image.x = position[0] * voxelSpacingWorldUnits;
                    image.y = position[1] * voxelSpacingWorldUnits;
                    image.z = position[2] * voxelSpacingWorldUnits;
                    changed = true;
                }
                float size[] = {image.sizeX, image.sizeY, image.sizeZ};
                if (ImGui::InputFloat3("Size (voxels X, Y, Z)", size, "%.1f")) {
                    image.sizeX = std::max(0.01f, size[0]);
                    image.sizeY = std::max(1.0f, std::round(size[1]));
                    image.sizeZ = std::max(0.01f, size[2]);
                    changed = true;
                }
                ImGui::SetItemTooltip("Set the TIFF layer's full dimensions in voxel units.");
                ImGui::Text("Pixels: %u x %u", image.pixelWidth, image.pixelHeight);

                ImGui::Separator();

                static float scaleFactor = 1.0f;

                if (ImGui::Button("Depth Transform")) {
                    extractDepth(image, scaleFactor);
                }

                ImGui::SameLine();

                ImGui::SetNextItemWidth(60.0f);
                ImGui::InputFloat("(Scale Factor)", &scaleFactor);

                if (changed) commitHistory(state, "Transform TIFF layer");
            } else if (state.selectedDataPoint >= 0 && state.selectedDataPoint < static_cast<int>(state.dataPoints.size())) {
                DataPoint& point = state.dataPoints[state.selectedDataPoint];
                ImGui::Text("Data Point %d selected", state.selectedDataPoint + 1);
                float position[] = {point.x / voxelSpacingWorldUnits,
                                    point.y / voxelSpacingWorldUnits,
                                    point.z / voxelSpacingWorldUnits};
                if (ImGui::InputFloat3("Position (voxels X, Y, Z)", position, "%.1f")) {
                    point.x = position[0] * voxelSpacingWorldUnits;
                    point.y = position[1] * voxelSpacingWorldUnits;
                    point.z = position[2] * voxelSpacingWorldUnits;
                    commitHistory(state, "Transform Data Point");
                }
                float size[] = {point.halfSizeX * 2.0f / voxelSpacingWorldUnits,
                                point.halfSizeY * 2.0f / voxelSpacingWorldUnits,
                                point.halfSizeZ * 2.0f / voxelSpacingWorldUnits};
                if (ImGui::InputFloat3("Size (voxels X, Y, Z)", size, "%.1f")) {
                    point.halfSizeX = std::max(0.01f, size[0] * 0.5f) * voxelSpacingWorldUnits;
                    point.halfSizeY = std::max(0.01f, size[1] * 0.5f) * voxelSpacingWorldUnits;
                    point.halfSizeZ = std::max(0.01f, size[2] * 0.5f) * voxelSpacingWorldUnits;
                    commitHistory(state, "Resize Data Point");
                }
                if (ImGui::Combo("Value type", &point.valueType, "Float\0Integer\0String\0")) {
                    commitHistory(state, "Change Data Point value type");
                }
                if (point.valueType == 0) {
                    if (ImGui::InputFloat("Value", &point.floatValue, 0.1f, 1.0f, "%.3f")) {
                        commitHistory(state, "Edit Data Point value");
                    }
                } else if (point.valueType == 1) {
                    if (ImGui::InputInt("Value", &point.integerValue)) {
                        commitHistory(state, "Edit Data Point value");
                    }
                } else if (ImGui::InputText("Value", point.stringValue, sizeof(point.stringValue))) {
                    commitHistory(state, "Edit Data Point value");
                }
                ImGui::TextUnformatted("Origin: (0, 0, 0)");
                if (ImGui::Button("Delete Data Point")) {
                    state.dataPoints.erase(state.dataPoints.begin() + state.selectedDataPoint);
                    state.selectedDataPoint = -1;
                    commitHistory(state, "Delete Data Point");
                }

            } else {
                ImGui::TextUnformatted("Select a layer in Layers to transform it.");
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Save")) {
            if (ImGui::Button("Save Project")) {
                static const char filter[] = "OBJ Projects (*.obj)\0*.obj\0\0";
                const std::string path = choosePath(true, filter, "obj");
                if (!path.empty()) writeObj(state, path);
            }
            ImGui::SameLine();
            if (ImGui::Button("Export (HDF5)")) {
                static const char filter[] = "HDF5 Files (*.h5;*.hdf5)\0*.h5;*.hdf5\0\0";
                const std::string path = choosePath(true, filter, "h5");
                if (!path.empty()) exportHdf5(state, path);
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("History")) {
            if (ImGui::Button("Undo") && state.historyIndex > 0) undo(state);
            ImGui::SameLine();
            if (ImGui::Button("Redo") && state.historyIndex + 1 < state.history.size()) redo(state);
            ImGui::BeginChild("ActionHistory", ImVec2(0.0f, 300.0f), true);
            for (std::size_t index = 0; index < state.history.size(); ++index) {
                const std::string label = std::to_string(index + 1) + ". " + state.history[index].action;
                if (ImGui::Selectable(label.c_str(), index == state.historyIndex)) {
                    state.historyIndex = index;
                    state.coalescingAction.clear();
                    restoreSnapshot(state, index);
                    state.previousAction = "Restored: " + state.history[index].action;
                }
            }
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::Separator();
    ImGui::TextWrapped("Previous Action: %s", state.previousAction.c_str());
    ImGui::End();
}

void updateCameraMovement(GLFWwindow* window, AppState& state, float deltaTime) {
    if (ImGui::GetIO().WantCaptureKeyboard) return;

    const float forwardInput = static_cast<float>(glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) -
                               static_cast<float>(glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS);
    const float strafeInput = static_cast<float>(glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) -
                              static_cast<float>(glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS);
    if (forwardInput == 0.0f && strafeInput == 0.0f) return;

    constexpr float pi = 3.14159265f;
    const float yaw = state.yaw * pi / 180.0f;
    const float pitch = state.pitch * pi / 180.0f;
    float moveX = std::sin(yaw) * std::cos(pitch) * forwardInput + std::cos(yaw) * strafeInput;
    float moveY = -std::sin(pitch) * forwardInput;
    float moveZ = -std::cos(yaw) * std::cos(pitch) * forwardInput + std::sin(yaw) * strafeInput;
    const float moveLength = std::sqrt(moveX * moveX + moveY * moveY + moveZ * moveZ);
    moveX /= moveLength;
    moveY /= moveLength;
    moveZ /= moveLength;

    constexpr float moveSpeed = 4.0f;
    const float distance = moveSpeed * deltaTime;
    state.cameraX += moveX * distance;
    state.cameraY += moveY * distance;
    state.cameraZ += moveZ * distance;
}

}  // namespace app
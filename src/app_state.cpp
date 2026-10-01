#include "app_state.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace app {

EditorSnapshot captureSnapshot(const AppState& state, const std::string& action) {
    EditorSnapshot snapshot;
    snapshot.dataPoints = state.dataPoints;
    snapshot.imageCount = state.activeImageCount;
    snapshot.selectedDataPoint = state.selectedDataPoint;
    snapshot.selectedImage = state.selectedImage;
    snapshot.action = action;
    snapshot.imageLayers.reserve(state.activeImageCount);
    for (std::size_t index = 0; index < state.activeImageCount; ++index) {
        const ImageLayer& image = state.imageLayers[index];
        snapshot.imageLayers.push_back({image.x, image.y, image.z, image.sizeX, image.sizeY, image.sizeZ, image.visible});
    }
    return snapshot;
}

void initializeHistory(AppState& state) {
    state.history.clear();
    state.history.push_back(captureSnapshot(state, "Initial state"));
    state.historyIndex = 0;
}

void commitHistory(AppState& state, const std::string& action) {
    if (state.history.empty()) initializeHistory(state);
    if (state.historyIndex + 1 < state.history.size()) {
        state.history.erase(state.history.begin() + static_cast<std::ptrdiff_t>(state.historyIndex + 1), state.history.end());
    }

    EditorSnapshot snapshot = captureSnapshot(state, action);
    const bool canCoalesce = state.historyIndex > 0 && state.historyIndex + 1 == state.history.size() &&
                             state.coalescingAction == action;
    if (canCoalesce) {
        state.history.back() = std::move(snapshot);
    } else {
        state.history.push_back(std::move(snapshot));
        state.historyIndex = state.history.size() - 1;
    }
    state.coalescingAction = action;
    if (state.history.size() > 100) {
        state.history.erase(state.history.begin());
        --state.historyIndex;
    }
    state.previousAction = action;
}

void restoreSnapshot(AppState& state, std::size_t index) {
    const EditorSnapshot& snapshot = state.history[index];
    state.dataPoints = snapshot.dataPoints;
    state.activeImageCount = std::min(snapshot.imageCount, state.imageLayers.size());
    for (std::size_t imageIndex = 0; imageIndex < state.activeImageCount && imageIndex < snapshot.imageLayers.size(); ++imageIndex) {
        ImageLayer& image = state.imageLayers[imageIndex];
        const ImageLayerSnapshot& layer = snapshot.imageLayers[imageIndex];
        image.x = layer.x;
        image.y = layer.y;
        image.z = layer.z;
        image.sizeX = layer.sizeX;
        image.sizeY = layer.sizeY;
        image.sizeZ = layer.sizeZ;
        image.visible = layer.visible;
    }
    state.selectedDataPoint = snapshot.selectedDataPoint;
    state.selectedImage = snapshot.selectedImage;
}

void undo(AppState& state) {
    if (state.historyIndex == 0) return;
    const std::string undoneAction = state.history[state.historyIndex].action;
    --state.historyIndex;
    restoreSnapshot(state, state.historyIndex);
    state.coalescingAction.clear();
    state.previousAction = "Undo: " + undoneAction;
}

void redo(AppState& state) {
    if (state.historyIndex + 1 >= state.history.size()) return;
    ++state.historyIndex;
    restoreSnapshot(state, state.historyIndex);
    state.coalescingAction.clear();
    state.previousAction = "Redo: " + state.history[state.historyIndex].action;
}

int hitDataPoint(const AppState& state, GLFWwindow* window, double cursorX, double cursorY) {
    int width = 0;
    int height = 0;
    glfwGetWindowSize(window, &width, &height);
    if (width <= 0 || height <= 0) return -1;

    constexpr float pi = 3.14159265f;
    const float tangent = std::tan(45.0f * pi / 360.0f);
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const float screenX = static_cast<float>(cursorX) / static_cast<float>(width) * 2.0f - 1.0f;
    const float screenY = 1.0f - static_cast<float>(cursorY) / static_cast<float>(height) * 2.0f;
    const float yaw = state.yaw * pi / 180.0f;
    const float pitch = state.pitch * pi / 180.0f;

    float directionX = screenX * aspect * tangent;
    const float directionY = std::cos(pitch) * screenY * tangent - std::sin(pitch);
    const float rotatedZ = -std::sin(pitch) * screenY * tangent - std::cos(pitch);
    float directionYWorld = directionY;
    float directionZ = std::sin(yaw) * directionX + std::cos(yaw) * rotatedZ;
    directionX = std::cos(yaw) * directionX - std::sin(yaw) * rotatedZ;

    const float directionLength = std::sqrt(directionX * directionX + directionYWorld * directionYWorld + directionZ * directionZ);
    directionX /= directionLength;
    directionYWorld /= directionLength;
    directionZ /= directionLength;

    const float originX = state.cameraX - std::sin(yaw) * std::cos(pitch) * state.distance;
    const float originY = state.cameraY + std::sin(pitch) * state.distance;
    const float originZ = state.cameraZ + std::cos(yaw) * std::cos(pitch) * state.distance;
    const float origins[] = {originX, originY, originZ};
    const float directions[] = {directionX, directionYWorld, directionZ};
    int nearestDataPoint = -1;
    float nearestDistance = std::numeric_limits<float>::max();

    for (int pointIndex = 0; pointIndex < static_cast<int>(state.dataPoints.size()); ++pointIndex) {
        const DataPoint& point = state.dataPoints[pointIndex];
        const float centers[] = {point.x, point.y, point.z};
        const float halfSizes[] = {point.halfSizeX, point.halfSizeY, point.halfSizeZ};
        float nearDistance = 0.0f;
        float farDistance = std::numeric_limits<float>::max();
        bool intersects = true;

        for (int axis = 0; axis < 3; ++axis) {
            const float lower = centers[axis] - halfSizes[axis];
            const float upper = centers[axis] + halfSizes[axis];
            if (std::abs(directions[axis]) < 1.0e-6f) {
                if (origins[axis] < lower || origins[axis] > upper) {
                    intersects = false;
                    break;
                }
                continue;
            }

            float first = (lower - origins[axis]) / directions[axis];
            float second = (upper - origins[axis]) / directions[axis];
            if (first > second) std::swap(first, second);
            nearDistance = std::max(nearDistance, first);
            farDistance = std::min(farDistance, second);
            if (nearDistance > farDistance) {
                intersects = false;
                break;
            }
        }

        if (intersects && farDistance >= 0.0f && nearDistance < nearestDistance) {
            nearestDataPoint = pointIndex;
            nearestDistance = nearDistance;
        }
    }
    return nearestDataPoint;
}

}  // namespace app
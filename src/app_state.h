#pragma once

#include <GLFW/glfw3.h>

#include <cstdint>
#include <string>
#include <vector>

namespace app {

inline constexpr float voxelSpacingWorldUnits = 0.01f;

struct DepthRenderScale {
    float minDepth = 0.0f;
    float maxDepth = 0.0f;
    float uniformScale = 1.0f;
    bool valid = false;
};

struct DepthMap
{
    int width = 0;
    int height = 0;
    std::vector<float> values;
};

struct DataPoint {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float halfSizeX = 0.005f;
    float halfSizeY = 0.005f;
    float halfSizeZ = 0.005f;
    int valueType = 0;
    float floatValue = 0.0f;
    int integerValue = 0;
    char stringValue[128] = "";
};

struct ImageLayer {
    unsigned int texture = 0;
    std::uint32_t pixelWidth = 0;
    std::uint32_t pixelHeight = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float sizeX = 0.0f;
    float sizeY = 1.0f;
    float sizeZ = 0.0f;
    bool visible = true;
    std::vector<std::uint8_t> rgbaPixels;
    std::vector<double> scalarPixels;
    std::string name;
    DepthMap depthMap;
    bool hasDepthMap = false;
};

struct ImageLayerSnapshot {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float sizeX = 0.0f;
    float sizeY = 1.0f;
    float sizeZ = 0.0f;
    bool visible = true;
};

struct EditorSnapshot {
    std::vector<DataPoint> dataPoints;
    std::vector<ImageLayerSnapshot> imageLayers;
    std::size_t imageCount = 0;
    int selectedDataPoint = -1;
    int selectedImage = -1;
    std::string action;
};

struct AppState {
    float yaw = 35.0f;
    float pitch = 25.0f;
    float distance = 7.0f;
    float cameraX = 0.0f;
    float cameraY = 0.0f;
    float cameraZ = 0.0f;
    std::vector<DataPoint> dataPoints{DataPoint{}};
    std::vector<ImageLayer> imageLayers;
    std::size_t activeImageCount = 0;
    int selectedDataPoint = 0;
    int selectedImage = -1;
    std::string previousAction = "Ready";
    std::vector<EditorSnapshot> history;
    std::size_t historyIndex = 0;
    std::string coalescingAction;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    bool orbiting = false;
};

EditorSnapshot captureSnapshot(const AppState& state, const std::string& action);
void initializeHistory(AppState& state);
void commitHistory(AppState& state, const std::string& action);
void restoreSnapshot(AppState& state, std::size_t index);
void undo(AppState& state);
void redo(AppState& state);
int hitDataPoint(const AppState& state, GLFWwindow* window, double cursorX, double cursorY);

}  // namespace app
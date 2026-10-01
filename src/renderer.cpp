#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include "renderer.h"

#include <GL/gl.h>

#include <algorithm>
#include <cmath>

namespace app {
namespace {

void drawDataPoint() {
    constexpr float vertices[8][3] = {
        {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
        {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1}
    };
    constexpr int faces[6][4] = {{0, 1, 2, 3}, {4, 7, 6, 5}, {0, 4, 5, 1},
                                 {1, 5, 6, 2}, {2, 6, 7, 3}, {4, 0, 3, 7}};
    constexpr float normals[6][3] = {{0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, 1.0f},
                                     {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
                                     {0.0f, 1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}};
    glBegin(GL_QUADS);
    for (int face = 0; face < 6; ++face) {
        glNormal3fv(normals[face]);
        glColor3f(0.78f, 0.39f, 0.23f);
        for (int corner = 0; corner < 4; ++corner) {
            glVertex3fv(vertices[faces[face][3 - corner]]);
        }
    }
    glEnd();
}

void drawSelectionOutline() {
    constexpr float size = 1.015f;
    constexpr float corners[8][3] = {
        {-size, -size, -size}, {size, -size, -size}, {size, size, -size}, {-size, size, -size},
        {-size, -size, size}, {size, -size, size}, {size, size, size}, {-size, size, size}
    };
    constexpr int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                  {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_LINE_BIT);
    glDisable(GL_LIGHTING);
    glLineWidth(2.0f);
    glColor3f(1.0f, 0.82f, 0.25f);
    glBegin(GL_LINES);
    for (const auto& edge : edges) {
        glVertex3fv(corners[edge[0]]);
        glVertex3fv(corners[edge[1]]);
    }
    glEnd();
    glPopAttrib();
}

void drawInfiniteGrid(const AppState& state) {
    constexpr int extent = 120;
    constexpr float floorY = -0.01f;
    const float centerX = std::floor(state.cameraX);
    const float centerZ = std::floor(state.cameraZ);

    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT);
    glDisable(GL_LIGHTING);
    glBegin(GL_LINES);
    for (int offset = -extent; offset <= extent; ++offset) {
        const float lineX = centerX + static_cast<float>(offset);
        const float lineZ = centerZ + static_cast<float>(offset);
        const float shadeX = std::fmod(std::abs(lineX), 10.0f) < 0.01f ? 0.30f : 0.17f;
        const float shadeZ = std::fmod(std::abs(lineZ), 10.0f) < 0.01f ? 0.30f : 0.17f;
        if (std::abs(lineX) > 0.01f) {
            glColor3f(shadeX, shadeX, shadeX);
            glVertex3f(lineX, floorY, centerZ - static_cast<float>(extent));
            glVertex3f(lineX, floorY, centerZ + static_cast<float>(extent));
        }
        if (std::abs(lineZ) > 0.01f) {
            glColor3f(shadeZ, shadeZ, shadeZ);
            glVertex3f(centerX - static_cast<float>(extent), floorY, lineZ);
            glVertex3f(centerX + static_cast<float>(extent), floorY, lineZ);
        }
    }
    glColor3f(0.78f, 0.24f, 0.20f);
    glVertex3f(-100000.0f, floorY, 0.0f);
    glVertex3f(100000.0f, floorY, 0.0f);
    glColor3f(0.20f, 0.48f, 0.82f);
    glVertex3f(0.0f, floorY, -100000.0f);
    glVertex3f(0.0f, floorY, 100000.0f);
    glEnd();
    glPopAttrib();
}

void drawImageLayers(const AppState& state) {
    glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT | GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    for (std::size_t index = 0; index < state.activeImageCount; ++index) {
        const ImageLayer& image = state.imageLayers[index];
        if (!image.visible) continue;
        const float halfWidth = image.sizeX * voxelSpacingWorldUnits * 0.5f;
        const float halfDepth = image.sizeZ * voxelSpacingWorldUnits * 0.5f;
        glBindTexture(GL_TEXTURE_2D, image.texture);
        const int ySizeVoxels = static_cast<int>(std::max(1.0f, std::round(image.sizeY)));
        for (int yVoxel = 0; yVoxel < ySizeVoxels; ++yVoxel) {
            const float layerY = image.y + static_cast<float>(yVoxel) * voxelSpacingWorldUnits;
            glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f); glVertex3f(image.x - halfWidth, layerY, image.z - halfDepth);
            glTexCoord2f(1.0f, 0.0f); glVertex3f(image.x + halfWidth, layerY, image.z - halfDepth);
            glTexCoord2f(1.0f, 1.0f); glVertex3f(image.x + halfWidth, layerY, image.z + halfDepth);
            glTexCoord2f(0.0f, 1.0f); glVertex3f(image.x - halfWidth, layerY, image.z + halfDepth);
            glEnd();
        }
    }
    glPopAttrib();
}

void setProjection(int width, int height) {
    const float aspect = static_cast<float>(width) / static_cast<float>(height > 0 ? height : 1);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    constexpr float nearPlane = 0.001f;
    constexpr float farPlane = 100.0f;
    constexpr float fieldOfView = 45.0f * 3.14159265f / 180.0f;
    const float top = nearPlane * std::tan(fieldOfView / 2.0f);
    const float right = top * aspect;
    glFrustum(-right, right, -top, top, nearPlane, farPlane);
    glMatrixMode(GL_MODELVIEW);
}

}  // namespace

void initializeRenderer() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glEnable(GL_NORMALIZE);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    constexpr GLfloat ambientLight[] = {0.24f, 0.24f, 0.24f, 1.0f};
    constexpr GLfloat diffuseLight[] = {0.9f, 0.9f, 0.9f, 1.0f};
    constexpr GLfloat lightDirection[] = {-0.5f, 0.8f, 0.6f, 0.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambientLight);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glLightfv(GL_LIGHT0, GL_POSITION, lightDirection);
}

void renderScene(const AppState& state, int width, int height) {
    glViewport(0, 0, width, height);
    setProjection(width, height);
    glClearColor(0.055f, 0.07f, 0.09f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -state.distance);
    glRotatef(state.pitch, 1.0f, 0.0f, 0.0f);
    glRotatef(state.yaw, 0.0f, 1.0f, 0.0f);
    glTranslatef(-state.cameraX, -state.cameraY, -state.cameraZ);

    drawInfiniteGrid(state);
    drawImageLayers(state);
    for (std::size_t index = 0; index < state.dataPoints.size(); ++index) {
        const DataPoint& point = state.dataPoints[index];
        glPushMatrix();
        glTranslatef(point.x, point.y, point.z);
        glScalef(point.halfSizeX, point.halfSizeY, point.halfSizeZ);
        drawDataPoint();
        if (static_cast<int>(index) == state.selectedDataPoint) drawSelectionOutline();
        glPopMatrix();
    }
}

}  // namespace app
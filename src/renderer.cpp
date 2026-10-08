#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include "renderer.h"

#include <GL/gl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace app {
namespace {

constexpr int maxMeshSamples = 768;

struct DepthMesh {
    GLuint list = 0;
    bool built = false;
    const float* data = nullptr;
    std::uint64_t hash = 0;
    std::array<float, 8> params{};
};

std::vector<DepthMesh> depthMeshes;

float layerHeight(const ImageLayer& image) {
    return std::max(1.0f, std::round(image.sizeY)) * voxelSpacingWorldUnits;
}

std::uint64_t sampleHash(const std::vector<float>& values) {
    std::uint64_t hash = values.size();
    for (std::size_t k = 0; k < 256 && !values.empty(); ++k) {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &values[(values.size() - 1) * k / 255], sizeof(bits));
        hash = (hash ^ bits) * 1099511628211ull;
    }
    return hash;
}

void emitDepthMesh(const ImageLayer& image) {
    const DepthMap& depth = image.depthMap;
    const float width = image.sizeX * voxelSpacingWorldUnits;
    const float length = image.sizeZ * voxelSpacingWorldUnits;
    const float height = layerHeight(image);
    const float invW = 1.0f / static_cast<float>(depth.width - 1);
    const float invH = 1.0f / static_cast<float>(depth.height - 1);
    const int step = std::max(1, std::max(depth.width, depth.height) / maxMeshSamples);

    auto at = [&](int x, int z) { return depth.values[static_cast<std::size_t>(z) * depth.width + x]; };
    auto vertex = [&](int x, int z) {
        const float u = static_cast<float>(x) * invW;
        const float v = static_cast<float>(z) * invH;
        glTexCoord2f(u, v);
        glVertex3f(image.x + (u - 0.5f) * width, image.y + at(x, z) * height, image.z + (v - 0.5f) * length);
    };

    glBegin(GL_TRIANGLES);
    for (int z = 0; z < depth.height - 1; z += step) {
        const int zn = std::min(z + step, depth.height - 1);
        for (int x = 0; x < depth.width - 1; x += step) {
            const int xn = std::min(x + step, depth.width - 1);
            if (!std::isfinite(at(x, z)) || !std::isfinite(at(xn, z)) ||
                !std::isfinite(at(x, zn)) || !std::isfinite(at(xn, zn))) {
                continue;
            }
            vertex(x, z);
            vertex(xn, z);
            vertex(xn, zn);
            vertex(x, z);
            vertex(xn, zn);
            vertex(x, zn);
        }
    }
    glEnd();
}

bool drawDepthLayer(const ImageLayer& image, DepthMesh& mesh) {
    const DepthMap& depth = image.depthMap;
    if (depth.width < 2 || depth.height < 2 ||
        depth.values.size() < static_cast<std::size_t>(depth.width) * static_cast<std::size_t>(depth.height)) {
        return false;
    }

    const std::uint64_t hash = sampleHash(depth.values);
    const std::array<float, 8> params = {image.x, image.y, image.z, image.sizeX, image.sizeY, image.sizeZ,
                                         static_cast<float>(depth.width), static_cast<float>(depth.height)};
    if (!mesh.built || mesh.data != depth.values.data() || mesh.hash != hash || mesh.params != params) {
        if (mesh.list == 0) mesh.list = glGenLists(1);
        glNewList(mesh.list, GL_COMPILE);
        emitDepthMesh(image);
        glEndList();
        mesh.built = true;
        mesh.data = depth.values.data();
        mesh.hash = hash;
        mesh.params = params;
    }
    glCallList(mesh.list);
    return true;
}

void drawImagePlane(const ImageLayer& image) {
    const float halfX = image.sizeX * voxelSpacingWorldUnits * 0.5f;
    const float halfZ = image.sizeZ * voxelSpacingWorldUnits * 0.5f;
    const float x0 = image.x - halfX, x1 = image.x + halfX;
    const float z0 = image.z - halfZ, z1 = image.z + halfZ;
    const float y0 = image.y, y1 = image.y + layerHeight(image);

    auto vertex = [](float u, float v, float x, float y, float z) {
        glTexCoord2f(u, v);
        glVertex3f(x, y, z);
    };

    glBegin(GL_QUADS);
    for (float y : {y0, y1}) {
        vertex(0, 0, x0, y, z0);
        vertex(1, 0, x1, y, z0);
        vertex(1, 1, x1, y, z1);
        vertex(0, 1, x0, y, z1);
    }
    for (float v : {0.0f, 1.0f}) {
        const float z = v == 0.0f ? z0 : z1;
        vertex(0, v, x0, y0, z);
        vertex(1, v, x1, y0, z);
        vertex(1, v, x1, y1, z);
        vertex(0, v, x0, y1, z);
    }
    for (float u : {0.0f, 1.0f}) {
        const float x = u == 0.0f ? x0 : x1;
        vertex(u, 0, x, y0, z0);
        vertex(u, 1, x, y0, z1);
        vertex(u, 1, x, y1, z1);
        vertex(u, 0, x, y1, z0);
    }
    glEnd();
}

void drawDataPoint() {
    constexpr float vertices[8][3] = {
        {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
        {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1}
    };
    constexpr int faces[6][4] = {{0, 1, 2, 3}, {4, 7, 6, 5}, {0, 4, 5, 1},
                                 {1, 5, 6, 2}, {2, 6, 7, 3}, {4, 0, 3, 7}};
    constexpr float normals[6][3] = {{0, 0, -1}, {0, 0, 1}, {0, -1, 0}, {1, 0, 0}, {0, 1, 0}, {-1, 0, 0}};
    glBegin(GL_QUADS);
    for (int face = 0; face < 6; ++face) {
        glNormal3fv(normals[face]);
        glColor3f(0.78f, 0.39f, 0.23f);
        for (int corner = 3; corner >= 0; --corner) glVertex3fv(vertices[faces[face][corner]]);
    }
    glEnd();
}

void drawSelectionOutline() {
    constexpr float s = 1.015f;
    constexpr float corners[8][3] = {
        {-s, -s, -s}, {s, -s, -s}, {s, s, -s}, {-s, s, -s},
        {-s, -s, s},  {s, -s, s},  {s, s, s},  {-s, s, s}
    };
    constexpr int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                  {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_LINE_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
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
    auto shade = [](float v) { return std::fmod(std::abs(v), 10.0f) < 0.01f ? 0.30f : 0.17f; };

    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT);
    glDisable(GL_LIGHTING);
    glBegin(GL_LINES);
    for (int offset = -extent; offset <= extent; ++offset) {
        const float lineX = centerX + static_cast<float>(offset);
        const float lineZ = centerZ + static_cast<float>(offset);
        if (std::abs(lineX) > 0.01f) {
            glColor3f(shade(lineX), shade(lineX), shade(lineX));
            glVertex3f(lineX, floorY, centerZ - extent);
            glVertex3f(lineX, floorY, centerZ + extent);
        }
        if (std::abs(lineZ) > 0.01f) {
            glColor3f(shade(lineZ), shade(lineZ), shade(lineZ));
            glVertex3f(centerX - extent, floorY, lineZ);
            glVertex3f(centerX + extent, floorY, lineZ);
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
    while (depthMeshes.size() > state.activeImageCount) {
        if (depthMeshes.back().list != 0) glDeleteLists(depthMeshes.back().list, 1);
        depthMeshes.pop_back();
    }
    depthMeshes.resize(state.activeImageCount);

    glPushAttrib(GL_ENABLE_BIT | GL_TEXTURE_BIT | GL_CURRENT_BIT | GL_COLOR_BUFFER_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glEnable(GL_ALPHA_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glAlphaFunc(GL_GREATER, 0.0f);

    for (std::size_t index = 0; index < state.activeImageCount; ++index) {
        const ImageLayer& image = state.imageLayers[index];
        if (!image.visible || image.texture == 0) continue;

        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glBindTexture(GL_TEXTURE_2D, image.texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        if (!image.hasDepthMap || !drawDepthLayer(image, depthMeshes[index])) drawImagePlane(image);

        if (static_cast<int>(index) == state.selectedImage) {
            const float height = layerHeight(image);
            glPushMatrix();
            glTranslatef(image.x, image.y + height * 0.5f, image.z);
            glScalef(image.sizeX * voxelSpacingWorldUnits * 0.5f, height * 0.5f, image.sizeZ * voxelSpacingWorldUnits * 0.5f);
            drawSelectionOutline();
            glPopMatrix();
        }
    }

    glPopAttrib();
}

void setProjection(int width, int height) {
    const float aspect = static_cast<float>(width) / static_cast<float>(height > 0 ? height : 1);
    constexpr float nearPlane = 0.001f;
    constexpr float farPlane = 1000.0f;
    constexpr float fieldOfView = 45.0f * 3.14159265f / 180.0f;
    const float top = nearPlane * std::tan(fieldOfView / 2.0f);
    const float right = top * aspect;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
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
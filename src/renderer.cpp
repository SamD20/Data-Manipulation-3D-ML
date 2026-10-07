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

constexpr float depthMapMaxHeight = 1.0f;

struct DepthRenderScale
{
    float minDepth = 0.0f;
    float maxDepth = 0.0f;
    float yScale = 0.0f;
    bool valid = false;
};

DepthRenderScale calculateDepthRenderScale(const AppState& state)
{
    DepthRenderScale result;

    bool foundDepth = false;

    for (std::size_t layerIndex = 0;
         layerIndex < state.activeImageCount;
         ++layerIndex)
    {
        const ImageLayer& image = state.imageLayers[layerIndex];

        if (!image.hasDepthMap)
            continue;

        for (float value : image.depthMap.values)
        {
            if (!std::isfinite(value))
                continue;

            if (!foundDepth)
            {
                result.minDepth = value;
                result.maxDepth = value;
                foundDepth = true;
            }
            else
            {
                result.minDepth =
                    std::min(result.minDepth, value);

                result.maxDepth =
                    std::max(result.maxDepth, value);
            }
        }
    }

    if (!foundDepth)
        return result;

    result.valid = true;

    const float depthRange =
        result.maxDepth - result.minDepth;

    if (depthRange > 0.0f)
    {
        result.yScale =
            depthMapMaxHeight / depthRange;
    }

    return result;
}

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

void drawDepthImage(const ImageLayer& image, const DepthRenderScale& scale)
{
    const DepthMap& depth = image.depthMap;

    if (depth.width <= 0 ||
        depth.height <= 0 ||
        depth.values.empty())
    {
        return;
    }

    const float width =
        image.sizeX * voxelSpacingWorldUnits;

    const float depthSize =
        image.sizeZ * voxelSpacingWorldUnits;

    const float xSpacing =
        width /
        static_cast<float>(std::max(1, depth.width - 1));

    const float zSpacing =
        depthSize /
        static_cast<float>(std::max(1, depth.height - 1));

    const float depthRange =
        scale.maxDepth - scale.minDepth;

    const float yScale =
        std::max(1.0f, image.sizeY);

    glPushAttrib(
        GL_ENABLE_BIT |
        GL_TEXTURE_BIT |
        GL_CURRENT_BIT |
        GL_COLOR_BUFFER_BIT
    );

    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindTexture(GL_TEXTURE_2D, image.texture);

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    for (int z = 0; z < depth.height - 1; ++z)
    {
        for (int x = 0; x < depth.width - 1; ++x)
        {
            const int i00 =
                z * depth.width + x;

            const int i10 =
                i00 + 1;

            const int i01 =
                (z + 1) * depth.width + x;

            const int i11 =
                i01 + 1;

            const float x0 =
                image.x +
                (static_cast<float>(x) -
                 static_cast<float>(depth.width - 1) * 0.5f) *
                xSpacing;

            const float x1 =
                image.x +
                (static_cast<float>(x + 1) -
                 static_cast<float>(depth.width - 1) * 0.5f) *
                xSpacing;

            const float z0 =
                image.z +
                (static_cast<float>(z) -
                 static_cast<float>(depth.height - 1) * 0.5f) *
                zSpacing;

            const float z1 =
                image.z +
                (static_cast<float>(z + 1) -
                 static_cast<float>(depth.height - 1) * 0.5f) *
                zSpacing;

            float d00 = 0.0f;
            float d10 = 0.0f;
            float d01 = 0.0f;
            float d11 = 0.0f;

            if (depthRange > 0.0f)
            {
                d00 =
                    (depth.values[i00] - scale.minDepth) /
                    depthRange;

                d10 =
                    (depth.values[i10] - scale.minDepth) /
                    depthRange;

                d01 =
                    (depth.values[i01] - scale.minDepth) /
                    depthRange;

                d11 =
                    (depth.values[i11] - scale.minDepth) /
                    depthRange;
            }

            d00 = std::clamp(d00, 0.0f, 1.0f);
            d10 = std::clamp(d10, 0.0f, 1.0f);
            d01 = std::clamp(d01, 0.0f, 1.0f);
            d11 = std::clamp(d11, 0.0f, 1.0f);

            const float y00 =
                image.y +
                d00 *
                depthMapMaxHeight *
                yScale;

            const float y10 =
                image.y +
                d10 *
                depthMapMaxHeight *
                yScale;

            const float y01 =
                image.y +
                d01 *
                depthMapMaxHeight *
                yScale;

            const float y11 =
                image.y +
                d11 *
                depthMapMaxHeight *
                yScale;

            const float u0 =
                static_cast<float>(x) /
                static_cast<float>(depth.width - 1);

            const float u1 =
                static_cast<float>(x + 1) /
                static_cast<float>(depth.width - 1);

            const float v0 =
                static_cast<float>(z) /
                static_cast<float>(depth.height - 1);

            const float v1 =
                static_cast<float>(z + 1) /
                static_cast<float>(depth.height - 1);

            glBegin(GL_TRIANGLES);

            glTexCoord2f(u0, v0);
            glVertex3f(x0, y00, z0);

            glTexCoord2f(u1, v0);
            glVertex3f(x1, y10, z0);

            glTexCoord2f(u1, v1);
            glVertex3f(x1, y11, z1);

            glTexCoord2f(u0, v0);
            glVertex3f(x0, y00, z0);

            glTexCoord2f(u1, v1);
            glVertex3f(x1, y11, z1);

            glTexCoord2f(u0, v1);
            glVertex3f(x0, y01, z1);

            glEnd();
        }
    }

    glPopAttrib();
}

void drawImageLayers(const AppState& state)
{
    const DepthRenderScale depthScale =
        calculateDepthRenderScale(state);

    glPushAttrib(
        GL_ENABLE_BIT |
        GL_TEXTURE_BIT |
        GL_CURRENT_BIT |
        GL_COLOR_BUFFER_BIT
    );

    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);

    glBlendFunc(
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA
    );

    for (std::size_t index = 0;
         index < state.activeImageCount;
         ++index)
    {
        const ImageLayer& image =
            state.imageLayers[index];

        if (!image.visible)
            continue;

        if (image.hasDepthMap && depthScale.valid)
        {
            drawDepthImage(
                image,
                depthScale
            );

            continue;
        }

        if (image.pixelWidth == 0 ||
            image.pixelHeight == 0 ||
            image.rgbaPixels.empty())
        {
            continue;
        }

        const float voxelSize =
            voxelSpacingWorldUnits;

        const int ySizeVoxels =
            static_cast<int>(
                std::max(
                    1.0f,
                    std::round(image.sizeY)
                )
            );

        const float startX =
            image.x -
            static_cast<float>(image.pixelWidth) *
            voxelSize * 0.5f;

        const float startZ =
            image.z -
            static_cast<float>(image.pixelHeight) *
            voxelSize * 0.5f;

        glBegin(GL_QUADS);

        for (std::uint32_t z = 0;
             z < image.pixelHeight;
             ++z)
        {
            for (std::uint32_t x = 0;
                 x < image.pixelWidth;
                 ++x)
            {
                const std::size_t pixelIndex =
                    (
                        static_cast<std::size_t>(z) *
                        image.pixelWidth +
                        x
                    ) * 4;

                if (pixelIndex + 3 >=
                    image.rgbaPixels.size())
                {
                    continue;
                }

                const float r =
                    static_cast<float>(
                        image.rgbaPixels[pixelIndex]
                    ) / 255.0f;

                const float g =
                    static_cast<float>(
                        image.rgbaPixels[pixelIndex + 1]
                    ) / 255.0f;

                const float b =
                    static_cast<float>(
                        image.rgbaPixels[pixelIndex + 2]
                    ) / 255.0f;

                const float a =
                    static_cast<float>(
                        image.rgbaPixels[pixelIndex + 3]
                    ) / 255.0f;

                glColor4f(r, g, b, a);

                const float minX =
                    startX +
                    static_cast<float>(x) *
                    voxelSize;

                const float maxX =
                    minX + voxelSize;

                const float minZ =
                    startZ +
                    static_cast<float>(z) *
                    voxelSize;

                const float maxZ =
                    minZ + voxelSize;

                for (int yVoxel = 0;
                     yVoxel < ySizeVoxels;
                     ++yVoxel)
                {
                    const float minY =
                        image.y +
                        static_cast<float>(yVoxel) *
                        voxelSize;

                    const float maxY =
                        minY + voxelSize;

                    if (yVoxel == 0)
                    {
                        glVertex3f(minX, minY, minZ);
                        glVertex3f(maxX, minY, minZ);
                        glVertex3f(maxX, minY, maxZ);
                        glVertex3f(minX, minY, maxZ);
                    }

                    if (yVoxel == ySizeVoxels - 1)
                    {
                        glVertex3f(minX, maxY, minZ);
                        glVertex3f(minX, maxY, maxZ);
                        glVertex3f(maxX, maxY, maxZ);
                        glVertex3f(maxX, maxY, minZ);
                    }

                    if (x == 0)
                    {
                        glVertex3f(minX, minY, minZ);
                        glVertex3f(minX, minY, maxZ);
                        glVertex3f(minX, maxY, maxZ);
                        glVertex3f(minX, maxY, minZ);
                    }

                    if (x == image.pixelWidth - 1)
                    {
                        glVertex3f(maxX, minY, minZ);
                        glVertex3f(maxX, maxY, minZ);
                        glVertex3f(maxX, maxY, maxZ);
                        glVertex3f(maxX, minY, maxZ);
                    }

                    if (z == 0)
                    {
                        glVertex3f(minX, minY, minZ);
                        glVertex3f(minX, maxY, minZ);
                        glVertex3f(maxX, maxY, minZ);
                        glVertex3f(maxX, minY, minZ);
                    }

                    if (z == image.pixelHeight - 1)
                    {
                        glVertex3f(minX, minY, maxZ);
                        glVertex3f(maxX, minY, maxZ);
                        glVertex3f(maxX, maxY, maxZ);
                        glVertex3f(minX, maxY, maxZ);
                    }
                }
            }
        }

        glEnd();
    }

    glPopAttrib();
}

void setProjection(int width, int height) {
    const float aspect = static_cast<float>(width) / static_cast<float>(height > 0 ? height : 1);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    constexpr float nearPlane = 0.001f;
    constexpr float farPlane = 1000.0f;
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
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <commdlg.h>
#include <wincodec.h>
#endif

#include "file_io.h"

#include <GL/gl.h>
#include <hdf5.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <utility>

namespace app {

std::string choosePath(bool save, const char* filter, const char* extension) {
#ifdef _WIN32
    std::array<char, 32768> path{};
    OPENFILENAMEA dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.lpstrDefExt = extension;
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (save) {
        dialog.Flags |= OFN_OVERWRITEPROMPT;
        if (GetSaveFileNameA(&dialog)) return path.data();
    } else {
        dialog.Flags |= OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
        if (GetOpenFileNameA(&dialog)) return path.data();
    }
#else
    (void)save;
    (void)filter;
    (void)extension;
#endif
    return {};
}

std::vector<std::string> chooseMultiplePaths(const char* filter) {
#ifdef _WIN32
    std::array<char, 32768> buffer{};
    OPENFILENAMEA dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.Flags = OFN_EXPLORER | OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST |
                   OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameA(&dialog)) return {};

    std::vector<std::string> paths;
    const char* cursor = buffer.data();
    const std::string first = cursor;
    cursor += first.size() + 1;
    if (*cursor == '\0') return {first};
    while (*cursor != '\0') {
        paths.push_back((std::filesystem::path(first) / cursor).string());
        cursor += std::char_traits<char>::length(cursor) + 1;
    }
    std::sort(paths.begin(), paths.end());
    return paths;
#else
    (void)filter;
    return {};
#endif
}

bool loadTiff(AppState& state, const std::string& path) {
#ifdef _WIN32
    const int wideLength = MultiByteToWideChar(CP_ACP, 0, path.c_str(), -1, nullptr, 0);
    if (wideLength <= 0) {
        state.previousAction = "TIFF import failed: invalid path";
        return false;
    }
    std::wstring widePath(static_cast<std::size_t>(wideLength), L'\0');
    MultiByteToWideChar(CP_ACP, 0, path.c_str(), -1, widePath.data(), wideLength);

    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    HRESULT result = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                      __uuidof(IWICImagingFactory), reinterpret_cast<void**>(&factory));
    if (SUCCEEDED(result)) result = factory->CreateDecoderFromFilename(widePath.c_str(), nullptr, GENERIC_READ,
                                                                       WICDecodeMetadataCacheOnLoad, &decoder);
    if (SUCCEEDED(result)) result = decoder->GetFrame(0, &frame);
    if (SUCCEEDED(result)) result = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(result)) result = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
                                                          WICBitmapDitherTypeNone, nullptr, 0.0,
                                                          WICBitmapPaletteTypeCustom);

    UINT width = 0;
    UINT height = 0;
    if (SUCCEEDED(result)) result = converter->GetSize(&width, &height);
    bool mismatchedDimensions = false;
    if (SUCCEEDED(result) && state.activeImageCount > 0 &&
        (width != state.imageLayers.front().pixelWidth || height != state.imageLayers.front().pixelHeight)) {
        result = E_INVALIDARG;
        mismatchedDimensions = true;
        state.previousAction = "TIFF import failed: slice dimensions do not match";
    }
    if (SUCCEEDED(result)) {
        const std::size_t rowBytes = static_cast<std::size_t>(width) * 4;
        if (width == 0 || height == 0 || width > static_cast<UINT>(std::numeric_limits<GLsizei>::max()) ||
            height > static_cast<UINT>(std::numeric_limits<GLsizei>::max()) || rowBytes > UINT_MAX ||
            static_cast<std::size_t>(height) > UINT_MAX / rowBytes) {
            result = E_OUTOFMEMORY;
        }
    }

    std::vector<std::uint8_t> pixels;
    if (SUCCEEDED(result)) {
        pixels.resize(static_cast<std::size_t>(width) * height * 4);
        result = converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(pixels.size()), pixels.data());
    }

    std::vector<std::uint16_t> scalarPixels;
    if (SUCCEEDED(result)) scalarPixels.resize(static_cast<std::size_t>(width) * height);
    IWICFormatConverter* grayscaleConverter = nullptr;
    HRESULT grayscaleResult = FAILED(result) ? result : factory->CreateFormatConverter(&grayscaleConverter);
    if (SUCCEEDED(grayscaleResult)) {
        grayscaleResult = grayscaleConverter->Initialize(frame, GUID_WICPixelFormat16bppGray,
                                                         WICBitmapDitherTypeNone, nullptr, 0.0,
                                                         WICBitmapPaletteTypeCustom);
    }
    if (SUCCEEDED(grayscaleResult)) {
        const UINT grayscaleStride = width * static_cast<UINT>(sizeof(std::uint16_t));
        const UINT grayscaleSize = static_cast<UINT>(scalarPixels.size() * sizeof(std::uint16_t));
        grayscaleResult = grayscaleConverter->CopyPixels(nullptr, grayscaleStride, grayscaleSize,
                                                           reinterpret_cast<BYTE*>(scalarPixels.data()));
    }
    if (grayscaleConverter) grayscaleConverter->Release();
    if (SUCCEEDED(result) && FAILED(grayscaleResult)) {
        for (std::size_t pixelIndex = 0; pixelIndex < scalarPixels.size(); ++pixelIndex) {
            const std::size_t colorIndex = pixelIndex * 4;
            const unsigned int red = pixels[colorIndex];
            const unsigned int green = pixels[colorIndex + 1];
            const unsigned int blue = pixels[colorIndex + 2];
            scalarPixels[pixelIndex] = static_cast<std::uint16_t>(((red * 299 + green * 587 + blue * 114) * 257) / 1000);
        }
    }

    if (converter) converter->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (factory) factory->Release();
    if (FAILED(result)) {
        if (!mismatchedDimensions) state.previousAction = "TIFF import failed: could not decode image";
        return false;
    }

    ImageLayer image;
    image.pixelWidth = width;
    image.pixelHeight = height;
    image.sizeX = static_cast<float>(width);
    image.sizeY = 1.0f;
    image.sizeZ = static_cast<float>(height);
    for (std::size_t index = 0; index < state.activeImageCount; ++index) {
        const ImageLayer& previousLayer = state.imageLayers[index];
        image.y = std::max(image.y, previousLayer.y + previousLayer.sizeY * voxelSpacingWorldUnits);
    }
    image.rgbaPixels = std::move(pixels);
    image.scalarPixels = std::move(scalarPixels);
    image.name = path.substr(path.find_last_of("\\/") == std::string::npos ? 0 : path.find_last_of("\\/") + 1);

    if (state.imageLayers.size() > state.activeImageCount) {
        for (std::size_t index = state.activeImageCount; index < state.imageLayers.size(); ++index) {
            if (state.imageLayers[index].texture != 0) glDeleteTextures(1, &state.imageLayers[index].texture);
        }
        state.imageLayers.resize(state.activeImageCount);
    }
    glGenTextures(1, &image.texture);
    glBindTexture(GL_TEXTURE_2D, image.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, image.rgbaPixels.data());
    if (glGetError() != GL_NO_ERROR) {
        glDeleteTextures(1, &image.texture);
        state.previousAction = "TIFF import failed: could not upload image";
        return false;
    }

    state.imageLayers.push_back(std::move(image));
    state.activeImageCount = state.imageLayers.size();
    state.selectedImage = static_cast<int>(state.activeImageCount) - 1;
    state.selectedDataPoint = -1;
    state.previousAction = "Imported TIFF layer";
    return true;
#else
    (void)path;
    state.previousAction = "TIFF import is currently supported on Windows";
    return false;
#endif
}

bool loadObj(AppState& state, const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        state.previousAction = "Open project failed";
        return false;
    }

    std::vector<std::vector<std::array<float, 3>>> groups;
    std::vector<std::array<float, 3>> vertices;
    bool voxelCoordinates = false;
    std::string line;
    while (std::getline(input, line)) {
        if (line == "# opengl_data_app_units voxels") {
            voxelCoordinates = true;
            continue;
        }
        std::istringstream parser(line);
        std::string kind;
        parser >> kind;
        if (kind == "o" || kind == "g") {
            if (!vertices.empty()) {
                groups.push_back(std::move(vertices));
                vertices.clear();
            }
        } else if (kind == "v") {
            std::array<float, 3> vertex{};
            if (parser >> vertex[0] >> vertex[1] >> vertex[2]) vertices.push_back(vertex);
        }
    }
    if (!vertices.empty()) groups.push_back(std::move(vertices));
    if (groups.empty()) {
        state.previousAction = "Open project failed: no Data Points found";
        return false;
    }

    std::vector<DataPoint> loadedDataPoints;
    for (const auto& group : groups) {
        if (group.size() != 8) {
            state.previousAction = "Open project failed: invalid Data Point geometry";
            return false;
        }
        DataPoint point;
        for (int axis = 0; axis < 3; ++axis) {
            float minimum = group.front()[axis];
            float maximum = minimum;
            for (const auto& vertex : group) {
                minimum = std::min(minimum, vertex[axis]);
                maximum = std::max(maximum, vertex[axis]);
            }
            const float extent = (maximum - minimum) * 0.5f;
            if (extent <= 0.0f) {
                state.previousAction = "Open project failed: degenerate Data Point";
                return false;
            }
            const float center = (maximum + minimum) * 0.5f;
            const float unitScale = voxelCoordinates ? voxelSpacingWorldUnits : 1.0f;
            if (axis == 0) { point.x = center * unitScale; point.halfSizeX = extent * unitScale; }
            if (axis == 1) { point.y = center * unitScale; point.halfSizeY = extent * unitScale; }
            if (axis == 2) { point.z = center * unitScale; point.halfSizeZ = extent * unitScale; }
        }
        loadedDataPoints.push_back(point);
    }

    state.dataPoints = std::move(loadedDataPoints);
    state.selectedDataPoint = 0;
    state.selectedImage = -1;
    state.previousAction = "Opened Data Point project";
    return true;
}

bool writeObj(AppState& state, const std::string& path) {
    if (state.dataPoints.empty()) {
        state.previousAction = "Save failed: no Data Points";
        return false;
    }
    std::ofstream output(path);
    if (!output) {
        state.previousAction = "Save project failed";
        return false;
    }

    output << "# opengl_data_app_units voxels\n";
    constexpr float vertices[8][3] = {
        {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
        {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1}
    };
    constexpr int faces[6][4] = {{1, 2, 3, 4}, {5, 8, 7, 6}, {1, 5, 6, 2},
                                 {2, 6, 7, 3}, {3, 7, 8, 4}, {5, 1, 4, 8}};
    for (std::size_t pointIndex = 0; pointIndex < state.dataPoints.size(); ++pointIndex) {
        const DataPoint& point = state.dataPoints[pointIndex];
        output << "o Data_Point_" << pointIndex + 1 << '\n';
        for (const auto& vertex : vertices) {
                 output << "v " << (vertex[0] * point.halfSizeX + point.x) / voxelSpacingWorldUnits << ' '
                     << (vertex[1] * point.halfSizeY + point.y) / voxelSpacingWorldUnits << ' '
                     << (vertex[2] * point.halfSizeZ + point.z) / voxelSpacingWorldUnits << '\n';
        }
        const std::size_t firstVertex = pointIndex * 8;
        for (const auto& face : faces) {
            output << "f";
            for (int vertexIndex : face) output << ' ' << firstVertex + vertexIndex;
            output << '\n';
        }
    }
    if (!output) {
        state.previousAction = "Save project failed while writing";
        return false;
    }
    state.previousAction = "Saved project with " + std::to_string(state.dataPoints.size()) + " Data Point(s)";
    return true;
}

namespace {
bool writeDataset(hid_t file, const char* name, hid_t type, const void* values,
                  const hsize_t* dimensions, int rank) {
    const hid_t space = H5Screate_simple(rank, dimensions, nullptr);
    if (space < 0) return false;
    const hid_t dataset = H5Dcreate2(file, name, type, space, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
    if (dataset < 0) {
        H5Sclose(space);
        return false;
    }
    const bool success = H5Dwrite(dataset, type, H5S_ALL, H5S_ALL, H5P_DEFAULT, values) >= 0;
    H5Dclose(dataset);
    H5Sclose(space);
    return success;
}
}

bool exportHdf5(AppState& state, const std::string& path) {
    if (state.dataPoints.empty() && state.activeImageCount == 0) {
        state.previousAction = "Export failed: no Data Points or TIFF slices";
        return false;
    }

    std::vector<float> positions;
    std::vector<float> sizes;
    std::vector<int> valueTypes;
    std::vector<float> floatValues;
    std::vector<int> integerValues;
    std::vector<char> stringValues;
    for (const DataPoint& point : state.dataPoints) {
        positions.insert(positions.end(), {point.x / voxelSpacingWorldUnits,
                           point.y / voxelSpacingWorldUnits,
                           point.z / voxelSpacingWorldUnits});
        sizes.insert(sizes.end(), {point.halfSizeX * 2.0f / voxelSpacingWorldUnits,
                       point.halfSizeY * 2.0f / voxelSpacingWorldUnits,
                       point.halfSizeZ * 2.0f / voxelSpacingWorldUnits});
        valueTypes.push_back(point.valueType);
        floatValues.push_back(point.floatValue);
        integerValues.push_back(point.integerValue);
        stringValues.insert(stringValues.end(), point.stringValue, point.stringValue + sizeof(point.stringValue));
    }

    std::vector<std::uint16_t> volume;
    std::vector<float> layerTransforms;
    if (state.activeImageCount > 0) {
        const ImageLayer& firstImage = state.imageLayers.front();
        const std::size_t sliceSize = static_cast<std::size_t>(firstImage.pixelWidth) * firstImage.pixelHeight;
        std::size_t volumeSliceCount = 0;
        layerTransforms.reserve(state.activeImageCount * 7);
        for (std::size_t index = 0; index < state.activeImageCount; ++index) {
            const ImageLayer& image = state.imageLayers[index];
            if (image.pixelWidth != firstImage.pixelWidth || image.pixelHeight != firstImage.pixelHeight ||
                image.scalarPixels.size() != sliceSize) {
                state.previousAction = "Export failed: TIFF slice dimensions do not match";
                return false;
            }
            const std::size_t ySizeVoxels = static_cast<std::size_t>(std::max(1.0f, std::round(image.sizeY)));
            if (volumeSliceCount > volume.max_size() / sliceSize ||
                ySizeVoxels > volume.max_size() / sliceSize - volumeSliceCount) {
                state.previousAction = "Export failed: TIFF volume is too large";
                return false;
            }
            volumeSliceCount += ySizeVoxels;
            layerTransforms.insert(layerTransforms.end(), {image.x / voxelSpacingWorldUnits,
                                                            image.y / voxelSpacingWorldUnits,
                                                            image.z / voxelSpacingWorldUnits,
                                                            image.sizeX, image.sizeY, image.sizeZ,
                                                            image.visible ? 1.0f : 0.0f});
        }
        volume.reserve(sliceSize * volumeSliceCount);
        for (std::size_t index = 0; index < state.activeImageCount; ++index) {
            const ImageLayer& image = state.imageLayers[index];
            const int ySizeVoxels = static_cast<int>(std::max(1.0f, std::round(image.sizeY)));
            for (int yVoxel = 0; yVoxel < ySizeVoxels; ++yVoxel) {
                volume.insert(volume.end(), image.scalarPixels.begin(), image.scalarPixels.end());
            }
        }
    }

    const hid_t file = H5Fcreate(path.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    if (file < 0) {
        state.previousAction = "HDF5 export failed: could not create file";
        return false;
    }

    bool success = true;
    if (!state.dataPoints.empty()) {
        const hsize_t pointCount = static_cast<hsize_t>(state.dataPoints.size());
        const hsize_t vectorDimensions[] = {pointCount, 3};
        const hsize_t scalarDimensions[] = {pointCount};
        const hsize_t stringDimensions[] = {pointCount, sizeof(DataPoint::stringValue)};
        const hid_t stringType = H5Tcopy(H5T_C_S1);
        success = stringType >= 0;
        if (success) success = H5Tset_size(stringType, sizeof(DataPoint::stringValue)) >= 0;
        if (success) success = writeDataset(file, "positions", H5T_NATIVE_FLOAT, positions.data(), vectorDimensions, 2);
        if (success) success = writeDataset(file, "sizes", H5T_NATIVE_FLOAT, sizes.data(), vectorDimensions, 2);
        if (success) success = writeDataset(file, "value_types", H5T_NATIVE_INT, valueTypes.data(), scalarDimensions, 1);
        if (success) success = writeDataset(file, "values_float", H5T_NATIVE_FLOAT, floatValues.data(), scalarDimensions, 1);
        if (success) success = writeDataset(file, "values_integer", H5T_NATIVE_INT, integerValues.data(), scalarDimensions, 1);
        if (success) success = writeDataset(file, "values_string", stringType, stringValues.data(), stringDimensions, 2);
        if (stringType >= 0) H5Tclose(stringType);
    }
    if (success && state.activeImageCount > 0) {
        const ImageLayer& firstImage = state.imageLayers.front();
        const hsize_t volumeDimensions[] = {static_cast<hsize_t>(volume.size() /
                              (static_cast<std::size_t>(firstImage.pixelWidth) * firstImage.pixelHeight)),
                                            firstImage.pixelHeight, firstImage.pixelWidth};
        const hsize_t transformDimensions[] = {static_cast<hsize_t>(state.activeImageCount), 7};
        const hsize_t spacingDimensions[] = {3};
        constexpr float voxelSpacing[] = {voxelSpacingWorldUnits, voxelSpacingWorldUnits, voxelSpacingWorldUnits};
        success = writeDataset(file, "volume", H5T_NATIVE_UINT16, volume.data(), volumeDimensions, 3);
        if (success) success = writeDataset(file, "image_layer_transforms", H5T_NATIVE_FLOAT,
                                            layerTransforms.data(), transformDimensions, 2);
        if (success) success = writeDataset(file, "voxel_spacing_world_units", H5T_NATIVE_FLOAT,
                                            voxelSpacing, spacingDimensions, 1);
    }
    H5Fclose(file);

    state.previousAction = success ? "Exported " + std::to_string(state.dataPoints.size()) + " Data Point(s) and " +
                                      std::to_string(state.activeImageCount) + " TIFF slice(s) to HDF5"
                                   : "HDF5 export failed while writing datasets";
    return success;
}

}  // namespace app
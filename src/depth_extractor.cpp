#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <iostream>

#include "depth_extractor.h"

namespace app {

DepthMap extractDepth(const ImageLayer* imgInput, float scaleFactor)
{
    if (imgInput == nullptr)
        return {};

    if (!imgInput->scalarPixels.empty())
    {
        return extractDepthFromTiff(imgInput, scaleFactor);
    }

    if (!imgInput->rgbaPixels.empty())
    {
        return extractDepthFromImage(imgInput, scaleFactor);
    }

    return {};
}

DepthMap extractDepthFromTiff(const ImageLayer* imgInput, float scaleFactor)
{
    DepthMap result;

    result.width =
        static_cast<int>(imgInput->pixelWidth);

    result.height =
        static_cast<int>(imgInput->pixelHeight);

    const std::size_t pixelCount =
        static_cast<std::size_t>(result.width) *
        static_cast<std::size_t>(result.height);

    if (imgInput->scalarPixels.size() != pixelCount)
    {
        std::cerr << "Scalar TIFF size does not match image dimensions.\n";
        return {};
    }

    double scalarMin = std::numeric_limits<double>::infinity();
    double scalarMax = -std::numeric_limits<double>::infinity();

    for (double value : imgInput->scalarPixels)
    {
        if (!std::isfinite(value))
            continue;

        scalarMin = std::min(scalarMin, value);
        scalarMax = std::max(scalarMax, value);
    }

    result.values.resize(pixelCount);

    for (std::size_t i = 0; i < pixelCount; ++i)
    {
        result.values[i] =
            static_cast<float>(imgInput->scalarPixels[i] * scaleFactor);
    }

    float minValue = result.values[0];
    float maxValue = result.values[0];

    for (float value : result.values) {
        minValue = std::min(minValue, value);
        maxValue = std::max(maxValue, value);
    }

    return result;
}

DepthMap extractDepthFromImage(const ImageLayer* imgInput, float scaleFactor)
{
    DepthMap result;
    return result;
}
}
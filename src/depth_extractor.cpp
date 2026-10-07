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
    return result;
}

DepthMap extractDepthFromImage(const ImageLayer* imgInput, float scaleFactor)
{
    DepthMap result;
    return result;
}
}
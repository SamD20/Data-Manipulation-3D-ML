#pragma once

#include "app_state.h"

namespace app {

struct DepthMap
{
    int width = 0;
    int height = 0;
    std::vector<float> values;
};

DepthMap extractDepth(
    const ImageLayer* imgInput,
    float scaleFactor
);

DepthMap extractDepthFromTiff(
    const ImageLayer* imgInput,
    float scaleFactor
);

DepthMap extractDepthFromImage(
    const ImageLayer* imgInput,
    float scaleFactor
);

}
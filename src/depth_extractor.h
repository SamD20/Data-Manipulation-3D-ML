#pragma once

#include "app_state.h"

namespace app {

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
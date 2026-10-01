#pragma once

#include "app_state.h"

namespace app {

void initializeRenderer();
void renderScene(const AppState& state, int width, int height);

}  // namespace app
#pragma once

#include "app_state.h"

#include <string>
#include <vector>

namespace app {

std::string choosePath(bool save, const char* filter, const char* extension);
std::vector<std::string> chooseMultiplePaths(const char* filter);
bool loadTiff(AppState& state, const std::string& path);
bool loadObj(AppState& state, const std::string& path);
bool writeObj(AppState& state, const std::string& path);
bool exportHdf5(AppState& state, const std::string& path);

}  // namespace app
cmake_minimum_required(VERSION 3.20)
project(opengl_data_app LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

set(HDF5_EXTERNALLY_CONFIGURED 1 CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(HDF5_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(HDF5_BUILD_UTILS OFF CACHE BOOL "" FORCE)
set(HDF5_BUILD_STATIC_TOOLS OFF CACHE BOOL "" FORCE)
set(HDF5_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(HDF5_BUILD_CPP_LIB OFF CACHE BOOL "" FORCE)
set(HDF5_BUILD_FORTRAN OFF CACHE BOOL "" FORCE)
set(HDF5_BUILD_HL_LIB OFF CACHE BOOL "" FORCE)
set(HDF5_ENABLE_Z_LIB_SUPPORT OFF CACHE BOOL "" FORCE)
set(HDF5_ENABLE_SZIP_SUPPORT OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    hdf5
    GIT_REPOSITORY https://github.com/HDFGroup/hdf5.git
    GIT_TAG hdf5_1.14.6
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(hdf5)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.8
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(imgui)
if (NOT imgui_POPULATED)
    cmake_policy(PUSH)
    if (POLICY CMP0169)
        cmake_policy(SET CMP0169 OLD)
    endif()
    FetchContent_Populate(imgui)
    cmake_policy(POP)
endif()

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl2.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})
target_link_libraries(imgui PUBLIC glfw)

add_executable(opengl_data_app
    src/main.cpp
    src/app_state.cpp
    src/editor.cpp
    src/file_io.cpp
    src/renderer.cpp
)
target_compile_definitions(opengl_data_app PRIVATE GLFW_INCLUDE_NONE)
target_link_libraries(opengl_data_app PRIVATE glfw imgui hdf5-static)

if (WIN32)
    target_link_libraries(opengl_data_app PRIVATE opengl32 comdlg32 ole32 windowscodecs)
elseif(APPLE)
    target_link_libraries(opengl_data_app PRIVATE "-framework OpenGL")
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(opengl_data_app PRIVATE OpenGL::GL)
endif()

if (MSVC)
    target_compile_options(opengl_data_app PRIVATE /W4 /permissive-)
else()
    target_compile_options(opengl_data_app PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_custom_target(package_executable
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_SOURCE_DIR}/bin"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:opengl_data_app>"
        "${PROJECT_SOURCE_DIR}/bin/$<TARGET_FILE_NAME:opengl_data_app>"
    DEPENDS opengl_data_app
    COMMENT "Packaging the application executable"
)

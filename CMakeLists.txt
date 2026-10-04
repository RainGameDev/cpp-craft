cmake_minimum_required(VERSION 3.22)
project(minecraft-cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)



FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
)
FetchContent_MakeAvailable(stb)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.9b
)
FetchContent_MakeAvailable(imgui)

file(GLOB IMGUI_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC Vulkan::Vulkan glfw)

file(GLOB_RECURSE MINECRAFT_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/*.cpp
)

add_executable(minecraft ${MINECRAFT_SOURCES})
target_include_directories(minecraft PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${stb_SOURCE_DIR}
)
target_link_libraries(minecraft PRIVATE glm::glm imgui Vulkan::Vulkan glfw nlohmann_json::nlohmann_json)
target_compile_definitions(minecraft PRIVATE VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1)

if (MSVC)
    target_compile_options(minecraft PRIVATE /W4)
else()
    target_compile_options(minecraft PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_custom_command(TARGET minecraft POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:minecraft>/assets
)

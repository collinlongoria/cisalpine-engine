cmake_minimum_required(VERSION 3.30)
project(cisalpine)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# GLFW options
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(external/glfw)

# GLAD
add_library(glad STATIC
        external/glad/src/glad.c
)
target_include_directories(glad PUBLIC
        external/glad/include
)

# GLM
add_subdirectory(external/glm)

# ImGui
add_library(imgui STATIC
        external/imgui/imgui.cpp
        external/imgui/imgui_demo.cpp
        external/imgui/imgui_draw.cpp
        external/imgui/imgui_tables.cpp
        external/imgui/imgui_widgets.cpp
        external/imgui/backends/imgui_impl_glfw.cpp
        external/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
        external/imgui
        external/imgui/backends
)
target_link_libraries(imgui PUBLIC glfw glad)

# STB
add_library(stb STATIC src/stb_image.cpp)
target_include_directories(stb PUBLIC external/stb)

# Executable
add_executable(CisalpineEngine
        src/main.cpp
        src/app.cpp
        src/world.cpp
        src/shader.cpp)
target_include_directories(CisalpineEngine PRIVATE include)
target_link_libraries(CisalpineEngine PRIVATE glfw glad imgui stb glm)

# OpenGl Linking
find_package(OpenGL REQUIRED)
target_link_libraries(CisalpineEngine PRIVATE OpenGL::GL)

# Copy shaders
add_custom_command(TARGET CisalpineEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:CisalpineEngine>/shaders
)
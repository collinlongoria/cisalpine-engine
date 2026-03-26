cmake_minimum_required(VERSION 3.30)
project(cisalpine)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Ob3 /GL /DNDEBUG")
    set(CMAKE_C_FLAGS_RELEASE   "/O2 /Ob3 /GL /DNDEBUG")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/LTCG /OPT:REF /OPT:ICF")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/O2 /Ob2 /GL /Zi /DNDEBUG")
    set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "/LTCG /DEBUG /OPT:REF /OPT:ICF")
    add_compile_options(/fp:fast)
    add_compile_options(/arch:AVX2)
    add_compile_options(/wd4005)
    add_compile_options(/MP)

else()
    # GCC / Clang
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -flto")
    set(CMAKE_C_FLAGS_RELEASE   "-O3 -DNDEBUG -flto")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-flto")

    add_compile_options(-ffast-math -march=native)
endif()

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(external/glfw)

# GLAD
add_library(glad STATIC external/glad/src/glad.c)
target_include_directories(glad PUBLIC external/glad/include)

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

# nlohmann/json
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE
        ${CMAKE_SOURCE_DIR}/external/json/single_include
)

add_executable(CisalpineEngine
        src/main.cpp
        src/app.cpp
        src/world.cpp
        src/shader.cpp
        src/registry.cpp
        src/compiler.cpp)
target_include_directories(CisalpineEngine PRIVATE include)
target_link_libraries(CisalpineEngine PRIVATE glfw glad imgui stb glm nlohmann_json)

# OpenGL
find_package(OpenGL REQUIRED)
target_link_libraries(CisalpineEngine PRIVATE OpenGL::GL)

# Copy shaders & data
add_custom_target(copy_assets ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:CisalpineEngine>/shaders
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/data
        $<TARGET_FILE_DIR:CisalpineEngine>/data
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:CisalpineEngine>/assets
        COMMENT "Copying all assets to output directory"
        VERBATIM
)

add_dependencies(CisalpineEngine copy_assets)
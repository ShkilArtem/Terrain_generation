cmake_minimum_required(VERSION 3.21)

if(WIN32 AND CMAKE_GENERATOR STREQUAL "NMake Makefiles")
    find_program(TERRAIN_NMAKE_EXECUTABLE NAMES nmake)
    if(NOT TERRAIN_NMAKE_EXECUTABLE)
        message(
            FATAL_ERROR
            "NMake is unavailable in this PowerShell session. "
            "Run .\\build-windows.cmd instead; it automatically finds the "
            "installed Visual Studio toolchain and its matching CMake."
        )
    endif()
endif()

project(
    TerrainGeneration
    VERSION 1.0.0
    DESCRIPTION "Interactive procedural terrain generation, erosion, and analysis"
    LANGUAGES C CXX
)

include(FetchContent)
include(GNUInstallDirs)

option(
    TERRAIN_FETCH_GLFW
    "Download GLFW when no bundled or system installation is available"
    ON
)
option(TERRAIN_ENABLE_WARNINGS "Enable useful compiler warnings" ON)

set(TERRAIN_TARGET terrain_generation)
set(TERRAIN_DEPENDENCIES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies")
set(TERRAIN_IMGUI_DIR "${TERRAIN_DEPENDENCIES_DIR}/imgui")

find_package(OpenGL REQUIRED)

# The repository contains a ready-to-use x64 MSVC GLFW library for a
# network-free Windows build. Other platforms use a system GLFW package when
# available, then fall back to the official GLFW 3.4 sources.
if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8
   AND EXISTS "${TERRAIN_DEPENDENCIES_DIR}/lib/glfw3.lib")
    add_library(terrain_glfw STATIC IMPORTED GLOBAL)
    set_target_properties(
        terrain_glfw
        PROPERTIES
            IMPORTED_LOCATION "${TERRAIN_DEPENDENCIES_DIR}/lib/glfw3.lib"
            INTERFACE_INCLUDE_DIRECTORIES "${TERRAIN_DEPENDENCIES_DIR}/include"
            INTERFACE_LINK_LIBRARIES "gdi32;user32;shell32"
    )
    set(TERRAIN_GLFW_TARGET terrain_glfw)
else()
    find_package(glfw3 3.3 QUIET)

    if(TARGET glfw)
        set(TERRAIN_GLFW_TARGET glfw)
    elseif(TARGET glfw3::glfw)
        set(TERRAIN_GLFW_TARGET glfw3::glfw)
    elseif(TERRAIN_FETCH_GLFW)
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

        FetchContent_Declare(
            glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG 3.4
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(glfw)
        set(TERRAIN_GLFW_TARGET glfw)
    else()
        message(
            FATAL_ERROR
            "GLFW was not found. Install GLFW 3.3+ or configure with "
            "-DTERRAIN_FETCH_GLFW=ON."
        )
    endif()
endif()

set(
    TERRAIN_SOURCES
    main.cpp
    Camera.cpp
    Shader.cpp
    Terrain.cpp
    glad.c
    "${TERRAIN_IMGUI_DIR}/imgui.cpp"
    "${TERRAIN_IMGUI_DIR}/imgui_draw.cpp"
    "${TERRAIN_IMGUI_DIR}/imgui_tables.cpp"
    "${TERRAIN_IMGUI_DIR}/imgui_widgets.cpp"
    "${TERRAIN_IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${TERRAIN_IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)

set(
    TERRAIN_HEADERS
    Camera.h
    Shader.h
    Terrain.h
    stb_image.h
)

add_executable(${TERRAIN_TARGET} ${TERRAIN_SOURCES} ${TERRAIN_HEADERS})
add_executable(TerrainGeneration::app ALIAS ${TERRAIN_TARGET})

target_compile_features(${TERRAIN_TARGET} PRIVATE cxx_std_17)
set_target_properties(
    ${TERRAIN_TARGET}
    PROPERTIES
        CXX_EXTENSIONS OFF
        OUTPUT_NAME TerrainGeneration
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${TERRAIN_TARGET}>"
)

if(MSVC)
    # All required libraries are provided by this repository. Disable any
    # machine-wide vcpkg integration so builds stay reproducible and quiet.
    set_property(TARGET ${TERRAIN_TARGET} PROPERTY VS_GLOBAL_VcpkgEnabled "false")
endif()

target_include_directories(
    ${TERRAIN_TARGET}
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${TERRAIN_DEPENDENCIES_DIR}/include"
        "${TERRAIN_IMGUI_DIR}"
        "${TERRAIN_IMGUI_DIR}/backends"
)

target_compile_definitions(
    ${TERRAIN_TARGET}
    PRIVATE
        GLFW_INCLUDE_NONE
        IMGUI_IMPL_OPENGL_LOADER_GLAD
)

target_link_libraries(
    ${TERRAIN_TARGET}
    PRIVATE
        ${TERRAIN_GLFW_TARGET}
        OpenGL::GL
)

if(TERRAIN_ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(
            ${TERRAIN_TARGET}
            PRIVATE
                /W4
                /permissive-
                /Zc:__cplusplus
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            ${TERRAIN_TARGET}
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
        )
    endif()
endif()

source_group("Source Files" FILES ${TERRAIN_SOURCES})
source_group("Header Files" FILES ${TERRAIN_HEADERS})
source_group("Shaders" FILES shaders/terrain.vert shaders/terrain.frag)

target_sources(
    ${TERRAIN_TARGET}
    PRIVATE
        shaders/terrain.vert
        shaders/terrain.frag
)

# Keep runtime assets next to the executable so it works from Visual Studio,
# a terminal, or Explorer without manually changing the working directory.
add_custom_command(
    TARGET ${TERRAIN_TARGET}
    POST_BUILD
    COMMAND
        ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:${TERRAIN_TARGET}>/shaders"
    COMMAND
        ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/textures"
        "$<TARGET_FILE_DIR:${TERRAIN_TARGET}>/textures"
    COMMENT "Copying shaders and textures"
)

if(MSVC)
    # Make the installed/packaged Windows build runnable on machines that do
    # not already have the matching Visual C++ Redistributable installed.
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ".")
    include(InstallRequiredSystemLibraries)
endif()

install(
    TARGETS ${TERRAIN_TARGET}
    RUNTIME DESTINATION .
    BUNDLE DESTINATION .
)
install(DIRECTORY shaders textures DESTINATION .)
install(
    FILES
        README.md
        CHANGELOG.md
        LICENSE
        THIRD_PARTY_NOTICES.md
    DESTINATION .
)

set(CPACK_PACKAGE_NAME "TerrainGeneration")
set(CPACK_PACKAGE_VENDOR "ShkilArtem")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(
    CPACK_PACKAGE_FILE_NAME
    "TerrainGeneration-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
)
set(CPACK_GENERATOR ZIP)
include(CPack)

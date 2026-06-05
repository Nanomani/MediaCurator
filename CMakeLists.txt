cmake_minimum_required(VERSION 3.25)

project(MediaCurator
    VERSION 0.1.0
    DESCRIPTION "Smart media library curator — scan, filter, and clean your video collection"
    HOMEPAGE_URL "https://github.com/yourusername/MediaCurator"
    LANGUAGES CXX
)

# ── C++ Standard ──────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Output directories ─────────────────────────────────────────────────────────
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# ── Platform detection ─────────────────────────────────────────────────────────
if(WIN32)
    add_compile_definitions(MC_PLATFORM_WINDOWS)
elseif(APPLE)
    add_compile_definitions(MC_PLATFORM_MACOS)
else()
    add_compile_definitions(MC_PLATFORM_LINUX)
endif()

# ── MSVC-specific flags ────────────────────────────────────────────────────────
if(MSVC)
    add_compile_options(/W4 /WX- /utf-8 /MP)
    add_compile_definitions(_UNICODE UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    # Enable parallel compilation
    add_compile_options(/MP)
endif()

# ── Find Qt6 ──────────────────────────────────────────────────────────────────
# Set Qt6_DIR in your CMake config or via:
#   cmake -DQt6_DIR="C:/Qt/6.7.2/msvc2022_64/lib/cmake/Qt6" ...
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Sql
    Concurrent
)

qt_standard_project_setup()

# ── Find nlohmann/json (via vcpkg or system) ───────────────────────────────────
find_package(nlohmann_json CONFIG REQUIRED)

# ── Sub-directories ────────────────────────────────────────────────────────────
add_subdirectory(src)

# ── CPack installer config ────────────────────────────────────────────────────
include(cmake/Packaging.cmake)

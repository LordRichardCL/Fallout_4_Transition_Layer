cmake_minimum_required(VERSION 3.16)

project(aSWMultiplexer LANGUAGES CXX)

# -------------------------------
# Compiler settings
# -------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -------------------------------
# External dependencies
# -------------------------------

# F4SE headers (adjusted to match your actual PluginAPI.h location)
include_directories(${CMAKE_SOURCE_DIR}/extern/f4se/src/f4se)

# CommonLibF4 headers
include_directories(${CMAKE_SOURCE_DIR}/extern/commonlibf4/include)

# Add CommonLibF4 as a subproject
add_subdirectory(${CMAKE_SOURCE_DIR}/extern/commonlibf4 commonlibf4_build)

# Add mmio as a subproject
add_subdirectory(${CMAKE_SOURCE_DIR}/extern/mmio mmio_build)

# -------------------------------
# Plugin target
# -------------------------------

add_library(aSWMultiplexer SHARED
    dllmain.cpp
    injector.cpp
    log.cpp
    main.cpp
    mapping.cpp
    pch.cpp        # stub source for PCH
    Plugin.cpp
    scanner.cpp
    utils.cpp
)

# -------------------------------
# Precompiled headers
# -------------------------------
target_precompile_headers(aSWMultiplexer PRIVATE pch.h)

# -------------------------------
# Dependencies via vcpkg
# -------------------------------
find_package(fmt REQUIRED)
find_package(spdlog REQUIRED)
find_package(Boost REQUIRED)
find_package(robin_hood REQUIRED)
find_package(frozen REQUIRED)
find_package(nowide REQUIRED)
find_package(xbyak REQUIRED)
find_package(Catch2 REQUIRED)

# -------------------------------
# Linking
# -------------------------------
# Header-only libraries (args, srell) are handled via include paths only
include_directories(${CMAKE_SOURCE_DIR}/../vcpkg/installed/x64-windows/include)

target_link_libraries(aSWMultiplexer
    commonlibf4
    fmt::fmt
    spdlog::spdlog
    mmio
    Boost::boost
    robin_hood::robin_hood
    frozen::frozen
    nowide::nowide
    xbyak::xbyak
    Catch2::Catch2
)

# -------------------------------
# Output
# -------------------------------
set_target_properties(aSWMultiplexer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/F4SE/Plugins
)

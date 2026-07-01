cmake_minimum_required(VERSION 3.20)

project(TickForge
    VERSION 0.1.0
    DESCRIPTION "A low-latency market data processing and order execution engine"
    LANGUAGES CXX
)

include(CTest)

option(TICKFORGE_ENABLE_WARNINGS "Enable a strong warning set for TickForge targets" ON)
option(TICKFORGE_ENABLE_DPDK "Enable DPDK detection for transport-oriented apps" ON)

set(TICKFORGE_DPDK_ENABLED OFF)
if (TICKFORGE_ENABLE_DPDK)
    include(CheckIncludeFileCXX)
    check_include_file_cxx("rte_eal.h" TICKFORGE_HAS_RTE_EAL_H)
    check_include_file_cxx("rte_ring.h" TICKFORGE_HAS_RTE_RING_H)
    check_include_file_cxx("rte_ethdev.h" TICKFORGE_HAS_RTE_ETHDEV_H)

    if (TICKFORGE_HAS_RTE_EAL_H AND TICKFORGE_HAS_RTE_RING_H AND TICKFORGE_HAS_RTE_ETHDEV_H)
        set(TICKFORGE_DPDK_ENABLED ON)
    endif()
endif()

message(STATUS "TickForge DPDK support: ${TICKFORGE_DPDK_ENABLED}")

add_library(tickforge_core INTERFACE)
add_library(tickforge::core ALIAS tickforge_core)

target_include_directories(tickforge_core
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(tickforge_core INTERFACE cxx_std_20)

if (MSVC)
    target_compile_options(tickforge_core INTERFACE /W4 /permissive- /EHsc-)
else()
    target_compile_options(tickforge_core INTERFACE
        -O3
        -march=native
        -fno-exceptions
    )
endif()

if (TICKFORGE_ENABLE_WARNINGS AND NOT MSVC)
    target_compile_options(tickforge_core INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
    )
endif()

add_library(tickforge_warnings INTERFACE)
add_library(tickforge::warnings ALIAS tickforge_warnings)

if (TICKFORGE_ENABLE_WARNINGS AND MSVC)
    target_compile_options(tickforge_warnings INTERFACE /W4 /permissive-)
endif()

if (BUILD_TESTING)
    add_executable(tickforge_tests tests/test_sanity.cpp)
    target_link_libraries(tickforge_tests PRIVATE tickforge::core)
    add_test(NAME tickforge.tests COMMAND $<TARGET_FILE:tickforge_tests>)
endif()

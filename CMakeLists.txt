cmake_minimum_required(VERSION 3.20)

project(DPDKTrade
    VERSION 0.1.0
    DESCRIPTION "A low-latency market data processing and order execution engine"
    LANGUAGES CXX
)

include(CTest)

option(DPDKTRADE_ENABLE_WARNINGS "Enable a strong warning set for DPDKTrade targets" ON)
option(DPDKTRADE_ENABLE_DPDK "Enable DPDK detection for transport-oriented apps" ON)
option(TICKFORGE_WITH_DPDK "Build the DPDK ring transport apps" OFF)

set(DPDKTRADE_DPDK_ENABLED OFF)
if (DPDKTRADE_ENABLE_DPDK)
    include(CheckIncludeFileCXX)
    check_include_file_cxx("rte_eal.h" DPDKTRADE_HAS_RTE_EAL_H)
    check_include_file_cxx("rte_ring.h" DPDKTRADE_HAS_RTE_RING_H)
    check_include_file_cxx("rte_ethdev.h" DPDKTRADE_HAS_RTE_ETHDEV_H)

    if (DPDKTRADE_HAS_RTE_EAL_H AND DPDKTRADE_HAS_RTE_RING_H AND DPDKTRADE_HAS_RTE_ETHDEV_H)
        set(DPDKTRADE_DPDK_ENABLED ON)
    endif()
endif()

message(STATUS "DPDKTrade DPDK support: ${DPDKTRADE_DPDK_ENABLED}")

add_library(dpdktrade_core INTERFACE)
add_library(dpdktrade::core ALIAS dpdktrade_core)

target_include_directories(dpdktrade_core
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(dpdktrade_core INTERFACE cxx_std_20)

if (MSVC)
    target_compile_options(dpdktrade_core INTERFACE /W4 /permissive- /EHsc-)
else()
    target_compile_options(dpdktrade_core INTERFACE
        -O3
        -march=native
        -fno-exceptions
    )
endif()

if (DPDKTRADE_ENABLE_WARNINGS AND NOT MSVC)
    target_compile_options(dpdktrade_core INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
    )
endif()

add_library(dpdktrade_warnings INTERFACE)
add_library(dpdktrade::warnings ALIAS dpdktrade_warnings)

if (DPDKTRADE_ENABLE_WARNINGS AND MSVC)
    target_compile_options(dpdktrade_warnings INTERFACE /W4 /permissive-)
endif()

if (BUILD_TESTING)
    add_executable(dpdktrade_tests tests/test_sanity.cpp)
    target_link_libraries(dpdktrade_tests PRIVATE dpdktrade::core)
    add_test(NAME dpdktrade.tests COMMAND $<TARGET_FILE:dpdktrade_tests>)

    add_executable(dpdktrade_test_book tests/test_book.cpp)
    target_link_libraries(dpdktrade_test_book PRIVATE dpdktrade::core)
    add_test(NAME dpdktrade.test_book COMMAND $<TARGET_FILE:dpdktrade_test_book>)
endif()

add_executable(dpdktrade_stress apps/stress.cpp)
target_link_libraries(dpdktrade_stress PRIVATE dpdktrade::core)
if (BUILD_TESTING)
    add_test(NAME dpdktrade.stress.smoke COMMAND dpdktrade_stress --iterations 4096)
endif()

add_executable(ring_benchmark apps/ring_benchmark.cpp)
target_link_libraries(ring_benchmark PRIVATE dpdktrade::core)

if (TICKFORGE_WITH_DPDK)
    add_executable(dpdk_engine apps/dpdk_engine.cpp)
    target_link_libraries(dpdk_engine PRIVATE dpdktrade::core)

    add_executable(dpdk_generator apps/dpdk_generator.cpp)
    target_link_libraries(dpdk_generator PRIVATE dpdktrade::core)

    add_executable(afpacket_vs_dpdk_benchmark apps/afpacket_vs_dpdk_benchmark.cpp)
    target_link_libraries(afpacket_vs_dpdk_benchmark PRIVATE dpdktrade::core)
endif()

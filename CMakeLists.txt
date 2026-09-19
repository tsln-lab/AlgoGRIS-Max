# AlgoGRIS-Max: a Max package wrapping AlgoGRIS (the GRIS spatialization DSP) with the Min API.
cmake_minimum_required(VERSION 3.30) # AlgoGRIS requires 3.30

project(AlgoGRIS-Max LANGUAGES C CXX)

if (NOT WIN32 AND NOT APPLE)
    message(FATAL_ERROR "Max externals can only be built on Windows or macOS.")
endif ()

# The Max SDK links the static C runtime (/MT); build AlgoGRIS and JUCE the same way.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(${CMAKE_CURRENT_SOURCE_DIR}/source/min-api/script/min-package.cmake)

# Max 9 needs macOS 11, and the external uses std::filesystem (macOS 10.15+).
# The Min scripts would otherwise set 10.11.
set(ALGOGRIS_MACOS_TARGET "11.0")
if (APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "${ALGOGRIS_MACOS_TARGET}" CACHE STRING "Minimum macOS version" FORCE)
endif ()

# AlgoGRIS checkout to build from. Defaults to a sibling folder.
set(ALGOGRIS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../AlgoGRIS" CACHE PATH "Path to the AlgoGRIS repository")
if (NOT EXISTS "${ALGOGRIS_DIR}/sg_AbstractSpatAlgorithm.hpp")
    message(FATAL_ERROR "AlgoGRIS not found at ${ALGOGRIS_DIR}. Set -DALGOGRIS_DIR=<path>.")
endif ()

# Only the AlgoGRIS library is needed, not its tests.
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory("${ALGOGRIS_DIR}" algogris EXCLUDE_FROM_ALL)

# Runtime data the binaural mode reads, plus a couple of default speaker setups.
set(ALGOGRIS_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/support/algogris-data")
file(COPY "${ALGOGRIS_DIR}/hrtf_compact" DESTINATION "${ALGOGRIS_DATA_DIR}")
file(COPY "${ALGOGRIS_DIR}/tests/util/BINAURAL_SPEAKER_SETUP.xml" DESTINATION "${ALGOGRIS_DATA_DIR}/tests/util")
file(COPY "${ALGOGRIS_DIR}/tests/temp/Dome_default_speaker_setup.xml"
          "${ALGOGRIS_DIR}/tests/util/Cube_default_speaker_setup.xml"
     DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/setups")

add_subdirectory(source/projects/algogris_tilde)

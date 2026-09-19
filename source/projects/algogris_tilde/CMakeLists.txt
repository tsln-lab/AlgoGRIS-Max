cmake_minimum_required(VERSION 3.30)

set(C74_MIN_API_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../min-api)
include(${C74_MIN_API_DIR}/script/min-pretarget.cmake)

include_directories("${C74_INCLUDES}")

add_library(${PROJECT_NAME} MODULE
    algogris_tilde.cpp
    algogris_engine.cpp
    algogris_engine.hpp
)

include(${C74_MIN_API_DIR}/script/min-posttarget.cmake)

target_link_libraries(${PROJECT_NAME} PRIVATE AlgoGRIS)

# Min is written for C++17 and AlgoGRIS needs C++20. The two only meet through
# algogris_engine.hpp, which is plain C++, so each file gets its own standard.
if (MSVC)
    set_source_files_properties(algogris_engine.cpp PROPERTIES COMPILE_OPTIONS "/std:c++20")
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj /utf-8)
else ()
    set_source_files_properties(algogris_engine.cpp PROPERTIES COMPILE_OPTIONS "-std=c++20")
endif ()

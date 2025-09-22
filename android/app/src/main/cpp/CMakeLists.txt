cmake_minimum_required(VERSION 3.22.1)
project(aat_bridge)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(GGWAVE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)

add_library(ggwave STATIC
    ${GGWAVE_ROOT}/src/ggwave.cpp
)

target_include_directories(ggwave PUBLIC
    ${GGWAVE_ROOT}/include
    ${GGWAVE_ROOT}/src
)

add_library(aat_bridge SHARED
    native-lib.cpp
)

target_link_libraries(aat_bridge
    ggwave
    log
)

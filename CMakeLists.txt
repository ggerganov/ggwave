cmake_minimum_required (VERSION 3.0)
project(ggwave VERSION 0.1.4)

configure_file(${CMAKE_SOURCE_DIR}/README.md.tmpl ${CMAKE_SOURCE_DIR}/README.md @ONLY)
configure_file(${CMAKE_SOURCE_DIR}/bindings/python/setup.py.tmpl ${CMAKE_SOURCE_DIR}/bindings/python/setup.py @ONLY)

set(CMAKE_EXPORT_COMPILE_COMMANDS "on")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(GGWAVE_STANDALONE ON)
    include(cmake/GitVars.cmake)
    include(cmake/BuildTypes.cmake)
else()
    set(GGWAVE_STANDALONE OFF)
endif()

if (EMSCRIPTEN)
    set(GGWAVE_SUPPORT_SDL2_DEFAULT ON)
    set(BUILD_SHARED_LIBS_DEFAULT OFF)
else()
    set(GGWAVE_SUPPORT_SDL2_DEFAULT ON)
    set(BUILD_SHARED_LIBS_DEFAULT ON)
endif()

# options

option(BUILD_SHARED_LIBS              "ggwave: build shared libs" ${BUILD_SHARED_LIBS_DEFAULT})
option(USE_FINDSDL2                   "ggwave: use the FindSDL2.cmake script" OFF)

option(GGWAVE_ALL_WARNINGS            "ggwave: enable all compiler warnings" ON)
option(GGWAVE_ALL_WARNINGS_3RD_PARTY  "ggwave: enable all compiler warnings in 3rd party libs" ON)

option(GGWAVE_SANITIZE_THREAD         "ggwave: enable thread sanitizer" OFF)
option(GGWAVE_SANITIZE_ADDRESS        "ggwave: enable address sanitizer" OFF)
option(GGWAVE_SANITIZE_UNDEFINED      "ggwave: enable undefined sanitizer" OFF)

option(GGWAVE_SUPPORT_SDL2            "ggwave: support for libSDL2" ${GGWAVE_SUPPORT_SDL2_DEFAULT})
option(GGWAVE_SUPPORT_PYTHON          "ggwave: support for python" OFF)

option(GGWAVE_BUILD_TESTS             "ggwave: build examples" ${GGWAVE_STANDALONE})
option(GGWAVE_BUILD_EXAMPLES          "ggwave: build examples" ${GGWAVE_STANDALONE})

# sanitizers

if (GGWAVE_SANITIZE_THREAD)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
endif()

if (GGWAVE_SANITIZE_ADDRESS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer -D_GLIBCXX_DEBUG")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -D_GLIBCXX_DEBUG")
endif()

if (GGWAVE_SANITIZE_UNDEFINED)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
endif()

# dependencies

# main

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (GGWAVE_ALL_WARNINGS)
    if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
    else()
        # todo : windows
    endif()
endif()

add_subdirectory(src)

if (GGWAVE_SUPPORT_PYTHON)
    file(GLOB_RECURSE GGWAVE_SOURCES "include/*" "src/*")

    add_custom_command(
        OUTPUT ${CMAKE_SOURCE_DIR}/bindings/python/ggwave.bycython.cpp
        OUTPUT ${CMAKE_SOURCE_DIR}/bindings/python/ggwave
        DEPENDS ${CMAKE_SOURCE_DIR}/bindings/python/Makefile
        DEPENDS ${CMAKE_SOURCE_DIR}/bindings/python/ggwave.pyx
        DEPENDS ${CMAKE_SOURCE_DIR}/bindings/python/cggwave.pxd
        DEPENDS ${CMAKE_SOURCE_DIR}/bindings/python/setup.py
        DEPENDS ${GGWAVE_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/bindings/python
        COMMAND make clean
        COMMAND make
        COMMENT "Compiling Python module"
        VERBATIM
        )

    add_custom_target(ggwave-py ALL
        DEPENDS bindings/python/ggwave.bycython.cpp
        DEPENDS bindings/python/ggwave
        )
endif()


if (GGWAVE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if (GGWAVE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

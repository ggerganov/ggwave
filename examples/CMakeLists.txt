find_package(Threads REQUIRED)

# third-party

add_subdirectory(third-party)

# helper libraries

add_library(ggwave-common ${GGWAVE_LIBRARY_TYPE}
    ggwave-common.cpp
    )

target_link_libraries(ggwave-common PRIVATE
    )

if (GGWAVE_SUPPORT_SDL2)
    # SDL2

    if (EMSCRIPTEN)
        set (CMAKE_CXX_FLAGS "-s ALLOW_MEMORY_GROWTH=1 -s USE_SDL=2 -s ASSERTIONS=1 -s DISABLE_EXCEPTION_CATCHING=0 -s 'EXTRA_EXPORTED_RUNTIME_METHODS=[\"writeArrayToMemory\"]'")
    endif()

    add_library(ggwave-common-sdl2 ${GGWAVE_LIBRARY_TYPE}
        ggwave-common-sdl2.cpp
        )

    if (NOT EMSCRIPTEN)
        find_package(SDL2)

        if (NOT USE_FINDSDL2 AND NOT SDL2_FOUND)
            message(WARNING "Unable to find SDL2 library. It is either not installed or CMake cannot find it."
                " In the latter case, setting the USE_FINDSDL2 variable might help:\n"
                "   $ cmake -D USE_FINDSDL2 .."
                )

            message(FATAL_ERROR "Aborting")
        endif()

        string(STRIP "${SDL2_LIBRARIES}" SDL2_LIBRARIES)
    endif()

    message(STATUS "SDL2_INCLUDE_DIRS = ${SDL2_INCLUDE_DIRS}")
    message(STATUS "SDL2_LIBRARIES = ${SDL2_LIBRARIES}")

    # ggwave-common-sdl2

    target_include_directories(ggwave-common-sdl2 PUBLIC
        ${SDL2_INCLUDE_DIRS}
        )

    target_link_libraries(ggwave-common-sdl2 PRIVATE
        ggwave
        ${SDL2_LIBRARIES}
        )
endif()

# examples

if (GGWAVE_SUPPORT_SDL2)
    if (EMSCRIPTEN)
        # emscripten sdl2 examples

        add_subdirectory(ggwave-wasm)
    else()
        # non-emscripten sdl2 examples

        add_subdirectory(ggwave-cli)
        add_subdirectory(ggwave-gui)
    endif()
endif()

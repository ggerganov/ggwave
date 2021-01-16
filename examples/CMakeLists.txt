# dependencies

find_package(Threads REQUIRED)

if (GGWAVE_SUPPORT_SDL2)
    # SDL2

    if (EMSCRIPTEN)
        set(CMAKE_CXX_FLAGS "-s TOTAL_MEMORY=67108864 -s USE_SDL=2 -s ASSERTIONS=1 -s DISABLE_EXCEPTION_CATCHING=0 -s 'EXTRA_EXPORTED_RUNTIME_METHODS=[\"writeArrayToMemory\"]'")

        unset(SDL2_INCLUDE_DIRS)
        unset(SDL2_LIBRARIES)
    endif()

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

        message(STATUS "SDL2_INCLUDE_DIRS = ${SDL2_INCLUDE_DIRS}")
        message(STATUS "SDL2_LIBRARIES = ${SDL2_LIBRARIES}")
    endif()
endif()

# third-party

add_subdirectory(third-party)

# helper libraries

add_library(ggwave-common ${GGWAVE_LIBRARY_TYPE}
    ggwave-common.cpp
    )

target_link_libraries(ggwave-common PRIVATE
    ${CMAKE_DL_LIBS}
    )

if (GGWAVE_SUPPORT_SDL2)
    # ggwave-common-sdl2

    add_library(ggwave-common-sdl2 ${GGWAVE_LIBRARY_TYPE}
        ggwave-common-sdl2.cpp
        )

    target_include_directories(ggwave-common-sdl2 PUBLIC
        ${SDL2_INCLUDE_DIRS}
        )

    target_link_libraries(ggwave-common-sdl2 PRIVATE
        ggwave
        imgui-sdl2
        ${SDL2_LIBRARIES}
        )
endif()

# examples

add_subdirectory(ggwave-to-file)

if (GGWAVE_SUPPORT_SDL2)
    if (EMSCRIPTEN)
        # emscripten sdl2 examples

        add_subdirectory(ggwave-wasm)
        add_subdirectory(waver)
    else()
        # non-emscripten sdl2 examples

        add_subdirectory(ggwave-rx)
        add_subdirectory(ggwave-cli)
        add_subdirectory(waver)
    endif()
endif()

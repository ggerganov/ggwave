if (GGWAVE_ALL_WARNINGS_3RD_PARTY)
    if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
    else()
        # todo : windows
    endif()
endif()

if (APPLE)
    set(ADDITIONAL_LIBRARIES "-framework Cocoa")
else (APPLE)
    unset(ADDITIONAL_LIBRARIES)
endif (APPLE)

add_library(imgui
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_widgets.cpp
    )

target_include_directories(imgui INTERFACE
    .
    )

target_include_directories(imgui PRIVATE
    imgui
    )

target_link_libraries(imgui PRIVATE
    ${ADDITIONAL_LIBRARIES}
    )

if (MINGW)
    target_link_libraries(imgui PUBLIC
        stdc++
        )
endif()

if (GGWAVE_SUPPORT_SDL2)
    if (MINGW)
        find_package(PkgConfig REQUIRED)
        pkg_search_module(SDL2 REQUIRED sdl2)

        add_library(imgui-sdl2
            imgui/examples/libs/gl3w/GL/gl3w.c
            imgui-extra/imgui_impl.cpp
            imgui-extra/imgui_impl_sdl.cpp
            imgui-extra/imgui_impl_opengl3.cpp
            )

        target_include_directories(imgui-sdl2 PUBLIC
            imgui/examples/libs/gl3w
            ${SDL2_INCLUDE_DIRS}
            )

        target_include_directories(imgui-sdl2 PRIVATE
            imgui
            imgui-extra
            )

        target_link_libraries(imgui-sdl2 PUBLIC
            imgui
            opengl32
            ${SDL2_LIBRARIES}
            )

        target_link_libraries(imgui-sdl2 PRIVATE
            ${CMAKE_DL_LIBS}
            ${CMAKE_THREAD_LIBS_INIT}
            ${ADDITIONAL_LIBRARIES}
            )
    elseif (EMSCRIPTEN)
        add_library(imgui-sdl2
            imgui-extra/imgui_impl.cpp
            imgui-extra/imgui_impl_sdl.cpp
            imgui-extra/imgui_impl_opengl3.cpp
            )

        target_include_directories(imgui-sdl2 PRIVATE
            imgui
            imgui-extra
            )

        target_link_libraries(imgui-sdl2 PUBLIC
            imgui
            )
    else()
        set(OpenGL_GL_PREFERENCE GLVND)
        find_package(OpenGL REQUIRED)

        add_library(imgui-sdl2
            imgui/examples/libs/gl3w/GL/gl3w.c
            imgui-extra/imgui_impl.cpp
            imgui-extra/imgui_impl_sdl.cpp
            imgui-extra/imgui_impl_opengl3.cpp
            )

        # force GL3W loader
        target_compile_definitions(imgui-sdl2 PUBLIC
            IMGUI_IMPL_OPENGL_LOADER_GL3W=1
            )

        target_include_directories(imgui-sdl2 PUBLIC
            imgui/examples/libs/gl3w
            ${SDL2_INCLUDE_DIRS}
            )

        target_include_directories(imgui-sdl2 PRIVATE
            imgui
            imgui-extra
            )

        target_link_libraries(imgui-sdl2 PUBLIC
            imgui
            ${OPENGL_LIBRARIES}
            ${SDL2_LIBRARIES}
            )

        target_link_libraries(imgui-sdl2 PRIVATE
            ${CMAKE_DL_LIBS}
            ${CMAKE_THREAD_LIBS_INIT}
            ${ADDITIONAL_LIBRARIES}
            )
    endif()
endif()

install(TARGETS imgui
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib/static
    )

if (GGWAVE_SUPPORT_SDL2)
    install(TARGETS imgui-sdl2
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib/static
        )
endif()

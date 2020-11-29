add_executable(ggwave-gui main.cpp)

target_include_directories(ggwave-gui PRIVATE
    ..
    ${SDL2_INCLUDE_DIRS}
    )

target_link_libraries(ggwave-gui PRIVATE
    ggwave
    ggwave-common
    ggwave-common-sdl2
    imgui-sdl2
    ${CMAKE_THREAD_LIBS_INIT}
    )

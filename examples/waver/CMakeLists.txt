add_executable(waver main.cpp common.cpp interface.cpp interface-unix.cpp)

target_include_directories(waver PRIVATE
    ..
    ${SDL2_INCLUDE_DIRS}
    )

target_link_libraries(waver PRIVATE
    ggwave
    ggwave-common
    ggwave-common-sdl2
    ggsock
    imgui-sdl2
    ${CMAKE_THREAD_LIBS_INIT}
    )

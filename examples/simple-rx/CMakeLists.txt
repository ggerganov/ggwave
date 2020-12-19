add_executable(simple-rx main.cpp)

target_include_directories(simple-rx PRIVATE
    ..
    ${SDL2_INCLUDE_DIRS}
    )

target_link_libraries(simple-rx PRIVATE
    ggwave
    ggwave-common
    ggwave-common-sdl2
    ${CMAKE_THREAD_LIBS_INIT}
    )

add_executable(ggwave-cli main.cpp)

target_include_directories(ggwave-cli PRIVATE
    ..
    ${SDL2_INCLUDE_DIRS}
    )

target_link_libraries(ggwave-cli PRIVATE
    ggwave
    ggwave-common
    ggwave-common-sdl2
    ${SDL2_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    )

install(TARGETS ggwave-cli RUNTIME DESTINATION bin)

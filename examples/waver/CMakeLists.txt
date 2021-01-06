set(TARGET waver)

if (EMSCRIPTEN)
    add_executable(${TARGET} main.cpp common.cpp interface.cpp interface-emscripten.cpp)

    target_include_directories(${TARGET} PRIVATE
        ..
        ${SDL2_INCLUDE_DIRS}
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/
        )

    target_link_libraries(${TARGET} PRIVATE
        ggwave
        ggwave-common
        ggwave-common-sdl2
        ggsock
        imgui-sdl2
        ${CMAKE_THREAD_LIBS_INIT}
        )

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/index-tmpl.html ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/index.html @ONLY)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/style.css ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/style.css COPYONLY)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/main.js ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/main.js COPYONLY)
else()
    add_executable(${TARGET} main.cpp common.cpp interface.cpp interface-unix.cpp)

    target_include_directories(${TARGET} PRIVATE
        ..
        ${SDL2_INCLUDE_DIRS}
        )

    target_link_libraries(${TARGET} PRIVATE
        ggwave
        ggwave-common
        ggwave-common-sdl2
        ggsock
        imgui-sdl2
        ${CMAKE_THREAD_LIBS_INIT}
        )

    install(TARGETS ${TARGET} RUNTIME DESTINATION bin)
endif()

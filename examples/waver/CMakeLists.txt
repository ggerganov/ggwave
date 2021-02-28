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

    set_target_properties(${TARGET} PROPERTIES LINK_FLAGS " \
        -s FORCE_FILESYSTEM=1 \
        --preload-file ${CMAKE_CURRENT_SOURCE_DIR}/../assets/fonts@/ \
        ")

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/build_timestamp-tmpl.h ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/build_timestamp.h @ONLY)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/index-tmpl.html ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/index.html @ONLY)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/style.css ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/style.css COPYONLY)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/background-0.png ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/background-0.png COPYONLY)
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

    install(FILES ${PROJECT_SOURCE_DIR}/examples/assets/fonts/DroidSans.ttf DESTINATION bin)
    install(FILES ${PROJECT_SOURCE_DIR}/examples/assets/fonts/fontawesome-webfont.ttf DESTINATION bin)
endif()

install(TARGETS ${TARGET} RUNTIME DESTINATION bin)

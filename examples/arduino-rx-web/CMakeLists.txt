#
# arduino-rx-web

set(TARGET arduino-rx-web)

if (NOT EMSCRIPTEN)
    add_executable(${TARGET}
        arduino-rx-web.cpp
        )

    target_include_directories(${TARGET} PRIVATE
        ..
        ${SDL2_INCLUDE_DIRS}
        )

    target_link_libraries(${TARGET} PRIVATE
        ggwave-common
        ggwave
        ${SDL2_LIBRARIES}
        )
else()
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/index-tmpl.html       ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/index.html @ONLY)
    configure_file(${CMAKE_SOURCE_DIR}/bindings/javascript/ggwave.js ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/ggwave.js COPYONLY)
endif()

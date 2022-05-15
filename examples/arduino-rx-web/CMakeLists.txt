#
# arduino-rx

set(TARGET arduino-rx)

if (NOT EMSCRIPTEN)
    add_executable(${TARGET}
        arduino-rx.cpp
        )

    target_include_directories(${TARGET} PRIVATE
        ..
        ${SDL2_INCLUDE_DIRS}
        )

    target_link_libraries(${TARGET} PRIVATE
        ggwave-common
        ggwave-mod
        ${SDL2_LIBRARIES}
        )
else()
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/index-tmpl.html               ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/index.html @ONLY)
    configure_file(${CMAKE_SOURCE_DIR}/examples/ggwave-mod/src/ggwave-mod.js ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/ggwave-mod.js COPYONLY)
endif()

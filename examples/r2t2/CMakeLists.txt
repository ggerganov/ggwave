#
# r2t2

set(TARGET r2t2)

if (NOT EMSCRIPTEN)
    add_executable(${TARGET}
        main.cpp
        ggwave-mod/src/ggwave.cpp
        ggwave-mod/src/resampler.cpp
        )

    target_include_directories(${TARGET} PRIVATE
        ..
        ggwave-mod/include
        ggwave-mod/src
        )

    target_link_libraries(${TARGET} PRIVATE
        ggwave-common
        )
endif()

#
# r2t2-rx

set(TARGET r2t2-rx)

if (NOT EMSCRIPTEN)
    add_executable(${TARGET}
        r2t2-rx.cpp
        ggwave-mod/src/ggwave.cpp
        ggwave-mod/src/resampler.cpp
        )

    target_include_directories(${TARGET} PRIVATE
        ..
        ggwave-mod/include
        ggwave-mod/src
        ${SDL2_INCLUDE_DIRS}
        )

    target_link_libraries(${TARGET} PRIVATE
        ggwave-common
        ${SDL2_LIBRARIES}
        )
else()
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/build_timestamp-tmpl.h ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/build_timestamp.h @ONLY)

    add_executable(${TARGET}
        r2t2-rx.cpp
        ggwave-mod/src/ggwave.cpp
        ggwave-mod/src/resampler.cpp
        )

    target_include_directories(${TARGET} PRIVATE
        ..
        ggwave-mod/include
        ggwave-mod/src
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/
        )

    target_link_libraries(${TARGET} PRIVATE
        ggwave-common
        )

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/index-tmpl.html ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/index.html @ONLY)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/style.css ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/style.css COPYONLY)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/main.js ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/main.js COPYONLY)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/plucky.mp3 ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/plucky.mp3 COPYONLY)
endif()

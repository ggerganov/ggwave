#
# r2t2

set(TARGET r2t2)

if (NOT EMSCRIPTEN)
    add_executable(${TARGET}
        main.cpp
        )

    target_include_directories(${TARGET} PRIVATE
        ..
        )

    target_link_libraries(${TARGET} PRIVATE
        ggwave-common
        ggwave
        )
endif()

#
# r2t2-rx

set(TARGET r2t2-rx)

if (NOT EMSCRIPTEN)
    add_executable(${TARGET}
        r2t2-rx.cpp
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
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/build_timestamp-tmpl.h ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/build_timestamp.h @ONLY)

    add_executable(${TARGET}
        r2t2-rx.cpp
        )

    target_include_directories(${TARGET} PRIVATE
        ..
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/
        )

    target_link_libraries(${TARGET} PRIVATE
        ggwave-common
        ggwave
        )

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/index-tmpl.html ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/index.html @ONLY)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/style.css ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/style.css COPYONLY)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/main.js ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/main.js COPYONLY)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/plucky.mp3 ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/plucky.mp3 COPYONLY)
endif()

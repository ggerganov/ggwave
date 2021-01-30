set(TARGET ggwave-wasm)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/build_timestamp-tmpl.h ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/build_timestamp.h @ONLY)

add_executable(${TARGET}
    main.cpp
    )

target_include_directories(${TARGET} PRIVATE
    ..
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/
    )

target_link_libraries(${TARGET} PRIVATE
    ggwave
    ggwave-common
    ggwave-common-sdl2
    )

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/index-tmpl.html ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/index.html @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/style.css ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/style.css COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/main.js ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/main.js COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/plucky.mp3 ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TARGET}/plucky.mp3 COPYONLY)

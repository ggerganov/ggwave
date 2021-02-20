set(TARGET libggwave)

add_executable(${TARGET}
    emscripten.cpp
    )

target_link_libraries(${TARGET} PRIVATE
    ggwave
    )

unset(EXTRA_FLAGS)
if (GGWAVE_WASM_SINGLE_FILE)
    set(EXTRA_FLAGS "-s SINGLE_FILE=1")
    message(STATUS "Embedding WASM inside ggwave.js")

    add_custom_command(
        TARGET libggwave POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_BINARY_DIR}/bin/libggwave.js
        ${CMAKE_CURRENT_SOURCE_DIR}/ggwave.js
        )
endif()

set_target_properties(${TARGET} PROPERTIES LINK_FLAGS " \
    --bind \
    -s MODULARIZE=1 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s EXPORT_NAME=\"'ggwave_factory'\" \
    ${EXTRA_FLAGS} \
    ")

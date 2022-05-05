set(TARGET ggwave-mod)

add_library(${TARGET}
    ggwave.cpp
    resampler.cpp
    )

target_include_directories(${TARGET} PUBLIC
    .
    ../include
    )

if (BUILD_SHARED_LIBS)
    target_link_libraries(${TARGET} PUBLIC
        ${CMAKE_DL_LIBS}
        )

    target_compile_definitions(${TARGET} PUBLIC
        GGWAVE_SHARED
        )
endif()

if (MINGW)
    target_link_libraries(${TARGET} PUBLIC
        stdc++
        )
endif()

if (EMSCRIPTEN)
    set(TARGET libggwave-mod)

    add_executable(${TARGET}
        ${PROJECT_SOURCE_DIR}/bindings/javascript/emscripten.cpp
        )

    target_link_libraries(${TARGET} PRIVATE
        ggwave-mod
        )

    unset(EXTRA_FLAGS)
    if (GGWAVE_WASM_SINGLE_FILE)
        set(EXTRA_FLAGS "-s SINGLE_FILE=1")
        message(STATUS "Embedding WASM inside ggwave-mod.js")

        add_custom_command(
            TARGET libggwave-mod POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_BINARY_DIR}/bin/libggwave-mod.js
            ${CMAKE_CURRENT_SOURCE_DIR}/ggwave-mod.js
            )
    endif()

    set_target_properties(${TARGET} PROPERTIES LINK_FLAGS " \
    --bind \
    -s MODULARIZE=1 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s EXPORT_NAME=\"'ggwave_factory'\" \
    ${EXTRA_FLAGS} \
    ")
endif()

if (EMSCRIPTEN)
    return()
endif()

set(TEST_TARGET test-ggwave)

add_executable(${TEST_TARGET}
    ${TEST_TARGET}.cpp
    )

target_link_libraries(${TEST_TARGET} PRIVATE
    ggwave
    )

add_test(NAME ${TEST_TARGET} COMMAND $<TARGET_FILE:${TEST_TARGET}>)

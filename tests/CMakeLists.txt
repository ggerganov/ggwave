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

add_test(NAME ${TEST_TARGET}-cpp COMMAND $<TARGET_FILE:${TEST_TARGET}>)

if (GGWAVE_SUPPORT_PYTHON)
    add_test(NAME ${TEST_TARGET}-py
        COMMAND python test-ggwave.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )

    set_tests_properties(${TEST_TARGET}-py
        PROPERTIES ENVIRONMENT "PYTHONPATH=${PROJECT_SOURCE_DIR}/bindings/python")
endif()

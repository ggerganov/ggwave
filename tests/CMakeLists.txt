if (EMSCRIPTEN)
    #
    # test-ggwave-js

    set(TEST_TARGET test-ggwave-js)

    add_test(NAME ${TEST_TARGET}
        COMMAND node test-ggwave.js
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )

    return()
endif()

#
# test-ggwave-c

set(TEST_TARGET test-ggwave-c)

add_executable(${TEST_TARGET}
    test-ggwave.c
    )

target_link_libraries(${TEST_TARGET} PRIVATE
    ggwave
    )

add_test(NAME ${TEST_TARGET} COMMAND $<TARGET_FILE:${TEST_TARGET}>)

#
# test-ggwave-cpp

set(TEST_TARGET test-ggwave-cpp)

add_executable(${TEST_TARGET}
    test-ggwave.cpp
    )

target_link_libraries(${TEST_TARGET} PRIVATE
    ggwave
    )

add_test(NAME ${TEST_TARGET} COMMAND $<TARGET_FILE:${TEST_TARGET}>)

if (GGWAVE_SUPPORT_PYTHON)
    #
    # test-ggwave-py

    set(TEST_TARGET test-ggwave-py)

    add_test(NAME ${TEST_TARGET}
        COMMAND python test-ggwave.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )

    set_tests_properties(${TEST_TARGET}
        PROPERTIES ENVIRONMENT "PYTHONPATH=${PROJECT_SOURCE_DIR}/bindings/python")
endif()

if (EMSCRIPTEN)
    add_subdirectory(javascript)
endif()

if (GGWAVE_SUPPORT_PYTHON)
    file(GLOB_RECURSE GGWAVE_SOURCES "../include/*" "../src/*")

    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/python/ggwave.bycython.cpp
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/python/ggwave
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/python/Makefile
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/python/ggwave.pyx
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/python/cggwave.pxd
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/python/setup.py
        DEPENDS ${GGWAVE_SOURCES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/python
        COMMAND make clean
        COMMAND make
        COMMENT "Compiling Python module"
        VERBATIM
        )

    add_custom_target(ggwave-py ALL
        DEPENDS python/ggwave.bycython.cpp
        DEPENDS python/ggwave
        )

    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/python/dist
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/python/Makefile
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/python/ggwave.pyx
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/python/cggwave.pxd
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/python/setup.py
        DEPENDS ${GGWAVE_SOURCES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/python
        COMMAND make publish
        COMMENT "Publishing Python module v${PROJECT_VERSION}"
        VERBATIM
        )

    add_custom_target(pypi-publish
        DEPENDS python/dist
        )
endif()


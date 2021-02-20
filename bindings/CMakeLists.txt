if (EMSCRIPTEN)
    add_subdirectory(javascript)

    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/javascript/publish.log
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/javascript/ggwave.js
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/javascript/package.json
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/javascript
        COMMAND npm publish
        COMMAND touch publish.log
        COMMENT "Publishing npm module v${PROJECT_VERSION}"
        VERBATIM
        )

    add_custom_target(publish-npm
        DEPENDS javascript/publish.log
        )
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
        COMMENT "Publishing Python module v${GGWAVE_VERSION_PYTHON}"
        VERBATIM
        )

    add_custom_target(publish-pypi
        DEPENDS python/dist
        )
endif()

if (GGWAVE_SUPPORT_SWIFT)
    file(GLOB_RECURSE GGWAVE_SOURCES "../include/*" "../src/*")

    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/ios/.build
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ios/Makefile
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ios/Package.swift
        DEPENDS ${GGWAVE_SOURCES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ios
        COMMAND make clean
        COMMAND make build-submodule
        COMMENT "Compiling Swift package"
        VERBATIM
        )

    add_custom_target(ggwave-spm ALL
        DEPENDS ios/.build
        )

    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/ios/publish
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ios/publish-trigger
        DEPENDS ${GGWAVE_SOURCES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ios
        COMMAND make publish
        COMMENT "Publishing Swift package v${PROJECT_VERSION}"
        VERBATIM
        )

    add_custom_target(publish-spm
        DEPENDS ios/publish
        )
endif()

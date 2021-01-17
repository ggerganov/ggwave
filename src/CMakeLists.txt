# core

set(TARGET ggwave)

add_library(${TARGET} ${GGWAVE_LIBRARY_TYPE}
    ggwave.cpp
    )

target_include_directories(${TARGET} PUBLIC
    .
    ../include
    )

if (GGWAVE_LIBRARY_TYPE STREQUAL "SHARED")
    target_link_libraries(${TARGET} PUBLIC
        ${CMAKE_DL_LIBS}
        )

    target_compile_definitions(${TARGET} PUBLIC
        GGWAVE_SHARED
        )
endif()

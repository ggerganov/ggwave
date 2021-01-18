# core

set(TARGET ggwave)

add_library(${TARGET}
    ggwave.cpp
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

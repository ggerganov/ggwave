# core

add_library(ggwave ${GGWAVE_LIBRARY_TYPE}
    ggwave.cpp
    )

target_include_directories(ggwave PUBLIC
    .
    ../include
    )

if (GGWAVE_LIBRARY_TYPE STREQUAL "SHARED")
    target_link_libraries(ggwave PUBLIC
        ${CMAKE_DL_LIBS}
        )
endif()

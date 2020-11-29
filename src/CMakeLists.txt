# core

add_library(ggwave ${GGWAVE_LIBRARY_TYPE}
    ggwave.cpp
    )

target_include_directories(ggwave PUBLIC
    .
    ../include
    )

target_link_libraries(ggwave PUBLIC
    )

target_link_libraries(ggwave PRIVATE
    ${CMAKE_DL_LIBS}
    )

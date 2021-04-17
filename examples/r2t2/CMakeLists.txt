add_executable(r2t2
    main.cpp
    ggwave-mod/src/ggwave.cpp
    ggwave-mod/src/resampler.cpp
    )

target_include_directories(r2t2 PRIVATE
    ..
    ggwave-mod/include
    ggwave-mod/src
    )

target_link_libraries(r2t2 PRIVATE
    ggwave-common
    ${CMAKE_THREAD_LIBS_INIT}
    )

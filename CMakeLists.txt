cmake_minimum_required (VERSION 3.0)
project (ggwave)

set(CMAKE_EXPORT_COMPILE_COMMANDS "on")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(GGWAVE_STANDALONE ON)
    include(cmake/GitVars.cmake)
    include(cmake/BuildTypes.cmake)
else()
    set(GGWAVE_STANDALONE OFF)
endif()

if (EMSCRIPTEN)
    set(GGWAVE_SUPPORT_SDL2_DEFAULT ON)
    set(GGWAVE_LIBRARY_TYPE STATIC)
else()
    set(GGWAVE_SUPPORT_SDL2_DEFAULT ON)
    if (BUILD_SHARED_LIBS)
        set(GGWAVE_LIBRARY_TYPE SHARED)
    else ()
        set(GGWAVE_LIBRARY_TYPE STATIC)
    endif()
endif()

# options

option(USE_FINDSDL2                   "ggwave: use the FindSDL2.cmake script" OFF)

option(GGWAVE_ALL_WARNINGS            "ggwave: enable all compiler warnings" ON)
option(GGWAVE_ALL_WARNINGS_3RD_PARTY  "ggwave: enable all compiler warnings in 3rd party libs" ON)

option(GGWAVE_SANITIZE_THREAD         "ggwave: enable thread sanitizer" OFF)
option(GGWAVE_SANITIZE_ADDRESS        "ggwave: enable address sanitizer" OFF)
option(GGWAVE_SANITIZE_UNDEFINED      "ggwave: enable undefined sanitizer" OFF)

option(GGWAVE_SUPPORT_SDL2            "ggwave: support for libSDL2" ${GGWAVE_SUPPORT_SDL2_DEFAULT})

option(GGWAVE_BUILD_EXAMPLES          "ggwave: build examples" ${GGWAVE_STANDALONE})

# sanitizers

if (GGWAVE_SANITIZE_THREAD)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
endif()

if (GGWAVE_SANITIZE_ADDRESS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer -D_GLIBCXX_DEBUG")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -D_GLIBCXX_DEBUG")
endif()

if (GGWAVE_SANITIZE_UNDEFINED)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
endif()

# dependencies

# main

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (GGWAVE_ALL_WARNINGS)
    if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
    else()
        # todo : windows
    endif()
endif()

add_subdirectory(src)

if (GGWAVE_STANDALONE AND GGWAVE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if (GGWAVE_SUPPORT_SDL2)
    install(TARGETS waver RUNTIME DESTINATION bin)
    install(FILES examples/assets/fonts/DroidSans.ttf DESTINATION bin)
    install(FILES examples/assets/fonts/fontawesome-webfont.ttf DESTINATION bin)
endif()

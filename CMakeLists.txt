cmake_minimum_required(VERSION 3.16)
project(sdl_games C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(USE_MIXER "Enable sound effects in bullethell via SDL2_mixer" OFF)

find_package(SDL2 REQUIRED)

# Older SDL2 packages (e.g. SDL 2.0.10 on some Linux distros) only provide
# SDL2_INCLUDE_DIRS / SDL2_LIBRARIES instead of imported targets.
if(NOT TARGET SDL2::SDL2)
    add_library(SDL2::SDL2 INTERFACE IMPORTED)
    set_target_properties(SDL2::SDL2 PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES "${SDL2_LIBRARIES}")
endif()

if(USE_MIXER)
    find_package(SDL2_mixer REQUIRED)
endif()

function(link_sdl2 target)
    if(WIN32 AND TARGET SDL2::SDL2main)
        # MinGW needs mingw32 before SDL2main so that SDL2main's main/WinMain is picked up.
        if(MINGW)
            target_link_libraries(${target} PRIVATE mingw32)
        endif()
        target_link_libraries(${target} PRIVATE SDL2::SDL2main)
    endif()
    target_link_libraries(${target} PRIVATE SDL2::SDL2)
endfunction()

add_executable(breakout breakout.c)
link_sdl2(breakout)

add_executable(bullethell bullethell.c)
link_sdl2(bullethell)
if(NOT WIN32)
    target_link_libraries(bullethell PRIVATE m)
endif()
if(USE_MIXER)
    target_compile_definitions(bullethell PRIVATE USE_MIXER)
    if(TARGET SDL2_mixer::SDL2_mixer)
        target_link_libraries(bullethell PRIVATE SDL2_mixer::SDL2_mixer)
    else()
        target_link_libraries(bullethell PRIVATE SDL2_mixer::SDL2_mixer-static)
    endif()
endif()

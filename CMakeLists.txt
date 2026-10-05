cmake_minimum_required(VERSION 3.25)

project(windows_live_ime LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(WINDOWS_LIVE_IME_BUILD_TSF "Build the Windows TSF text service DLL" ON)

include(CTest)

function(windows_live_ime_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /Zc:__cplusplus /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_subdirectory(core)

if(WIN32)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "Windows Live IME currently supports x64 builds only.")
    endif()

    add_subdirectory(platform/windows/ime)
elseif(WINDOWS_LIVE_IME_BUILD_TSF)
    message(STATUS "The TSF DLL is Windows-only; building the OS-independent Core only.")
endif()

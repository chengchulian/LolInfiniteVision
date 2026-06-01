cmake_minimum_required(VERSION 3.23)

project(LolInfiniteVision VERSION 1.1 LANGUAGES CXX)

add_library(LolInfiniteVision SHARED
    src/Constant.cpp
    src/Constant.h
    src/dllmain.cpp
    src/GameTime.cpp
    src/GameTime.h
    src/Memory.cpp
    src/Memory.h
    src/Method.cpp
    src/Method.h
)

target_compile_features(LolInfiniteVision PRIVATE cxx_std_17)

target_compile_definitions(LolInfiniteVision
    PRIVATE
        UNICODE
        _UNICODE
        _WINDOWS
        _USRDLL
        LOLINFINITEVISION_EXPORTS
)

target_include_directories(LolInfiniteVision
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(LolInfiniteVision PRIVATE psapi)

if(MSVC)
    set_property(TARGET LolInfiniteVision PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )

    target_compile_options(LolInfiniteVision
        PRIVATE
            /W3
            /permissive-
            /sdl
            $<$<CONFIG:Release>:/GL>
            $<$<CONFIG:Release>:/Gy>
            $<$<CONFIG:Release>:/Oi>
    )

    target_link_options(LolInfiniteVision
        PRIVATE
            /DEBUG
            $<$<CONFIG:Release>:/LTCG>
    )
endif()

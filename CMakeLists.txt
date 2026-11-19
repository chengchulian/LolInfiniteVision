cmake_minimum_required(VERSION 3.23)

project(LolInfiniteVision VERSION 1.1 LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Build with MSVC (or clang-cl with the MSVC SDK) targeting Windows x64.")
endif()
enable_language(RC)
include(CTest)

add_library(LolInfiniteVision SHARED
    src/Constant.cpp
    src/Constant.h
    src/FeatureControls.cpp
    src/FeatureControls.h
    src/Hotkeys.h
    src/OpcodePatch.cpp
    src/OpcodePatch.h
    src/AttackRange.cpp
    src/AttackRange.h
    src/AttackRangeSignatures.h
    src/NativeOverlayBindings.cpp
    src/NativeOverlayBindings.h
    src/OverlayEventPump.cpp
    src/OverlayEventPump.h
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

target_link_libraries(LolInfiniteVision PRIVATE psapi user32)

add_custom_command(TARGET LolInfiniteVision POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/LolInfiniteVision.ini"
        "$<TARGET_FILE_DIR:LolInfiniteVision>/LolInfiniteVision.ini"
    VERBATIM
)

add_library(VisionDeployment STATIC installer/Deployment.cpp)
target_compile_features(VisionDeployment PUBLIC cxx_std_17)
target_include_directories(VisionDeployment PUBLIC installer)
target_compile_definitions(VisionDeployment PUBLIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)

set(assistant_rc "${CMAKE_CURRENT_BINARY_DIR}/assistant/$<CONFIG>/Assistant.rc")
add_custom_command(OUTPUT "${assistant_rc}"
    COMMAND ${CMAKE_COMMAND}
        "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DOUTPUT_RC=${assistant_rc}"
        "-DASSISTANT_PLUGIN=$<TARGET_FILE:LolInfiniteVision>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/GenerateAssistantResource.cmake"
    DEPENDS LolInfiniteVision installer/Assistant.rc.in installer/Assistant.manifest
        installer/resource.h installer/assets/version.dll installer/assets/dinput8.dll LolInfiniteVision.ini
        tools/GenerateAssistantResource.cmake
    VERBATIM)
add_executable(LolInfiniteVisionAssistant WIN32 installer/Assistant.cpp "${assistant_rc}")
target_link_libraries(LolInfiniteVisionAssistant PRIVATE VisionDeployment comctl32 shell32 ole32 uuid advapi32)
target_link_options(LolInfiniteVisionAssistant PRIVATE /MANIFEST:NO)

if(BUILD_TESTING)
    add_executable(OpcodePatchTests tests/OpcodePatchTests.cpp src/OpcodePatch.cpp src/Memory.cpp
        src/Method.cpp src/Constant.cpp)
    target_compile_features(OpcodePatchTests PRIVATE cxx_std_17)
    target_include_directories(OpcodePatchTests PRIVATE src)
    target_compile_definitions(OpcodePatchTests PRIVATE UNICODE _UNICODE)
    target_link_libraries(OpcodePatchTests PRIVATE psapi user32)
    add_test(NAME reversible-feature-patches COMMAND OpcodePatchTests)
    add_executable(HotkeyTests tests/HotkeyTests.cpp)
    target_compile_features(HotkeyTests PRIVATE cxx_std_17)
    target_include_directories(HotkeyTests PRIVATE src)
    add_test(NAME in-game-hotkeys COMMAND HotkeyTests)
    add_executable(AttackRangeTests tests/AttackRangeTests.cpp src/AttackRange.cpp
        src/NativeOverlayBindings.cpp src/OverlayEventPump.cpp)
    target_compile_features(AttackRangeTests PRIVATE cxx_std_17)
    target_include_directories(AttackRangeTests PRIVATE src)
    target_compile_definitions(AttackRangeTests PRIVATE UNICODE _UNICODE)
    target_link_libraries(AttackRangeTests PRIVATE user32)
    add_test(NAME native-attack-range-offline COMMAND AttackRangeTests)
    set_tests_properties(native-attack-range-offline PROPERTIES TIMEOUT 15)
    add_executable(DeploymentTests tests/DeploymentTests.cpp)
    target_link_libraries(DeploymentTests PRIVATE VisionDeployment)
    add_test(NAME plugin-deployment COMMAND DeploymentTests)
    add_test(NAME assistant-gui-startup COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_SOURCE_DIR}/tools/SmokeAssistant.ps1"
        -Executable "$<TARGET_FILE:LolInfiniteVisionAssistant>")
    add_test(NAME assistant-packaged-deployment COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_SOURCE_DIR}/tools/VerifyAssistant.ps1"
        -Executable "$<TARGET_FILE:LolInfiniteVisionAssistant>"
        -Plugin "$<TARGET_FILE:LolInfiniteVision>")
    set_tests_properties(assistant-gui-startup assistant-packaged-deployment PROPERTIES SKIP_RETURN_CODE 77)
    set_tests_properties(assistant-gui-startup PROPERTIES TIMEOUT 15)
    set_tests_properties(assistant-packaged-deployment PROPERTIES TIMEOUT 40)
endif()

if(MSVC)
    foreach(target VisionDeployment LolInfiniteVisionAssistant DeploymentTests AttackRangeTests HotkeyTests OpcodePatchTests)
        if(TARGET ${target})
            target_compile_options(${target} PRIVATE /utf-8 /W4 /permissive- /sdl)
            set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY
                "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        endif()
    endforeach()
    set_property(TARGET LolInfiniteVision PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )

    target_compile_options(LolInfiniteVision
        PRIVATE
            /utf-8
            /W3
            /permissive-
            /sdl
            $<$<CONFIG:Release>:/GL>
            $<$<CONFIG:Release>:/Gy>
            $<$<CONFIG:Release>:/Oi>
    )

    target_link_options(LolInfiniteVision
        PRIVATE
            $<$<CONFIG:Debug,RelWithDebInfo>:/DEBUG>
            $<$<CONFIG:Release>:/LTCG>
    )
endif()

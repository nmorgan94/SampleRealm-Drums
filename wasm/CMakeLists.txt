cmake_minimum_required(VERSION 3.25)

if(NOT CMAKE_TOOLCHAIN_FILE)
    execute_process(
        COMMAND em-config EMSCRIPTEN_ROOT
        OUTPUT_VARIABLE emscriptenRoot
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY)
    set(CMAKE_TOOLCHAIN_FILE "${emscriptenRoot}/cmake/Modules/Platform/Emscripten.cmake")
endif()

project(WebDemo LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)

include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/Modules.cmake)
include(${SampleRealm-Modules_SOURCE_DIR}/cmake/WasmDemo.cmake)

# A new .cpp in Source/dsp/ goes here by hand
add_wasm_demo(
    SOURCES
        Engine.cpp
        ../Source/dsp/ClickGenerator.cpp
        ../Source/dsp/CymbalVoice.cpp
        ../Source/dsp/Drive.cpp
        ../Source/dsp/DrumSynth.cpp
        ../Source/dsp/FxChain.cpp
        ../Source/dsp/KickVoice.cpp
        ../Source/dsp/PreviewRenderer.cpp
        ../Source/dsp/SnareVoice.cpp)

# The filters and oversampling
target_link_libraries(Engine PRIVATE juce::juce_dsp)

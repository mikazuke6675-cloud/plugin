cmake_minimum_required(VERSION 3.22)
project(Afterhours VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Uses a local JUCE checkout if present (../JUCE or ./JUCE), otherwise downloads JUCE 8.0.4.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.4.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(AFTERHOURS_FORMATS VST3 Standalone CACHE STRING "Plugin formats to build")
option(AFTERHOURS_BUILD_DEMO "Build the offline preset demo renderer" OFF)

juce_add_plugin(Afterhours
    COMPANY_NAME "Afterhours Audio"
    PRODUCT_NAME "Afterhours"
    BUNDLE_ID com.afterhoursaudio.afterhours
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Afhr
    PLUGIN_CODE Afh1
    VST3_CATEGORIES Instrument Synth
    FORMATS ${AFTERHOURS_FORMATS})

set(AFTERHOURS_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_sources(Afterhours PRIVATE ${AFTERHOURS_SOURCES})

target_compile_definitions(Afterhours PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Afterhours
    PRIVATE
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(AFTERHOURS_BUILD_DEMO)
    juce_add_console_app(AfterhoursDemo PRODUCT_NAME "AfterhoursDemo")
    target_sources(AfterhoursDemo PRIVATE tests/RenderDemo.cpp ${AFTERHOURS_SOURCES})
    target_compile_definitions(AfterhoursDemo PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(AfterhoursDemo PRIVATE
        juce::juce_audio_processors juce::juce_audio_formats juce::juce_dsp juce::juce_gui_basics
        juce::juce_recommended_config_flags)
endif()


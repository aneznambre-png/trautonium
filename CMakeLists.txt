cmake_minimum_required(VERSION 3.22)
project(Trautonium VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Universal binary so the AU loads in Logic Pro on both Apple Silicon and Intel
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Trautonium
    COMPANY_NAME "TrautoniumLab"
    PLUGIN_MANUFACTURER_CODE Trau
    PLUGIN_CODE Trtn
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Trautonium"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD TRUE)   # installs AU to ~/Library/Audio/Plug-Ins/Components, VST3 to ~/Library/Audio/Plug-Ins/VST3

target_sources(Trautonium PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Trautonium PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Trautonium
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

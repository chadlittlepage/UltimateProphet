cmake_minimum_required(VERSION 3.22)

project(UltimateProphet VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../JUCE ${CMAKE_BINARY_DIR}/JUCE)

juce_add_plugin(UltimateProphet
    COMPANY_NAME "CellDivision"
    PLUGIN_MANUFACTURER_CODE Cldv
    PLUGIN_CODE Upro
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "UltimateProphet"
    BUNDLE_ID "com.celldivision.ultimateprophet"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
)

target_sources(UltimateProphet
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/DSP/CEM3340Oscillator.cpp
        Source/DSP/CEM3320Filter.cpp
        Source/DSP/CEM3310Envelope.cpp
        Source/DSP/Prophet5Voice.cpp
        Source/DSP/SSM2040Filter.cpp
        Source/DSP/LFO.cpp
        Source/SysExLoader.cpp
        Source/UI/DebugConsole.cpp
)

juce_generate_juce_header(UltimateProphet)

target_include_directories(UltimateProphet
    PRIVATE
        Source
)

target_compile_definitions(UltimateProphet
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(UltimateProphet
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

cmake_minimum_required(VERSION 3.22)

project(WaveForge VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.12
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(WaveForge
    COMPANY_NAME            "WaveForge Audio"
    PLUGIN_MANUFACTURER_CODE WFrg
    PLUGIN_CODE             Wfg1
    FORMATS                 VST3 AU
    PRODUCT_NAME            "WaveForge"
    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES         "Instrument|Synth"
    AU_MAIN_TYPE            kAudioUnitType_MusicDevice
    BUNDLE_ID               com.waveforgeaudio.waveforge
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(WaveForge)

target_sources(WaveForge PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/WavetableEngine.cpp
    Source/SampleAnalyzer.cpp
    Source/SynthVoice.cpp
    Source/ADSRVisualizer.cpp
    Source/WaveformDisplay.cpp
    Source/LookAndFeel.cpp
)

target_compile_definitions(WaveForge PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_MODAL_LOOPS_PERMITTED=1
)

target_link_libraries(WaveForge PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_warning_flags
)

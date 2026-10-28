cmake_minimum_required(VERSION 3.22)

project(XOST VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    endif()
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
endif()

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(XOST_FORMATS VST3 AU)
else()
    set(XOST_FORMATS VST3)
endif()

juce_add_plugin(XOST
    COMPANY_NAME "XO Audio"
    BUNDLE_ID com.xoaudio.xostsynth
    PLUGIN_MANUFACTURER_CODE Xosy
    PLUGIN_CODE Xst1
    FORMATS ${XOST_FORMATS}
    PRODUCT_NAME "XOST"
    DESCRIPTION "Text-to-sound synth"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

juce_add_binary_data(XOSTData SOURCES
    Resources/SpaceGrotesk-Bold.ttf
    Resources/Inter-Medium.ttf
    Resources/Inter-Bold.ttf
    Resources/FactoryPresets.txt
    Resources/MatrixPresets.txt
    Resources/RackPresets.txt
    Resources/SeedPatches.txt
    Resources/Lexicon.txt)

set(XOST_SOURCES
    Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Presets.cpp
    Source/PresetBrowser.cpp Source/XOLookAndFeel.cpp Source/Wavetables.cpp
    Source/Engine.cpp Source/Widgets.cpp Source/ToolPages.cpp
    Source/WordEngine.cpp Source/SampleTools.cpp Source/CreatePanel.cpp)

target_sources(XOST PRIVATE ${XOST_SOURCES})
if(MSVC)
    target_compile_options(XOST PRIVATE /bigobj /utf-8)
endif()
target_include_directories(XOST PRIVATE Source)
target_compile_definitions(XOST PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(XOST
    PRIVATE juce::juce_audio_utils juce::juce_dsp XOSTData
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

option(XOST_TESTS "Build the offline test harness" OFF)
if(XOST_TESTS)
    juce_add_console_app(XOSTTest PRODUCT_NAME "XOSTTest")
    target_sources(XOSTTest PRIVATE Tests/TestMain.cpp ${XOST_SOURCES})
    target_include_directories(XOSTTest PRIVATE Source)
    target_compile_definitions(XOSTTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="XOST" JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(XOSTTest PRIVATE juce::juce_audio_utils juce::juce_dsp XOSTData
                          PUBLIC juce::juce_recommended_config_flags)
endif()

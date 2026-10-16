cmake_minimum_required(VERSION 3.22)
project(VoxWarp VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
  add_subdirectory(JUCE)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(VoxWarp
  COMPANY_NAME "Tell Your Friends"
  COMPANY_WEBSITE "https://tellyourfriends.tv"
  BUNDLE_ID com.tellyourfriends.voxwarp
  PLUGIN_MANUFACTURER_CODE Tyft
  PLUGIN_CODE Vxwp
  FORMATS VST3
  PRODUCT_NAME "Vox Warp"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Fx Pitch Shift)

juce_add_binary_data(VoxWarpData SOURCES Resources/Michroma.ttf Resources/ChakraPetchBold.ttf Resources/Logo.png)

target_sources(VoxWarp PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(VoxWarp PRIVATE Source)
set(VOXWARP_BUILD "dev" CACHE STRING "build number shown in the UI")
target_compile_definitions(VoxWarp PUBLIC
  VOXWARP_BUILD="${VOXWARP_BUILD}"
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(VoxWarp
  PRIVATE VoxWarpData juce::juce_audio_utils juce::juce_dsp
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

if(VOXWARP_TESTS)
  juce_add_console_app(VoxWarpTest PRODUCT_NAME "VoxWarpTest")
  target_sources(VoxWarpTest PRIVATE test/TestMain.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
  target_compile_definitions(VoxWarpTest PRIVATE VOXWARP_BUILD="test" VW_DEBUG=1 JucePlugin_Name="Vox Warp" JucePlugin_Build_VST3=0 JUCE_STANDALONE_APPLICATION=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(VoxWarpTest PRIVATE VoxWarpData juce::juce_audio_utils juce::juce_dsp juce::juce_audio_processors)
endif()

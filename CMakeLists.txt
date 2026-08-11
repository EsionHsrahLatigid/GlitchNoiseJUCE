cmake_minimum_required(VERSION 3.22)

project(GlitchNoiseJUCE VERSION 0.1.1 LANGUAGES C CXX OBJC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GLITCHNOISEJUCE_BUILD_PLUGIN "Build the JUCE plug-in targets" ON)
option(GLITCHNOISEJUCE_BUILD_TESTS "Build the integration tests" ON)
set(GLITCHNOISEJUCE_ARTIFACT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/artifacts" CACHE PATH
    "Directory for final VST3, AU, and Standalone products")

if(GLITCHNOISEJUCE_BUILD_PLUGIN)
  include(FetchContent)

  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
  else()
    FetchContent_Declare(
      JUCE
      GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
      GIT_TAG 8.0.15
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
  endif()

  set(GLITCHNOISEJUCE_FORMATS VST3 Standalone)
  if(APPLE)
    list(APPEND GLITCHNOISEJUCE_FORMATS AU)
  endif()

  juce_add_plugin(GlitchNoiseJUCE
    VERSION ${PROJECT_VERSION}
    COMPANY_NAME "EsionHsrahLatigid"
    BUNDLE_ID "jp.ehl.glitchnoisejuce"
    PRODUCT_NAME "GlitchNoiseJUCE"
    DESCRIPTION "Integer and bitwise digital glitch noise effect"
    PLUGIN_MANUFACTURER_CODE EHL_
    PLUGIN_CODE GlNJ
    FORMATS ${GLITCHNOISEJUCE_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

  juce_generate_juce_header(GlitchNoiseJUCE)

  target_sources(GlitchNoiseJUCE PRIVATE
    Source/GlitchEngine.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PresetBank.h)

  target_compile_definitions(GlitchNoiseJUCE
    PUBLIC
      JUCE_REPORT_APP_USAGE=0
      JUCE_WEB_BROWSER=1
      JUCE_USE_CURL=0
      JUCE_VST3_CAN_REPLACE_VST2=0)

  target_link_libraries(GlitchNoiseJUCE
    PRIVATE
      juce::juce_audio_utils
      juce::juce_dsp
      juce::juce_gui_extra
    PUBLIC
      juce::juce_recommended_config_flags
      juce::juce_recommended_lto_flags
      juce::juce_recommended_warning_flags)

  if(APPLE)
    set_target_properties(GlitchNoiseJUCE PROPERTIES
      XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC YES)
  endif()

  set(glitchnoise_juce_products "${CMAKE_CURRENT_BINARY_DIR}/GlitchNoiseJUCE_artefacts/$<CONFIG>")
  set(glitchnoise_staged_products "${GLITCHNOISEJUCE_ARTIFACT_DIR}/$<CONFIG>")
  set(glitchnoise_artifact_dependencies GlitchNoiseJUCE_VST3 GlitchNoiseJUCE_Standalone)
  set(glitchnoise_artifact_commands
    COMMAND ${CMAKE_COMMAND} -E make_directory "${glitchnoise_staged_products}"
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${glitchnoise_staged_products}/VST3"
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${glitchnoise_staged_products}/Standalone"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${glitchnoise_juce_products}/VST3" "${glitchnoise_staged_products}/VST3"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${glitchnoise_juce_products}/Standalone" "${glitchnoise_staged_products}/Standalone")

  if(TARGET GlitchNoiseJUCE_AU)
    list(APPEND glitchnoise_artifact_dependencies GlitchNoiseJUCE_AU)
    list(APPEND glitchnoise_artifact_commands
      COMMAND ${CMAKE_COMMAND} -E remove_directory "${glitchnoise_staged_products}/AU"
      COMMAND ${CMAKE_COMMAND} -E copy_directory
              "${glitchnoise_juce_products}/AU" "${glitchnoise_staged_products}/AU")
  endif()

  if(APPLE)
    list(APPEND glitchnoise_artifact_commands
      COMMAND codesign --force --deep --sign -
              "${glitchnoise_staged_products}/VST3/GlitchNoiseJUCE.vst3"
      COMMAND codesign --force --deep --sign -
              "${glitchnoise_staged_products}/Standalone/GlitchNoiseJUCE.app"
      COMMAND codesign --force --deep --sign -
              "${glitchnoise_staged_products}/AU/GlitchNoiseJUCE.component")
  endif()

  add_custom_target(GlitchNoiseJUCE_Artifacts
    ${glitchnoise_artifact_commands}
    DEPENDS ${glitchnoise_artifact_dependencies}
    COMMENT "Staging GlitchNoiseJUCE products in ${GLITCHNOISEJUCE_ARTIFACT_DIR}/$<CONFIG>")

  if(GLITCHNOISEJUCE_BUILD_TESTS)
    enable_testing()
    add_executable(GlitchNoiseJUCEIntegrationTests Tests/PluginIntegrationTest.cpp)
    target_include_directories(GlitchNoiseJUCEIntegrationTests PRIVATE
      Source
      "${CMAKE_CURRENT_BINARY_DIR}/GlitchNoiseJUCE_artefacts/JuceLibraryCode")
    target_link_libraries(GlitchNoiseJUCEIntegrationTests PRIVATE
      GlitchNoiseJUCE
      juce::juce_audio_utils)
    add_dependencies(GlitchNoiseJUCEIntegrationTests GlitchNoiseJUCE_Artifacts)
    add_test(NAME GlitchNoiseJUCEPluginIntegrationTests COMMAND GlitchNoiseJUCEIntegrationTests)
    add_test(
      NAME GlitchNoiseJUCEArtifactChecks
      COMMAND ${CMAKE_COMMAND}
              -DGLITCHNOISEJUCE_STAGED_DIR=${GLITCHNOISEJUCE_ARTIFACT_DIR}/$<CONFIG>
              -DGLITCHNOISEJUCE_EXPECT_AU=$<BOOL:${APPLE}>
              -P ${CMAKE_CURRENT_SOURCE_DIR}/Tests/VerifyArtifacts.cmake)
  endif()
endif()

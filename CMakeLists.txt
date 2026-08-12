cmake_minimum_required(VERSION 3.22)

project(GlitchNoiseJUCE VERSION 0.1.1 LANGUAGES C CXX)

if(APPLE)
  enable_language(OBJC)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GLITCHNOISEJUCE_BUILD_PLUGIN "Build the JUCE plug-in targets" ON)
option(GLITCHNOISEJUCE_BUILD_TESTS "Build the integration tests" ON)
set(GLITCHNOISEJUCE_ARTIFACT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/artifacts" CACHE PATH
    "Directory for final VST3, AU, and Standalone products")
set(EHL_JUCE_SOURCE_DIR "" CACHE PATH "Existing JUCE source checkout; avoids network when set")

if(GLITCHNOISEJUCE_BUILD_PLUGIN)
  include(FetchContent)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)

  if(EHL_JUCE_SOURCE_DIR)
    if(NOT EXISTS "${EHL_JUCE_SOURCE_DIR}/CMakeLists.txt")
      message(FATAL_ERROR "EHL_JUCE_SOURCE_DIR does not contain JUCE: ${EHL_JUCE_SOURCE_DIR}")
    endif()
    add_subdirectory("${EHL_JUCE_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/juce")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
  else()
    FetchContent_Declare(
      JUCE
      GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
      GIT_TAG 8.0.15
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
  endif()
  add_subdirectory(modules/juce-ehl-design-module)

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
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
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
      JUCE_WEB_BROWSER=0
      JUCE_USE_CURL=0
      JUCE_VST3_CAN_REPLACE_VST2=0)

  target_link_libraries(GlitchNoiseJUCE
    PRIVATE
      juce::juce_audio_utils
      juce::juce_dsp
      EHL::JuceDesign
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
    COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/NormalizeVst3ModuleInfo.py"
            "${glitchnoise_staged_products}/VST3/GlitchNoiseJUCE.vst3/Contents/Resources/moduleinfo.json"
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
    set(ehl_stage_platform "linux-x64")
    if(APPLE)
        set(ehl_stage_platform "macos-arm64")
    elseif(WIN32)
        set(ehl_stage_platform "windows-x64")
    endif()
    set(ehl_stage_source_products "${GLITCHNOISEJUCE_ARTIFACT_DIR}/$<CONFIG>")
    set(ehl_staged_products "${GLITCHNOISEJUCE_ARTIFACT_DIR}/plugin-release/${ehl_stage_platform}")

    set(ehl_stage_commands
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ehl_staged_products}/vst3"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ehl_staged_products}/standalone"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ehl_staged_products}/au")

    if(APPLE)
        list(APPEND ehl_stage_commands
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${ehl_staged_products}/vst3/glitchnoisejuce_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${ehl_staged_products}/standalone/glitchnoisejuce_standalone_plugin.app"
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${ehl_staged_products}/au/glitchnoisejuce_au_plugin.component"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${ehl_stage_source_products}/VST3/GlitchNoiseJUCE.vst3" "${ehl_staged_products}/vst3/glitchnoisejuce_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${ehl_stage_source_products}/Standalone/GlitchNoiseJUCE.app" "${ehl_staged_products}/standalone/glitchnoisejuce_standalone_plugin.app"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${ehl_stage_source_products}/AU/GlitchNoiseJUCE.component" "${ehl_staged_products}/au/glitchnoisejuce_au_plugin.component")
    elseif(WIN32)
        list(APPEND ehl_stage_commands
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${ehl_staged_products}/vst3/glitchnoisejuce_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E rm -f "${ehl_staged_products}/standalone/glitchnoisejuce_standalone_plugin.exe"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${ehl_stage_source_products}/VST3/GlitchNoiseJUCE.vst3" "${ehl_staged_products}/vst3/glitchnoisejuce_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy "${ehl_stage_source_products}/Standalone/GlitchNoiseJUCE.exe" "${ehl_staged_products}/standalone/glitchnoisejuce_standalone_plugin.exe")
    else()
        list(APPEND ehl_stage_commands
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${ehl_staged_products}/vst3/glitchnoisejuce_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E rm -f "${ehl_staged_products}/standalone/glitchnoisejuce_standalone_plugin"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${ehl_stage_source_products}/VST3/GlitchNoiseJUCE.vst3" "${ehl_staged_products}/vst3/glitchnoisejuce_vst3_plugin.vst3"
            COMMAND ${CMAKE_COMMAND} -E copy "${ehl_stage_source_products}/Standalone/GlitchNoiseJUCE" "${ehl_staged_products}/standalone/glitchnoisejuce_standalone_plugin")
    endif()

    list(APPEND ehl_stage_commands
        COMMAND ${CMAKE_COMMAND}
            -DPRODUCT_NAME=GlitchNoiseJUCE
            -DSLUG=glitchnoisejuce
            -DBUNDLE_ID=jp.ehl.glitchnoisejuce
            -DPLUGIN_CODE=GlNJ
            -DSTAGE_DIR=${ehl_staged_products}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/WriteArtifactsManifest.cmake")

    add_custom_target(ehl_stage_products
        ${ehl_stage_commands}
        DEPENDS GlitchNoiseJUCE_Artifacts
        COMMENT "Staging GlitchNoiseJUCE CI release products under ${GLITCHNOISEJUCE_ARTIFACT_DIR}/plugin-release")


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
    add_executable(GlitchNoiseJUCEEditorTests Tests/EditorTests.cpp)
    target_include_directories(GlitchNoiseJUCEEditorTests PRIVATE
      Source
      "${CMAKE_CURRENT_BINARY_DIR}/GlitchNoiseJUCE_artefacts/JuceLibraryCode")
    target_link_libraries(GlitchNoiseJUCEEditorTests PRIVATE
      GlitchNoiseJUCE
      EHL::JuceDesign
      juce::juce_audio_utils)
    add_test(NAME GlitchNoiseJUCEEditorTests COMMAND GlitchNoiseJUCEEditorTests)
    add_test(
      NAME GlitchNoiseJUCEArtifactChecks
      COMMAND ${CMAKE_COMMAND}
              -DGLITCHNOISEJUCE_STAGED_DIR=${GLITCHNOISEJUCE_ARTIFACT_DIR}/$<CONFIG>
              -DGLITCHNOISEJUCE_EXPECT_AU=$<BOOL:${APPLE}>
              -DPython3_EXECUTABLE=${Python3_EXECUTABLE}
              -P ${CMAKE_CURRENT_SOURCE_DIR}/Tests/VerifyArtifacts.cmake)
  endif()
endif()

if(NOT DEFINED GLITCHNOISEJUCE_STAGED_DIR)
    message(FATAL_ERROR "GLITCHNOISEJUCE_STAGED_DIR is required")
endif()

set(vst3 "${GLITCHNOISEJUCE_STAGED_DIR}/VST3/GlitchNoiseJUCE.vst3")
if(GLITCHNOISEJUCE_EXPECT_AU)
    set(standalone "${GLITCHNOISEJUCE_STAGED_DIR}/Standalone/GlitchNoiseJUCE.app")
elseif(WIN32)
    set(standalone "${GLITCHNOISEJUCE_STAGED_DIR}/Standalone/GlitchNoiseJUCE.exe")
else()
    set(standalone "${GLITCHNOISEJUCE_STAGED_DIR}/Standalone/GlitchNoiseJUCE")
endif()

if(NOT EXISTS "${vst3}")
    message(FATAL_ERROR "Missing staged VST3: ${vst3}")
endif()

if(NOT EXISTS "${standalone}")
    message(FATAL_ERROR "Missing staged Standalone app: ${standalone}")
endif()

if(GLITCHNOISEJUCE_EXPECT_AU)
    set(module_info "${vst3}/Contents/Resources/moduleinfo.json")
    set(au "${GLITCHNOISEJUCE_STAGED_DIR}/AU/GlitchNoiseJUCE.component")

    if(NOT EXISTS "${module_info}")
        message(FATAL_ERROR "Missing staged VST3 moduleinfo.json: ${module_info}")
    endif()

    if(NOT EXISTS "${au}")
        message(FATAL_ERROR "Missing staged AU: ${au}")
    endif()

    foreach(bundle IN ITEMS "${vst3}" "${standalone}" "${au}")
        execute_process(
            COMMAND codesign --verify --deep --strict "${bundle}"
            RESULT_VARIABLE codesign_result
            ERROR_VARIABLE codesign_error)
        if(NOT codesign_result EQUAL 0)
            message(FATAL_ERROR "Invalid signature for ${bundle}: ${codesign_error}")
        endif()
    endforeach()
endif()

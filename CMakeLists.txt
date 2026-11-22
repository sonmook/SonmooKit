cmake_minimum_required(VERSION 3.22)
project(SonmooKit VERSION 0.8.0 LANGUAGES C CXX)
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)
function(add_effect target code kind category)
    if(kind EQUAL 9)
        set(effect_options NEEDS_MIDI_INPUT TRUE PRODUCT_NAME "SonmooTune" VERSION 0.8.1)
    endif()
    juce_add_plugin(${target}
        COMPANY_NAME "SonmooKit"
        BUNDLE_ID "org.audiokit.${target}"
        PLUGIN_MANUFACTURER_CODE Akfx
        PLUGIN_CODE ${code}
        ${effect_options}
        FORMATS VST3
        VST3_CATEGORIES Fx ${category}
        COPY_PLUGIN_AFTER_BUILD FALSE)
    if(kind GREATER_EQUAL 5 AND kind LESS_EQUAL 7)
        target_sources(${target} PRIVATE Advanced.cpp)
        target_link_libraries(${target} PRIVATE juce::juce_dsp)
    elseif(kind EQUAL 9)
        target_sources(${target} PRIVATE PitchPilot.cpp)
    else()
        target_sources(${target} PRIVATE Plugin.cpp)
    endif()
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_compile_definitions(${target} PRIVATE EFFECT_KIND=${kind}
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(${target} PRIVATE juce::juce_audio_processors)
endfunction()
add_effect(StereoEcho Stec 0 Delay)
add_effect(SoftCompressor Sfcp 1 Dynamics)
add_effect(PeakCeiling Pkcl 2 Dynamics)
add_effect(SibilanceControl Sbct 3 Dynamics)
add_effect(DriveTone Drtn 4 Distortion)
add_effect(CurveTen Crtn 5 EQ)
add_effect(VoiceStack Vstk 6 Delay)
add_effect(PulseEQ Plsq 7 EQ)
add_effect(FETStrike Ftst 8 Dynamics)
add_effect(PitchPilot PtPl 9 PitchShift)

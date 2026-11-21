cmake_minimum_required(VERSION 3.22)
project(AudioKit VERSION 0.1.0 LANGUAGES C CXX)
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)
function(add_effect target code kind category)
    juce_add_plugin(${target}
        COMPANY_NAME "AudioKit"
        BUNDLE_ID "org.audiokit.${target}"
        PLUGIN_MANUFACTURER_CODE Akfx
        PLUGIN_CODE ${code}
        FORMATS VST3
        VST3_CATEGORIES Fx ${category}
        COPY_PLUGIN_AFTER_BUILD FALSE)
    target_sources(${target} PRIVATE Plugin.cpp)
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_compile_definitions(${target} PRIVATE EFFECT_KIND=${kind}
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(${target} PRIVATE juce::juce_audio_processors)
endfunction()
add_effect(StereoEcho Stec 0 Delay)
add_effect(SoftCompressor Sfcp 1 Dynamics)
add_effect(PeakCeiling Pkcl 2 Dynamics)

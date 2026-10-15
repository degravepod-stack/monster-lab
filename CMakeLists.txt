cmake_minimum_required(VERSION 3.22)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

project(MonsterLab VERSION 0.4.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(MONSTERLAB_BUILD_TESTS "Build the command-line render test tool" OFF)

# ---------------------------------------------------------------- JUCE
# Use a local checkout if one is supplied (-DJUCE_DIR=...), otherwise download it.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} juce)
else()
    include(FetchContent)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.10
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
endif()

# ---------------------------------------------------------------- Rubber Band (GPL, vendored)
add_library(rubberband STATIC ThirdParty/rubberband/single/RubberBandSingle.cpp)
target_include_directories(rubberband PUBLIC ThirdParty/rubberband)
set_target_properties(rubberband PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(APPLE)
    target_link_libraries(rubberband PUBLIC "-framework Accelerate")
endif()

set(ENGINE_SOURCES
    Source/Engine/Model.cpp
    Source/Engine/Renderer.cpp)

# ---------------------------------------------------------------- The app
juce_add_gui_app(MonsterLab
    PRODUCT_NAME "Monster Lab"
    COMPANY_NAME "DeGrave Brothers"
    BUNDLE_ID "com.degrave.monsterlab"
    VERSION 0.4.0
    MICROPHONE_PERMISSION_ENABLED FALSE)

juce_generate_juce_header(MonsterLab)

target_sources(MonsterLab PRIVATE
    Source/Main.cpp
    Source/UI/MainComponent.cpp
    ${ENGINE_SOURCES})

target_include_directories(MonsterLab PRIVATE Source Source/Engine Source/UI)

target_compile_definitions(MonsterLab PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:MonsterLab,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:MonsterLab,JUCE_VERSION>")

target_link_libraries(MonsterLab
    PRIVATE
        rubberband
        juce::juce_gui_extra
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- Test tool
if(MONSTERLAB_BUILD_TESTS)
    juce_add_console_app(MonsterLabRenderTest PRODUCT_NAME "MonsterLabRenderTest")
    juce_generate_juce_header(MonsterLabRenderTest)
    target_sources(MonsterLabRenderTest PRIVATE Tools/RenderTest.cpp ${ENGINE_SOURCES})
    target_include_directories(MonsterLabRenderTest PRIVATE Source Source/Engine)
    target_compile_definitions(MonsterLabRenderTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(MonsterLabRenderTest PRIVATE rubberband
        juce::juce_audio_formats juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()

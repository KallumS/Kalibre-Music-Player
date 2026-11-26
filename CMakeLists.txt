cmake_minimum_required(VERSION 3.22)

project(KalibrePlayer VERSION 0.3.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(APPLE)
    # Apple silicon only, as Kalibre (decision 0001).
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Apple silicon")
endif()

if(MSVC)
    # Windows (decision 0020): the C++ runtime inside the .exe, so nothing else needs
    # installing; sources are UTF-8; Windows' min and max macros kept out of std::min.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/utf-8 /bigobj /Zc:__cplusplus)
    add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS _USE_MATH_DEFINES WIN32_LEAN_AND_MEAN)
endif()

option(KP_BUILD_APP   "Build the app (fetches JUCE, TagLib, libarchive and builds FFmpeg)" ON)
option(KP_BUILD_TESTS "Build the tests" ON)

#==============================================================================
# Lua 5.4, vendored from Kalibre: add-ons (decision 0012). Built as C++ so an
# error in an add-on unwinds as an exception, as in Kalibre (its 0068).
#==============================================================================
file(GLOB LUA_SOURCES ThirdParty/lua/*.c)
list(REMOVE_ITEM LUA_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/lua/lua.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/lua/luac.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/lua/onelua.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/lua/ltests.c)
set_source_files_properties(${LUA_SOURCES} PROPERTIES LANGUAGE CXX)
add_library(KPLua STATIC ${LUA_SOURCES})
target_include_directories(KPLua PUBLIC ThirdParty/lua)
set_target_properties(KPLua PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(APPLE)
    target_compile_definitions(KPLua PRIVATE LUA_USE_MACOSX)
elseif(UNIX)
    target_compile_definitions(KPLua PRIVATE LUA_USE_LINUX)
endif()

#==============================================================================
# The core: tracks, title formatting, queries, the library, playlists,
# ReplayGain, audio CDs, tagging, the layout and add-ons. No JUCE: it builds
# and tests in seconds.
#==============================================================================
add_library(KPCore STATIC
    Source/Core/Track.cpp
    Source/Core/TitleFormat.cpp
    Source/Core/Query.cpp
    Source/Core/Library.cpp
    Source/Core/Playlists.cpp
    Source/Core/ReplayGain.cpp
    Source/Core/AudioCd.cpp
    Source/Core/Tagging.cpp
    Source/Core/Layout.cpp
    Source/Core/Loudness.cpp
    Source/Core/Addons.cpp)
target_include_directories(KPCore PUBLIC Source/Core ThirdParty)
target_link_libraries(KPCore PUBLIC KPLua)
set_target_properties(KPCore PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(KPCore PRIVATE -Wall -Wextra)
endif()

if(KP_BUILD_TESTS)
    enable_testing()
    add_executable(KPTests
        Tests/TestMain.cpp
        Tests/Core/TestTitleFormat.cpp
        Tests/Core/TestQuery.cpp
        Tests/Core/TestLibrary.cpp
        Tests/Core/TestPlaylists.cpp
        Tests/Core/TestReplayGain.cpp
        Tests/Core/TestAudioCd.cpp
        Tests/Core/TestTagging.cpp
        Tests/Core/TestLayout.cpp
        Tests/Core/TestAddons.cpp)
    target_include_directories(KPTests PRIVATE Tests)
    target_link_libraries(KPTests PRIVATE KPCore)
    add_test(NAME Core COMMAND KPTests)
endif()

if(NOT KP_BUILD_APP)
    return()
endif()

include(cmake/App.cmake)

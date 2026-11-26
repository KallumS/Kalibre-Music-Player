# The app: JUCE (as Kalibre, at the same version), TagLib, libarchive and
# FFmpeg (with LAME and libopus), all at fixed versions. To use copies
# already on disk, pass -DFETCHCONTENT_SOURCE_DIR_JUCE=... (and _TAGLIB,
# _LIBARCHIVE) and -DKP_FFMPEG_DIR=...

include(FetchContent)

FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.5
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(juce)

include(cmake/FFmpeg.cmake)

#==============================================================================
# TagLib (LGPL-2.1/MPL-1.1): every format's tags (decision 0009).
FetchContent_Declare(taglib
    GIT_REPOSITORY https://github.com/taglib/taglib.git
    GIT_TAG        v2.3.2
    GIT_SHALLOW    TRUE
    GIT_SUBMODULES 3rdparty/utfcpp)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_BINDINGS OFF CACHE BOOL "" FORCE)
set(WITH_ZLIB OFF CACHE BOOL "" FORCE)
FetchContent_GetProperties(taglib)
if(NOT taglib_POPULATED)
    FetchContent_Populate(taglib)
endif()
add_subdirectory(${taglib_SOURCE_DIR} ${taglib_BINARY_DIR} EXCLUDE_FROM_ALL)
# TagLib's headers as an install would lay them out (<taglib/fileref.h>), for its build tree has no such folder.
file(GLOB_RECURSE TAGLIB_HEADERS ${taglib_SOURCE_DIR}/taglib/*.h ${taglib_SOURCE_DIR}/taglib/*.tcc)
file(GLOB TAGLIB_MADE_HEADERS ${taglib_BINARY_DIR}/*.h ${taglib_BINARY_DIR}/taglib/*.h)
file(COPY ${TAGLIB_HEADERS} ${TAGLIB_MADE_HEADERS} DESTINATION ${CMAKE_BINARY_DIR}/taglib-include/taglib)
add_library(KPTagLib INTERFACE)
target_include_directories(KPTagLib INTERFACE ${CMAKE_BINARY_DIR}/taglib-include ${CMAKE_BINARY_DIR}/taglib-include/taglib)
target_compile_definitions(KPTagLib INTERFACE TAGLIB_STATIC)
target_link_libraries(KPTagLib INTERFACE tag)

#==============================================================================
# libarchive (BSD): music inside ZIP, RAR, 7-Zip, tar and gzip (decision 0004).
FetchContent_Declare(libarchive
    GIT_REPOSITORY https://github.com/libarchive/libarchive.git
    GIT_TAG        v3.8.9
    GIT_SHALLOW    TRUE)
foreach(opt ENABLE_OPENSSL ENABLE_MBEDTLS ENABLE_NETTLE ENABLE_LIBB2 ENABLE_LZ4 ENABLE_LZO ENABLE_LZMA ENABLE_ZSTD ENABLE_BZip2
            ENABLE_LIBXML2 ENABLE_EXPAT ENABLE_WIN32_XMLLITE ENABLE_PCREPOSIX ENABLE_PCRE2POSIX ENABLE_LIBGCC ENABLE_CNG ENABLE_TAR
            ENABLE_CPIO ENABLE_CAT ENABLE_UNZIP ENABLE_XATTR ENABLE_ACL ENABLE_TEST ENABLE_INSTALL)
    set(${opt} OFF CACHE BOOL "" FORCE)
endforeach()
set(POSIX_REGEX_LIB NONE CACHE STRING "" FORCE)   # patterns are for its own tools; Windows has no regex.h
set(ENABLE_ZLIB ON CACHE BOOL "" FORCE)    # ZIP's deflate and gzip: the system's zlib
set(ENABLE_ICONV ON CACHE BOOL "" FORCE)   # names in old ZIPs
FetchContent_GetProperties(libarchive)
if(NOT libarchive_POPULATED)
    FetchContent_Populate(libarchive)
endif()
add_subdirectory(${libarchive_SOURCE_DIR} ${libarchive_BINARY_DIR} EXCLUDE_FROM_ALL)
if(NOT MSVC)
    target_compile_options(archive_static PRIVATE -Wno-error -w)   # its own warnings aren't ours to fix
endif()
add_library(KPArchive INTERFACE)
target_include_directories(KPArchive INTERFACE ${libarchive_SOURCE_DIR}/libarchive)
target_compile_definitions(KPArchive INTERFACE LIBARCHIVE_STATIC)
target_link_libraries(KPArchive INTERFACE archive_static)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

#==============================================================================
# The engine and the window, shared by the app, its tests and the renderer.
add_library(KPApp STATIC
    Source/Engine/Decoder.cpp
    Source/Engine/Encoder.cpp
    Source/Engine/Sources.cpp
    Source/Engine/TagIO.cpp
    Source/Engine/Dsp.cpp
    Source/Engine/Player.cpp
    Source/Engine/Plugins.cpp
    Source/Engine/LibraryScanner.cpp
    Source/Engine/FolderWatcher.cpp
    Source/Engine/Jobs.cpp
    Source/Engine/CdDrive.cpp
    Source/Engine/CdRom.cpp
    Source/App/Theme.cpp
    Source/App/Settings.cpp
    Source/App/Controller.cpp
    Source/App/Commands.cpp
    Source/App/Panels.cpp
    Source/App/PlaylistView.cpp
    Source/App/LibraryView.cpp
    Source/App/Visualisers.cpp
    Source/App/TransportBar.cpp
    Source/App/MiniPlayer.cpp
    Source/App/EqualizerView.cpp
    Source/App/LayoutView.cpp
    Source/App/Dialogs.cpp
    Source/App/TagEditor.cpp
    Source/App/ConvertDialog.cpp
    Source/App/Preferences.cpp
    Source/App/MainComponent.cpp)

if(APPLE)
    target_sources(KPApp PRIVATE Source/App/FilePanelsMac.mm Source/Engine/FolderWatcherMac.mm Source/App/MediaKeysMac.mm)
    target_link_libraries(KPApp PRIVATE "-framework CoreServices" "-framework MediaPlayer" "-framework AppKit")
    target_link_libraries(KPApp PUBLIC iconv)
elseif(WIN32)
    # Windows (decision 0020): folders watched by ReadDirectoryChangesW, media keys as hot keys.
    target_sources(KPApp PRIVATE Source/App/FilePanelsOther.cpp Source/Engine/FolderWatcherWin.cpp Source/App/MediaKeysWin.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(KPApp PRIVATE Source/App/FilePanelsOther.cpp Source/Engine/FolderWatcherLinux.cpp Source/App/MediaKeysOther.cpp)
else()
    target_sources(KPApp PRIVATE Source/App/FilePanelsOther.cpp Source/Engine/FolderWatcherOther.cpp Source/App/MediaKeysOther.cpp)
endif()

target_include_directories(KPApp PUBLIC Source/App Source/Engine)
target_link_libraries(KPApp
    PUBLIC
        KPCore
        KPFFmpeg
        KPTagLib
        KPArchive
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
    PRIVATE
        juce::juce_recommended_config_flags)

target_compile_definitions(KPApp PUBLIC
    JUCE_WEB_BROWSER=0
    # Linux reaches MusicBrainz through libcurl, loaded when it's first needed (so it isn't needed to start).
    JUCE_USE_CURL=$<IF:$<PLATFORM_ID:Linux>,1,0>
    JUCE_LOAD_CURL_SYMBOLS_LAZILY=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=1
    JUCE_PLUGINHOST_LV2=0
    JUCE_PLUGINHOST_LADSPA=0
    JUCE_USE_MP3AUDIOFORMAT=0
    JUCE_USE_FLAC=1
    JUCE_USE_OGGVORBIS=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    KP_VERSION="${PROJECT_VERSION}")

#==============================================================================
juce_add_gui_app(KalibrePlayer
    PRODUCT_NAME          "Kalibre Player"
    COMPANY_NAME          "Kallum Shah"
    BUNDLE_ID             "com.kallumshah.kalibreplayer"
    VERSION               ${PROJECT_VERSION}
    DOCUMENT_EXTENSIONS   mp1 mp2 mp3 mpc aac m4a wma ogg oga flac wv wav aif aiff au snd spx opus ape m3u m3u8 pls
    ICON_BIG              "${CMAKE_SOURCE_DIR}/Resources/Icon.png"
    HARDENED_RUNTIME_ENABLED FALSE
    PLIST_TO_MERGE "<plist><dict><key>LSMinimumSystemVersion</key><string>12.0</string><key>NSHighResolutionCapable</key><true/><key>LSApplicationCategoryType</key><string>public.app-category.music</string></dict></plist>")

target_sources(KalibrePlayer PRIVATE Source/App/Main.cpp)
target_compile_definitions(KalibrePlayer PRIVATE
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:KalibrePlayer,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:KalibrePlayer,JUCE_VERSION>")
target_link_libraries(KalibrePlayer PRIVATE
    KPApp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

#==============================================================================
# The engine's and window's tests, and a tool that draws the window into PNGs
# so a change can be seen from Linux (Kalibre's decision 0023).
if(KP_BUILD_TESTS)
    juce_add_console_app(KPAppTests PRODUCT_NAME "Kalibre Player App Tests")
    target_sources(KPAppTests PRIVATE Tests/App/TestAppMain.cpp Tests/App/TestFormats.cpp Tests/App/TestPlayer.cpp
                                      Tests/App/TestArchives.cpp Tests/App/TestTags.cpp Tests/App/TestJobs.cpp Tests/App/TestWindow.cpp Tests/App/TestPlatform.cpp)
    target_include_directories(KPAppTests PRIVATE Tests)
    target_compile_definitions(KPAppTests PRIVATE JUCE_STANDALONE_APPLICATION=1 KP_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(KPAppTests PRIVATE KPApp juce::juce_recommended_config_flags)
    add_test(NAME App COMMAND KPAppTests)

    juce_add_console_app(KPRender PRODUCT_NAME "Kalibre Player Render")
    target_sources(KPRender PRIVATE tools/RenderWindow.cpp)
    target_include_directories(KPRender PRIVATE Tests/App)
    target_compile_definitions(KPRender PRIVATE JUCE_STANDALONE_APPLICATION=1 KP_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(KPRender PRIVATE KPApp juce::juce_recommended_config_flags)
endif()

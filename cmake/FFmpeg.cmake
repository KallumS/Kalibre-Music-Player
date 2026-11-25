# FFmpeg (LGPL-2.1), built small and static by tools/build-ffmpeg.sh
# (decision 0003): every format's decoder, the converter's encoders, and
# libswresample. Built once when configuring, into KP_FFMPEG_DIR (by default
# inside the build folder); pass -DKP_FFMPEG_DIR=... to use one built before
# (CI keeps one between runs).

set(KP_FFMPEG_VERSION "n8.0.3")
set(KP_FFMPEG_BUILD "${KP_FFMPEG_VERSION}+lame+opus")   # what tools/build-ffmpeg.sh writes in VERSION
set(KP_FFMPEG_DIR "${CMAKE_BINARY_DIR}/ffmpeg" CACHE PATH "Where FFmpeg is (or is built)")

set(_have "")
if(EXISTS "${KP_FFMPEG_DIR}/VERSION")
    file(READ "${KP_FFMPEG_DIR}/VERSION" _have)
    string(STRIP "${_have}" _have)
endif()
if(NOT _have STREQUAL KP_FFMPEG_BUILD)
    message(STATUS "Building FFmpeg ${KP_FFMPEG_VERSION} into ${KP_FFMPEG_DIR} (a few minutes, once)")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env FFMPEG_VERSION=${KP_FFMPEG_VERSION}
                bash ${CMAKE_CURRENT_SOURCE_DIR}/tools/build-ffmpeg.sh ${KP_FFMPEG_DIR}
        RESULT_VARIABLE _ffmpeg_result
        OUTPUT_FILE ${CMAKE_BINARY_DIR}/ffmpeg-build.log
        ERROR_FILE ${CMAKE_BINARY_DIR}/ffmpeg-build.log)
    if(NOT _ffmpeg_result EQUAL 0)
        message(FATAL_ERROR "FFmpeg didn't build: see ${CMAKE_BINARY_DIR}/ffmpeg-build.log")
    endif()
endif()

add_library(KPFFmpeg INTERFACE)
target_include_directories(KPFFmpeg INTERFACE ${KP_FFMPEG_DIR}/include)
# In this order: each needs those after it.
target_link_libraries(KPFFmpeg INTERFACE
    ${KP_FFMPEG_DIR}/lib/libavformat.a
    ${KP_FFMPEG_DIR}/lib/libavcodec.a
    ${KP_FFMPEG_DIR}/lib/libswresample.a
    ${KP_FFMPEG_DIR}/lib/libavutil.a
    ${KP_FFMPEG_DIR}/lib/libmp3lame.a
    ${KP_FFMPEG_DIR}/lib/libopus.a)
find_package(Threads REQUIRED)
target_link_libraries(KPFFmpeg INTERFACE Threads::Threads)
if(UNIX AND NOT APPLE)
    target_link_libraries(KPFFmpeg INTERFACE m atomic)
endif()

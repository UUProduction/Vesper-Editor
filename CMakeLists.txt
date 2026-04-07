cmake_minimum_required(VERSION 3.20)
project(VesperEditor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Qt6 ──────────────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    OpenGL
    OpenGLWidgets
    Svg
    Multimedia
    MultimediaWidgets
    Network
)

# ── FFmpeg ────────────────────────────────────────────────────────────────────
find_path(FFMPEG_INCLUDE_DIR libavcodec/avcodec.h
    PATHS /usr/include /usr/local/include /opt/homebrew/include
)
find_library(AVCODEC_LIB    avcodec)
find_library(AVFORMAT_LIB   avformat)
find_library(AVFILTER_LIB   avfilter)
find_library(AVUTIL_LIB     avutil)
find_library(SWSCALE_LIB    swscale)
find_library(SWRESAMPLE_LIB swresample)

set(FFMPEG_LIBS
    ${AVCODEC_LIB}
    ${AVFORMAT_LIB}
    ${AVFILTER_LIB}
    ${AVUTIL_LIB}
    ${SWSCALE_LIB}
    ${SWRESAMPLE_LIB}
)

# ── PortAudio ─────────────────────────────────────────────────────────────────
find_library(PORTAUDIO_LIB portaudio)
find_path(PORTAUDIO_INCLUDE_DIR portaudio.h
    PATHS /usr/include /usr/local/include /opt/homebrew/include
)

# ── Sources ───────────────────────────────────────────────────────────────────
set(CORE_SOURCES
    core/MediaEngine.cpp
    core/Timeline.cpp
    core/Renderer.cpp
    core/AudioMixer.cpp
    core/ExportEngine.cpp
    core/ProjectManager.cpp
)

set(UI_SOURCES
    ui/MainWindow.cpp
    ui/TimelineWidget.cpp
    ui/PreviewPlayer.cpp
    ui/ToolbarWidget.cpp
    ui/MediaBrowser.cpp
    ui/PropertiesPanel.cpp
    ui/TransitionPanel.cpp
    ui/TextEditor.cpp
    ui/ExportDialog.cpp
)

set(HEADERS
    core/MediaEngine.h
    core/Timeline.h
    core/Renderer.h
    core/AudioMixer.h
    core/ExportEngine.h
    core/ProjectManager.h
    ui/MainWindow.h
    ui/TimelineWidget.h
    ui/PreviewPlayer.h
    ui/ToolbarWidget.h
    ui/MediaBrowser.h
    ui/PropertiesPanel.h
    ui/TransitionPanel.h
    ui/TextEditor.h
    ui/ExportDialog.h
    assets/VesperTheme.h
)

# ── Executable ────────────────────────────────────────────────────────────────
add_executable(VesperEditor
    main.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
    ${HEADERS}
)

target_include_directories(VesperEditor PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/core
    ${CMAKE_SOURCE_DIR}/ui
    ${CMAKE_SOURCE_DIR}/assets
    ${FFMPEG_INCLUDE_DIR}
    ${PORTAUDIO_INCLUDE_DIR}
)

target_link_libraries(VesperEditor PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Svg
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Network
    ${FFMPEG_LIBS}
    ${PORTAUDIO_LIB}
)

# ── Android ───────────────────────────────────────────────────────────────────
if(ANDROID)
    set_target_properties(VesperEditor PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android"
        QT_ANDROID_TARGET_SDK_VERSION 34
        QT_ANDROID_MIN_SDK_VERSION 26
    )
endif()

# ── iOS ───────────────────────────────────────────────────────────────────────
if(IOS)
    set_target_properties(VesperEditor PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/ios/Info.plist"
    )
endif()

# ── Copy Assets ───────────────────────────────────────────────────────────────
add_custom_command(TARGET VesperEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:VesperEditor>/assets"
    COMMENT "Copying Vesper assets..."
)

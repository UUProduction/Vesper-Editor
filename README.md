# ⬡ Vesper Editor

**"Edit Like a Pro. Feel Like Magic."**

A professional mobile video editing application — the power of DaVinci Resolve + Premiere Pro, the layering of Alight Motion, the simplicity of CapCut. All in one sleek cross-platform app.

---

## Tech Stack

| Layer | Technology |
|---|---|
| Language | C++20 |
| Mobile Framework | Qt 6 (Android + iOS) |
| Media Engine | FFmpeg (libavcodec, libavformat, libavfilter, libswscale) |
| Rendering | OpenGL ES 3.0 (Qt RHI) |
| Audio | PortAudio |
| UI Icons | Inline SVG (Qt SVG module) |
| Build | CMake 3.20+ |

---

## Build Instructions

### Prerequisites

- Qt 6.5+ with modules: Core, Gui, Widgets, OpenGL, OpenGLWidgets, Svg, Multimedia, Network
- FFmpeg 6.x (libavcodec, libavformat, libavfilter, libavutil, libswscale, libswresample)
- PortAudio v19
- CMake 3.20+
- C++20 compiler (GCC 12+, Clang 14+, MSVC 2022)

### Desktop (Linux/macOS/Windows)

```bash
git clone https://github.com/your-org/vesper-editor.git
cd vesper-editor
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
./VesperEditor
Android
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-26 \
  -DQt6_DIR=$QT_DIR/android_arm64_v8a/lib/cmake/Qt6
cmake --build . --target apk
iOS
cmake .. \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphoneos \
  -DQt6_DIR=$QT_DIR/ios/lib/cmake/Qt6
cmake --build . -- -sdk iphoneos CODE_SIGN_IDENTITY="iPhone Developer"

#!/bin/bash -ex

# SPDX-FileCopyrightText: 2019 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

set -e

#cd /yuzu

##ccache -sv

mkdir -p build && cd build
export LDFLAGS="-fuse-ld=lld"
# -femulated-tls required due to an incompatibility between GCC and Clang
# TODO(lat9nq): If this is widespread, we probably need to add this to CMakeLists where appropriate
export CXXFLAGS="-femulated-tls"
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="${PWD}/../CMakeModules/MinGWClangCross.cmake" \
    -DTITLE_BAR_FORMAT_IDLE="yuzu Early Access ${{ github.ref_name }}-${{ steps.vars.outputs.sha_short }}" \
	-DTITLE_BAR_FORMAT_RUNNING="yuzu Early Access ${{ github.ref_name }}-${{ steps.vars.outputs.sha_short }} | {3}" \
    -DENABLE_COMPATIBILITY_LIST_DOWNLOAD=ON \
    -DENABLE_QT_TRANSLATION=ON \
    -DUSE_CCACHE=OFF \
    -DYUZU_USE_BUNDLED_SDL2=OFF \
    -DYUZU_USE_EXTERNAL_SDL2=OFF \
    -DYUZU_TESTS=OFF \
    -GNinja
ninja yuzu yuzu-cmd

##ccache -sv

echo "Tests skipped"
#ctest -VV -C Release

echo 'Prepare binaries...'
cd ..
mkdir package

if [ -d "/usr/x86_64-w64-mingw32/lib/qt5/plugins/platforms/" ]; then
  QT_PLUGINS_PATH='/usr/x86_64-w64-mingw32/lib/qt5/plugins'
else
  #fallback to qt
  QT_PLUGINS_PATH='/usr/x86_64-w64-mingw32/lib/qt/plugins'
fi

find build/ -name "yuzu*.exe" -exec cp {} 'package' \;

# copy Qt plugins
mkdir package/platforms
cp -v "${QT_PLUGINS_PATH}/platforms/qwindows.dll" package/platforms/
cp -rv "${QT_PLUGINS_PATH}/mediaservice/" package/
cp -rv "${QT_PLUGINS_PATH}/imageformats/" package/
cp -rv "${QT_PLUGINS_PATH}/styles/" package/
rm -f package/mediaservice/*d.dll

for i in package/*.exe; do
  # we need to process pdb here, however, cv2pdb
  # does not work here, so we just simply strip all the debug symbols
  x86_64-w64-mingw32-strip "${i}"
done

pip3 install pefile
python3 .github/docker/scripts/linux-mingw/scan_dll.py package/*.exe package/imageformats/*.dll "package/"

# copy FFmpeg libraries
EXTERNALS_PATH="$(pwd)/build/externals"
FFMPEG_DLL_PATH="$(find "${EXTERNALS_PATH}" -maxdepth 1 -type d | grep 'ffmpeg-')/bin"
find ${FFMPEG_DLL_PATH} -type f -regex ".*\.dll" -exec cp -nv {} package/ ';'

# copy libraries from yuzu.exe path
find "$(pwd)/build/bin/" -type f -regex ".*\.dll" -exec cp -v {} package/ ';'
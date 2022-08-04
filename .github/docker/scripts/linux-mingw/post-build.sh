#!/bin/bash -ex

set -e

ccache -sv

echo 'Prepare binaries...'
mkdir package

if [ -d "/usr/x86_64-w64-mingw32/lib/qt5/plugins/platforms/" ]; then
  QT_PLUGINS_PATH='/usr/x86_64-w64-mingw32/lib/qt5/plugins'
else
  #fallback to qt
  QT_PLUGINS_PATH='/usr/x86_64-w64-mingw32/lib/qt/plugins'
fi

find build/ -name "yuzu*.exe" -exec cp --preserve=timestamps {} 'package' \;

# copy Qt plugins
mkdir package/platforms
cp -v --preserve=timestamps "${QT_PLUGINS_PATH}/platforms/qwindows.dll" package/platforms/
cp -rv --preserve=timestamps "${QT_PLUGINS_PATH}/mediaservice/" package/
cp -rv --preserve=timestamps "${QT_PLUGINS_PATH}/imageformats/" package/
cp -rv --preserve=timestamps "${QT_PLUGINS_PATH}/styles/" package/
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
find ${FFMPEG_DLL_PATH} -type f -regex ".*\.dll" -exec cp -nv --preserve=timestamps {} package/ ';'

# copy libraries from yuzu.exe path
find "$(pwd)/build/bin/" -type f -regex ".*\.dll" -exec cp -v --preserve=timestamps {} package/ ';'
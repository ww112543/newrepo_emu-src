#!/bin/bash -ex

set -e

#cd /yuzu

ccache -sv

mkdir -p build && cd build
if [ "${COMPILER}" == "clang" ]; then
  toolchainfile="${PWD}/../CMakeModules/MinGWClangCross.cmake"
  export LDFLAGS="-fuse-ld=lld"
else
  toolchainfile="${PWD}/../CMakeModules/MinGWCross.cmake"
fi
# -femulated-tls required due to an incompatibility between GCC and Clang
# TODO(lat9nq): If this is widespread, we probably need to add this to CMakeLists where appropriate
if [ "${COMP_FLAG}" != "" ]; then
  export CFLAGS="${COMP_FLAG}"
  if [ "${COMPILER}" == "clang" ]; then
    export CXXFLAGS="-femulated-tls ${COMP_FLAG}"
  else
    export CXXFLAGS="${COMP_FLAG}"
  fi
  echo "build-suffix=-avx2" >> $GITHUB_ENV
elif [ "${COMPILER}" == "clang" ]; then
  export CXXFLAGS="-femulated-tls"
fi
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="${toolchainfile}" \
    -DTITLE_BAR_FORMAT_IDLE="yuzu Early Access ${REF_NAME}-${SHA_SHORT}" \
    -DTITLE_BAR_FORMAT_RUNNING="yuzu Early Access ${REF_NAME}-${SHA_SHORT} | {3}" \
    -DENABLE_COMPATIBILITY_LIST_DOWNLOAD=ON \
    -DENABLE_QT_TRANSLATION=ON \
    -DUSE_CCACHE=ON \
    -DYUZU_USE_BUNDLED_SDL2=OFF \
    -DYUZU_USE_EXTERNAL_SDL2=OFF \
    -DYUZU_TESTS=OFF \
    -GNinja
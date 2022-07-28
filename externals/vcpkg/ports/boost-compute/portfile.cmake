# Automatically generated by scripts/boost/generate-ports.ps1

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO boostorg/compute
    REF boost-1.79.0
    SHA512 de8a38259c65e4ab0945dbad42ee507ac46974660beb2dce92212f5fd64fb963f7fa8eb9db907e435b72c430dff15405d7f04c3d4d8687c969608e88774e35e8
    HEAD_REF master
)

include(${CURRENT_INSTALLED_DIR}/share/boost-vcpkg-helpers/boost-modular-headers.cmake)
boost_modular_headers(SOURCE_PATH ${SOURCE_PATH})
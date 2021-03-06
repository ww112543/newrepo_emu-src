# Automatically generated by scripts/boost/generate-ports.ps1

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO boostorg/static_assert
    REF boost-1.79.0
    SHA512 3b0a26ec1ac9887610ebe68820c05324e1ac7a8ebff46333577b0d6571a512ffdccffc24124fa90f8cdf322bac8a66d4da523c8a872b0ff6e149fab4e921bdd3
    HEAD_REF master
)

include(${CURRENT_INSTALLED_DIR}/share/boost-vcpkg-helpers/boost-modular-headers.cmake)
boost_modular_headers(SOURCE_PATH ${SOURCE_PATH})

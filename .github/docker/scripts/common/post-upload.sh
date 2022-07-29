#!/bin/bash -ex

# SPDX-FileCopyrightText: 2019 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

# Copy documentation
cp LICENSE.txt "$DIR_NAME"
cp README.md "$DIR_NAME"

if [[ -z "${NO_SOURCE_PACK}" ]]; then
  tar -cJvf "${REV_NAME}-source.tar.xz" src externals CMakeLists.txt README.md LICENSE.txt
  cp -v "${REV_NAME}-source.tar.xz" "$DIR_NAME"
fi

tar $COMPRESSION_FLAGS "$ARCHIVE_NAME" "$DIR_NAME"

# move the compiled archive into the artifacts directory to be uploaded by travis releases
mv "$ARCHIVE_NAME" "${ARTIFACTS_DIR}/"
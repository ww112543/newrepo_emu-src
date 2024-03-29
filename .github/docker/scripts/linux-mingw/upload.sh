#!/bin/bash -ex

# SPDX-FileCopyrightText: 2019 yuzu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later

. .github/docker/scripts/common/pre-upload.sh

REV_NAME="yuzu-windows-mingw-${GITDATE}-${GITREV}"
ARCHIVE_NAME="${REV_NAME}.tar.xz"
COMPRESSION_FLAGS="-cJvf"

if [ "${RELEASE_NAME}" = "mainline" ]; then
    DIR_NAME="${REV_NAME}"
else
    DIR_NAME="${REV_NAME}_${RELEASE_NAME}"
fi

mkdir "$DIR_NAME"
# get around the permission issues
cp -r package/* "$DIR_NAME"

. .github/docker/scripts/common/post-upload.sh
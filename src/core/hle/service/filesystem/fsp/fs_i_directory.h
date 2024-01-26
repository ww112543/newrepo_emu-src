// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/file_sys/vfs/vfs.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/hle/service/service.h"

namespace FileSys {
struct DirectoryEntry;
}

namespace Service::FileSystem {

class IDirectory final : public ServiceFramework<IDirectory> {
public:
    explicit IDirectory(Core::System& system_, FileSys::VirtualDir backend_,
                        FileSys::OpenDirectoryMode mode);

private:
    FileSys::VirtualDir backend;
    std::vector<FileSys::DirectoryEntry> entries;
    u64 next_entry_index = 0;

    void Read(HLERequestContext& ctx);
    void GetEntryCount(HLERequestContext& ctx);
};

} // namespace Service::FileSystem

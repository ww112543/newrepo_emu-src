// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstddef>
#include <vector>

#include "common/logging/log.h"
#include "core/file_sys/program_metadata.h"
#include "core/file_sys/vfs.h"
#include "core/loader/loader.h"

namespace FileSys {

ProgramMetadata::ProgramMetadata() = default;

ProgramMetadata::~ProgramMetadata() = default;

Loader::ResultStatus ProgramMetadata::Load(VirtualFile file) {
    const std::size_t total_size = file->GetSize();
    if (total_size < sizeof(Header)) {
        return Loader::ResultStatus::ErrorBadNPDMHeader;
    }

    if (sizeof(Header) != file->ReadObject(&npdm_header)) {
        return Loader::ResultStatus::ErrorBadNPDMHeader;
    }

    if (sizeof(AcidHeader) != file->ReadObject(&acid_header, npdm_header.acid_offset)) {
        return Loader::ResultStatus::ErrorBadACIDHeader;
    }

    if (sizeof(AciHeader) != file->ReadObject(&aci_header, npdm_header.aci_offset)) {
        return Loader::ResultStatus::ErrorBadACIHeader;
    }

    if (sizeof(FileAccessControl) != file->ReadObject(&acid_file_access, acid_header.fac_offset)) {
        return Loader::ResultStatus::ErrorBadFileAccessControl;
    }

    if (sizeof(FileAccessHeader) != file->ReadObject(&aci_file_access, aci_header.fah_offset)) {
        return Loader::ResultStatus::ErrorBadFileAccessHeader;
    }

    aci_kernel_capabilities.resize(aci_header.kac_size / sizeof(u32));
    const u64 read_size = aci_header.kac_size;
    const u64 read_offset = npdm_header.aci_offset + aci_header.kac_offset;
    if (file->ReadBytes(aci_kernel_capabilities.data(), read_size, read_offset) != read_size) {
        return Loader::ResultStatus::ErrorBadKernelCapabilityDescriptors;
    }

    return Loader::ResultStatus::Success;
}

/*static*/ ProgramMetadata ProgramMetadata::GetDefault() {
    // Allow use of cores 0~3 and thread priorities 1~63.
    constexpr u32 default_thread_info_capability = 0x30007F7;

    ProgramMetadata result;

    result.LoadManual(
        true /*is_64_bit*/, FileSys::ProgramAddressSpaceType::Is39Bit /*address_space*/,
        0x2c /*main_thread_prio*/, 0 /*main_thread_core*/, 0x00100000 /*main_thread_stack_size*/,
        0 /*title_id*/, 0xFFFFFFFFFFFFFFFF /*filesystem_permissions*/,
        0x1FE00000 /*system_resource_size*/, {default_thread_info_capability} /*capabilities*/);

    return result;
}

void ProgramMetadata::LoadManual(bool is_64_bit, ProgramAddressSpaceType address_space,
                                 s32 main_thread_prio, u32 main_thread_core,
                                 u32 main_thread_stack_size, u64 title_id,
                                 u64 filesystem_permissions, u32 system_resource_size,
                                 KernelCapabilityDescriptors capabilities) {
    npdm_header.has_64_bit_instructions.Assign(is_64_bit);
    npdm_header.address_space_type.Assign(address_space);
    npdm_header.main_thread_priority = static_cast<u8>(main_thread_prio);
    npdm_header.main_thread_cpu = static_cast<u8>(main_thread_core);
    npdm_header.main_stack_size = main_thread_stack_size;
    aci_header.title_id = title_id;
    aci_file_access.permissions = filesystem_permissions;
    npdm_header.system_resource_size = system_resource_size;
    aci_kernel_capabilities = std::move(capabilities);
}

bool ProgramMetadata::Is64BitProgram() const {
    return npdm_header.has_64_bit_instructions;
}

ProgramAddressSpaceType ProgramMetadata::GetAddressSpaceType() const {
    return npdm_header.address_space_type;
}

u8 ProgramMetadata::GetMainThreadPriority() const {
    return npdm_header.main_thread_priority;
}

u8 ProgramMetadata::GetMainThreadCore() const {
    return npdm_header.main_thread_cpu;
}

u32 ProgramMetadata::GetMainThreadStackSize() const {
    return npdm_header.main_stack_size;
}

u64 ProgramMetadata::GetTitleID() const {
    return aci_header.title_id;
}

u64 ProgramMetadata::GetFilesystemPermissions() const {
    return aci_file_access.permissions;
}

u32 ProgramMetadata::GetSystemResourceSize() const {
    return npdm_header.system_resource_size;
}

const ProgramMetadata::KernelCapabilityDescriptors& ProgramMetadata::GetKernelCapabilities() const {
    return aci_kernel_capabilities;
}

void ProgramMetadata::Print() const {
    LOG_DEBUG(Service_FS, "Magic:                  {:.4}", npdm_header.magic.data());
    LOG_DEBUG(Service_FS, "Main thread priority:   0x{:02X}", npdm_header.main_thread_priority);
    LOG_DEBUG(Service_FS, "Main thread core:       {}", npdm_header.main_thread_cpu);
    LOG_DEBUG(Service_FS, "Main thread stack size: 0x{:X} bytes", npdm_header.main_stack_size);
    LOG_DEBUG(Service_FS, "Process category:       {}", npdm_header.process_category);
    LOG_DEBUG(Service_FS, "Flags:                  0x{:02X}", npdm_header.flags);
    LOG_DEBUG(Service_FS, " > 64-bit instructions: {}",
              npdm_header.has_64_bit_instructions ? "YES" : "NO");

    const char* address_space = "Unknown";
    switch (npdm_header.address_space_type) {
    case ProgramAddressSpaceType::Is36Bit:
        address_space = "64-bit (36-bit address space)";
        break;
    case ProgramAddressSpaceType::Is39Bit:
        address_space = "64-bit (39-bit address space)";
        break;
    case ProgramAddressSpaceType::Is32Bit:
        address_space = "32-bit";
        break;
    case ProgramAddressSpaceType::Is32BitNoMap:
        address_space = "32-bit (no map region)";
        break;
    }

    LOG_DEBUG(Service_FS, " > Address space:       {}\n", address_space);

    // Begin ACID printing (potential perms, signed)
    LOG_DEBUG(Service_FS, "Magic:                  {:.4}", acid_header.magic.data());
    LOG_DEBUG(Service_FS, "Flags:                  0x{:02X}", acid_header.flags);
    LOG_DEBUG(Service_FS, " > Is Retail:           {}", acid_header.is_retail ? "YES" : "NO");
    LOG_DEBUG(Service_FS, "Title ID Min:           0x{:016X}", acid_header.title_id_min);
    LOG_DEBUG(Service_FS, "Title ID Max:           0x{:016X}", acid_header.title_id_max);
    u64_le permissions_l; // local copy to fix alignment error
    std::memcpy(&permissions_l, &acid_file_access.permissions, sizeof(permissions_l));
    LOG_DEBUG(Service_FS, "Filesystem Access:      0x{:016X}\n", permissions_l);

    // Begin ACI0 printing (actual perms, unsigned)
    LOG_DEBUG(Service_FS, "Magic:                  {:.4}", aci_header.magic.data());
    LOG_DEBUG(Service_FS, "Title ID:               0x{:016X}", aci_header.title_id);
    LOG_DEBUG(Service_FS, "Filesystem Access:      0x{:016X}\n", aci_file_access.permissions);
}
} // namespace FileSys

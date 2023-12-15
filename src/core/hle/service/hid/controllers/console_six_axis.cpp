// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core_timing.h"
#include "core/hid/emulated_console.h"
#include "core/hid/hid_core.h"
#include "core/hle/service/hid/controllers/console_six_axis.h"
#include "core/hle/service/hid/controllers/shared_memory_format.h"

namespace Service::HID {

ConsoleSixAxis::ConsoleSixAxis(Core::HID::HIDCore& hid_core_,
                               ConsoleSixAxisSensorSharedMemoryFormat& console_shared_memory)
    : ControllerBase{hid_core_}, shared_memory{console_shared_memory} {
    console = hid_core.GetEmulatedConsole();
}

ConsoleSixAxis::~ConsoleSixAxis() = default;

void ConsoleSixAxis::OnInit() {}

void ConsoleSixAxis::OnRelease() {}

void ConsoleSixAxis::OnUpdate(const Core::Timing::CoreTiming& core_timing) {
    if (!IsControllerActivated()) {
        return;
    }

    const auto motion_status = console->GetMotion();

    shared_memory.sampling_number++;
    shared_memory.is_seven_six_axis_sensor_at_rest = motion_status.is_at_rest;
    shared_memory.verticalization_error = motion_status.verticalization_error;
    shared_memory.gyro_bias = motion_status.gyro_bias;
}

} // namespace Service::HID

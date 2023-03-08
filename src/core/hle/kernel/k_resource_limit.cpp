// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "common/overflow.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/k_resource_limit.h"
#include "core/hle/kernel/svc_results.h"

namespace Kernel {
constexpr s64 DefaultTimeout = 10000000000; // 10 seconds

KResourceLimit::KResourceLimit(KernelCore& kernel_)
    : KAutoObjectWithSlabHeapAndContainer{kernel_}, lock{kernel_}, cond_var{kernel_} {}
KResourceLimit::~KResourceLimit() = default;

void KResourceLimit::Initialize(const Core::Timing::CoreTiming* core_timing_) {
    core_timing = core_timing_;
}

void KResourceLimit::Finalize() {}

s64 KResourceLimit::GetLimitValue(LimitableResource which) const {
    const auto index = static_cast<std::size_t>(which);
    s64 value{};
    {
        KScopedLightLock lk{lock};
        value = limit_values[index];
        ASSERT(value >= 0);
        ASSERT(current_values[index] <= limit_values[index]);
        ASSERT(current_hints[index] <= current_values[index]);
    }
    return value;
}

s64 KResourceLimit::GetCurrentValue(LimitableResource which) const {
    const auto index = static_cast<std::size_t>(which);
    s64 value{};
    {
        KScopedLightLock lk{lock};
        value = current_values[index];
        ASSERT(value >= 0);
        ASSERT(current_values[index] <= limit_values[index]);
        ASSERT(current_hints[index] <= current_values[index]);
    }
    return value;
}

s64 KResourceLimit::GetPeakValue(LimitableResource which) const {
    const auto index = static_cast<std::size_t>(which);
    s64 value{};
    {
        KScopedLightLock lk{lock};
        value = peak_values[index];
        ASSERT(value >= 0);
        ASSERT(current_values[index] <= limit_values[index]);
        ASSERT(current_hints[index] <= current_values[index]);
    }
    return value;
}

s64 KResourceLimit::GetFreeValue(LimitableResource which) const {
    const auto index = static_cast<std::size_t>(which);
    s64 value{};
    {
        KScopedLightLock lk(lock);
        ASSERT(current_values[index] >= 0);
        ASSERT(current_values[index] <= limit_values[index]);
        ASSERT(current_hints[index] <= current_values[index]);
        value = limit_values[index] - current_values[index];
    }

    return value;
}

Result KResourceLimit::SetLimitValue(LimitableResource which, s64 value) {
    const auto index = static_cast<std::size_t>(which);
    KScopedLightLock lk(lock);
    R_UNLESS(current_values[index] <= value, ResultInvalidState);

    limit_values[index] = value;
    peak_values[index] = current_values[index];

    return ResultSuccess;
}

bool KResourceLimit::Reserve(LimitableResource which, s64 value) {
    return Reserve(which, value, core_timing->GetGlobalTimeNs().count() + DefaultTimeout);
}

bool KResourceLimit::Reserve(LimitableResource which, s64 value, s64 timeout) {
    ASSERT(value >= 0);
    const auto index = static_cast<std::size_t>(which);
    KScopedLightLock lk(lock);

    ASSERT(current_hints[index] <= current_values[index]);
    if (current_hints[index] >= limit_values[index]) {
        return false;
    }

    // Loop until we reserve or run out of time.
    while (true) {
        ASSERT(current_values[index] <= limit_values[index]);
        ASSERT(current_hints[index] <= current_values[index]);

        // If we would overflow, don't allow to succeed.
        if (Common::WrappingAdd(current_values[index], value) <= current_values[index]) {
            break;
        }

        if (current_values[index] + value <= limit_values[index]) {
            current_values[index] += value;
            current_hints[index] += value;
            peak_values[index] = std::max(peak_values[index], current_values[index]);
            return true;
        }

        if (current_hints[index] + value <= limit_values[index] &&
            (timeout < 0 || core_timing->GetGlobalTimeNs().count() < timeout)) {
            waiter_count++;
            cond_var.Wait(&lock, timeout, false);
            waiter_count--;
        } else {
            break;
        }
    }

    return false;
}

void KResourceLimit::Release(LimitableResource which, s64 value) {
    Release(which, value, value);
}

void KResourceLimit::Release(LimitableResource which, s64 value, s64 hint) {
    ASSERT(value >= 0);
    ASSERT(hint >= 0);

    const auto index = static_cast<std::size_t>(which);
    KScopedLightLock lk(lock);
    ASSERT(current_values[index] <= limit_values[index]);
    ASSERT(current_hints[index] <= current_values[index]);
    ASSERT(value <= current_values[index]);
    ASSERT(hint <= current_hints[index]);

    current_values[index] -= value;
    current_hints[index] -= hint;

    if (waiter_count != 0) {
        cond_var.Broadcast();
    }
}

KResourceLimit* CreateResourceLimitForProcess(Core::System& system, s64 physical_memory_size) {
    auto* resource_limit = KResourceLimit::Create(system.Kernel());
    resource_limit->Initialize(&system.CoreTiming());

    // Initialize default resource limit values.
    // TODO(bunnei): These values are the system defaults, the limits for service processes are
    // lower. These should use the correct limit values.

    ASSERT(resource_limit->SetLimitValue(LimitableResource::PhysicalMemoryMax, physical_memory_size)
               .IsSuccess());
    ASSERT(resource_limit->SetLimitValue(LimitableResource::ThreadCountMax, 800).IsSuccess());
    ASSERT(resource_limit->SetLimitValue(LimitableResource::EventCountMax, 900).IsSuccess());
    ASSERT(
        resource_limit->SetLimitValue(LimitableResource::TransferMemoryCountMax, 200).IsSuccess());
    ASSERT(resource_limit->SetLimitValue(LimitableResource::SessionCountMax, 1133).IsSuccess());

    return resource_limit;
}

} // namespace Kernel

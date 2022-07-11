// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include "common/assert.h"
#include "core/hle/kernel/k_spin_lock.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/kernel.h"

namespace Kernel {

class KernelCore;

template <typename SchedulerType>
class KAbstractSchedulerLock {
public:
    explicit KAbstractSchedulerLock(KernelCore& kernel_) : kernel{kernel_} {}

    bool IsLockedByCurrentThread() const {
        return owner_thread == GetCurrentThreadPointer(kernel);
    }

    void Lock() {
        // If we are shutting down the kernel, none of this is relevant anymore.
        if (kernel.IsShuttingDown()) {
            return;
        }

        if (IsLockedByCurrentThread()) {
            // If we already own the lock, we can just increment the count.
            ASSERT(lock_count > 0);
            lock_count++;
        } else {
            // Otherwise, we want to disable scheduling and acquire the spinlock.
            SchedulerType::DisableScheduling(kernel);
            spin_lock.Lock();

            // For debug, ensure that our state is valid.
            ASSERT(lock_count == 0);
            ASSERT(owner_thread == nullptr);

            // Increment count, take ownership.
            lock_count = 1;
            owner_thread = GetCurrentThreadPointer(kernel);
        }
    }

    void Unlock() {
        // If we are shutting down the kernel, none of this is relevant anymore.
        if (kernel.IsShuttingDown()) {
            return;
        }

        ASSERT(IsLockedByCurrentThread());
        ASSERT(lock_count > 0);

        // Release an instance of the lock.
        if ((--lock_count) == 0) {
            // We're no longer going to hold the lock. Take note of what cores need scheduling.
            const u64 cores_needing_scheduling =
                SchedulerType::UpdateHighestPriorityThreads(kernel);

            // Note that we no longer hold the lock, and unlock the spinlock.
            owner_thread = nullptr;
            spin_lock.Unlock();

            // Enable scheduling, and perform a rescheduling operation.
            SchedulerType::EnableScheduling(kernel, cores_needing_scheduling);
        }
    }

private:
    KernelCore& kernel;
    KAlignedSpinLock spin_lock{};
    s32 lock_count{};
    std::atomic<KThread*> owner_thread{};
};

} // namespace Kernel

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/arm/exclusive_monitor.h"
#include "core/core.h"
#include "core/hle/kernel/k_condition_variable.h"
#include "core/hle/kernel/k_linked_list.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_scheduler.h"
#include "core/hle/kernel/k_scoped_scheduler_lock_and_sleep.h"
#include "core/hle/kernel/k_thread.h"
#include "core/hle/kernel/k_thread_queue.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/svc_common.h"
#include "core/hle/kernel/svc_results.h"
#include "core/memory.h"

namespace Kernel {

namespace {

bool ReadFromUser(Core::System& system, u32* out, VAddr address) {
    *out = system.Memory().Read32(address);
    return true;
}

bool WriteToUser(Core::System& system, VAddr address, const u32* p) {
    system.Memory().Write32(address, *p);
    return true;
}

bool UpdateLockAtomic(Core::System& system, u32* out, VAddr address, u32 if_zero,
                      u32 new_orr_mask) {
    auto& monitor = system.Monitor();
    const auto current_core = system.Kernel().CurrentPhysicalCoreIndex();

    // Load the value from the address.
    const auto expected = monitor.ExclusiveRead32(current_core, address);

    // Orr in the new mask.
    u32 value = expected | new_orr_mask;

    // If the value is zero, use the if_zero value, otherwise use the newly orr'd value.
    if (!expected) {
        value = if_zero;
    }

    // Try to store.
    if (!monitor.ExclusiveWrite32(current_core, address, value)) {
        // If we failed to store, try again.
        return UpdateLockAtomic(system, out, address, if_zero, new_orr_mask);
    }

    // We're done.
    *out = expected;
    return true;
}

class ThreadQueueImplForKConditionVariableWaitForAddress final : public KThreadQueue {
public:
    explicit ThreadQueueImplForKConditionVariableWaitForAddress(KernelCore& kernel_)
        : KThreadQueue(kernel_) {}

    void CancelWait(KThread* waiting_thread, Result wait_result, bool cancel_timer_task) override {
        // Remove the thread as a waiter from its owner.
        waiting_thread->GetLockOwner()->RemoveWaiter(waiting_thread);

        // Invoke the base cancel wait handler.
        KThreadQueue::CancelWait(waiting_thread, wait_result, cancel_timer_task);
    }
};

class ThreadQueueImplForKConditionVariableWaitConditionVariable final : public KThreadQueue {
private:
    KConditionVariable::ThreadTree* m_tree;

public:
    explicit ThreadQueueImplForKConditionVariableWaitConditionVariable(
        KernelCore& kernel_, KConditionVariable::ThreadTree* t)
        : KThreadQueue(kernel_), m_tree(t) {}

    void CancelWait(KThread* waiting_thread, Result wait_result, bool cancel_timer_task) override {
        // Remove the thread as a waiter from its owner.
        if (KThread* owner = waiting_thread->GetLockOwner(); owner != nullptr) {
            owner->RemoveWaiter(waiting_thread);
        }

        // If the thread is waiting on a condvar, remove it from the tree.
        if (waiting_thread->IsWaitingForConditionVariable()) {
            m_tree->erase(m_tree->iterator_to(*waiting_thread));
            waiting_thread->ClearConditionVariable();
        }

        // Invoke the base cancel wait handler.
        KThreadQueue::CancelWait(waiting_thread, wait_result, cancel_timer_task);
    }
};

} // namespace

KConditionVariable::KConditionVariable(Core::System& system_)
    : system{system_}, kernel{system.Kernel()} {}

KConditionVariable::~KConditionVariable() = default;

Result KConditionVariable::SignalToAddress(VAddr addr) {
    KThread* owner_thread = GetCurrentThreadPointer(kernel);

    // Signal the address.
    {
        KScopedSchedulerLock sl(kernel);

        // Remove waiter thread.
        s32 num_waiters{};
        KThread* next_owner_thread =
            owner_thread->RemoveWaiterByKey(std::addressof(num_waiters), addr);

        // Determine the next tag.
        u32 next_value{};
        if (next_owner_thread != nullptr) {
            next_value = next_owner_thread->GetAddressKeyValue();
            if (num_waiters > 1) {
                next_value |= Svc::HandleWaitMask;
            }

            // Write the value to userspace.
            Result result{ResultSuccess};
            if (WriteToUser(system, addr, std::addressof(next_value))) [[likely]] {
                result = ResultSuccess;
            } else {
                result = ResultInvalidCurrentMemory;
            }

            // Signal the next owner thread.
            next_owner_thread->EndWait(result);
            return result;
        } else {
            // Just write the value to userspace.
            R_UNLESS(WriteToUser(system, addr, std::addressof(next_value)),
                     ResultInvalidCurrentMemory);

            return ResultSuccess;
        }
    }
}

Result KConditionVariable::WaitForAddress(Handle handle, VAddr addr, u32 value) {
    KThread* cur_thread = GetCurrentThreadPointer(kernel);
    ThreadQueueImplForKConditionVariableWaitForAddress wait_queue(kernel);

    // Wait for the address.
    KThread* owner_thread{};
    {
        KScopedSchedulerLock sl(kernel);

        // Check if the thread should terminate.
        R_UNLESS(!cur_thread->IsTerminationRequested(), ResultTerminationRequested);

        // Read the tag from userspace.
        u32 test_tag{};
        R_UNLESS(ReadFromUser(system, std::addressof(test_tag), addr), ResultInvalidCurrentMemory);

        // If the tag isn't the handle (with wait mask), we're done.
        R_SUCCEED_IF(test_tag != (handle | Svc::HandleWaitMask));

        // Get the lock owner thread.
        owner_thread = kernel.CurrentProcess()
                           ->GetHandleTable()
                           .GetObjectWithoutPseudoHandle<KThread>(handle)
                           .ReleasePointerUnsafe();
        R_UNLESS(owner_thread != nullptr, ResultInvalidHandle);

        // Update the lock.
        cur_thread->SetAddressKey(addr, value);
        owner_thread->AddWaiter(cur_thread);

        // Begin waiting.
        cur_thread->BeginWait(std::addressof(wait_queue));
        cur_thread->SetWaitReasonForDebugging(ThreadWaitReasonForDebugging::ConditionVar);
        cur_thread->SetMutexWaitAddressForDebugging(addr);
    }

    // Close our reference to the owner thread, now that the wait is over.
    owner_thread->Close();

    // Get the wait result.
    return cur_thread->GetWaitResult();
}

void KConditionVariable::SignalImpl(KThread* thread) {
    // Check pre-conditions.
    ASSERT(kernel.GlobalSchedulerContext().IsLocked());

    // Update the tag.
    VAddr address = thread->GetAddressKey();
    u32 own_tag = thread->GetAddressKeyValue();

    u32 prev_tag{};
    bool can_access{};
    {
        // TODO(bunnei): We should disable interrupts here via KScopedInterruptDisable.
        // TODO(bunnei): We should call CanAccessAtomic(..) here.
        can_access = true;
        if (can_access) [[likely]] {
            UpdateLockAtomic(system, std::addressof(prev_tag), address, own_tag,
                             Svc::HandleWaitMask);
        }
    }

    if (can_access) [[likely]] {
        if (prev_tag == Svc::InvalidHandle) {
            // If nobody held the lock previously, we're all good.
            thread->EndWait(ResultSuccess);
        } else {
            // Get the previous owner.
            KThread* owner_thread = kernel.CurrentProcess()
                                        ->GetHandleTable()
                                        .GetObjectWithoutPseudoHandle<KThread>(
                                            static_cast<Handle>(prev_tag & ~Svc::HandleWaitMask))
                                        .ReleasePointerUnsafe();

            if (owner_thread) [[likely]] {
                // Add the thread as a waiter on the owner.
                owner_thread->AddWaiter(thread);
                owner_thread->Close();
            } else {
                // The lock was tagged with a thread that doesn't exist.
                thread->EndWait(ResultInvalidState);
            }
        }
    } else {
        // If the address wasn't accessible, note so.
        thread->EndWait(ResultInvalidCurrentMemory);
    }
}

void KConditionVariable::Signal(u64 cv_key, s32 count) {
    // Perform signaling.
    s32 num_waiters{};
    {
        KScopedSchedulerLock sl(kernel);

        auto it = thread_tree.nfind_key({cv_key, -1});
        while ((it != thread_tree.end()) && (count <= 0 || num_waiters < count) &&
               (it->GetConditionVariableKey() == cv_key)) {
            KThread* target_thread = std::addressof(*it);

            this->SignalImpl(target_thread);
            it = thread_tree.erase(it);
            target_thread->ClearConditionVariable();
            ++num_waiters;
        }

        // If we have no waiters, clear the has waiter flag.
        if (it == thread_tree.end() || it->GetConditionVariableKey() != cv_key) {
            const u32 has_waiter_flag{};
            WriteToUser(system, cv_key, std::addressof(has_waiter_flag));
        }
    }
}

Result KConditionVariable::Wait(VAddr addr, u64 key, u32 value, s64 timeout) {
    // Prepare to wait.
    KThread* cur_thread = GetCurrentThreadPointer(kernel);
    ThreadQueueImplForKConditionVariableWaitConditionVariable wait_queue(
        kernel, std::addressof(thread_tree));

    {
        KScopedSchedulerLockAndSleep slp(kernel, cur_thread, timeout);

        // Check that the thread isn't terminating.
        if (cur_thread->IsTerminationRequested()) {
            slp.CancelSleep();
            return ResultTerminationRequested;
        }

        // Update the value and process for the next owner.
        {
            // Remove waiter thread.
            s32 num_waiters{};
            KThread* next_owner_thread =
                cur_thread->RemoveWaiterByKey(std::addressof(num_waiters), addr);

            // Update for the next owner thread.
            u32 next_value{};
            if (next_owner_thread != nullptr) {
                // Get the next tag value.
                next_value = next_owner_thread->GetAddressKeyValue();
                if (num_waiters > 1) {
                    next_value |= Svc::HandleWaitMask;
                }

                // Wake up the next owner.
                next_owner_thread->EndWait(ResultSuccess);
            }

            // Write to the cv key.
            {
                const u32 has_waiter_flag = 1;
                WriteToUser(system, key, std::addressof(has_waiter_flag));
                // TODO(bunnei): We should call DataMemoryBarrier(..) here.
            }

            // Write the value to userspace.
            if (!WriteToUser(system, addr, std::addressof(next_value))) {
                slp.CancelSleep();
                return ResultInvalidCurrentMemory;
            }
        }

        // If timeout is zero, time out.
        R_UNLESS(timeout != 0, ResultTimedOut);

        // Update condition variable tracking.
        cur_thread->SetConditionVariable(std::addressof(thread_tree), addr, key, value);
        thread_tree.insert(*cur_thread);

        // Begin waiting.
        cur_thread->BeginWait(std::addressof(wait_queue));
        cur_thread->SetWaitReasonForDebugging(ThreadWaitReasonForDebugging::ConditionVar);
        cur_thread->SetMutexWaitAddressForDebugging(addr);
    }

    // Get the wait result.
    return cur_thread->GetWaitResult();
}

} // namespace Kernel

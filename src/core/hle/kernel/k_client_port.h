// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include "common/common_types.h"
#include "core/hle/kernel/k_synchronization_object.h"
#include "core/hle/result.h"

namespace Kernel {

class KClientSession;
class KernelCore;
class KPort;
class SessionRequestManager;

class KClientPort final : public KSynchronizationObject {
    KERNEL_AUTOOBJECT_TRAITS(KClientPort, KSynchronizationObject);

public:
    explicit KClientPort(KernelCore& kernel_);
    ~KClientPort() override;

    void Initialize(KPort* parent_, s32 max_sessions_, std::string&& name_);
    void OnSessionFinalized();
    void OnServerClosed();

    const KPort* GetParent() const {
        return parent;
    }
    KPort* GetParent() {
        return parent;
    }

    s32 GetNumSessions() const {
        return num_sessions;
    }
    s32 GetPeakSessions() const {
        return peak_sessions;
    }
    s32 GetMaxSessions() const {
        return max_sessions;
    }

    bool IsLight() const;
    bool IsServerClosed() const;

    // Overridden virtual functions.
    void Destroy() override;
    bool IsSignaled() const override;

    ResultCode CreateSession(KClientSession** out,
                             std::shared_ptr<SessionRequestManager> session_manager = nullptr);

private:
    std::atomic<s32> num_sessions{};
    std::atomic<s32> peak_sessions{};
    s32 max_sessions{};
    KPort* parent{};
};

} // namespace Kernel

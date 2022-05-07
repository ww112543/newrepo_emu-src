// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Kernel {
class HLERequestContext;
}

namespace Service::NVFlinger {
class HosBinderDriverServer;
class NVFlinger;
} // namespace Service::NVFlinger

namespace Service::VI {

class VI_S final : public ServiceFramework<VI_S> {
public:
    explicit VI_S(Core::System& system_, NVFlinger::NVFlinger& nv_flinger_,
                  NVFlinger::HosBinderDriverServer& hos_binder_driver_server_);
    ~VI_S() override;

private:
    void GetDisplayService(Kernel::HLERequestContext& ctx);

    NVFlinger::NVFlinger& nv_flinger;
    NVFlinger::HosBinderDriverServer& hos_binder_driver_server;
};

} // namespace Service::VI

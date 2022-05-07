// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/string_util.h"
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/ngct/ngct.h"
#include "core/hle/service/service.h"

namespace Service::NGCT {

class IService final : public ServiceFramework<IService> {
public:
    explicit IService(Core::System& system_) : ServiceFramework{system_, "ngct:u"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IService::Match, "Match"},
            {1, &IService::Filter, "Filter"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void Match(Kernel::HLERequestContext& ctx) {
        const auto buffer = ctx.ReadBuffer();
        const auto text = Common::StringFromFixedZeroTerminatedBuffer(
            reinterpret_cast<const char*>(buffer.data()), buffer.size());

        LOG_WARNING(Service_NGCT, "(STUBBED) called, text={}", text);

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(ResultSuccess);
        // Return false since we don't censor anything
        rb.Push(false);
    }

    void Filter(Kernel::HLERequestContext& ctx) {
        const auto buffer = ctx.ReadBuffer();
        const auto text = Common::StringFromFixedZeroTerminatedBuffer(
            reinterpret_cast<const char*>(buffer.data()), buffer.size());

        LOG_WARNING(Service_NGCT, "(STUBBED) called, text={}", text);

        // Return the same string since we don't censor anything
        ctx.WriteBuffer(buffer);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }
};

void InstallInterfaces(SM::ServiceManager& service_manager, Core::System& system) {
    std::make_shared<IService>(system)->InstallAsService(system.ServiceManager());
}

} // namespace Service::NGCT

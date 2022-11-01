// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "common/concepts.h"
#include "core/hle/kernel/k_port.h"
#include "core/hle/result.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Kernel {
class KClientPort;
class KClientSession;
class KernelCore;
class KPort;
class SessionRequestHandler;
} // namespace Kernel

namespace Service::SM {

class Controller;

/// Interface to "sm:" service
class SM final : public ServiceFramework<SM> {
public:
    explicit SM(ServiceManager& service_manager_, Core::System& system_);
    ~SM() override;

private:
    void Initialize(Kernel::HLERequestContext& ctx);
    void GetService(Kernel::HLERequestContext& ctx);
    void GetServiceTipc(Kernel::HLERequestContext& ctx);
    void RegisterService(Kernel::HLERequestContext& ctx);
    void UnregisterService(Kernel::HLERequestContext& ctx);

    ResultVal<Kernel::KClientSession*> GetServiceImpl(Kernel::HLERequestContext& ctx);

    ServiceManager& service_manager;
    bool is_initialized{};
    Kernel::KernelCore& kernel;
};

class ServiceManager {
public:
    static Kernel::KClientPort& InterfaceFactory(ServiceManager& self, Core::System& system);
    static void SessionHandler(ServiceManager& self, Kernel::KServerPort* server_port);

    explicit ServiceManager(Kernel::KernelCore& kernel_);
    ~ServiceManager();

    Result RegisterService(std::string name, u32 max_sessions,
                           Kernel::SessionRequestHandlerPtr handler);
    Result UnregisterService(const std::string& name);
    ResultVal<Kernel::KPort*> GetServicePort(const std::string& name);

    template <Common::DerivedFrom<Kernel::SessionRequestHandler> T>
    std::shared_ptr<T> GetService(const std::string& service_name) const {
        auto service = registered_services.find(service_name);
        if (service == registered_services.end()) {
            LOG_DEBUG(Service, "Can't find service: {}", service_name);
            return nullptr;
        }
        return std::static_pointer_cast<T>(service->second);
    }

    void InvokeControlRequest(Kernel::HLERequestContext& context);

private:
    std::shared_ptr<SM> sm_interface;
    std::unique_ptr<Controller> controller_interface;

    /// Map of registered services, retrieved using GetServicePort.
    std::unordered_map<std::string, Kernel::SessionRequestHandlerPtr> registered_services;
    std::unordered_map<std::string, Kernel::KPort*> service_ports;

    /// Kernel context
    Kernel::KernelCore& kernel;
};

} // namespace Service::SM

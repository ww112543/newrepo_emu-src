// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/k_client_port.h"
#include "core/hle/kernel/k_port.h"
#include "core/hle/kernel/k_scoped_resource_reservation.h"
#include "core/hle/kernel/k_server_session.h"
#include "core/hle/kernel/k_session.h"
#include "core/hle/service/sm/sm_controller.h"

namespace Service::SM {

void Controller::ConvertCurrentObjectToDomain(Kernel::HLERequestContext& ctx) {
    ASSERT_MSG(!ctx.Session()->GetSessionRequestManager()->IsDomain(),
               "Session is already a domain");
    LOG_DEBUG(Service, "called, server_session={}", ctx.Session()->GetId());
    ctx.Session()->GetSessionRequestManager()->ConvertToDomainOnRequestEnd();

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u32>(1); // Converted sessions start with 1 request handler
}

void Controller::CloneCurrentObject(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service, "called");

    auto& process = *ctx.GetThread().GetOwnerProcess();
    auto& parent_session = *ctx.Session()->GetParent();
    auto& session_manager = parent_session.GetServerSession().GetSessionRequestManager();
    auto& session_handler = session_manager->SessionHandler();

    // FIXME: this is duplicated from the SVC, it should just call it instead
    // once this is a proper process

    // Reserve a new session from the process resource limit.
    Kernel::KScopedResourceReservation session_reservation(&process,
                                                           Kernel::LimitableResource::Sessions);
    ASSERT(session_reservation.Succeeded());

    // Create the sessionn.
    Kernel::KSession* session = Kernel::KSession::Create(system.Kernel());
    ASSERT(session != nullptr);

    // Initialize the session.
    session->Initialize(nullptr, parent_session.GetName(), session_manager);

    // Commit the session reservation.
    session_reservation.Commit();

    // Register the session.
    session_handler.ClientConnected(&session->GetServerSession());

    // We succeeded.
    IPC::ResponseBuilder rb{ctx, 2, 0, 1, IPC::ResponseBuilder::Flags::AlwaysMoveHandles};
    rb.Push(ResultSuccess);
    rb.PushMoveObjects(session->GetClientSession());
}

void Controller::CloneCurrentObjectEx(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service, "called");

    CloneCurrentObject(ctx);
}

void Controller::QueryPointerBufferSize(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u16>(0x8000);
}

// https://switchbrew.org/wiki/IPC_Marshalling
Controller::Controller(Core::System& system_) : ServiceFramework{system_, "IpcController"} {
    static const FunctionInfo functions[] = {
        {0, &Controller::ConvertCurrentObjectToDomain, "ConvertCurrentObjectToDomain"},
        {1, nullptr, "CopyFromCurrentDomain"},
        {2, &Controller::CloneCurrentObject, "CloneCurrentObject"},
        {3, &Controller::QueryPointerBufferSize, "QueryPointerBufferSize"},
        {4, &Controller::CloneCurrentObjectEx, "CloneCurrentObjectEx"},
    };
    RegisterHandlers(functions);
}

Controller::~Controller() = default;

} // namespace Service::SM

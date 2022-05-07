// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/nfp/nfp_user.h"

namespace Service::NFP {

NFP_User::NFP_User(std::shared_ptr<Module> module_, Core::System& system_)
    : Interface(std::move(module_), system_, "nfp:user") {
    static const FunctionInfo functions[] = {
        {0, &NFP_User::CreateUserInterface, "CreateUserInterface"},
    };
    RegisterHandlers(functions);
}

NFP_User::~NFP_User() = default;

} // namespace Service::NFP

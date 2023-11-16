// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "frontend_common/config.h"

class AndroidConfig final : public Config {
public:
    explicit AndroidConfig(const std::string& config_name = "config",
                           ConfigType config_type = ConfigType::GlobalConfig);
    ~AndroidConfig() override;

    void ReloadAllValues() override;
    void SaveAllValues() override;

protected:
    void ReadAndroidValues();
    void ReadAndroidUIValues();
    void ReadHidbusValues() override {}
    void ReadDebugControlValues() override {}
    void ReadPathValues() override {}
    void ReadShortcutValues() override {}
    void ReadUIValues() override {}
    void ReadUIGamelistValues() override {}
    void ReadUILayoutValues() override {}
    void ReadMultiplayerValues() override {}

    void SaveAndroidValues();
    void SaveAndroidUIValues();
    void SaveHidbusValues() override {}
    void SaveDebugControlValues() override {}
    void SavePathValues() override {}
    void SaveShortcutValues() override {}
    void SaveUIValues() override {}
    void SaveUIGamelistValues() override {}
    void SaveUILayoutValues() override {}
    void SaveMultiplayerValues() override {}

    std::vector<Settings::BasicSetting*>& FindRelevantList(Settings::Category category) override;
};

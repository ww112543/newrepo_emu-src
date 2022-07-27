// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include <QWidget>

namespace Core {
class System;
}

namespace ConfigurationShared {
enum class CheckState;
}

namespace Ui {
class ConfigureSystem;
}

class ConfigureSystem : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureSystem(Core::System& system_, QWidget* parent = nullptr);
    ~ConfigureSystem() override;

    void ApplyConfiguration();
    void SetConfiguration();

private:
    void changeEvent(QEvent* event) override;
    void RetranslateUI();

    void ReadSystemSettings();

    void RefreshConsoleID();

    void SetupPerGameUI();

    std::unique_ptr<Ui::ConfigureSystem> ui;
    bool enabled = false;

    int language_index = 0;
    int region_index = 0;
    int time_zone_index = 0;
    int sound_index = 0;

    ConfigurationShared::CheckState use_rng_seed;
    ConfigurationShared::CheckState use_custom_rtc;

    Core::System& system;
};

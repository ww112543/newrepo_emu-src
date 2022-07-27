// SPDX-FileCopyrightText: 2017 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <QFutureWatcher>
#include <QWizard>
#include "core/telemetry_session.h"

namespace Ui {
class CompatDB;
}

class CompatDB : public QWizard {
    Q_OBJECT

public:
    explicit CompatDB(Core::TelemetrySession& telemetry_session_, QWidget* parent = nullptr);
    ~CompatDB();

private:
    QFutureWatcher<bool> testcase_watcher;

    std::unique_ptr<Ui::CompatDB> ui;

    void Submit();
    void OnTestcaseSubmitted();
    void EnableNext();

    Core::TelemetrySession& telemetry_session;
};

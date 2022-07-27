// SPDX-FileCopyrightText: 2017 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QButtonGroup>
#include <QMessageBox>
#include <QPushButton>
#include <QtConcurrent/qtconcurrentrun.h>
#include "common/logging/log.h"
#include "common/telemetry.h"
#include "core/telemetry_session.h"
#include "ui_compatdb.h"
#include "yuzu/compatdb.h"

CompatDB::CompatDB(Core::TelemetrySession& telemetry_session_, QWidget* parent)
    : QWizard(parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowSystemMenuHint),
      ui{std::make_unique<Ui::CompatDB>()}, telemetry_session{telemetry_session_} {
    ui->setupUi(this);
    connect(ui->radioButton_Perfect, &QRadioButton::clicked, this, &CompatDB::EnableNext);
    connect(ui->radioButton_Great, &QRadioButton::clicked, this, &CompatDB::EnableNext);
    connect(ui->radioButton_Okay, &QRadioButton::clicked, this, &CompatDB::EnableNext);
    connect(ui->radioButton_Bad, &QRadioButton::clicked, this, &CompatDB::EnableNext);
    connect(ui->radioButton_IntroMenu, &QRadioButton::clicked, this, &CompatDB::EnableNext);
    connect(ui->radioButton_WontBoot, &QRadioButton::clicked, this, &CompatDB::EnableNext);
    connect(button(NextButton), &QPushButton::clicked, this, &CompatDB::Submit);
    connect(&testcase_watcher, &QFutureWatcher<bool>::finished, this,
            &CompatDB::OnTestcaseSubmitted);
}

CompatDB::~CompatDB() = default;

enum class CompatDBPage {
    Intro = 0,
    Selection = 1,
    Final = 2,
};

void CompatDB::Submit() {
    QButtonGroup* compatibility = new QButtonGroup(this);
    compatibility->addButton(ui->radioButton_Perfect, 0);
    compatibility->addButton(ui->radioButton_Great, 1);
    compatibility->addButton(ui->radioButton_Okay, 2);
    compatibility->addButton(ui->radioButton_Bad, 3);
    compatibility->addButton(ui->radioButton_IntroMenu, 4);
    compatibility->addButton(ui->radioButton_WontBoot, 5);
    switch ((static_cast<CompatDBPage>(currentId()))) {
    case CompatDBPage::Selection:
        if (compatibility->checkedId() == -1) {
            button(NextButton)->setEnabled(false);
        }
        break;
    case CompatDBPage::Final:
        back();
        LOG_DEBUG(Frontend, "Compatibility Rating: {}", compatibility->checkedId());
        telemetry_session.AddField(Common::Telemetry::FieldType::UserFeedback, "Compatibility",
                                   compatibility->checkedId());

        button(NextButton)->setEnabled(false);
        button(NextButton)->setText(tr("Submitting"));
        button(CancelButton)->setVisible(false);

        testcase_watcher.setFuture(
            QtConcurrent::run([this] { return telemetry_session.SubmitTestcase(); }));
        break;
    default:
        LOG_ERROR(Frontend, "Unexpected page: {}", currentId());
    }
}

void CompatDB::OnTestcaseSubmitted() {
    if (!testcase_watcher.result()) {
        QMessageBox::critical(this, tr("Communication error"),
                              tr("An error occurred while sending the Testcase"));
        button(NextButton)->setEnabled(true);
        button(NextButton)->setText(tr("Next"));
        button(CancelButton)->setVisible(true);
    } else {
        next();
        // older versions of QT don't support the "NoCancelButtonOnLastPage" option, this is a
        // workaround
        button(CancelButton)->setVisible(false);
    }
}

void CompatDB::EnableNext() {
    button(NextButton)->setEnabled(true);
}

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <forward_list>
#include <functional>
#include <memory>
#include <string>
#include <QString>
#include <QStringLiteral>
#include <QWidget>
#include <qobjectdefs.h>
#include "yuzu/configuration/shared_translation.h"

class QCheckBox;
class QComboBox;
class QDateTimeEdit;
class QLabel;
class QLineEdit;
class QObject;
class QPushButton;
class QSlider;
class QSpinBox;

namespace Settings {
class BasicSetting;
} // namespace Settings

namespace ConfigurationShared {

enum class RequestType {
    Default,
    ComboBox,
    SpinBox,
    Slider,
    ReverseSlider,
    LineEdit,
    HexEdit,
    DateTimeEdit,
    MaxEnum,
};

class Widget : public QWidget {
    Q_OBJECT

public:
    /**
     * @param setting The primary Setting to create the Widget for
     * @param translations Map of translations to display on the left side label/checkbox
     * @param combobox_translations Map of translations for enumerating combo boxes
     * @param parent Qt parent
     * @param runtime_lock Emulated guest powered on state, for use on settings that should be
     * configured during guest execution
     * @param apply_funcs_ List to append, functions to run to apply the widget state to the setting
     * @param request What type of data representation component to create -- not always respected
     * for the Setting data type
     * @param managed Set true if the caller will set up component data and handling
     * @param multiplier Value to multiply the slider feedback label
     * @param other_setting Second setting to modify, to replace the label with a checkbox
     * @param suffix Set to specify formats for Slider feedback labels or SpinBox
     */
    explicit Widget(Settings::BasicSetting* setting, const TranslationMap& translations,
                    const ComboboxTranslationMap& combobox_translations, QWidget* parent,
                    bool runtime_lock, std::forward_list<std::function<void(bool)>>& apply_funcs_,
                    RequestType request = RequestType::Default, bool managed = true,
                    float multiplier = 1.0f, Settings::BasicSetting* other_setting = nullptr,
                    const QString& suffix = QStringLiteral(""));
    virtual ~Widget();

    /**
     * @returns True if the Widget successfully created the components for the setting
     */
    bool Valid() const;

    /**
     * Creates a button to appear when a setting has been modified. This exists for custom
     * configurations and wasn't designed to work for the global configuration. It has public access
     * for settings that need to be unmanaged but can be custom.
     *
     * @param using_global The global state of the setting this button is for
     * @param parent QWidget parent
     */
    [[nodiscard]] static QPushButton* CreateRestoreGlobalButton(bool using_global, QWidget* parent);

    // Direct handles to sub components created
    QPushButton* restore_button{}; ///< Restore button for custom configurations
    QLineEdit* line_edit{};        ///< QLineEdit, used for LineEdit and HexEdit
    QSpinBox* spinbox{};
    QCheckBox* checkbox{};
    QSlider* slider{};
    QComboBox* combobox{};
    QDateTimeEdit* date_time_edit{};

private:
    void SetupComponent(const QString& label, std::function<void()>& load_func, bool managed,
                        RequestType request, float multiplier,
                        Settings::BasicSetting* other_setting, const QString& suffix);

    QLabel* CreateLabel(const QString& text);
    QWidget* CreateCheckBox(Settings::BasicSetting* bool_setting, const QString& label,
                            std::function<std::string()>& serializer,
                            std::function<void()>& restore_func,
                            const std::function<void()>& touch);

    QWidget* CreateCombobox(std::function<std::string()>& serializer,
                            std::function<void()>& restore_func,
                            const std::function<void()>& touch);
    QWidget* CreateLineEdit(std::function<std::string()>& serializer,
                            std::function<void()>& restore_func, const std::function<void()>& touch,
                            bool managed = true);
    QWidget* CreateHexEdit(std::function<std::string()>& serializer,
                           std::function<void()>& restore_func, const std::function<void()>& touch);
    QWidget* CreateSlider(bool reversed, float multiplier, const QString& suffix,
                          std::function<std::string()>& serializer,
                          std::function<void()>& restore_func, const std::function<void()>& touch);
    QWidget* CreateDateTimeEdit(bool disabled, bool restrict,
                                std::function<std::string()>& serializer,
                                std::function<void()>& restore_func,
                                const std::function<void()>& touch);
    QWidget* CreateSpinBox(const QString& suffix, std::function<std::string()>& serializer,
                           std::function<void()>& restore_func, const std::function<void()>& touch);

    QWidget* parent;
    const TranslationMap& translations;
    const ComboboxTranslationMap& combobox_enumerations;
    Settings::BasicSetting& setting;
    std::forward_list<std::function<void(bool)>>& apply_funcs;

    bool created{false};
    bool runtime_lock{false};
};

class Builder {
public:
    explicit Builder(QWidget* parent, bool runtime_lock);
    ~Builder();

    Widget* BuildWidget(Settings::BasicSetting* setting,
                        std::forward_list<std::function<void(bool)>>& apply_funcs,
                        RequestType request = RequestType::Default, bool managed = true,
                        float multiplier = 1.0f, Settings::BasicSetting* other_setting = nullptr,
                        const QString& suffix = QStringLiteral("")) const;

    Widget* BuildWidget(Settings::BasicSetting* setting,
                        std::forward_list<std::function<void(bool)>>& apply_funcs,
                        Settings::BasicSetting* other_setting,
                        RequestType request = RequestType::Default,
                        const QString& suffix = QStringLiteral("")) const;

    const ComboboxTranslationMap& ComboboxTranslations() const;

private:
    std::unique_ptr<TranslationMap> translations;
    std::unique_ptr<ComboboxTranslationMap> combobox_translations;

    QWidget* parent;
    const bool runtime_lock;
};

} // namespace ConfigurationShared

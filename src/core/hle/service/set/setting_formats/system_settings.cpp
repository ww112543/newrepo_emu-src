// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/set/setting_formats/system_settings.h"

namespace Service::Set {

SystemSettings DefaultSystemSettings() {
    SystemSettings settings{};

    settings.version = 0x140000;
    settings.flags = 7;

    settings.mii_author_id = Common::UUID::MakeDefault();

    settings.color_set_id = ColorSet::BasicWhite;

    settings.notification_settings = {
        .flags{0x300},
        .volume = NotificationVolume::High,
        .start_time = {.hour = 9, .minute = 0},
        .stop_time = {.hour = 21, .minute = 0},
    };

    settings.tv_settings = {
        .flags = {0xC},
        .tv_resolution = TvResolution::Auto,
        .hdmi_content_type = HdmiContentType::Game,
        .rgb_range = RgbRange::Auto,
        .cmu_mode = CmuMode::None,
        .tv_underscan = {},
        .tv_gama = 1.0f,
        .contrast_ratio = 0.5f,
    };

    settings.initial_launch_settings_packed = {
        .flags = {0x10001},
        .timestamp = {},
    };

    settings.sleep_settings = {
        .flags = {0x3},
        .handheld_sleep_plan = HandheldSleepPlan::Sleep10Min,
        .console_sleep_plan = ConsoleSleepPlan::Sleep1Hour,
    };

    settings.device_time_zone_location_name = {"UTC"};
    settings.user_system_clock_automatic_correction_enabled = true;

    settings.primary_album_storage = PrimaryAlbumStorage::SdCard;
    settings.battery_percentage_flag = true;
    settings.chinese_traditional_input_method = ChineseTraditionalInputMethod::Unknown0;
    settings.vibration_master_volume = 1.0f;

    return settings;
}

} // namespace Service::Set

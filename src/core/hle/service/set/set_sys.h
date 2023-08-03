// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/uuid.h"
#include "core/hle/service/service.h"
#include "core/hle/service/time/clock_types.h"

namespace Core {
class System;
}

namespace Service::Set {

class SET_SYS final : public ServiceFramework<SET_SYS> {
public:
    explicit SET_SYS(Core::System& system_);
    ~SET_SYS() override;

private:
    /// Indicates the current theme set by the system settings
    enum class ColorSet : u32 {
        BasicWhite = 0,
        BasicBlack = 1,
    };

    /// Indicates the current console is a retail or kiosk unit
    enum class QuestFlag : u8 {
        Retail = 0,
        Kiosk = 1,
    };

    /// This is nn::settings::system::TvResolution
    enum class TvResolution : u32 {
        Auto,
        Resolution1080p,
        Resolution720p,
        Resolution480p,
    };

    /// This is nn::settings::system::HdmiContentType
    enum class HdmiContentType : u32 {
        None,
        Graphics,
        Cinema,
        Photo,
        Game,
    };

    /// This is nn::settings::system::RgbRange
    enum class RgbRange : u32 {
        Auto,
        Full,
        Limited,
    };

    /// This is nn::settings::system::CmuMode
    enum class CmuMode : u32 {
        None,
        ColorInvert,
        HighContrast,
        GrayScale,
    };

    /// This is nn::settings::system::PrimaryAlbumStorage
    enum class PrimaryAlbumStorage : u32 {
        Nand,
        SdCard,
    };

    /// This is nn::settings::system::NotificationVolume
    enum class NotificationVolume : u32 {
        Mute,
        Low,
        High,
    };

    /// This is nn::settings::system::ChineseTraditionalInputMethod
    enum class ChineseTraditionalInputMethod : u32 {
        Unknown0 = 0,
        Unknown1 = 1,
        Unknown2 = 2,
    };

    /// This is nn::settings::system::ErrorReportSharePermission
    enum class ErrorReportSharePermission : u32 {
        NotConfirmed,
        Granted,
        Denied,
    };

    /// This is nn::settings::system::FriendPresenceOverlayPermission
    enum class FriendPresenceOverlayPermission : u8 {
        NotConfirmed,
        NoDisplay,
        FavoriteFriends,
        Friends,
    };

    /// This is nn::settings::system::HandheldSleepPlan
    enum class HandheldSleepPlan : u32 {
        Sleep1Min,
        Sleep3Min,
        Sleep5Min,
        Sleep10Min,
        Sleep30Min,
        Never,
    };

    /// This is nn::settings::system::ConsoleSleepPlan
    enum class ConsoleSleepPlan : u32 {
        Sleep1Hour,
        Sleep2Hour,
        Sleep3Hour,
        Sleep6Hour,
        Sleep12Hour,
        Never,
    };

    /// This is nn::settings::system::SleepFlag
    struct SleepFlag {
        union {
            u32 raw{};

            BitField<0, 1, u32> SleepsWhilePlayingMedia;
            BitField<1, 1, u32> WakesAtPowerStateChange;
        };
    };
    static_assert(sizeof(SleepFlag) == 4, "TvFlag is an invalid size");

    /// This is nn::settings::system::TvFlag
    struct TvFlag {
        union {
            u32 raw{};

            BitField<0, 1, u32> Allows4k;
            BitField<1, 1, u32> Allows3d;
            BitField<2, 1, u32> AllowsCec;
            BitField<3, 1, u32> PreventsScreenBurnIn;
        };
    };
    static_assert(sizeof(TvFlag) == 4, "TvFlag is an invalid size");

    /// This is nn::settings::system::InitialLaunchFlag
    struct InitialLaunchFlag {
        union {
            u32 raw{};

            BitField<0, 1, u32> InitialLaunchCompletionFlag;
            BitField<8, 1, u32> InitialLaunchUserAdditionFlag;
            BitField<16, 1, u32> InitialLaunchTimestampFlag;
        };
    };
    static_assert(sizeof(InitialLaunchFlag) == 4, "InitialLaunchFlag is an invalid size");

    /// This is nn::settings::system::NotificationFlag
    struct NotificationFlag {
        union {
            u32 raw{};

            BitField<0, 1, u32> RingtoneFlag;
            BitField<1, 1, u32> DownloadCompletionFlag;
            BitField<8, 1, u32> EnablesNews;
            BitField<9, 1, u32> IncomingLampFlag;
        };
    };
    static_assert(sizeof(NotificationFlag) == 4, "NotificationFlag is an invalid size");

    /// This is nn::settings::system::AccountNotificationFlag
    struct AccountNotificationFlag {
        union {
            u32 raw{};

            BitField<0, 1, u32> FriendOnlineFlag;
            BitField<1, 1, u32> FriendRequestFlag;
            BitField<8, 1, u32> CoralInvitationFlag;
        };
    };
    static_assert(sizeof(AccountNotificationFlag) == 4,
                  "AccountNotificationFlag is an invalid size");

    /// This is nn::settings::system::TvSettings
    struct TvSettings {
        TvFlag flags;
        TvResolution tv_resolution;
        HdmiContentType hdmi_content_type;
        RgbRange rgb_range;
        CmuMode cmu_mode;
        u32 tv_underscan;
        f32 tv_gama;
        f32 constrast_ratio;
    };
    static_assert(sizeof(TvSettings) == 0x20, "TvSettings is an invalid size");

    /// This is nn::settings::system::NotificationTime
    struct NotificationTime {
        u32 hour;
        u32 minute;
    };
    static_assert(sizeof(NotificationTime) == 0x8, "NotificationTime is an invalid size");

    /// This is nn::settings::system::NotificationSettings
    struct NotificationSettings {
        NotificationFlag flags;
        NotificationVolume volume;
        NotificationTime start_time;
        NotificationTime stop_time;
    };
    static_assert(sizeof(NotificationSettings) == 0x18, "NotificationSettings is an invalid size");

    /// This is nn::settings::system::AccountSettings
    struct AccountSettings {
        u32 flags;
    };
    static_assert(sizeof(AccountSettings) == 0x4, "AccountSettings is an invalid size");

    /// This is nn::settings::system::AccountNotificationSettings
    struct AccountNotificationSettings {
        Common::UUID uid;
        AccountNotificationFlag flags;
        FriendPresenceOverlayPermission friend_presence_permission;
        FriendPresenceOverlayPermission friend_invitation_permission;
        INSERT_PADDING_BYTES(0x2);
    };
    static_assert(sizeof(AccountNotificationSettings) == 0x18,
                  "AccountNotificationSettings is an invalid size");

    /// This is nn::settings::system::InitialLaunchSettings
    struct SleepSettings {
        SleepFlag flags;
        HandheldSleepPlan handheld_sleep_plan;
        ConsoleSleepPlan console_sleep_plan;
    };
    static_assert(sizeof(SleepSettings) == 0xc, "SleepSettings is incorrect size");

    /// This is nn::settings::system::InitialLaunchSettings
    struct InitialLaunchSettings {
        InitialLaunchFlag flags;
        INSERT_PADDING_BYTES(0x4);
        Time::Clock::SteadyClockTimePoint timestamp;
    };
    static_assert(sizeof(InitialLaunchSettings) == 0x20, "InitialLaunchSettings is incorrect size");

    void GetFirmwareVersion(HLERequestContext& ctx);
    void GetFirmwareVersion2(HLERequestContext& ctx);
    void GetAccountSettings(HLERequestContext& ctx);
    void SetAccountSettings(HLERequestContext& ctx);
    void GetColorSetId(HLERequestContext& ctx);
    void SetColorSetId(HLERequestContext& ctx);
    void GetNotificationSettings(HLERequestContext& ctx);
    void SetNotificationSettings(HLERequestContext& ctx);
    void GetAccountNotificationSettings(HLERequestContext& ctx);
    void SetAccountNotificationSettings(HLERequestContext& ctx);
    void GetSettingsItemValueSize(HLERequestContext& ctx);
    void GetSettingsItemValue(HLERequestContext& ctx);
    void GetTvSettings(HLERequestContext& ctx);
    void SetTvSettings(HLERequestContext& ctx);
    void GetQuestFlag(HLERequestContext& ctx);
    void GetPrimaryAlbumStorage(HLERequestContext& ctx);
    void GetSleepSettings(HLERequestContext& ctx);
    void SetSleepSettings(HLERequestContext& ctx);
    void GetInitialLaunchSettings(HLERequestContext& ctx);
    void SetInitialLaunchSettings(HLERequestContext& ctx);
    void GetDeviceNickName(HLERequestContext& ctx);
    void SetDeviceNickName(HLERequestContext& ctx);
    void GetProductModel(HLERequestContext& ctx);
    void GetMiiAuthorId(HLERequestContext& ctx);
    void GetAutoUpdateEnableFlag(HLERequestContext& ctx);
    void GetBatteryPercentageFlag(HLERequestContext& ctx);
    void GetErrorReportSharePermission(HLERequestContext& ctx);
    void GetAppletLaunchFlags(HLERequestContext& ctx);
    void SetAppletLaunchFlags(HLERequestContext& ctx);
    void GetKeyboardLayout(HLERequestContext& ctx);
    void GetChineseTraditionalInputMethod(HLERequestContext& ctx);

    AccountSettings account_settings{
        .flags = {},
    };

    ColorSet color_set = ColorSet::BasicWhite;

    NotificationSettings notification_settings{
        .flags = {0x300},
        .volume = NotificationVolume::High,
        .start_time = {.hour = 9, .minute = 0},
        .stop_time = {.hour = 21, .minute = 0},
    };

    std::vector<AccountNotificationSettings> account_notifications{};

    TvSettings tv_settings{
        .flags = {0xc},
        .tv_resolution = TvResolution::Auto,
        .hdmi_content_type = HdmiContentType::Game,
        .rgb_range = RgbRange::Auto,
        .cmu_mode = CmuMode::None,
        .tv_underscan = {},
        .tv_gama = 1.0f,
        .constrast_ratio = 0.5f,
    };

    InitialLaunchSettings launch_settings{
        .flags = {0x10001},
        .timestamp = {},
    };

    SleepSettings sleep_settings{
        .flags = {0x3},
        .handheld_sleep_plan = HandheldSleepPlan::Sleep10Min,
        .console_sleep_plan = ConsoleSleepPlan::Sleep1Hour,
    };

    u32 applet_launch_flag{};
};

} // namespace Service::Set

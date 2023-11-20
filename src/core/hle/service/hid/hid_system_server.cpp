// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/hid/hid_core.h"
#include "core/hle/service/hid/controllers/npad.h"
#include "core/hle/service/hid/controllers/touchscreen.h"
#include "core/hle/service/hid/errors.h"
#include "core/hle/service/hid/hid_system_server.h"
#include "core/hle/service/hid/resource_manager.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::HID {

IHidSystemServer::IHidSystemServer(Core::System& system_, std::shared_ptr<ResourceManager> resource)
    : ServiceFramework{system_, "hid:sys"}, service_context{system_, service_name},
      resource_manager{resource} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {31, nullptr, "SendKeyboardLockKeyEvent"},
        {101, nullptr, "AcquireHomeButtonEventHandle"},
        {111, nullptr, "ActivateHomeButton"},
        {121, nullptr, "AcquireSleepButtonEventHandle"},
        {131, nullptr, "ActivateSleepButton"},
        {141, nullptr, "AcquireCaptureButtonEventHandle"},
        {151, nullptr, "ActivateCaptureButton"},
        {161, nullptr, "GetPlatformConfig"},
        {210, nullptr, "AcquireNfcDeviceUpdateEventHandle"},
        {211, nullptr, "GetNpadsWithNfc"},
        {212, nullptr, "AcquireNfcActivateEventHandle"},
        {213, nullptr, "ActivateNfc"},
        {214, nullptr, "GetXcdHandleForNpadWithNfc"},
        {215, nullptr, "IsNfcActivated"},
        {230, nullptr, "AcquireIrSensorEventHandle"},
        {231, nullptr, "ActivateIrSensor"},
        {232, nullptr, "GetIrSensorState"},
        {233, nullptr, "GetXcdHandleForNpadWithIrSensor"},
        {301, nullptr, "ActivateNpadSystem"},
        {303, &IHidSystemServer::ApplyNpadSystemCommonPolicy, "ApplyNpadSystemCommonPolicy"},
        {304, nullptr, "EnableAssigningSingleOnSlSrPress"},
        {305, nullptr, "DisableAssigningSingleOnSlSrPress"},
        {306, &IHidSystemServer::GetLastActiveNpad, "GetLastActiveNpad"},
        {307, nullptr, "GetNpadSystemExtStyle"},
        {308, nullptr, "ApplyNpadSystemCommonPolicyFull"},
        {309, nullptr, "GetNpadFullKeyGripColor"},
        {310, nullptr, "GetMaskedSupportedNpadStyleSet"},
        {311, nullptr, "SetNpadPlayerLedBlinkingDevice"},
        {312, nullptr, "SetSupportedNpadStyleSetAll"},
        {313, nullptr, "GetNpadCaptureButtonAssignment"},
        {314, nullptr, "GetAppletFooterUiType"},
        {315, nullptr, "GetAppletDetailedUiType"},
        {316, nullptr, "GetNpadInterfaceType"},
        {317, nullptr, "GetNpadLeftRightInterfaceType"},
        {318, nullptr, "HasBattery"},
        {319, nullptr, "HasLeftRightBattery"},
        {321, &IHidSystemServer::GetUniquePadsFromNpad, "GetUniquePadsFromNpad"},
        {322, nullptr, "GetIrSensorState"},
        {323, nullptr, "GetXcdHandleForNpadWithIrSensor"},
        {324, nullptr, "GetUniquePadButtonSet"},
        {325, nullptr, "GetUniquePadColor"},
        {326, nullptr, "GetUniquePadAppletDetailedUiType"},
        {327, nullptr, "GetAbstractedPadIdDataFromNpad"},
        {328, nullptr, "AttachAbstractedPadToNpad"},
        {329, nullptr, "DetachAbstractedPadAll"},
        {330, nullptr, "CheckAbstractedPadConnection"},
        {500, nullptr, "SetAppletResourceUserId"},
        {501, nullptr, "RegisterAppletResourceUserId"},
        {502, nullptr, "UnregisterAppletResourceUserId"},
        {503, nullptr, "EnableAppletToGetInput"},
        {504, nullptr, "SetAruidValidForVibration"},
        {505, nullptr, "EnableAppletToGetSixAxisSensor"},
        {506, nullptr, "EnableAppletToGetPadInput"},
        {507, nullptr, "EnableAppletToGetTouchScreen"},
        {510, nullptr, "SetVibrationMasterVolume"},
        {511, nullptr, "GetVibrationMasterVolume"},
        {512, nullptr, "BeginPermitVibrationSession"},
        {513, nullptr, "EndPermitVibrationSession"},
        {514, nullptr, "Unknown514"},
        {520, nullptr, "EnableHandheldHids"},
        {521, nullptr, "DisableHandheldHids"},
        {522, nullptr, "SetJoyConRailEnabled"},
        {523, nullptr, "IsJoyConRailEnabled"},
        {524, nullptr, "IsHandheldHidsEnabled"},
        {525, nullptr, "IsJoyConAttachedOnAllRail"},
        {540, nullptr, "AcquirePlayReportControllerUsageUpdateEvent"},
        {541, nullptr, "GetPlayReportControllerUsages"},
        {542, nullptr, "AcquirePlayReportRegisteredDeviceUpdateEvent"},
        {543, nullptr, "GetRegisteredDevicesOld"},
        {544, nullptr, "AcquireConnectionTriggerTimeoutEvent"},
        {545, nullptr, "SendConnectionTrigger"},
        {546, nullptr, "AcquireDeviceRegisteredEventForControllerSupport"},
        {547, nullptr, "GetAllowedBluetoothLinksCount"},
        {548, nullptr, "GetRegisteredDevices"},
        {549, nullptr, "GetConnectableRegisteredDevices"},
        {700, nullptr, "ActivateUniquePad"},
        {702, nullptr, "AcquireUniquePadConnectionEventHandle"},
        {703, nullptr, "GetUniquePadIds"},
        {751, &IHidSystemServer::AcquireJoyDetachOnBluetoothOffEventHandle, "AcquireJoyDetachOnBluetoothOffEventHandle"},
        {800, nullptr, "ListSixAxisSensorHandles"},
        {801, nullptr, "IsSixAxisSensorUserCalibrationSupported"},
        {802, nullptr, "ResetSixAxisSensorCalibrationValues"},
        {803, nullptr, "StartSixAxisSensorUserCalibration"},
        {804, nullptr, "CancelSixAxisSensorUserCalibration"},
        {805, nullptr, "GetUniquePadBluetoothAddress"},
        {806, nullptr, "DisconnectUniquePad"},
        {807, nullptr, "GetUniquePadType"},
        {808, nullptr, "GetUniquePadInterface"},
        {809, nullptr, "GetUniquePadSerialNumber"},
        {810, nullptr, "GetUniquePadControllerNumber"},
        {811, nullptr, "GetSixAxisSensorUserCalibrationStage"},
        {812, nullptr, "GetConsoleUniqueSixAxisSensorHandle"},
        {821, nullptr, "StartAnalogStickManualCalibration"},
        {822, nullptr, "RetryCurrentAnalogStickManualCalibrationStage"},
        {823, nullptr, "CancelAnalogStickManualCalibration"},
        {824, nullptr, "ResetAnalogStickManualCalibration"},
        {825, nullptr, "GetAnalogStickState"},
        {826, nullptr, "GetAnalogStickManualCalibrationStage"},
        {827, nullptr, "IsAnalogStickButtonPressed"},
        {828, nullptr, "IsAnalogStickInReleasePosition"},
        {829, nullptr, "IsAnalogStickInCircumference"},
        {830, nullptr, "SetNotificationLedPattern"},
        {831, nullptr, "SetNotificationLedPatternWithTimeout"},
        {832, nullptr, "PrepareHidsForNotificationWake"},
        {850, &IHidSystemServer::IsUsbFullKeyControllerEnabled, "IsUsbFullKeyControllerEnabled"},
        {851, nullptr, "EnableUsbFullKeyController"},
        {852, nullptr, "IsUsbConnected"},
        {870, nullptr, "IsHandheldButtonPressedOnConsoleMode"},
        {900, nullptr, "ActivateInputDetector"},
        {901, nullptr, "NotifyInputDetector"},
        {1000, nullptr, "InitializeFirmwareUpdate"},
        {1001, nullptr, "GetFirmwareVersion"},
        {1002, nullptr, "GetAvailableFirmwareVersion"},
        {1003, nullptr, "IsFirmwareUpdateAvailable"},
        {1004, nullptr, "CheckFirmwareUpdateRequired"},
        {1005, nullptr, "StartFirmwareUpdate"},
        {1006, nullptr, "AbortFirmwareUpdate"},
        {1007, nullptr, "GetFirmwareUpdateState"},
        {1008, nullptr, "ActivateAudioControl"},
        {1009, nullptr, "AcquireAudioControlEventHandle"},
        {1010, nullptr, "GetAudioControlStates"},
        {1011, nullptr, "DeactivateAudioControl"},
        {1050, nullptr, "IsSixAxisSensorAccurateUserCalibrationSupported"},
        {1051, nullptr, "StartSixAxisSensorAccurateUserCalibration"},
        {1052, nullptr, "CancelSixAxisSensorAccurateUserCalibration"},
        {1053, nullptr, "GetSixAxisSensorAccurateUserCalibrationState"},
        {1100, nullptr, "GetHidbusSystemServiceObject"},
        {1120, nullptr, "SetFirmwareHotfixUpdateSkipEnabled"},
        {1130, nullptr, "InitializeUsbFirmwareUpdate"},
        {1131, nullptr, "FinalizeUsbFirmwareUpdate"},
        {1132, nullptr, "CheckUsbFirmwareUpdateRequired"},
        {1133, nullptr, "StartUsbFirmwareUpdate"},
        {1134, nullptr, "GetUsbFirmwareUpdateState"},
        {1150, nullptr, "SetTouchScreenMagnification"},
        {1151, nullptr, "GetTouchScreenFirmwareVersion"},
        {1152, nullptr, "SetTouchScreenDefaultConfiguration"},
        {1153, &IHidSystemServer::GetTouchScreenDefaultConfiguration, "GetTouchScreenDefaultConfiguration"},
        {1154, nullptr, "IsFirmwareAvailableForNotification"},
        {1155, nullptr, "SetForceHandheldStyleVibration"},
        {1156, nullptr, "SendConnectionTriggerWithoutTimeoutEvent"},
        {1157, nullptr, "CancelConnectionTrigger"},
        {1200, nullptr, "IsButtonConfigSupported"},
        {1201, nullptr, "IsButtonConfigEmbeddedSupported"},
        {1202, nullptr, "DeleteButtonConfig"},
        {1203, nullptr, "DeleteButtonConfigEmbedded"},
        {1204, nullptr, "SetButtonConfigEnabled"},
        {1205, nullptr, "SetButtonConfigEmbeddedEnabled"},
        {1206, nullptr, "IsButtonConfigEnabled"},
        {1207, nullptr, "IsButtonConfigEmbeddedEnabled"},
        {1208, nullptr, "SetButtonConfigEmbedded"},
        {1209, nullptr, "SetButtonConfigFull"},
        {1210, nullptr, "SetButtonConfigLeft"},
        {1211, nullptr, "SetButtonConfigRight"},
        {1212, nullptr, "GetButtonConfigEmbedded"},
        {1213, nullptr, "GetButtonConfigFull"},
        {1214, nullptr, "GetButtonConfigLeft"},
        {1215, nullptr, "GetButtonConfigRight"},
        {1250, nullptr, "IsCustomButtonConfigSupported"},
        {1251, nullptr, "IsDefaultButtonConfigEmbedded"},
        {1252, nullptr, "IsDefaultButtonConfigFull"},
        {1253, nullptr, "IsDefaultButtonConfigLeft"},
        {1254, nullptr, "IsDefaultButtonConfigRight"},
        {1255, nullptr, "IsButtonConfigStorageEmbeddedEmpty"},
        {1256, nullptr, "IsButtonConfigStorageFullEmpty"},
        {1257, nullptr, "IsButtonConfigStorageLeftEmpty"},
        {1258, nullptr, "IsButtonConfigStorageRightEmpty"},
        {1259, nullptr, "GetButtonConfigStorageEmbeddedDeprecated"},
        {1260, nullptr, "GetButtonConfigStorageFullDeprecated"},
        {1261, nullptr, "GetButtonConfigStorageLeftDeprecated"},
        {1262, nullptr, "GetButtonConfigStorageRightDeprecated"},
        {1263, nullptr, "SetButtonConfigStorageEmbeddedDeprecated"},
        {1264, nullptr, "SetButtonConfigStorageFullDeprecated"},
        {1265, nullptr, "SetButtonConfigStorageLeftDeprecated"},
        {1266, nullptr, "SetButtonConfigStorageRightDeprecated"},
        {1267, nullptr, "DeleteButtonConfigStorageEmbedded"},
        {1268, nullptr, "DeleteButtonConfigStorageFull"},
        {1269, nullptr, "DeleteButtonConfigStorageLeft"},
        {1270, nullptr, "DeleteButtonConfigStorageRight"},
        {1271, nullptr, "IsUsingCustomButtonConfig"},
        {1272, nullptr, "IsAnyCustomButtonConfigEnabled"},
        {1273, nullptr, "SetAllCustomButtonConfigEnabled"},
        {1274, nullptr, "SetDefaultButtonConfig"},
        {1275, nullptr, "SetAllDefaultButtonConfig"},
        {1276, nullptr, "SetHidButtonConfigEmbedded"},
        {1277, nullptr, "SetHidButtonConfigFull"},
        {1278, nullptr, "SetHidButtonConfigLeft"},
        {1279, nullptr, "SetHidButtonConfigRight"},
        {1280, nullptr, "GetHidButtonConfigEmbedded"},
        {1281, nullptr, "GetHidButtonConfigFull"},
        {1282, nullptr, "GetHidButtonConfigLeft"},
        {1283, nullptr, "GetHidButtonConfigRight"},
        {1284, nullptr, "GetButtonConfigStorageEmbedded"},
        {1285, nullptr, "GetButtonConfigStorageFull"},
        {1286, nullptr, "GetButtonConfigStorageLeft"},
        {1287, nullptr, "GetButtonConfigStorageRight"},
        {1288, nullptr, "SetButtonConfigStorageEmbedded"},
        {1289, nullptr, "SetButtonConfigStorageFull"},
        {1290, nullptr, "DeleteButtonConfigStorageRight"},
        {1291, nullptr, "DeleteButtonConfigStorageRight"},
    };
    // clang-format on

    RegisterHandlers(functions);

    joy_detach_event = service_context.CreateEvent("HidSys::JoyDetachEvent");
}

IHidSystemServer::~IHidSystemServer() {
    service_context.CloseEvent(joy_detach_event);
};

void IHidSystemServer::ApplyNpadSystemCommonPolicy(HLERequestContext& ctx) {
    LOG_WARNING(Service_HID, "called");

    GetResourceManager()->GetNpad()->ApplyNpadSystemCommonPolicy();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidSystemServer::GetLastActiveNpad(HLERequestContext& ctx) {
    LOG_DEBUG(Service_HID, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(system.HIDCore().GetLastActiveController());
}

void IHidSystemServer::GetUniquePadsFromNpad(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto npad_id_type{rp.PopEnum<Core::HID::NpadIdType>()};

    LOG_WARNING(Service_HID, "(STUBBED) called, npad_id_type={}", npad_id_type);

    const std::vector<Core::HID::UniquePadId> unique_pads{};

    ctx.WriteBuffer(unique_pads);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(static_cast<u32>(unique_pads.size()));
}

void IHidSystemServer::AcquireJoyDetachOnBluetoothOffEventHandle(HLERequestContext& ctx) {
    LOG_INFO(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(joy_detach_event->GetReadableEvent());
}

void IHidSystemServer::IsUsbFullKeyControllerEnabled(HLERequestContext& ctx) {
    const bool is_enabled = false;

    LOG_WARNING(Service_HID, "(STUBBED) called, is_enabled={}", is_enabled);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(is_enabled);
}

void IHidSystemServer::GetTouchScreenDefaultConfiguration(HLERequestContext& ctx) {
    LOG_WARNING(Service_HID, "(STUBBED) called");

    Core::HID::TouchScreenConfigurationForNx touchscreen_config{
        .mode = Core::HID::TouchScreenModeForNx::Finger,
    };

    if (touchscreen_config.mode != Core::HID::TouchScreenModeForNx::Heat2 &&
        touchscreen_config.mode != Core::HID::TouchScreenModeForNx::Finger) {
        touchscreen_config.mode = Core::HID::TouchScreenModeForNx::UseSystemSetting;
    }

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(ResultSuccess);
    rb.PushRaw(touchscreen_config);
}

std::shared_ptr<ResourceManager> IHidSystemServer::GetResourceManager() {
    resource_manager->Initialize();
    return resource_manager;
}

} // namespace Service::HID

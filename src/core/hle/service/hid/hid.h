// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>

#include "core/hle/service/hid/controllers/controller_base.h"
#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/service.h"

namespace Core::Timing {
struct EventType;
}

namespace Service::SM {
class ServiceManager;
}

namespace Service::HID {

enum class HidController : std::size_t {
    DebugPad,
    Touchscreen,
    Mouse,
    Keyboard,
    XPad,
    HomeButton,
    SleepButton,
    CaptureButton,
    InputDetector,
    UniquePad,
    NPad,
    Gesture,
    ConsoleSixAxisSensor,
    Palma,

    MaxControllers,
};

class IAppletResource final : public ServiceFramework<IAppletResource> {
public:
    explicit IAppletResource(Core::System& system_,
                             KernelHelpers::ServiceContext& service_context_);
    ~IAppletResource() override;

    void ActivateController(HidController controller);
    void DeactivateController(HidController controller);

    template <typename T>
    T& GetController(HidController controller) {
        return static_cast<T&>(*controllers[static_cast<size_t>(controller)]);
    }

    template <typename T>
    const T& GetController(HidController controller) const {
        return static_cast<T&>(*controllers[static_cast<size_t>(controller)]);
    }

private:
    template <typename T>
    void MakeController(HidController controller, u8* shared_memory) {
        controllers[static_cast<std::size_t>(controller)] =
            std::make_unique<T>(system.HIDCore(), shared_memory);
    }
    template <typename T>
    void MakeControllerWithServiceContext(HidController controller, u8* shared_memory) {
        controllers[static_cast<std::size_t>(controller)] =
            std::make_unique<T>(system.HIDCore(), shared_memory, service_context);
    }

    void GetSharedMemoryHandle(Kernel::HLERequestContext& ctx);
    void UpdateControllers(std::uintptr_t user_data, std::chrono::nanoseconds ns_late);
    void UpdateMouseKeyboard(std::uintptr_t user_data, std::chrono::nanoseconds ns_late);
    void UpdateMotion(std::uintptr_t user_data, std::chrono::nanoseconds ns_late);

    KernelHelpers::ServiceContext& service_context;

    std::shared_ptr<Core::Timing::EventType> pad_update_event;
    std::shared_ptr<Core::Timing::EventType> mouse_keyboard_update_event;
    std::shared_ptr<Core::Timing::EventType> motion_update_event;

    std::array<std::unique_ptr<ControllerBase>, static_cast<size_t>(HidController::MaxControllers)>
        controllers{};
};

class Hid final : public ServiceFramework<Hid> {
public:
    explicit Hid(Core::System& system_);
    ~Hid() override;

    std::shared_ptr<IAppletResource> GetAppletResource();

private:
    void CreateAppletResource(Kernel::HLERequestContext& ctx);
    void ActivateDebugPad(Kernel::HLERequestContext& ctx);
    void ActivateTouchScreen(Kernel::HLERequestContext& ctx);
    void ActivateMouse(Kernel::HLERequestContext& ctx);
    void ActivateKeyboard(Kernel::HLERequestContext& ctx);
    void SendKeyboardLockKeyEvent(Kernel::HLERequestContext& ctx);
    void ActivateXpad(Kernel::HLERequestContext& ctx);
    void GetXpadIDs(Kernel::HLERequestContext& ctx);
    void ActivateSixAxisSensor(Kernel::HLERequestContext& ctx);
    void DeactivateSixAxisSensor(Kernel::HLERequestContext& ctx);
    void StartSixAxisSensor(Kernel::HLERequestContext& ctx);
    void StopSixAxisSensor(Kernel::HLERequestContext& ctx);
    void IsSixAxisSensorFusionEnabled(Kernel::HLERequestContext& ctx);
    void EnableSixAxisSensorFusion(Kernel::HLERequestContext& ctx);
    void SetSixAxisSensorFusionParameters(Kernel::HLERequestContext& ctx);
    void GetSixAxisSensorFusionParameters(Kernel::HLERequestContext& ctx);
    void ResetSixAxisSensorFusionParameters(Kernel::HLERequestContext& ctx);
    void SetGyroscopeZeroDriftMode(Kernel::HLERequestContext& ctx);
    void GetGyroscopeZeroDriftMode(Kernel::HLERequestContext& ctx);
    void ResetGyroscopeZeroDriftMode(Kernel::HLERequestContext& ctx);
    void IsSixAxisSensorAtRest(Kernel::HLERequestContext& ctx);
    void IsFirmwareUpdateAvailableForSixAxisSensor(Kernel::HLERequestContext& ctx);
    void EnableSixAxisSensorUnalteredPassthrough(Kernel::HLERequestContext& ctx);
    void IsSixAxisSensorUnalteredPassthroughEnabled(Kernel::HLERequestContext& ctx);
    void LoadSixAxisSensorCalibrationParameter(Kernel::HLERequestContext& ctx);
    void GetSixAxisSensorIcInformation(Kernel::HLERequestContext& ctx);
    void ResetIsSixAxisSensorDeviceNewlyAssigned(Kernel::HLERequestContext& ctx);
    void ActivateGesture(Kernel::HLERequestContext& ctx);
    void SetSupportedNpadStyleSet(Kernel::HLERequestContext& ctx);
    void GetSupportedNpadStyleSet(Kernel::HLERequestContext& ctx);
    void SetSupportedNpadIdType(Kernel::HLERequestContext& ctx);
    void ActivateNpad(Kernel::HLERequestContext& ctx);
    void DeactivateNpad(Kernel::HLERequestContext& ctx);
    void AcquireNpadStyleSetUpdateEventHandle(Kernel::HLERequestContext& ctx);
    void DisconnectNpad(Kernel::HLERequestContext& ctx);
    void GetPlayerLedPattern(Kernel::HLERequestContext& ctx);
    void ActivateNpadWithRevision(Kernel::HLERequestContext& ctx);
    void SetNpadJoyHoldType(Kernel::HLERequestContext& ctx);
    void GetNpadJoyHoldType(Kernel::HLERequestContext& ctx);
    void SetNpadJoyAssignmentModeSingleByDefault(Kernel::HLERequestContext& ctx);
    void SetNpadJoyAssignmentModeSingle(Kernel::HLERequestContext& ctx);
    void SetNpadJoyAssignmentModeDual(Kernel::HLERequestContext& ctx);
    void MergeSingleJoyAsDualJoy(Kernel::HLERequestContext& ctx);
    void StartLrAssignmentMode(Kernel::HLERequestContext& ctx);
    void StopLrAssignmentMode(Kernel::HLERequestContext& ctx);
    void SetNpadHandheldActivationMode(Kernel::HLERequestContext& ctx);
    void GetNpadHandheldActivationMode(Kernel::HLERequestContext& ctx);
    void SwapNpadAssignment(Kernel::HLERequestContext& ctx);
    void IsUnintendedHomeButtonInputProtectionEnabled(Kernel::HLERequestContext& ctx);
    void EnableUnintendedHomeButtonInputProtection(Kernel::HLERequestContext& ctx);
    void SetNpadAnalogStickUseCenterClamp(Kernel::HLERequestContext& ctx);
    void SetNpadCaptureButtonAssignment(Kernel::HLERequestContext& ctx);
    void ClearNpadCaptureButtonAssignment(Kernel::HLERequestContext& ctx);
    void GetVibrationDeviceInfo(Kernel::HLERequestContext& ctx);
    void SendVibrationValue(Kernel::HLERequestContext& ctx);
    void GetActualVibrationValue(Kernel::HLERequestContext& ctx);
    void CreateActiveVibrationDeviceList(Kernel::HLERequestContext& ctx);
    void PermitVibration(Kernel::HLERequestContext& ctx);
    void IsVibrationPermitted(Kernel::HLERequestContext& ctx);
    void SendVibrationValues(Kernel::HLERequestContext& ctx);
    void SendVibrationGcErmCommand(Kernel::HLERequestContext& ctx);
    void GetActualVibrationGcErmCommand(Kernel::HLERequestContext& ctx);
    void BeginPermitVibrationSession(Kernel::HLERequestContext& ctx);
    void EndPermitVibrationSession(Kernel::HLERequestContext& ctx);
    void IsVibrationDeviceMounted(Kernel::HLERequestContext& ctx);
    void ActivateConsoleSixAxisSensor(Kernel::HLERequestContext& ctx);
    void StartConsoleSixAxisSensor(Kernel::HLERequestContext& ctx);
    void StopConsoleSixAxisSensor(Kernel::HLERequestContext& ctx);
    void ActivateSevenSixAxisSensor(Kernel::HLERequestContext& ctx);
    void StartSevenSixAxisSensor(Kernel::HLERequestContext& ctx);
    void StopSevenSixAxisSensor(Kernel::HLERequestContext& ctx);
    void InitializeSevenSixAxisSensor(Kernel::HLERequestContext& ctx);
    void FinalizeSevenSixAxisSensor(Kernel::HLERequestContext& ctx);
    void ResetSevenSixAxisSensorTimestamp(Kernel::HLERequestContext& ctx);
    void IsUsbFullKeyControllerEnabled(Kernel::HLERequestContext& ctx);
    void GetPalmaConnectionHandle(Kernel::HLERequestContext& ctx);
    void InitializePalma(Kernel::HLERequestContext& ctx);
    void AcquirePalmaOperationCompleteEvent(Kernel::HLERequestContext& ctx);
    void GetPalmaOperationInfo(Kernel::HLERequestContext& ctx);
    void PlayPalmaActivity(Kernel::HLERequestContext& ctx);
    void SetPalmaFrModeType(Kernel::HLERequestContext& ctx);
    void ReadPalmaStep(Kernel::HLERequestContext& ctx);
    void EnablePalmaStep(Kernel::HLERequestContext& ctx);
    void ResetPalmaStep(Kernel::HLERequestContext& ctx);
    void ReadPalmaApplicationSection(Kernel::HLERequestContext& ctx);
    void WritePalmaApplicationSection(Kernel::HLERequestContext& ctx);
    void ReadPalmaUniqueCode(Kernel::HLERequestContext& ctx);
    void SetPalmaUniqueCodeInvalid(Kernel::HLERequestContext& ctx);
    void WritePalmaActivityEntry(Kernel::HLERequestContext& ctx);
    void WritePalmaRgbLedPatternEntry(Kernel::HLERequestContext& ctx);
    void WritePalmaWaveEntry(Kernel::HLERequestContext& ctx);
    void SetPalmaDataBaseIdentificationVersion(Kernel::HLERequestContext& ctx);
    void GetPalmaDataBaseIdentificationVersion(Kernel::HLERequestContext& ctx);
    void SuspendPalmaFeature(Kernel::HLERequestContext& ctx);
    void GetPalmaOperationResult(Kernel::HLERequestContext& ctx);
    void ReadPalmaPlayLog(Kernel::HLERequestContext& ctx);
    void ResetPalmaPlayLog(Kernel::HLERequestContext& ctx);
    void SetIsPalmaAllConnectable(Kernel::HLERequestContext& ctx);
    void SetIsPalmaPairedConnectable(Kernel::HLERequestContext& ctx);
    void PairPalma(Kernel::HLERequestContext& ctx);
    void SetPalmaBoostMode(Kernel::HLERequestContext& ctx);
    void CancelWritePalmaWaveEntry(Kernel::HLERequestContext& ctx);
    void EnablePalmaBoostMode(Kernel::HLERequestContext& ctx);
    void GetPalmaBluetoothAddress(Kernel::HLERequestContext& ctx);
    void SetDisallowedPalmaConnection(Kernel::HLERequestContext& ctx);
    void SetNpadCommunicationMode(Kernel::HLERequestContext& ctx);
    void GetNpadCommunicationMode(Kernel::HLERequestContext& ctx);
    void SetTouchScreenConfiguration(Kernel::HLERequestContext& ctx);
    void IsFirmwareUpdateNeededForNotification(Kernel::HLERequestContext& ctx);

    std::shared_ptr<IAppletResource> applet_resource;

    KernelHelpers::ServiceContext service_context;
};

/// Registers all HID services with the specified service manager.
void InstallInterfaces(SM::ServiceManager& service_manager, Core::System& system);

} // namespace Service::HID

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <codecvt>
#include <locale>
#include <string>
#include <string_view>
#include <dlfcn.h>

#ifdef ARCHITECTURE_arm64
#include <adrenotools/driver.h>
#endif

#include <android/api-level.h>
#include <android/native_window_jni.h>
#include <core/loader/nro.h>

#include "common/detached_tasks.h"
#include "common/dynamic_library.h"
#include "common/fs/path_util.h"
#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/scm_rev.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/cpu_manager.h"
#include "core/crypto/key_manager.h"
#include "core/file_sys/card_image.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/submission_package.h"
#include "core/file_sys/vfs.h"
#include "core/file_sys/vfs_real.h"
#include "core/frontend/applets/cabinet.h"
#include "core/frontend/applets/controller.h"
#include "core/frontend/applets/error.h"
#include "core/frontend/applets/general_frontend.h"
#include "core/frontend/applets/mii_edit.h"
#include "core/frontend/applets/profile_select.h"
#include "core/frontend/applets/software_keyboard.h"
#include "core/frontend/applets/web_browser.h"
#include "core/hid/emulated_controller.h"
#include "core/hid/hid_core.h"
#include "core/hid/hid_types.h"
#include "core/hle/service/acc/profile_manager.h"
#include "core/hle/service/am/applet_ae.h"
#include "core/hle/service/am/applet_oe.h"
#include "core/hle/service/am/applets/applets.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/loader/loader.h"
#include "core/perf_stats.h"
#include "jni/android_common/android_common.h"
#include "jni/applets/software_keyboard.h"
#include "jni/config.h"
#include "jni/emu_window/emu_window.h"
#include "jni/id_cache.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/renderer_base.h"

namespace {

class EmulationSession final {
public:
    EmulationSession() {
        m_vfs = std::make_shared<FileSys::RealVfsFilesystem>();
    }

    ~EmulationSession() = default;

    static EmulationSession& GetInstance() {
        return s_instance;
    }

    const Core::System& System() const {
        return m_system;
    }

    Core::System& System() {
        return m_system;
    }

    const EmuWindow_Android& Window() const {
        return *m_window;
    }

    EmuWindow_Android& Window() {
        return *m_window;
    }

    ANativeWindow* NativeWindow() const {
        return m_native_window;
    }

    void SetNativeWindow(ANativeWindow* native_window) {
        m_native_window = native_window;
    }

    int InstallFileToNand(std::string filename) {
        const auto copy_func = [](const FileSys::VirtualFile& src, const FileSys::VirtualFile& dest,
                                  std::size_t block_size) {
            if (src == nullptr || dest == nullptr) {
                return false;
            }
            if (!dest->Resize(src->GetSize())) {
                return false;
            }

            using namespace Common::Literals;
            std::vector<u8> buffer(1_MiB);

            for (std::size_t i = 0; i < src->GetSize(); i += buffer.size()) {
                const auto read = src->Read(buffer.data(), buffer.size(), i);
                dest->Write(buffer.data(), read, i);
            }
            return true;
        };

        enum InstallResult {
            Success = 0,
            SuccessFileOverwritten = 1,
            InstallError = 2,
            ErrorBaseGame = 3,
            ErrorFilenameExtension = 4,
        };

        m_system.SetContentProvider(std::make_unique<FileSys::ContentProviderUnion>());
        m_system.GetFileSystemController().CreateFactories(*m_vfs);

        std::shared_ptr<FileSys::NSP> nsp;
        if (filename.ends_with("nsp")) {
            nsp = std::make_shared<FileSys::NSP>(m_vfs->OpenFile(filename, FileSys::Mode::Read));
            if (nsp->IsExtractedType()) {
                return InstallError;
            }
        } else if (filename.ends_with("xci")) {
            const auto xci =
                std::make_shared<FileSys::XCI>(m_vfs->OpenFile(filename, FileSys::Mode::Read));
            nsp = xci->GetSecurePartitionNSP();
        } else {
            return ErrorFilenameExtension;
        }

        if (!nsp) {
            return InstallError;
        }

        if (nsp->GetStatus() != Loader::ResultStatus::Success) {
            return InstallError;
        }

        const auto res = m_system.GetFileSystemController().GetUserNANDContents()->InstallEntry(
            *nsp, true, copy_func);

        switch (res) {
        case FileSys::InstallResult::Success:
            return Success;
        case FileSys::InstallResult::OverwriteExisting:
            return SuccessFileOverwritten;
        case FileSys::InstallResult::ErrorBaseInstall:
            return ErrorBaseGame;
        default:
            return InstallError;
        }
    }

    void InitializeGpuDriver(const std::string& hook_lib_dir, const std::string& custom_driver_dir,
                             const std::string& custom_driver_name,
                             const std::string& file_redirect_dir) {
#ifdef ARCHITECTURE_arm64
        void* handle{};
        const char* file_redirect_dir_{};
        int featureFlags{};

        // Enable driver file redirection when renderer debugging is enabled.
        if (Settings::values.renderer_debug && file_redirect_dir.size()) {
            featureFlags |= ADRENOTOOLS_DRIVER_FILE_REDIRECT;
            file_redirect_dir_ = file_redirect_dir.c_str();
        }

        // Try to load a custom driver.
        if (custom_driver_name.size()) {
            handle = adrenotools_open_libvulkan(
                RTLD_NOW, featureFlags | ADRENOTOOLS_DRIVER_CUSTOM, nullptr, hook_lib_dir.c_str(),
                custom_driver_dir.c_str(), custom_driver_name.c_str(), file_redirect_dir_, nullptr);
        }

        // Try to load the system driver.
        if (!handle) {
            handle =
                adrenotools_open_libvulkan(RTLD_NOW, featureFlags, nullptr, hook_lib_dir.c_str(),
                                           nullptr, nullptr, file_redirect_dir_, nullptr);
        }

        m_vulkan_library = std::make_shared<Common::DynamicLibrary>(handle);
#endif
    }

    bool IsRunning() const {
        std::scoped_lock lock(m_mutex);
        return m_is_running;
    }

    const Core::PerfStatsResults& PerfStats() const {
        std::scoped_lock m_perf_stats_lock(m_perf_stats_mutex);
        return m_perf_stats;
    }

    void SurfaceChanged() {
        if (!IsRunning()) {
            return;
        }
        m_window->OnSurfaceChanged(m_native_window);
        m_system.Renderer().NotifySurfaceChanged();
    }

    Core::SystemResultStatus InitializeEmulation(const std::string& filepath) {
        std::scoped_lock lock(m_mutex);

        // Loads the configuration.
        Config{};

        // Create the render window.
        m_window = std::make_unique<EmuWindow_Android>(&m_input_subsystem, m_native_window,
                                                       m_vulkan_library);

        m_system.SetFilesystem(m_vfs);

        // Initialize system.
        auto android_keyboard = std::make_unique<SoftwareKeyboard::AndroidKeyboard>();
        m_software_keyboard = android_keyboard.get();
        m_system.SetShuttingDown(false);
        m_system.ApplySettings();
        m_system.HIDCore().ReloadInputDevices();
        m_system.SetAppletFrontendSet({
            nullptr,                     // Amiibo Settings
            nullptr,                     // Controller Selector
            nullptr,                     // Error Display
            nullptr,                     // Mii Editor
            nullptr,                     // Parental Controls
            nullptr,                     // Photo Viewer
            nullptr,                     // Profile Selector
            std::move(android_keyboard), // Software Keyboard
            nullptr,                     // Web Browser
        });
        m_system.SetContentProvider(std::make_unique<FileSys::ContentProviderUnion>());
        m_system.GetFileSystemController().CreateFactories(*m_vfs);

        // Initialize account manager
        m_profile_manager = std::make_unique<Service::Account::ProfileManager>();

        // Load the ROM.
        m_load_result = m_system.Load(EmulationSession::GetInstance().Window(), filepath);
        if (m_load_result != Core::SystemResultStatus::Success) {
            return m_load_result;
        }

        // Complete initialization.
        m_system.GPU().Start();
        m_system.GetCpuManager().OnGpuReady();
        m_system.RegisterExitCallback([&] { HaltEmulation(); });

        return Core::SystemResultStatus::Success;
    }

    void ShutdownEmulation() {
        std::scoped_lock lock(m_mutex);

        m_is_running = false;

        // Unload user input.
        m_system.HIDCore().UnloadInputDevices();

        // Shutdown the main emulated process
        if (m_load_result == Core::SystemResultStatus::Success) {
            m_system.DetachDebugger();
            m_system.ShutdownMainProcess();
            m_detached_tasks.WaitForAllTasks();
            m_load_result = Core::SystemResultStatus::ErrorNotInitialized;
        }

        // Tear down the render window.
        m_window.reset();
    }

    void PauseEmulation() {
        std::scoped_lock lock(m_mutex);
        m_system.Pause();
    }

    void UnPauseEmulation() {
        std::scoped_lock lock(m_mutex);
        m_system.Run();
    }

    void HaltEmulation() {
        std::scoped_lock lock(m_mutex);
        m_is_running = false;
        m_cv.notify_one();
    }

    void RunEmulation() {
        {
            std::scoped_lock lock(m_mutex);
            m_is_running = true;
        }

        // Load the disk shader cache.
        if (Settings::values.use_disk_shader_cache.GetValue()) {
            LoadDiskCacheProgress(VideoCore::LoadCallbackStage::Prepare, 0, 0);
            m_system.Renderer().ReadRasterizer()->LoadDiskResources(
                m_system.GetApplicationProcessProgramID(), std::stop_token{},
                LoadDiskCacheProgress);
            LoadDiskCacheProgress(VideoCore::LoadCallbackStage::Complete, 0, 0);
        }

        void(m_system.Run());

        if (m_system.DebuggerEnabled()) {
            m_system.InitializeDebugger();
        }

        while (true) {
            {
                std::unique_lock lock(m_mutex);
                if (m_cv.wait_for(lock, std::chrono::milliseconds(800),
                                  [&]() { return !m_is_running; })) {
                    // Emulation halted.
                    break;
                }
            }
            {
                // Refresh performance stats.
                std::scoped_lock m_perf_stats_lock(m_perf_stats_mutex);
                m_perf_stats = m_system.GetAndResetPerfStats();
            }
        }
    }

    std::string GetRomTitle(const std::string& path) {
        return GetRomMetadata(path).title;
    }

    std::vector<u8> GetRomIcon(const std::string& path) {
        return GetRomMetadata(path).icon;
    }

    bool GetIsHomebrew(const std::string& path) {
        return GetRomMetadata(path).isHomebrew;
    }

    void ResetRomMetadata() {
        m_rom_metadata_cache.clear();
    }

    bool IsHandheldOnly() {
        const auto npad_style_set = m_system.HIDCore().GetSupportedStyleTag();

        if (npad_style_set.fullkey == 1) {
            return false;
        }

        if (npad_style_set.handheld == 0) {
            return false;
        }

        return !Settings::values.use_docked_mode.GetValue();
    }

    void SetDeviceType(int index, int type) {
        auto controller = m_system.HIDCore().GetEmulatedControllerByIndex(index);
        controller->SetNpadStyleIndex(static_cast<Core::HID::NpadStyleIndex>(type));
    }

    void OnGamepadConnectEvent(int index) {
        auto controller = m_system.HIDCore().GetEmulatedControllerByIndex(index);

        // Ensure that player1 is configured correctly and handheld disconnected
        if (controller->GetNpadIdType() == Core::HID::NpadIdType::Player1) {
            auto handheld =
                m_system.HIDCore().GetEmulatedController(Core::HID::NpadIdType::Handheld);

            if (controller->GetNpadStyleIndex() == Core::HID::NpadStyleIndex::Handheld) {
                handheld->SetNpadStyleIndex(Core::HID::NpadStyleIndex::ProController);
                controller->SetNpadStyleIndex(Core::HID::NpadStyleIndex::ProController);
                handheld->Disconnect();
            }
        }

        // Ensure that handheld is configured correctly and player 1 disconnected
        if (controller->GetNpadIdType() == Core::HID::NpadIdType::Handheld) {
            auto player1 = m_system.HIDCore().GetEmulatedController(Core::HID::NpadIdType::Player1);

            if (controller->GetNpadStyleIndex() != Core::HID::NpadStyleIndex::Handheld) {
                player1->SetNpadStyleIndex(Core::HID::NpadStyleIndex::Handheld);
                controller->SetNpadStyleIndex(Core::HID::NpadStyleIndex::Handheld);
                player1->Disconnect();
            }
        }

        if (!controller->IsConnected()) {
            controller->Connect();
        }
    }

    void OnGamepadDisconnectEvent(int index) {
        auto controller = m_system.HIDCore().GetEmulatedControllerByIndex(index);
        controller->Disconnect();
    }

    SoftwareKeyboard::AndroidKeyboard* SoftwareKeyboard() {
        return m_software_keyboard;
    }

private:
    struct RomMetadata {
        std::string title;
        std::vector<u8> icon;
        bool isHomebrew;
    };

    RomMetadata GetRomMetadata(const std::string& path) {
        if (auto search = m_rom_metadata_cache.find(path); search != m_rom_metadata_cache.end()) {
            return search->second;
        }

        return CacheRomMetadata(path);
    }

    RomMetadata CacheRomMetadata(const std::string& path) {
        const auto file = Core::GetGameFileFromPath(m_vfs, path);
        auto loader = Loader::GetLoader(EmulationSession::GetInstance().System(), file, 0, 0);

        RomMetadata entry;
        loader->ReadTitle(entry.title);
        loader->ReadIcon(entry.icon);
        if (loader->GetFileType() == Loader::FileType::NRO) {
            auto loader_nro = dynamic_cast<Loader::AppLoader_NRO*>(loader.get());
            entry.isHomebrew = loader_nro->IsHomebrew();
        } else {
            entry.isHomebrew = false;
        }

        m_rom_metadata_cache[path] = entry;

        return entry;
    }

private:
    static void LoadDiskCacheProgress(VideoCore::LoadCallbackStage stage, int progress, int max) {
        JNIEnv* env = IDCache::GetEnvForThread();
        env->CallStaticVoidMethod(IDCache::GetDiskCacheProgressClass(),
                                  IDCache::GetDiskCacheLoadProgress(), static_cast<jint>(stage),
                                  static_cast<jint>(progress), static_cast<jint>(max));
    }

private:
    static EmulationSession s_instance;

    // Frontend management
    std::unordered_map<std::string, RomMetadata> m_rom_metadata_cache;

    // Window management
    std::unique_ptr<EmuWindow_Android> m_window;
    ANativeWindow* m_native_window{};

    // Core emulation
    Core::System m_system;
    InputCommon::InputSubsystem m_input_subsystem;
    Common::DetachedTasks m_detached_tasks;
    Core::PerfStatsResults m_perf_stats{};
    std::shared_ptr<FileSys::VfsFilesystem> m_vfs;
    Core::SystemResultStatus m_load_result{Core::SystemResultStatus::ErrorNotInitialized};
    bool m_is_running{};
    SoftwareKeyboard::AndroidKeyboard* m_software_keyboard{};
    std::unique_ptr<Service::Account::ProfileManager> m_profile_manager;

    // GPU driver parameters
    std::shared_ptr<Common::DynamicLibrary> m_vulkan_library;

    // Synchronization
    std::condition_variable_any m_cv;
    mutable std::mutex m_perf_stats_mutex;
    mutable std::mutex m_mutex;
};

/*static*/ EmulationSession EmulationSession::s_instance;

} // Anonymous namespace

static Core::SystemResultStatus RunEmulation(const std::string& filepath) {
    Common::Log::Initialize();
    Common::Log::SetColorConsoleBackendEnabled(true);
    Common::Log::Start();

    MicroProfileOnThreadCreate("EmuThread");
    SCOPE_EXIT({ MicroProfileShutdown(); });

    LOG_INFO(Frontend, "starting");

    if (filepath.empty()) {
        LOG_CRITICAL(Frontend, "failed to load: filepath empty!");
        return Core::SystemResultStatus::ErrorLoader;
    }

    SCOPE_EXIT({ EmulationSession::GetInstance().ShutdownEmulation(); });

    const auto result = EmulationSession::GetInstance().InitializeEmulation(filepath);
    if (result != Core::SystemResultStatus::Success) {
        return result;
    }

    EmulationSession::GetInstance().RunEmulation();

    return Core::SystemResultStatus::Success;
}

extern "C" {

void Java_org_yuzu_yuzu_1emu_NativeLibrary_surfaceChanged(JNIEnv* env,
                                                          [[maybe_unused]] jclass clazz,
                                                          jobject surf) {
    EmulationSession::GetInstance().SetNativeWindow(ANativeWindow_fromSurface(env, surf));
    EmulationSession::GetInstance().SurfaceChanged();
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_surfaceDestroyed(JNIEnv* env,
                                                            [[maybe_unused]] jclass clazz) {
    ANativeWindow_release(EmulationSession::GetInstance().NativeWindow());
    EmulationSession::GetInstance().SetNativeWindow(nullptr);
    EmulationSession::GetInstance().SurfaceChanged();
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_setAppDirectory(JNIEnv* env,
                                                           [[maybe_unused]] jclass clazz,
                                                           jstring j_directory) {
    Common::FS::SetAppDirectory(GetJString(env, j_directory));
}

int Java_org_yuzu_yuzu_1emu_NativeLibrary_installFileToNand(JNIEnv* env,
                                                            [[maybe_unused]] jclass clazz,
                                                            jstring j_file) {
    return EmulationSession::GetInstance().InstallFileToNand(GetJString(env, j_file));
}

void JNICALL Java_org_yuzu_yuzu_1emu_NativeLibrary_initializeGpuDriver(
    JNIEnv* env, [[maybe_unused]] jclass clazz, jstring hook_lib_dir, jstring custom_driver_dir,
    jstring custom_driver_name, jstring file_redirect_dir) {
    EmulationSession::GetInstance().InitializeGpuDriver(
        GetJString(env, hook_lib_dir), GetJString(env, custom_driver_dir),
        GetJString(env, custom_driver_name), GetJString(env, file_redirect_dir));
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_reloadKeys(JNIEnv* env,
                                                          [[maybe_unused]] jclass clazz) {
    Core::Crypto::KeyManager::Instance().ReloadKeys();
    return static_cast<jboolean>(Core::Crypto::KeyManager::Instance().AreKeysLoaded());
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_unPauseEmulation([[maybe_unused]] JNIEnv* env,
                                                            [[maybe_unused]] jclass clazz) {
    EmulationSession::GetInstance().UnPauseEmulation();
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_pauseEmulation([[maybe_unused]] JNIEnv* env,
                                                          [[maybe_unused]] jclass clazz) {
    EmulationSession::GetInstance().PauseEmulation();
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_stopEmulation([[maybe_unused]] JNIEnv* env,
                                                         [[maybe_unused]] jclass clazz) {
    EmulationSession::GetInstance().HaltEmulation();
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_resetRomMetadata([[maybe_unused]] JNIEnv* env,
                                                            [[maybe_unused]] jclass clazz) {
    EmulationSession::GetInstance().ResetRomMetadata();
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_isRunning([[maybe_unused]] JNIEnv* env,
                                                         [[maybe_unused]] jclass clazz) {
    return static_cast<jboolean>(EmulationSession::GetInstance().IsRunning());
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_isHandheldOnly([[maybe_unused]] JNIEnv* env,
                                                              [[maybe_unused]] jclass clazz) {
    return EmulationSession::GetInstance().IsHandheldOnly();
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_setDeviceType([[maybe_unused]] JNIEnv* env,
                                                             [[maybe_unused]] jclass clazz,
                                                             jint j_device, jint j_type) {
    if (EmulationSession::GetInstance().IsRunning()) {
        EmulationSession::GetInstance().SetDeviceType(j_device, j_type);
    }
    return static_cast<jboolean>(true);
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_onGamePadConnectEvent([[maybe_unused]] JNIEnv* env,
                                                                     [[maybe_unused]] jclass clazz,
                                                                     jint j_device) {
    if (EmulationSession::GetInstance().IsRunning()) {
        EmulationSession::GetInstance().OnGamepadConnectEvent(j_device);
    }
    return static_cast<jboolean>(true);
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_onGamePadDisconnectEvent(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint j_device) {
    if (EmulationSession::GetInstance().IsRunning()) {
        EmulationSession::GetInstance().OnGamepadDisconnectEvent(j_device);
    }
    return static_cast<jboolean>(true);
}
jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_onGamePadButtonEvent([[maybe_unused]] JNIEnv* env,
                                                                    [[maybe_unused]] jclass clazz,
                                                                    [[maybe_unused]] jint j_device,
                                                                    jint j_button, jint action) {
    if (EmulationSession::GetInstance().IsRunning()) {
        // Ensure gamepad is connected
        EmulationSession::GetInstance().OnGamepadConnectEvent(j_device);
        EmulationSession::GetInstance().Window().OnGamepadButtonEvent(j_device, j_button,
                                                                      action != 0);
    }
    return static_cast<jboolean>(true);
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_onGamePadJoystickEvent([[maybe_unused]] JNIEnv* env,
                                                                      [[maybe_unused]] jclass clazz,
                                                                      jint j_device, jint stick_id,
                                                                      jfloat x, jfloat y) {
    if (EmulationSession::GetInstance().IsRunning()) {
        EmulationSession::GetInstance().Window().OnGamepadJoystickEvent(j_device, stick_id, x, y);
    }
    return static_cast<jboolean>(true);
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_onGamePadMotionEvent(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jint j_device,
    jlong delta_timestamp, jfloat gyro_x, jfloat gyro_y, jfloat gyro_z, jfloat accel_x,
    jfloat accel_y, jfloat accel_z) {
    if (EmulationSession::GetInstance().IsRunning()) {
        EmulationSession::GetInstance().Window().OnGamepadMotionEvent(
            j_device, delta_timestamp, gyro_x, gyro_y, gyro_z, accel_x, accel_y, accel_z);
    }
    return static_cast<jboolean>(true);
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_onReadNfcTag([[maybe_unused]] JNIEnv* env,
                                                            [[maybe_unused]] jclass clazz,
                                                            jbyteArray j_data) {
    jboolean isCopy{false};
    std::span<u8> data(reinterpret_cast<u8*>(env->GetByteArrayElements(j_data, &isCopy)),
                       static_cast<size_t>(env->GetArrayLength(j_data)));

    if (EmulationSession::GetInstance().IsRunning()) {
        EmulationSession::GetInstance().Window().OnReadNfcTag(data);
    }
    return static_cast<jboolean>(true);
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_onRemoveNfcTag([[maybe_unused]] JNIEnv* env,
                                                              [[maybe_unused]] jclass clazz) {
    if (EmulationSession::GetInstance().IsRunning()) {
        EmulationSession::GetInstance().Window().OnRemoveNfcTag();
    }
    return static_cast<jboolean>(true);
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_onTouchPressed([[maybe_unused]] JNIEnv* env,
                                                          [[maybe_unused]] jclass clazz, jint id,
                                                          jfloat x, jfloat y) {
    if (EmulationSession::GetInstance().IsRunning()) {
        EmulationSession::GetInstance().Window().OnTouchPressed(id, x, y);
    }
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_onTouchMoved([[maybe_unused]] JNIEnv* env,
                                                        [[maybe_unused]] jclass clazz, jint id,
                                                        jfloat x, jfloat y) {
    if (EmulationSession::GetInstance().IsRunning()) {
        EmulationSession::GetInstance().Window().OnTouchMoved(id, x, y);
    }
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_onTouchReleased([[maybe_unused]] JNIEnv* env,
                                                           [[maybe_unused]] jclass clazz, jint id) {
    if (EmulationSession::GetInstance().IsRunning()) {
        EmulationSession::GetInstance().Window().OnTouchReleased(id);
    }
}

jbyteArray Java_org_yuzu_yuzu_1emu_NativeLibrary_getIcon([[maybe_unused]] JNIEnv* env,
                                                         [[maybe_unused]] jclass clazz,
                                                         [[maybe_unused]] jstring j_filename) {
    auto icon_data = EmulationSession::GetInstance().GetRomIcon(GetJString(env, j_filename));
    jbyteArray icon = env->NewByteArray(static_cast<jsize>(icon_data.size()));
    env->SetByteArrayRegion(icon, 0, env->GetArrayLength(icon),
                            reinterpret_cast<jbyte*>(icon_data.data()));
    return icon;
}

jstring Java_org_yuzu_yuzu_1emu_NativeLibrary_getTitle([[maybe_unused]] JNIEnv* env,
                                                       [[maybe_unused]] jclass clazz,
                                                       [[maybe_unused]] jstring j_filename) {
    auto title = EmulationSession::GetInstance().GetRomTitle(GetJString(env, j_filename));
    return env->NewStringUTF(title.c_str());
}

jstring Java_org_yuzu_yuzu_1emu_NativeLibrary_getDescription([[maybe_unused]] JNIEnv* env,
                                                             [[maybe_unused]] jclass clazz,
                                                             jstring j_filename) {
    return j_filename;
}

jstring Java_org_yuzu_yuzu_1emu_NativeLibrary_getGameId([[maybe_unused]] JNIEnv* env,
                                                        [[maybe_unused]] jclass clazz,
                                                        jstring j_filename) {
    return j_filename;
}

jstring Java_org_yuzu_yuzu_1emu_NativeLibrary_getRegions([[maybe_unused]] JNIEnv* env,
                                                         [[maybe_unused]] jclass clazz,
                                                         [[maybe_unused]] jstring j_filename) {
    return env->NewStringUTF("");
}

jstring Java_org_yuzu_yuzu_1emu_NativeLibrary_getCompany([[maybe_unused]] JNIEnv* env,
                                                         [[maybe_unused]] jclass clazz,
                                                         [[maybe_unused]] jstring j_filename) {
    return env->NewStringUTF("");
}

jboolean Java_org_yuzu_yuzu_1emu_NativeLibrary_isHomebrew([[maybe_unused]] JNIEnv* env,
                                                          [[maybe_unused]] jclass clazz,
                                                          [[maybe_unused]] jstring j_filename) {
    return EmulationSession::GetInstance().GetIsHomebrew(GetJString(env, j_filename));
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_initializeEmulation
    [[maybe_unused]] (JNIEnv* env, [[maybe_unused]] jclass clazz) {
    // Create the default config.ini.
    Config{};
    // Initialize the emulated system.
    EmulationSession::GetInstance().System().Initialize();
}

jint Java_org_yuzu_yuzu_1emu_NativeLibrary_defaultCPUCore([[maybe_unused]] JNIEnv* env,
                                                          [[maybe_unused]] jclass clazz) {
    return {};
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_run__Ljava_lang_String_2Ljava_lang_String_2Z(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, [[maybe_unused]] jstring j_file,
    [[maybe_unused]] jstring j_savestate, [[maybe_unused]] jboolean j_delete_savestate) {}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_reloadSettings([[maybe_unused]] JNIEnv* env,
                                                          [[maybe_unused]] jclass clazz) {
    Config{};
}

jstring Java_org_yuzu_yuzu_1emu_NativeLibrary_getUserSetting([[maybe_unused]] JNIEnv* env,
                                                             [[maybe_unused]] jclass clazz,
                                                             jstring j_game_id, jstring j_section,
                                                             jstring j_key) {
    std::string_view game_id = env->GetStringUTFChars(j_game_id, 0);
    std::string_view section = env->GetStringUTFChars(j_section, 0);
    std::string_view key = env->GetStringUTFChars(j_key, 0);

    env->ReleaseStringUTFChars(j_game_id, game_id.data());
    env->ReleaseStringUTFChars(j_section, section.data());
    env->ReleaseStringUTFChars(j_key, key.data());

    return env->NewStringUTF("");
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_setUserSetting([[maybe_unused]] JNIEnv* env,
                                                          [[maybe_unused]] jclass clazz,
                                                          jstring j_game_id, jstring j_section,
                                                          jstring j_key, jstring j_value) {
    std::string_view game_id = env->GetStringUTFChars(j_game_id, 0);
    std::string_view section = env->GetStringUTFChars(j_section, 0);
    std::string_view key = env->GetStringUTFChars(j_key, 0);
    std::string_view value = env->GetStringUTFChars(j_value, 0);

    env->ReleaseStringUTFChars(j_game_id, game_id.data());
    env->ReleaseStringUTFChars(j_section, section.data());
    env->ReleaseStringUTFChars(j_key, key.data());
    env->ReleaseStringUTFChars(j_value, value.data());
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_initGameIni([[maybe_unused]] JNIEnv* env,
                                                       [[maybe_unused]] jclass clazz,
                                                       jstring j_game_id) {
    std::string_view game_id = env->GetStringUTFChars(j_game_id, 0);

    env->ReleaseStringUTFChars(j_game_id, game_id.data());
}

jdoubleArray Java_org_yuzu_yuzu_1emu_NativeLibrary_getPerfStats([[maybe_unused]] JNIEnv* env,
                                                                [[maybe_unused]] jclass clazz) {
    jdoubleArray j_stats = env->NewDoubleArray(4);

    if (EmulationSession::GetInstance().IsRunning()) {
        const auto results = EmulationSession::GetInstance().PerfStats();

        // Converting the structure into an array makes it easier to pass it to the frontend
        double stats[4] = {results.system_fps, results.average_game_fps, results.frametime,
                           results.emulation_speed};

        env->SetDoubleArrayRegion(j_stats, 0, 4, stats);
    }

    return j_stats;
}

void Java_org_yuzu_yuzu_1emu_utils_DirectoryInitialization_setSysDirectory(
    [[maybe_unused]] JNIEnv* env, [[maybe_unused]] jclass clazz, jstring j_path) {}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_run__Ljava_lang_String_2([[maybe_unused]] JNIEnv* env,
                                                                    [[maybe_unused]] jclass clazz,
                                                                    jstring j_path) {
    const std::string path = GetJString(env, j_path);

    const Core::SystemResultStatus result{RunEmulation(path)};
    if (result != Core::SystemResultStatus::Success) {
        env->CallStaticVoidMethod(IDCache::GetNativeLibraryClass(),
                                  IDCache::GetExitEmulationActivity(), static_cast<int>(result));
    }
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_logDeviceInfo([[maybe_unused]] JNIEnv* env,
                                                         [[maybe_unused]] jclass clazz) {
    LOG_INFO(Frontend, "yuzu Version: {}-{}", Common::g_scm_branch, Common::g_scm_desc);
    LOG_INFO(Frontend, "Host OS: Android API level {}", android_get_device_api_level());
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_submitInlineKeyboardText(JNIEnv* env, jclass clazz,
                                                                    jstring j_text) {
    const std::u16string input = Common::UTF8ToUTF16(GetJString(env, j_text));
    EmulationSession::GetInstance().SoftwareKeyboard()->SubmitInlineKeyboardText(input);
}

void Java_org_yuzu_yuzu_1emu_NativeLibrary_submitInlineKeyboardInput(JNIEnv* env, jclass clazz,
                                                                     jint j_key_code) {
    EmulationSession::GetInstance().SoftwareKeyboard()->SubmitInlineKeyboardInput(j_key_code);
}

} // extern "C"

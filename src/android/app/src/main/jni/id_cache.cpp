// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <jni.h>

#include "common/assert.h"
#include "common/fs/fs_android.h"
#include "jni/applets/software_keyboard.h"
#include "jni/id_cache.h"
#include "video_core/rasterizer_interface.h"

static JavaVM* s_java_vm;
static jclass s_native_library_class;
static jclass s_disk_cache_progress_class;
static jclass s_load_callback_stage_class;
static jclass s_game_dir_class;
static jmethodID s_game_dir_constructor;
static jmethodID s_exit_emulation_activity;
static jmethodID s_disk_cache_load_progress;
static jmethodID s_on_emulation_started;
static jmethodID s_on_emulation_stopped;

static jclass s_game_class;
static jmethodID s_game_constructor;
static jfieldID s_game_title_field;
static jfieldID s_game_path_field;
static jfieldID s_game_program_id_field;
static jfieldID s_game_developer_field;
static jfieldID s_game_version_field;
static jfieldID s_game_is_homebrew_field;

static jclass s_string_class;
static jclass s_pair_class;
static jmethodID s_pair_constructor;
static jfieldID s_pair_first_field;
static jfieldID s_pair_second_field;

static constexpr jint JNI_VERSION = JNI_VERSION_1_6;

namespace IDCache {

JNIEnv* GetEnvForThread() {
    thread_local static struct OwnedEnv {
        OwnedEnv() {
            status = s_java_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
            if (status == JNI_EDETACHED)
                s_java_vm->AttachCurrentThread(&env, nullptr);
        }

        ~OwnedEnv() {
            if (status == JNI_EDETACHED)
                s_java_vm->DetachCurrentThread();
        }

        int status;
        JNIEnv* env = nullptr;
    } owned;
    return owned.env;
}

jclass GetNativeLibraryClass() {
    return s_native_library_class;
}

jclass GetDiskCacheProgressClass() {
    return s_disk_cache_progress_class;
}

jclass GetDiskCacheLoadCallbackStageClass() {
    return s_load_callback_stage_class;
}

jclass GetGameDirClass() {
    return s_game_dir_class;
}

jmethodID GetGameDirConstructor() {
    return s_game_dir_constructor;
}

jmethodID GetExitEmulationActivity() {
    return s_exit_emulation_activity;
}

jmethodID GetDiskCacheLoadProgress() {
    return s_disk_cache_load_progress;
}

jmethodID GetOnEmulationStarted() {
    return s_on_emulation_started;
}

jmethodID GetOnEmulationStopped() {
    return s_on_emulation_stopped;
}

jclass GetGameClass() {
    return s_game_class;
}

jmethodID GetGameConstructor() {
    return s_game_constructor;
}

jfieldID GetGameTitleField() {
    return s_game_title_field;
}

jfieldID GetGamePathField() {
    return s_game_path_field;
}

jfieldID GetGameProgramIdField() {
    return s_game_program_id_field;
}

jfieldID GetGameDeveloperField() {
    return s_game_developer_field;
}

jfieldID GetGameVersionField() {
    return s_game_version_field;
}

jfieldID GetGameIsHomebrewField() {
    return s_game_is_homebrew_field;
}

jclass GetStringClass() {
    return s_string_class;
}

jclass GetPairClass() {
    return s_pair_class;
}

jmethodID GetPairConstructor() {
    return s_pair_constructor;
}

jfieldID GetPairFirstField() {
    return s_pair_first_field;
}

jfieldID GetPairSecondField() {
    return s_pair_second_field;
}

} // namespace IDCache

#ifdef __cplusplus
extern "C" {
#endif

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    s_java_vm = vm;

    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION) != JNI_OK)
        return JNI_ERR;

    // Initialize Java classes
    const jclass native_library_class = env->FindClass("org/yuzu/yuzu_emu/NativeLibrary");
    s_native_library_class = reinterpret_cast<jclass>(env->NewGlobalRef(native_library_class));
    s_disk_cache_progress_class = reinterpret_cast<jclass>(env->NewGlobalRef(
        env->FindClass("org/yuzu/yuzu_emu/disk_shader_cache/DiskShaderCacheProgress")));
    s_load_callback_stage_class = reinterpret_cast<jclass>(env->NewGlobalRef(env->FindClass(
        "org/yuzu/yuzu_emu/disk_shader_cache/DiskShaderCacheProgress$LoadCallbackStage")));

    const jclass game_dir_class = env->FindClass("org/yuzu/yuzu_emu/model/GameDir");
    s_game_dir_class = reinterpret_cast<jclass>(env->NewGlobalRef(game_dir_class));
    s_game_dir_constructor = env->GetMethodID(game_dir_class, "<init>", "(Ljava/lang/String;Z)V");
    env->DeleteLocalRef(game_dir_class);

    // Initialize methods
    s_exit_emulation_activity =
        env->GetStaticMethodID(s_native_library_class, "exitEmulationActivity", "(I)V");
    s_disk_cache_load_progress =
        env->GetStaticMethodID(s_disk_cache_progress_class, "loadProgress", "(III)V");
    s_on_emulation_started =
        env->GetStaticMethodID(s_native_library_class, "onEmulationStarted", "()V");
    s_on_emulation_stopped =
        env->GetStaticMethodID(s_native_library_class, "onEmulationStopped", "(I)V");

    const jclass game_class = env->FindClass("org/yuzu/yuzu_emu/model/Game");
    s_game_class = reinterpret_cast<jclass>(env->NewGlobalRef(game_class));
    s_game_constructor = env->GetMethodID(game_class, "<init>",
                                          "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/"
                                          "String;Ljava/lang/String;Ljava/lang/String;Z)V");
    s_game_title_field = env->GetFieldID(game_class, "title", "Ljava/lang/String;");
    s_game_path_field = env->GetFieldID(game_class, "path", "Ljava/lang/String;");
    s_game_program_id_field = env->GetFieldID(game_class, "programId", "Ljava/lang/String;");
    s_game_developer_field = env->GetFieldID(game_class, "developer", "Ljava/lang/String;");
    s_game_version_field = env->GetFieldID(game_class, "version", "Ljava/lang/String;");
    s_game_is_homebrew_field = env->GetFieldID(game_class, "isHomebrew", "Z");
    env->DeleteLocalRef(game_class);

    const jclass string_class = env->FindClass("java/lang/String");
    s_string_class = reinterpret_cast<jclass>(env->NewGlobalRef(string_class));
    env->DeleteLocalRef(string_class);

    const jclass pair_class = env->FindClass("kotlin/Pair");
    s_pair_class = reinterpret_cast<jclass>(env->NewGlobalRef(pair_class));
    s_pair_constructor =
        env->GetMethodID(pair_class, "<init>", "(Ljava/lang/Object;Ljava/lang/Object;)V");
    s_pair_first_field = env->GetFieldID(pair_class, "first", "Ljava/lang/Object;");
    s_pair_second_field = env->GetFieldID(pair_class, "second", "Ljava/lang/Object;");
    env->DeleteLocalRef(pair_class);

    // Initialize Android Storage
    Common::FS::Android::RegisterCallbacks(env, s_native_library_class);

    // Initialize applets
    SoftwareKeyboard::InitJNI(env);

    return JNI_VERSION;
}

void JNI_OnUnload(JavaVM* vm, void* reserved) {
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION) != JNI_OK) {
        return;
    }

    // UnInitialize Android Storage
    Common::FS::Android::UnRegisterCallbacks();
    env->DeleteGlobalRef(s_native_library_class);
    env->DeleteGlobalRef(s_disk_cache_progress_class);
    env->DeleteGlobalRef(s_load_callback_stage_class);
    env->DeleteGlobalRef(s_game_dir_class);
    env->DeleteGlobalRef(s_game_class);
    env->DeleteGlobalRef(s_string_class);
    env->DeleteGlobalRef(s_pair_class);

    // UnInitialize applets
    SoftwareKeyboard::CleanupJNI(env);
}

#ifdef __cplusplus
}
#endif

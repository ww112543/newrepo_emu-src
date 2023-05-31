// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>
#include <jni.h>

#define ANDROID_STORAGE_FUNCTIONS(V)                                                               \
    V(OpenContentUri, int, (const std::string& filepath, OpenMode openmode), open_content_uri,     \
      "openContentUri", "(Ljava/lang/String;Ljava/lang/String;)I")

#define ANDROID_SINGLE_PATH_DETERMINE_FUNCTIONS(V)                                                 \
    V(GetSize, std::uint64_t, get_size, CallStaticLongMethod, "getSize", "(Ljava/lang/String;)J")

namespace Common::FS::Android {

static JavaVM* g_jvm = nullptr;
static jclass native_library = nullptr;

#define FR(FunctionName, ReturnValue, JMethodID, Caller, JMethodName, Signature) F(JMethodID)
#define FS(FunctionName, ReturnValue, Parameters, JMethodID, JMethodName, Signature) F(JMethodID)
#define F(JMethodID) static jmethodID JMethodID = nullptr;
ANDROID_SINGLE_PATH_DETERMINE_FUNCTIONS(FR)
ANDROID_STORAGE_FUNCTIONS(FS)
#undef F
#undef FS
#undef FR

enum class OpenMode {
    Read,
    Write,
    ReadWrite,
    WriteAppend,
    WriteTruncate,
    ReadWriteAppend,
    ReadWriteTruncate,
    Never
};

void RegisterCallbacks(JNIEnv* env, jclass clazz);

void UnRegisterCallbacks();

bool IsContentUri(const std::string& path);

#define FS(FunctionName, ReturnValue, Parameters, JMethodID, JMethodName, Signature)               \
    F(FunctionName, Parameters, ReturnValue)
#define F(FunctionName, Parameters, ReturnValue) ReturnValue FunctionName Parameters;
ANDROID_STORAGE_FUNCTIONS(FS)
#undef F
#undef FS

#define FR(FunctionName, ReturnValue, JMethodID, Caller, JMethodName, Signature)                   \
    F(FunctionName, ReturnValue)
#define F(FunctionName, ReturnValue) ReturnValue FunctionName(const std::string& filepath);
ANDROID_SINGLE_PATH_DETERMINE_FUNCTIONS(FR)
#undef F
#undef FR

} // namespace Common::FS::Android

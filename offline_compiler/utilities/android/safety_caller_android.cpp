/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "offline_compiler/offline_compiler.h"
#include "offline_compiler/utilities/android/safety_guard_android.h"
#include "runtime/os_interface/os_library.h"

using namespace NEO;

int buildWithSafetyGuard(OfflineCompiler *compiler) {
    SafetyGuardAndroid safetyGuard;
    int retVal = 0;

    return safetyGuard.call<int, OfflineCompiler, decltype(&OfflineCompiler::build)>(compiler, &OfflineCompiler::build, retVal);
}

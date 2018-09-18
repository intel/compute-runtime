/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/os_library.h"
#include "offline_compiler/offline_compiler.h"
#include "offline_compiler/utilities/linux/safety_guard_linux.h"

using namespace OCLRT;

int buildWithSafetyGuard(OfflineCompiler *compiler) {
    SafetyGuardLinux safetyGuard;
    int retVal = 0;

    return safetyGuard.call<int, OfflineCompiler, decltype(&OfflineCompiler::build)>(compiler, &OfflineCompiler::build, retVal);
}

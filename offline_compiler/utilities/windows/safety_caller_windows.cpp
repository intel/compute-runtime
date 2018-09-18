/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "offline_compiler/offline_compiler.h"
#include "offline_compiler/utilities/windows/safety_guard_windows.h"

using namespace OCLRT;

int buildWithSafetyGuard(OfflineCompiler *compiler) {
    SafetyGuardWindows safetyGuard;
    int retVal = 0;
    return safetyGuard.call<int, OfflineCompiler, decltype(&OfflineCompiler::build)>(compiler, &OfflineCompiler::build, retVal);
}

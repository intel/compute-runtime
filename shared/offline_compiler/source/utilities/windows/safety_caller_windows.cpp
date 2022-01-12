/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/offline_compiler.h"
#include "shared/offline_compiler/source/offline_linker.h"
#include "shared/offline_compiler/source/utilities/windows/safety_guard_windows.h"

using namespace NEO;

int buildWithSafetyGuard(OfflineCompiler *compiler) {
    SafetyGuardWindows safetyGuard;
    int retVal = 0;
    return safetyGuard.call<int, OfflineCompiler, decltype(&OfflineCompiler::build)>(compiler, &OfflineCompiler::build, retVal);
}

int linkWithSafetyGuard(OfflineLinker *linker) {
    SafetyGuardWindows safetyGuard{};
    int returnValueOnCrash{-1};

    return safetyGuard.call(linker, &OfflineLinker::execute, returnValueOnCrash);
}

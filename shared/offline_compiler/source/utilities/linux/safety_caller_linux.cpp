/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_error_code.h"
#include "shared/offline_compiler/source/offline_compiler.h"
#include "shared/offline_compiler/source/offline_linker.h"
#include "shared/offline_compiler/source/utilities/linux/safety_guard_linux.h"
#include "shared/source/os_interface/os_library.h"

using namespace NEO;

int buildWithSafetyGuard(OfflineCompiler *compiler) {
    SafetyGuardLinux safetyGuard;
    int retVal = NEO::OclocErrorCode::COMPILATION_CRASH;

    return safetyGuard.call<int, OfflineCompiler, decltype(&OfflineCompiler::build)>(compiler, &OfflineCompiler::build, retVal);
}

int linkWithSafetyGuard(OfflineLinker *linker) {
    SafetyGuardLinux safetyGuard{};
    int returnValueOnCrash{NEO::OclocErrorCode::COMPILATION_CRASH};

    return safetyGuard.call(linker, &OfflineLinker::execute, returnValueOnCrash);
}

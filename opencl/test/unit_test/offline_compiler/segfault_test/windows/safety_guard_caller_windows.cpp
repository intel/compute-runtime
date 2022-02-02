/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/utilities/windows/safety_guard_windows.h"

#include "opencl/test/unit_test/offline_compiler/segfault_test/segfault_helper.h"

int generateSegfaultWithSafetyGuard(SegfaultHelper *segfaultHelper) {
    SafetyGuardWindows safetyGuard;
    safetyGuard.onExcept = segfaultHelper->segfaultHandlerCallback;
    int retVal = -60;

    return safetyGuard.call<int, SegfaultHelper, decltype(&SegfaultHelper::generateSegfault)>(segfaultHelper, &SegfaultHelper::generateSegfault, retVal);
}

/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "offline_compiler/utilities/windows/safety_guard_windows.h"
#include "opencl/test/unit_test/offline_compiler/segfault_test/segfault_helper.h"

void generateSegfaultWithSafetyGuard(SegfaultHelper *segfaultHelper) {
    SafetyGuardWindows safetyGuard;
    safetyGuard.onExcept = segfaultHelper->segfaultHandlerCallback;
    int retVal = 0;

    safetyGuard.call<int, SegfaultHelper, decltype(&SegfaultHelper::generateSegfault)>(segfaultHelper, &SegfaultHelper::generateSegfault, retVal);
}
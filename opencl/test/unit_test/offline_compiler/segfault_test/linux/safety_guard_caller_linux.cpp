/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "offline_compiler/utilities/linux/safety_guard_linux.h"

#include "../segfault_helper.h"

void generateSegfaultWithSafetyGuard(SegfaultHelper *segfaultHelper) {
    SafetyGuardLinux safetyGuard;
    safetyGuard.onSigSegv = segfaultHelper->segfaultHandlerCallback;
    int retVal = 0;

    safetyGuard.call<int, SegfaultHelper, decltype(&SegfaultHelper::generateSegfault)>(segfaultHelper, &SegfaultHelper::generateSegfault, retVal);
}

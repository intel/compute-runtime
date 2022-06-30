/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/utilities/linux/safety_guard_linux.h"

#include "../segfault_helper.h"

int generateSegfaultWithSafetyGuard(SegfaultHelper *segfaultHelper) {
    SafetyGuardLinux safetyGuard;
    safetyGuard.onSigSegv = segfaultHelper->segfaultHandlerCallback;
    int retVal = -60;

    return safetyGuard.call<int, SegfaultHelper, decltype(&SegfaultHelper::generateSegfault)>(segfaultHelper, &SegfaultHelper::generateSegfault, retVal);
}

/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/wait_util.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

namespace NEO {

namespace WaitUtils {

uint32_t waitCount = defaultWaitCount;

void init() {
    int32_t overrideWaitCount = debugManager.flags.WaitLoopCount.get();
    if (overrideWaitCount != -1) {
        waitCount = static_cast<uint32_t>(overrideWaitCount);
    }
}

} // namespace WaitUtils

} // namespace NEO

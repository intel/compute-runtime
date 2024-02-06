/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/wait_util.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/utilities/cpu_info.h"

namespace NEO {

namespace WaitUtils {

constexpr uint64_t defaultCounterValue = 10000;
constexpr uint32_t defaultControlValue = 0;
constexpr bool defaultEnableWaitPkg = false;

uint64_t waitpkgCounterValue = defaultCounterValue;
uint32_t waitpkgControlValue = defaultControlValue;

uint32_t waitCount = defaultWaitCount;

#ifdef SUPPORTS_WAITPKG
bool waitpkgSupport = SUPPORTS_WAITPKG;
#else
bool waitpkgSupport = false;
#endif
bool waitpkgUse = false;

void init() {
    bool enableWaitPkg = defaultEnableWaitPkg;
    int32_t overrideEnableWaitPkg = debugManager.flags.EnableWaitpkg.get();
    if (overrideEnableWaitPkg != -1) {
        enableWaitPkg = !!(overrideEnableWaitPkg);
    }
    if (enableWaitPkg && waitpkgSupport) {
        if (CpuInfo::getInstance().isFeatureSupported(CpuInfo::featureWaitPkg)) {
            waitpkgUse = true;
            waitCount = 0;
        }
    }

    int64_t overrideWaitPkgCounter = debugManager.flags.WaitpkgCounterValue.get();
    if (overrideWaitPkgCounter != -1) {
        waitpkgCounterValue = static_cast<uint64_t>(overrideWaitPkgCounter);
    }
    int32_t overrideWaitPkgControl = debugManager.flags.WaitpkgControlValue.get();
    if (overrideWaitPkgControl != -1) {
        waitpkgControlValue = static_cast<uint32_t>(overrideWaitPkgControl);
    }

    int32_t overrideWaitCount = debugManager.flags.WaitLoopCount.get();
    if (overrideWaitCount != -1) {
        waitCount = static_cast<uint32_t>(overrideWaitCount);
    }
}

} // namespace WaitUtils

} // namespace NEO

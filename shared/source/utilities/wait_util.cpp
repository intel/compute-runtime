/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/wait_util.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/utilities/cpu_info.h"

namespace NEO {

namespace WaitUtils {

WaitpkgUse waitpkgUse = WaitpkgUse::uninitialized;

int64_t waitPkgThresholdInMicroSeconds = defaultWaitPkgThresholdInMicroSeconds;
uint64_t waitpkgCounterValue = defaultCounterValue;
uint32_t waitpkgControlValue = defaultControlValue;
uint32_t waitCount = defaultWaitCount;

#ifdef SUPPORTS_WAITPKG
bool waitpkgSupport = SUPPORTS_WAITPKG;
#else
bool waitpkgSupport = false;
#endif

void init(WaitpkgUse inputWaitpkgUse, const HardwareInfo &hwInfo) {
    if (debugManager.flags.WaitLoopCount.get() != -1) {
        waitCount = debugManager.flags.WaitLoopCount.get();
    }

    if (waitpkgUse > WaitpkgUse::noUse) {
        return;
    }

    if (!(waitpkgSupport && CpuInfo::getInstance().isFeatureSupported(CpuInfo::featureWaitPkg))) {
        waitpkgUse = WaitpkgUse::noUse;
        return;
    }

    if (debugManager.flags.EnableWaitpkg.get() != -1) {
        inputWaitpkgUse = static_cast<WaitpkgUse>(debugManager.flags.EnableWaitpkg.get());
    }

    waitpkgUse = inputWaitpkgUse;

    if (!hwInfo.capabilityTable.isIntegratedDevice) {
        waitPkgThresholdInMicroSeconds = WaitUtils::defaultWaitPkgThresholdForDiscreteInMicroSeconds;
    }

    if (waitpkgUse == WaitpkgUse::umonitorAndUmwait) {
        waitCount = 0u;
    }

    overrideWaitpkgParams();
}

void overrideWaitpkgParams() {
    if (debugManager.flags.WaitpkgCounterValue.get() != -1) {
        waitpkgCounterValue = debugManager.flags.WaitpkgCounterValue.get();
    }

    if (debugManager.flags.WaitpkgControlValue.get() != -1) {
        waitpkgControlValue = debugManager.flags.WaitpkgControlValue.get();
    }

    if (debugManager.flags.WaitpkgThreshold.get() != -1) {
        waitPkgThresholdInMicroSeconds = debugManager.flags.WaitpkgThreshold.get();
    }
}

void adjustWaitpkgParamsForUllsLight() {
    waitPkgThresholdInMicroSeconds = defaultWaitPkgThresholdForUllsLightInMicroSeconds;
    waitpkgCounterValue = defaultCounterValueForUllsLight;
    overrideWaitpkgParams();
}

} // namespace WaitUtils

} // namespace NEO

/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"

#include "neo_igfxfmid.h"

namespace NEO {
void UnitTestSetter::disableHeapless([[maybe_unused]] const DebugManagerStateRestore &restorer) {
    debugManager.flags.Enable64BitAddressing.set(0);
};

void UnitTestSetter::disableHeaplessStateInit([[maybe_unused]] const DebugManagerStateRestore &restorer) {
    debugManager.flags.Enable64bAddressingStateInit.set(0);
};

void UnitTestSetter::setCcsExposure(RootDeviceEnvironment &rootDeviceEnvironment) {
    auto hwInfo = rootDeviceEnvironment.getMutableHardwareInfo();

    if (hwInfo && (hwInfo->platform.eRenderCoreFamily >= IGFX_XE3P_CORE)) {
        uint32_t numCcs = hwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled;
        hwInfo->featureTable.flags.ftrCCSNode = numCcs > 0;
    }
}

void UnitTestSetter::setRcsExposure(RootDeviceEnvironment &rootDeviceEnvironment) {
    auto hwInfo = rootDeviceEnvironment.getMutableHardwareInfo();

    if (hwInfo && (hwInfo->platform.eRenderCoreFamily >= IGFX_XE3P_CORE)) {
        rootDeviceEnvironment.setRcsExposure();
    }
}

void UnitTestSetter::setupSemaphore64bCmdSupport([[maybe_unused]] const DebugManagerStateRestore &restorer, uint32_t gfxCoreFamily) {
    // for Xe3p force 64b semaphore cmd to have consistency between platforms in tests,
    // as default semaphore cmd is set to 64b for Xe3p gen but some platforms may have it disabled (see isAvailableSemaphore64 method)
    if (gfxCoreFamily == IGFX_XE3P_CORE) {
        debugManager.flags.Enable64BitSemaphore.set(1);
    }
}

} // namespace NEO

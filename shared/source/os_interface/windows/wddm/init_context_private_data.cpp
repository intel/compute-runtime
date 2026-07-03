/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/release_helper/release_helper.h"

namespace NEO {

CREATECONTEXT_PVTDATA initPrivateData(OsContextWin &osContext) {
    auto &rootDeviceEnvironment = osContext.getWddm()->getRootDeviceEnvironment();
    CREATECONTEXT_PVTDATA privateData = {};
    privateData.IsProtectedProcess = FALSE;
    privateData.IsDwm = FALSE;
    privateData.GpuVAContext = TRUE;
    privateData.IsMediaUsage = FALSE;
    privateData.UmdContextType = UMD_OCL;
    privateData.UseHw64bToken = debugManager.flags.WddmUseHw64bToken.get() &&
                                rootDeviceEnvironment.getReleaseHelper().isAvailableSemaphore64(*rootDeviceEnvironment.getHardwareInfo());
    if (osContext.checkLatePreemptionStartSupport()) {
        osContext.prepareLatePreemptionStart(privateData);
    }

    privateData.PowerHint.Data = 0;
    if (osContext.getUmdPowerHintValue() > 0) {
        privateData.PowerHint.IsValid = 1;
        privateData.PowerHint.Value = osContext.getUmdPowerHintValue();
    }
    if (osContext.isDebuggableContext()) {
        privateData.DebugSIPInstalled = TRUE;
    }

    if (osContext.isPartOfContextGroup()) {
        privateData.NumHwQueues = osContext.getContextGroupCount();
    }

    if (debugManager.flags.OverrideWddmContextPowerHint.get() != -1) {
        privateData.PowerHint.IsValid = 1;
        privateData.PowerHint.Value = static_cast<uint8_t>(debugManager.flags.OverrideWddmContextPowerHint.get());
    }

    return privateData;
}

CREATEHWQUEUE_PVTDATA initHwQueuePrivateData(OsContextWin &osContext) {
    CREATEHWQUEUE_PVTDATA privateData = {};
    QUEUE_PRIORITY priority = QUEUE_PRIORITY::XE3P_QUEUE_PRIORITY_NORMAL;

    switch (osContext.getEngineUsage()) {
    case EngineUsage::lowPriority:
        priority = QUEUE_PRIORITY::XE3P_QUEUE_PRIORITY_LOW;
        break;
    case EngineUsage::highPriority:
        priority = QUEUE_PRIORITY::XE3P_QUEUE_PRIORITY_HIGH;
        break;
    default:
        break;
    }
    privateData.QueuePriority = priority;

    return privateData;
}

} // namespace NEO

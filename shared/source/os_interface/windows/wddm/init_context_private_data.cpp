/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {

CREATECONTEXT_PVTDATA initPrivateData(OsContextWin &osContext) {
    CREATECONTEXT_PVTDATA privateData = {};
    privateData.IsProtectedProcess = FALSE;
    privateData.IsDwm = FALSE;
    privateData.GpuVAContext = TRUE;
    privateData.IsMediaUsage = FALSE;
    privateData.UmdContextType = UMD_OCL;
    if (osContext.checkLatePreemptionStartSupport()) {
        osContext.prepareLatePreemptionStart(privateData);
    }

    return privateData;
}

CREATEHWQUEUE_PVTDATA initHwQueuePrivateData(OsContextWin &osContext) {
    CREATEHWQUEUE_PVTDATA privateData = {};
    return privateData;
}

} // namespace NEO

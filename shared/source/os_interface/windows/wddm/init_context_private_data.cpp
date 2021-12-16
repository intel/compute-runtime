/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {

CREATECONTEXT_PVTDATA initPrivateData(OsContextWin &osContext) {
    CREATECONTEXT_PVTDATA PrivateData = {};
    PrivateData.IsProtectedProcess = FALSE;
    PrivateData.IsDwm = FALSE;
    PrivateData.GpuVAContext = TRUE;
    PrivateData.IsMediaUsage = false;

    return PrivateData;
}

} // namespace NEO
/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/wddm/wddm_interface.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

namespace NEO {
class WddmMockInterface : public WddmInterface {
  public:
    using WddmInterface::WddmInterface;
    ADDMETHOD_NOBASE(createHwQueue, bool, true, (OsContextWin & osContext));
    ADDMETHOD_NOBASE_VOIDRETURN(destroyHwQueue, (D3DKMT_HANDLE hwQueue));
    ADDMETHOD_NOBASE(createMonitoredFence, bool, true, (OsContextWin & osContext));
    ADDMETHOD_NOBASE_VOIDRETURN(destroyMonitorFence, (MonitoredFence & monitorFence));
    ADDMETHOD_NOBASE(hwQueuesSupported, bool, false, ());
    ADDMETHOD_NOBASE(submit, bool, true, (uint64_t commandBuffer, size_t size, void *commandHeader, WddmSubmitArguments &submitArguments));
    ADDMETHOD_NOBASE(createFenceForDirectSubmission, bool, true, (MonitoredFence & monitorFence, OsContextWin &osContext));
};
} // namespace NEO

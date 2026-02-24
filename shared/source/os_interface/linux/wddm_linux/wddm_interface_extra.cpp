/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm/wddm_interface.h"

using namespace NEO;

bool WddmInterface32::createSyncObject(MonitoredFence &monitorFence) {
    UNRECOVERABLE_IF(true);
    return false;
}

bool WddmInterface32::createNativeFence(MonitoredFence &monitorFence, bool useForWalkerInterrupt) {
    return false;
}

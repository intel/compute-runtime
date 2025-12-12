/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/unified_memory/usm_memory_support.h"
#include "shared/source/xe3p_core/hw_cmds_cri.h"

namespace NEO {

void CRI::adjustHardwareInfo(HardwareInfo *hwInfo) {
    hwInfo->capabilityTable.sharedSystemMemCapabilities = (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess);
}

} // namespace NEO

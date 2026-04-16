/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/unified_memory/usm_memory_support.h"
#include "shared/source/xe3p_core/hw_cmds_nvlp.h"

namespace NEO {

void NVLP::adjustHardwareInfo(HardwareInfo *hwInfo) {
    hwInfo->capabilityTable.sharedSystemMemCapabilities = (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess);
}

} // namespace NEO

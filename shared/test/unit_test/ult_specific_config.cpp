/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/memory_manager/compression_selector.h"
#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"

namespace NEO {
namespace ImplicitScaling {
bool apiSupport = false;
bool semaphoreProgrammingRequired = false;
bool crossTileAtomicSynchronization = false;
} // namespace ImplicitScaling
bool CompressionSelector::preferRenderCompressedBuffer(const AllocationProperties &properties, const HardwareInfo &hwInfo) {
    return false;
}
void PageFaultManager::transferToCpu(void *ptr, size_t size, void *cmdQ) {
}
void PageFaultManager::transferToGpu(void *ptr, void *cmdQ) {
}
} // namespace NEO

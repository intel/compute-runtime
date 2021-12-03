/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/compiler_interface/default_cache_config.h"
#include "shared/source/memory_manager/compression_selector.h"
#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"

namespace NEO {
namespace ImplicitScaling {
bool apiSupport = false;
} // namespace ImplicitScaling
bool CompressionSelector::preferCompressedAllocation(const AllocationProperties &properties, const HardwareInfo &hwInfo) {
    return false;
}
void PageFaultManager::transferToCpu(void *ptr, size_t size, void *cmdQ) {
}
void PageFaultManager::transferToGpu(void *ptr, void *cmdQ) {
}
CompilerCacheConfig getDefaultCompilerCacheConfig() { return {}; }
const char *getAdditionalBuiltinAsString(EBuiltInOps::Type builtin) { return nullptr; }
} // namespace NEO

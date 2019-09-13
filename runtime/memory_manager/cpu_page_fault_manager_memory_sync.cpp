/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/debug_helpers.h"
#include "core/page_fault_manager/cpu_page_fault_manager.h"
#include "runtime/command_queue/command_queue.h"
#include "runtime/memory_manager/unified_memory_manager.h"

namespace NEO {
void PageFaultManager::transferToCpu(void *ptr, size_t size, void *cmdQ) {
    auto commandQueue = static_cast<CommandQueue *>(cmdQ);
    auto retVal = commandQueue->enqueueSVMMap(true, CL_MAP_WRITE, ptr, size, 0, nullptr, nullptr, false);
    UNRECOVERABLE_IF(retVal);
}
void PageFaultManager::transferToGpu(void *ptr, void *cmdQ) {
    auto commandQueue = static_cast<CommandQueue *>(cmdQ);
    auto retVal = commandQueue->enqueueSVMUnmap(ptr, 0, nullptr, nullptr, false);
    UNRECOVERABLE_IF(retVal);
    retVal = commandQueue->finish();
    UNRECOVERABLE_IF(retVal);
}
} // namespace NEO

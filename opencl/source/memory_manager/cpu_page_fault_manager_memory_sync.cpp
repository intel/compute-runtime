/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"

#include "opencl/source/command_queue/command_queue.h"

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

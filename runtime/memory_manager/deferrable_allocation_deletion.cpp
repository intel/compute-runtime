/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/memory_manager/deferrable_allocation_deletion.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/os_context.h"

namespace OCLRT {

DeferrableAllocationDeletion::DeferrableAllocationDeletion(MemoryManager &memoryManager, GraphicsAllocation &graphicsAllocation) : memoryManager(memoryManager),
                                                                                                                                   graphicsAllocation(graphicsAllocation) {}
bool DeferrableAllocationDeletion::apply() {
    if (graphicsAllocation.isUsed()) {

        for (auto &deviceCsrs : memoryManager.getCommandStreamReceivers()) {
            for (auto &csr : deviceCsrs) {
                auto contextId = csr->getOsContext().getContextId();
                if (graphicsAllocation.isUsedByOsContext(contextId)) {
                    auto currentContextTaskCount = *csr->getTagAddress();
                    if (graphicsAllocation.getTaskCount(contextId) <= currentContextTaskCount) {
                        graphicsAllocation.releaseUsageInOsContext(contextId);
                    } else {
                        csr->flushBatchedSubmissions();
                    }
                }
            }
        }
        if (graphicsAllocation.isUsed()) {
            return false;
        }
    }
    memoryManager.freeGraphicsMemory(&graphicsAllocation);
    return true;
}
} // namespace OCLRT

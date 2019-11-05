/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/deferrable_allocation_deletion.h"

#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/helpers/engine_control.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/os_context.h"

namespace NEO {

DeferrableAllocationDeletion::DeferrableAllocationDeletion(MemoryManager &memoryManager, GraphicsAllocation &graphicsAllocation) : memoryManager(memoryManager),
                                                                                                                                   graphicsAllocation(graphicsAllocation) {}
bool DeferrableAllocationDeletion::apply() {
    if (graphicsAllocation.isUsed()) {
        bool isStillUsed = false;
        for (auto &engine : memoryManager.getRegisteredEngines()) {
            auto contextId = engine.osContext->getContextId();
            if (graphicsAllocation.isUsedByOsContext(contextId)) {
                auto currentContextTaskCount = *engine.commandStreamReceiver->getTagAddress();
                if (graphicsAllocation.getTaskCount(contextId) <= currentContextTaskCount) {
                    graphicsAllocation.releaseUsageInOsContext(contextId);
                } else {
                    isStillUsed = true;
                    engine.commandStreamReceiver->flushBatchedSubmissions();
                }
            }
        }
        if (isStillUsed) {
            return false;
        }
    }
    memoryManager.freeGraphicsMemory(&graphicsAllocation);
    return true;
}
} // namespace NEO

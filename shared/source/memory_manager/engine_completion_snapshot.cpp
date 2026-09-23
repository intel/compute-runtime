/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/engine_completion_snapshot.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/task_count_helper.h"
#include "shared/source/helpers/completion_stamp.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_context.h"

namespace NEO {

namespace {

bool checkCommandBufferTagsReady(const GraphicsAllocation &allocation, CommandStreamReceiver &csr) {
    const auto taskCount = allocation.getTaskCount(csr.getOsContext().getContextId());
    if (taskCount >= CompletionStamp::notReady) {
        return false;
    }
    const auto partitionCount = csr.getOsContext().getDeviceBitfield().count();
    const auto partitionOffset = csr.getImmWritePostSyncWriteOffset();
    return TaskCountHelper::isReady(csr.getUcTagAddress(), taskCount, partitionCount, partitionOffset) ||
           TaskCountHelper::isReady(csr.getTagAddress(), taskCount, partitionCount, partitionOffset);
}

} // namespace

bool isCommandBufferReady(const GraphicsAllocation &allocation, CommandStreamReceiver *csr, MemoryManager &memoryManager) {
    const auto numContexts = allocation.getNumRegisteredContexts();
    if (numContexts == 0) {
        return true;
    }
    if (numContexts == 1 && csr && allocation.isUsedByOsContext(csr->getOsContext().getContextId())) {
        return checkCommandBufferTagsReady(allocation, *csr);
    }

    uint32_t checkedContexts = 0;
    for (const auto &engine : memoryManager.getRegisteredEngines(allocation.getRootDeviceIndex())) {
        if (allocation.isUsedByOsContext(engine.osContext->getContextId())) {
            if (!checkCommandBufferTagsReady(allocation, *engine.commandStreamReceiver)) {
                return false;
            }
            if (++checkedContexts == numContexts) {
                return true;
            }
        }
    }
    return false;
}

bool isEngineCompletionSnapshotReady(const EngineCompletionSnapshot &snapshot) {
    for (const auto &[commandStreamReceiver, taskCount] : snapshot) {
        volatile TagAddressType *pollAddress = commandStreamReceiver->getTagAddress();
        if (nullptr == pollAddress) {
            // nothing to poll, so nothing to wait for - same as MemoryManager::allocInUse
            // and MemoryManager::waitForEnginesCompletion
            continue;
        }
        if (!TaskCountHelper::isReady(pollAddress, taskCount, commandStreamReceiver->getActivePartitions(),
                                      commandStreamReceiver->getImmWritePostSyncWriteOffset())) {
            return false;
        }
    }
    return true;
}

} // namespace NEO

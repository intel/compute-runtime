/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_imp.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"

#include "hw_helpers.h"
#include "igfxfmid.h"

namespace L0 {

CommandQueueAllocatorFn commandQueueFactory[IGFX_MAX_PRODUCT] = {};

CommandQueueImp::CommandQueueImp(Device *device, NEO::CommandStreamReceiver *csr, const ze_command_queue_desc_t *desc)
    : desc(*desc), device(device), csr(csr) {
    int overrideCmdQueueSyncMode = NEO::DebugManager.flags.OverrideCmdQueueSynchronousMode.get();
    if (overrideCmdQueueSyncMode != -1) {
        this->desc.mode = static_cast<ze_command_queue_mode_t>(overrideCmdQueueSyncMode);
    }

    int overrideUseKmdWaitFunction = NEO::DebugManager.flags.OverrideUseKmdWaitFunction.get();
    if (overrideUseKmdWaitFunction != -1) {
        useKmdWaitFunction = !!(overrideUseKmdWaitFunction);
    }
}

ze_result_t CommandQueueImp::destroy() {
    delete this;
    return ZE_RESULT_SUCCESS;
}

ze_result_t CommandQueueImp::initialize(bool copyOnly, bool isInternal) {
    ze_result_t returnValue;
    internalUsage = isInternal;
    returnValue = buffers.initialize(device, totalCmdBufferSize);
    if (returnValue == ZE_RESULT_SUCCESS) {
        NEO::GraphicsAllocation *bufferAllocation = buffers.getCurrentBufferAllocation();
        UNRECOVERABLE_IF(bufferAllocation == nullptr);
        commandStream = new NEO::LinearStream(bufferAllocation->getUnderlyingBuffer(),
                                              defaultQueueCmdBufferSize);
        UNRECOVERABLE_IF(commandStream == nullptr);
        commandStream->replaceGraphicsAllocation(bufferAllocation);
        isCopyOnlyCommandQueue = copyOnly;
        preemptionCmdSyncProgramming = getPreemptionCmdProgramming();
    }
    return returnValue;
}

void CommandQueueImp::reserveLinearStreamSize(size_t size) {
    UNRECOVERABLE_IF(commandStream == nullptr);
    if (commandStream->getAvailableSpace() < size) {
        buffers.switchBuffers(csr);
        NEO::GraphicsAllocation *nextBufferAllocation = buffers.getCurrentBufferAllocation();
        commandStream->replaceBuffer(nextBufferAllocation->getUnderlyingBuffer(),
                                     defaultQueueCmdBufferSize);
        commandStream->replaceGraphicsAllocation(nextBufferAllocation);
    }
}

void CommandQueueImp::submitBatchBuffer(size_t offset, NEO::ResidencyContainer &residencyContainer, void *endingCmdPtr) {
    UNRECOVERABLE_IF(csr == nullptr);

    NEO::BatchBuffer batchBuffer(commandStream->getGraphicsAllocation(), offset, 0u, nullptr, false, false,
                                 NEO::QueueThrottle::HIGH, NEO::QueueSliceCount::defaultSliceCount,
                                 commandStream->getUsed(), commandStream, endingCmdPtr, false);

    commandStream->getGraphicsAllocation()->updateTaskCount(csr->peekTaskCount() + 1, csr->getOsContext().getContextId());
    commandStream->getGraphicsAllocation()->updateResidencyTaskCount(csr->peekTaskCount() + 1, csr->getOsContext().getContextId());

    csr->submitBatchBuffer(batchBuffer, csr->getResidencyAllocations());
    buffers.setCurrentFlushStamp(csr->peekTaskCount(), csr->obtainCurrentFlushStamp());
}

ze_result_t CommandQueueImp::synchronize(uint64_t timeout) {
    if ((timeout == std::numeric_limits<uint64_t>::max()) && useKmdWaitFunction) {
        auto &waitPair = buffers.getCurrentFlushStamp();
        csr->waitForTaskCountWithKmdNotifyFallback(waitPair.first, waitPair.second, false, false);
        postSyncOperations();
        return ZE_RESULT_SUCCESS;
    } else {
        return synchronizeByPollingForTaskCount(timeout);
    }
}

ze_result_t CommandQueueImp::synchronizeByPollingForTaskCount(uint64_t timeout) {
    UNRECOVERABLE_IF(csr == nullptr);

    auto taskCountToWait = getTaskCount();
    bool enableTimeout = true;
    int64_t timeoutMicroseconds = static_cast<int64_t>(timeout);
    if (timeout == std::numeric_limits<uint64_t>::max()) {
        enableTimeout = false;
        timeoutMicroseconds = NEO::TimeoutControls::maxTimeout;
    }

    csr->waitForCompletionWithTimeout(enableTimeout, timeoutMicroseconds, this->taskCount);

    if (*csr->getTagAddress() < taskCountToWait) {
        return ZE_RESULT_NOT_READY;
    }

    postSyncOperations();

    return ZE_RESULT_SUCCESS;
}

void CommandQueueImp::printFunctionsPrintfOutput() {
    size_t size = this->printfFunctionContainer.size();
    for (size_t i = 0; i < size; i++) {
        this->printfFunctionContainer[i]->printPrintfOutput();
    }
    this->printfFunctionContainer.clear();
}

void CommandQueueImp::postSyncOperations() {
    printFunctionsPrintfOutput();

    if (NEO::Debugger::isDebugEnabled(internalUsage) && device->getL0Debugger() && NEO::DebugManager.flags.DebuggerLogBitmask.get()) {
        device->getL0Debugger()->printTrackedAddresses(csr->getOsContext().getContextId());
    }
}

CommandQueue *CommandQueue::create(uint32_t productFamily, Device *device, NEO::CommandStreamReceiver *csr,
                                   const ze_command_queue_desc_t *desc, bool isCopyOnly, bool isInternal, ze_result_t &returnValue) {
    CommandQueueAllocatorFn allocator = nullptr;
    if (productFamily < IGFX_MAX_PRODUCT) {
        allocator = commandQueueFactory[productFamily];
    }

    CommandQueueImp *commandQueue = nullptr;
    returnValue = ZE_RESULT_ERROR_UNINITIALIZED;

    if (allocator) {
        commandQueue = static_cast<CommandQueueImp *>((*allocator)(device, csr, desc));
        returnValue = commandQueue->initialize(isCopyOnly, isInternal);
        if (returnValue != ZE_RESULT_SUCCESS) {
            commandQueue->destroy();
            commandQueue = nullptr;
        }
    }

    csr->getOsContext().ensureContextInitialized();
    csr->initDirectSubmission(*device->getNEODevice(), csr->getOsContext());
    return commandQueue;
}

ze_command_queue_mode_t CommandQueueImp::getSynchronousMode() const {
    return desc.mode;
}

ze_result_t CommandQueueImp::CommandBufferManager::initialize(Device *device, size_t sizeRequested) {
    size_t alignedSize = alignUp<size_t>(sizeRequested, MemoryConstants::pageSize64k);
    NEO::AllocationProperties properties{device->getRootDeviceIndex(), true, alignedSize,
                                         NEO::GraphicsAllocation::AllocationType::COMMAND_BUFFER,
                                         device->isMultiDeviceCapable(),
                                         false,
                                         device->getNEODevice()->getDeviceBitfield()};

    buffers[BUFFER_ALLOCATION::FIRST] = device->getNEODevice()->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
    buffers[BUFFER_ALLOCATION::SECOND] = device->getNEODevice()->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);

    if (!buffers[BUFFER_ALLOCATION::FIRST] || !buffers[BUFFER_ALLOCATION::SECOND]) {
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    memset(buffers[BUFFER_ALLOCATION::FIRST]->getUnderlyingBuffer(), 0, buffers[BUFFER_ALLOCATION::FIRST]->getUnderlyingBufferSize());
    memset(buffers[BUFFER_ALLOCATION::SECOND]->getUnderlyingBuffer(), 0, buffers[BUFFER_ALLOCATION::SECOND]->getUnderlyingBufferSize());
    flushId[BUFFER_ALLOCATION::FIRST] = std::make_pair(0u, 0u);
    flushId[BUFFER_ALLOCATION::SECOND] = std::make_pair(0u, 0u);
    return ZE_RESULT_SUCCESS;
}

void CommandQueueImp::CommandBufferManager::destroy(NEO::MemoryManager *memoryManager) {
    if (buffers[BUFFER_ALLOCATION::FIRST]) {
        memoryManager->freeGraphicsMemory(buffers[BUFFER_ALLOCATION::FIRST]);
        buffers[BUFFER_ALLOCATION::FIRST] = nullptr;
    }
    if (buffers[BUFFER_ALLOCATION::SECOND]) {
        memoryManager->freeGraphicsMemory(buffers[BUFFER_ALLOCATION::SECOND]);
        buffers[BUFFER_ALLOCATION::SECOND] = nullptr;
    }
}

void CommandQueueImp::CommandBufferManager::switchBuffers(NEO::CommandStreamReceiver *csr) {
    if (bufferUse == BUFFER_ALLOCATION::FIRST) {
        bufferUse = BUFFER_ALLOCATION::SECOND;
    } else {
        bufferUse = BUFFER_ALLOCATION::FIRST;
    }

    auto completionId = flushId[bufferUse];
    if (completionId.second != 0u) {
        UNRECOVERABLE_IF(csr == nullptr);
        csr->waitForTaskCountWithKmdNotifyFallback(completionId.first, completionId.second, false, false);
    }
}

} // namespace L0

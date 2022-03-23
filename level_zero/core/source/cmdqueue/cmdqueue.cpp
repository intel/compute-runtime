/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/queue_throttle.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_imp.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"

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
        activeSubDevices = static_cast<uint32_t>(csr->getOsContext().getDeviceBitfield().count());
        if (!isInternal) {
            partitionCount = csr->getActivePartitions();
        }
        if (NEO::Debugger::isDebugEnabled(internalUsage) && device->getL0Debugger()) {
            device->getL0Debugger()->notifyCommandQueueCreated();
        }
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

NEO::SubmissionStatus CommandQueueImp::submitBatchBuffer(size_t offset, NEO::ResidencyContainer &residencyContainer, void *endingCmdPtr,
                                                         bool isCooperative) {
    UNRECOVERABLE_IF(csr == nullptr);

    NEO::BatchBuffer batchBuffer(commandStream->getGraphicsAllocation(), offset, 0u, nullptr, false, false,
                                 NEO::QueueThrottle::HIGH, NEO::QueueSliceCount::defaultSliceCount,
                                 commandStream->getUsed(), commandStream, endingCmdPtr, isCooperative);

    commandStream->getGraphicsAllocation()->updateTaskCount(csr->peekTaskCount() + 1, csr->getOsContext().getContextId());
    commandStream->getGraphicsAllocation()->updateResidencyTaskCount(csr->peekTaskCount() + 1, csr->getOsContext().getContextId());

    csr->setActivePartitions(partitionCount);
    auto ret = csr->submitBatchBuffer(batchBuffer, csr->getResidencyAllocations());
    if (ret != NEO::SubmissionStatus::SUCCESS) {
        return ret;
    }

    buffers.setCurrentFlushStamp(csr->peekTaskCount(), csr->obtainCurrentFlushStamp());

    return ret;
}

ze_result_t CommandQueueImp::synchronize(uint64_t timeout) {
    if ((timeout == std::numeric_limits<uint64_t>::max()) && useKmdWaitFunction) {
        auto &waitPair = buffers.getCurrentFlushStamp();
        const auto waitStatus = csr->waitForTaskCountWithKmdNotifyFallback(waitPair.first, waitPair.second, false, NEO::QueueThrottle::MEDIUM);
        if (waitStatus == NEO::WaitStatus::GpuHang) {
            return ZE_RESULT_ERROR_DEVICE_LOST;
        }

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

    const auto waitStatus = csr->waitForCompletionWithTimeout(NEO::WaitParams{false, enableTimeout, timeoutMicroseconds}, taskCountToWait);
    if (waitStatus == NEO::WaitStatus::NotReady) {
        return ZE_RESULT_NOT_READY;
    }
    if (waitStatus == NEO::WaitStatus::GpuHang) {
        return ZE_RESULT_ERROR_DEVICE_LOST;
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

    auto &osContext = csr->getOsContext();
    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(device->getDriverHandle());
    if (driverHandleImp->powerHint && driverHandleImp->powerHint != osContext.getUmdPowerHintValue()) {
        osContext.setUmdPowerHintValue(driverHandleImp->powerHint);
        osContext.reInitializeContext();
    }
    osContext.ensureContextInitialized();
    csr->initDirectSubmission(*device->getNEODevice(), osContext);
    return commandQueue;
}

ze_command_queue_mode_t CommandQueueImp::getSynchronousMode() const {
    return desc.mode;
}

ze_result_t CommandQueueImp::CommandBufferManager::initialize(Device *device, size_t sizeRequested) {
    size_t alignedSize = alignUp<size_t>(sizeRequested, MemoryConstants::pageSize64k);
    NEO::AllocationProperties properties{device->getRootDeviceIndex(), true, alignedSize,
                                         NEO::AllocationType::COMMAND_BUFFER,
                                         (device->getNEODevice()->getNumGenericSubDevices() > 1u) /* multiOsContextCapable */,
                                         false,
                                         device->getNEODevice()->getDeviceBitfield()};

    auto firstBuffer = device->obtainReusableAllocation(alignedSize, NEO::AllocationType::COMMAND_BUFFER);
    if (!firstBuffer) {
        firstBuffer = device->getNEODevice()->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
    }

    auto secondBuffer = device->obtainReusableAllocation(alignedSize, NEO::AllocationType::COMMAND_BUFFER);
    if (!secondBuffer) {
        secondBuffer = device->getNEODevice()->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
    }

    buffers[BUFFER_ALLOCATION::FIRST] = firstBuffer;
    buffers[BUFFER_ALLOCATION::SECOND] = secondBuffer;

    if (!buffers[BUFFER_ALLOCATION::FIRST] || !buffers[BUFFER_ALLOCATION::SECOND]) {
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    memset(buffers[BUFFER_ALLOCATION::FIRST]->getUnderlyingBuffer(), 0, buffers[BUFFER_ALLOCATION::FIRST]->getUnderlyingBufferSize());
    memset(buffers[BUFFER_ALLOCATION::SECOND]->getUnderlyingBuffer(), 0, buffers[BUFFER_ALLOCATION::SECOND]->getUnderlyingBufferSize());
    flushId[BUFFER_ALLOCATION::FIRST] = std::make_pair(0u, 0u);
    flushId[BUFFER_ALLOCATION::SECOND] = std::make_pair(0u, 0u);
    return ZE_RESULT_SUCCESS;
}

void CommandQueueImp::CommandBufferManager::destroy(Device *device) {
    if (buffers[BUFFER_ALLOCATION::FIRST]) {
        device->storeReusableAllocation(*buffers[BUFFER_ALLOCATION::FIRST]);
        buffers[BUFFER_ALLOCATION::FIRST] = nullptr;
    }
    if (buffers[BUFFER_ALLOCATION::SECOND]) {
        device->storeReusableAllocation(*buffers[BUFFER_ALLOCATION::SECOND]);
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
        csr->waitForTaskCountWithKmdNotifyFallback(completionId.first, completionId.second, false, NEO::QueueThrottle::MEDIUM);
    }
}

} // namespace L0

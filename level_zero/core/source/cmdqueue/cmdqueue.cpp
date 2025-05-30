/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/assert_handler/assert_handler.h"
#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/queue_throttle.h"
#include "shared/source/command_stream/submissions_aggregator.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/product_helper.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_imp.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/source/helpers/properties_parser.h"
#include "level_zero/core/source/kernel/kernel.h"

#include "neo_igfxfmid.h"

namespace L0 {

CommandQueueAllocatorFn commandQueueFactory[IGFX_MAX_PRODUCT] = {};

bool CommandQueue::frontEndTrackingEnabled() const {
    return NEO::debugManager.flags.AllowPatchingVfeStateInCommandLists.get() || this->frontEndStateTracking;
}

CommandQueueImp::CommandQueueImp(Device *device, NEO::CommandStreamReceiver *csr, const ze_command_queue_desc_t *desc)
    : desc(*desc), device(device), csr(csr) {
    int overrideCmdQueueSyncMode = NEO::debugManager.flags.OverrideCmdQueueSynchronousMode.get();
    if (overrideCmdQueueSyncMode != -1) {
        this->desc.mode = static_cast<ze_command_queue_mode_t>(overrideCmdQueueSyncMode);
    }

    int overrideUseKmdWaitFunction = NEO::debugManager.flags.OverrideUseKmdWaitFunction.get();
    if (overrideUseKmdWaitFunction != -1) {
        useKmdWaitFunction = !!(overrideUseKmdWaitFunction);
    }
}

ze_result_t CommandQueueImp::destroy() {
    unregisterCsrClient();

    if (commandStream.getCpuBase() != nullptr) {
        commandStream.replaceGraphicsAllocation(nullptr);
        commandStream.replaceBuffer(nullptr, 0);
    }
    buffers.destroy(this->getDevice());
    if (NEO::Debugger::isDebugEnabled(internalUsage) && device->getL0Debugger()) {
        device->getL0Debugger()->notifyCommandQueueDestroyed(device->getNEODevice());
    }

    delete this;
    return ZE_RESULT_SUCCESS;
}

ze_result_t CommandQueueImp::initialize(bool copyOnly, bool isInternal, bool immediateCmdListQueue) {
    ze_result_t returnValue;
    internalUsage = isInternal;
    returnValue = buffers.initialize(device, totalCmdBufferSize);
    if (returnValue == ZE_RESULT_SUCCESS) {
        NEO::GraphicsAllocation *bufferAllocation = buffers.getCurrentBufferAllocation();
        UNRECOVERABLE_IF(bufferAllocation == nullptr);
        commandStream.replaceBuffer(bufferAllocation->getUnderlyingBuffer(),
                                    defaultQueueCmdBufferSize);
        commandStream.replaceGraphicsAllocation(bufferAllocation);
        isCopyOnlyCommandQueue = copyOnly;
        preemptionCmdSyncProgramming = getPreemptionCmdProgramming();
        activeSubDevices = static_cast<uint32_t>(csr->getOsContext().getDeviceBitfield().count());
        if (!isInternal) {
            partitionCount = csr->getActivePartitions();
        }
        if (NEO::Debugger::isDebugEnabled(internalUsage) && device->getL0Debugger()) {
            device->getL0Debugger()->notifyCommandQueueCreated(device->getNEODevice());
        }
        auto &hwInfo = device->getHwInfo();
        auto &rootDeviceEnvironment = device->getNEODevice()->getRootDeviceEnvironment();
        this->stateComputeModeTracking = L0GfxCoreHelper::enableStateComputeModeTracking(rootDeviceEnvironment);
        this->frontEndStateTracking = L0GfxCoreHelper::enableFrontEndStateTracking(rootDeviceEnvironment);
        this->pipelineSelectStateTracking = L0GfxCoreHelper::enablePipelineSelectStateTracking(rootDeviceEnvironment);
        this->stateBaseAddressTracking = L0GfxCoreHelper::enableStateBaseAddressTracking(rootDeviceEnvironment);
        auto &productHelper = rootDeviceEnvironment.getHelper<NEO::ProductHelper>();
        this->doubleSbaWa = productHelper.isAdditionalStateBaseAddressWARequired(hwInfo);
        this->cmdListHeapAddressModel = L0GfxCoreHelper::getHeapAddressModel(rootDeviceEnvironment);
        this->dispatchCmdListBatchBufferAsPrimary = L0GfxCoreHelper::dispatchCmdListBatchBufferAsPrimary(rootDeviceEnvironment, !(internalUsage && immediateCmdListQueue));
        auto &compilerProductHelper = rootDeviceEnvironment.getHelper<NEO::CompilerProductHelper>();
        this->heaplessModeEnabled = compilerProductHelper.isHeaplessModeEnabled(hwInfo);
        this->heaplessStateInitEnabled = compilerProductHelper.isHeaplessStateInitEnabled(this->heaplessModeEnabled);
    }
    return returnValue;
}

NEO::WaitStatus CommandQueueImp::reserveLinearStreamSize(size_t size) {
    auto waitStatus{NEO::WaitStatus::ready};

    if (commandStream.getAvailableSpace() < size) {
        waitStatus = buffers.switchBuffers(csr);

        NEO::GraphicsAllocation *nextBufferAllocation = buffers.getCurrentBufferAllocation();
        commandStream.replaceBuffer(nextBufferAllocation->getUnderlyingBuffer(),
                                    defaultQueueCmdBufferSize);
        commandStream.replaceGraphicsAllocation(nextBufferAllocation);
    }

    return waitStatus;
}

NEO::SubmissionStatus CommandQueueImp::submitBatchBuffer(size_t offset, NEO::ResidencyContainer &residencyContainer, void *endingCmdPtr,
                                                         bool isCooperative) {
    UNRECOVERABLE_IF(csr == nullptr);

    NEO::BatchBuffer batchBuffer(this->startingCmdBuffer->getGraphicsAllocation(), offset, 0, 0, nullptr, false,
                                 NEO::getThrottleFromPowerSavingUint(csr->getUmdPowerHintValue()), NEO::QueueSliceCount::defaultSliceCount,
                                 this->startingCmdBuffer->getUsed(), this->startingCmdBuffer, endingCmdPtr, csr->getNumClients(), true, false, true, false);
    batchBuffer.disableFlatRingBuffer = true;

    if (this->startingCmdBuffer != &this->commandStream) {
        this->csr->makeResident(*this->commandStream.getGraphicsAllocation());
    }

    commandStream.getGraphicsAllocation()->updateTaskCount(csr->peekTaskCount() + 1, csr->getOsContext().getContextId());
    commandStream.getGraphicsAllocation()->updateResidencyTaskCount(csr->peekTaskCount() + 1, csr->getOsContext().getContextId());

    csr->setActivePartitions(partitionCount);
    auto ret = csr->submitBatchBuffer(batchBuffer, csr->getResidencyAllocations());
    if (ret != NEO::SubmissionStatus::success) {
        commandStream.getGraphicsAllocation()->updateTaskCount(csr->peekTaskCount(), csr->getOsContext().getContextId());
        commandStream.getGraphicsAllocation()->updateResidencyTaskCount(csr->peekTaskCount(), csr->getOsContext().getContextId());
        return ret;
    }

    buffers.setCurrentFlushStamp(csr->peekTaskCount(), csr->obtainCurrentFlushStamp());

    return ret;
}

ze_result_t CommandQueueImp::synchronize(uint64_t timeout) {
    if ((timeout == std::numeric_limits<uint64_t>::max()) && useKmdWaitFunction) {
        auto &waitPair = buffers.getCurrentFlushStamp();
        const auto waitStatus = csr->waitForTaskCountWithKmdNotifyFallback(waitPair.first, waitPair.second, false, NEO::QueueThrottle::MEDIUM);
        if (waitStatus == NEO::WaitStatus::gpuHang) {
            postSyncOperations(true);
            return ZE_RESULT_ERROR_DEVICE_LOST;
        }
        postSyncOperations(false);

        return ZE_RESULT_SUCCESS;
    } else {
        return synchronizeByPollingForTaskCount(timeout);
    }
}

ze_result_t CommandQueueImp::synchronizeByPollingForTaskCount(uint64_t timeoutNanoseconds) {
    UNRECOVERABLE_IF(csr == nullptr);

    auto taskCountToWait = getTaskCount();
    bool enableTimeout = true;
    auto microsecondResolution = device->getNEODevice()->getMicrosecondResolution();
    int64_t timeoutMicroseconds = static_cast<int64_t>(timeoutNanoseconds / microsecondResolution);
    if (timeoutNanoseconds == std::numeric_limits<uint64_t>::max()) {
        enableTimeout = false;
        timeoutMicroseconds = NEO::TimeoutControls::maxTimeout;
    }

    const auto waitStatus = csr->waitForCompletionWithTimeout(NEO::WaitParams{false, enableTimeout, false, timeoutMicroseconds}, taskCountToWait);
    if (waitStatus == NEO::WaitStatus::notReady) {
        return ZE_RESULT_NOT_READY;
    }
    if (waitStatus == NEO::WaitStatus::gpuHang) {
        postSyncOperations(true);
        return ZE_RESULT_ERROR_DEVICE_LOST;
    }

    postSyncOperations(false);
    return ZE_RESULT_SUCCESS;
}

void CommandQueueImp::printKernelsPrintfOutput(bool hangDetected) {
    for (auto &kernelWeakPtr : this->printfKernelContainer) {
        std::lock_guard<std::mutex> lock(static_cast<DeviceImp *>(this->getDevice())->printfKernelMutex);
        if (!kernelWeakPtr.expired()) {
            kernelWeakPtr.lock()->printPrintfOutput(hangDetected);
        }
    }
    this->printfKernelContainer.clear();
}

void CommandQueueImp::checkAssert() {
    bool valueExpected = true;
    bool hadAssert = cmdListWithAssertExecuted.compare_exchange_strong(valueExpected, false);

    if (hadAssert) {
        UNRECOVERABLE_IF(device->getNEODevice()->getRootDeviceEnvironment().assertHandler.get() == nullptr);
        device->getNEODevice()->getRootDeviceEnvironment().assertHandler->printAssertAndAbort();
    }
}

void CommandQueueImp::postSyncOperations(bool hangDetected) {
    printKernelsPrintfOutput(hangDetected);
    checkAssert();

    if (NEO::Debugger::isDebugEnabled(internalUsage) && device->getL0Debugger() && NEO::debugManager.flags.DebuggerLogBitmask.get()) {
        device->getL0Debugger()->printTrackedAddresses(csr->getOsContext().getContextId());
    }

    unregisterCsrClient();
}

CommandQueue *CommandQueue::create(uint32_t productFamily, Device *device, NEO::CommandStreamReceiver *csr,
                                   const ze_command_queue_desc_t *desc, bool isCopyOnly, bool isInternal, bool immediateCmdListQueue, ze_result_t &returnValue) {
    CommandQueueAllocatorFn allocator = nullptr;
    if (productFamily < IGFX_MAX_PRODUCT) {
        allocator = commandQueueFactory[productFamily];
    }

    CommandQueueImp *commandQueue = nullptr;
    returnValue = ZE_RESULT_ERROR_UNINITIALIZED;
    if (!allocator) {
        return nullptr;
    }

    commandQueue = static_cast<CommandQueueImp *>((*allocator)(device, csr, desc));
    returnValue = commandQueue->initialize(isCopyOnly, isInternal, immediateCmdListQueue);
    if (returnValue != ZE_RESULT_SUCCESS) {
        commandQueue->destroy();
        commandQueue = nullptr;
        return nullptr;
    }

    auto &osContext = csr->getOsContext();
    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(device->getDriverHandle());
    if (driverHandleImp->powerHint && driverHandleImp->powerHint != osContext.getUmdPowerHintValue()) {
        osContext.setUmdPowerHintValue(driverHandleImp->powerHint);
        osContext.reInitializeContext();
    }

    csr->initializeResources(false, device->getDevicePreemptionMode());
    csr->initDirectSubmission();
    if (commandQueue->cmdListHeapAddressModel == NEO::HeapAddressModel::globalStateless) {
        csr->createGlobalStatelessHeap();
    }

    return commandQueue;
}

void CommandQueueImp::unregisterCsrClient() {
    this->csr->unregisterClient(this);
}

void CommandQueueImp::registerCsrClient() {
    this->csr->registerClient(this);
}

ze_result_t CommandQueueImp::CommandBufferManager::initialize(Device *device, size_t sizeRequested) {
    size_t alignedSize = alignUp<size_t>(sizeRequested, MemoryConstants::pageSize64k);
    NEO::AllocationProperties properties{device->getRootDeviceIndex(), true, alignedSize,
                                         NEO::AllocationType::commandBuffer,
                                         (device->getNEODevice()->getNumGenericSubDevices() > 1u) /* multiOsContextCapable */,
                                         false,
                                         device->getNEODevice()->getDeviceBitfield()};

    auto firstBuffer = device->obtainReusableAllocation(alignedSize, NEO::AllocationType::commandBuffer);
    if (!firstBuffer) {
        firstBuffer = device->getNEODevice()->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
    }

    auto secondBuffer = device->obtainReusableAllocation(alignedSize, NEO::AllocationType::commandBuffer);
    if (!secondBuffer) {
        secondBuffer = device->getNEODevice()->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
    }

    buffers[BufferAllocation::first] = firstBuffer;
    buffers[BufferAllocation::second] = secondBuffer;

    if (!buffers[BufferAllocation::first] || !buffers[BufferAllocation::second]) {
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    flushId[BufferAllocation::first] = std::make_pair(0u, 0u);
    flushId[BufferAllocation::second] = std::make_pair(0u, 0u);
    return ZE_RESULT_SUCCESS;
}

void CommandQueueImp::CommandBufferManager::destroy(Device *device) {
    if (buffers[BufferAllocation::first]) {
        device->storeReusableAllocation(*buffers[BufferAllocation::first]);
        buffers[BufferAllocation::first] = nullptr;
    }
    if (buffers[BufferAllocation::second]) {
        device->storeReusableAllocation(*buffers[BufferAllocation::second]);
        buffers[BufferAllocation::second] = nullptr;
    }
}

NEO::WaitStatus CommandQueueImp::CommandBufferManager::switchBuffers(NEO::CommandStreamReceiver *csr) {
    if (bufferUse == BufferAllocation::first) {
        bufferUse = BufferAllocation::second;
    } else {
        bufferUse = BufferAllocation::first;
    }

    auto waitStatus{NEO::WaitStatus::ready};
    auto completionId = flushId[bufferUse];
    if (completionId.second != 0u) {
        UNRECOVERABLE_IF(csr == nullptr);
        waitStatus = csr->waitForTaskCountWithKmdNotifyFallback(completionId.first, completionId.second, false, NEO::QueueThrottle::MEDIUM);
    }

    return waitStatus;
}

void CommandQueueImp::handleIndirectAllocationResidency(UnifiedMemoryControls unifiedMemoryControls, std::unique_lock<std::mutex> &lockForIndirect, bool performMigration) {
    NEO::Device *neoDevice = this->device->getNEODevice();
    auto svmAllocsManager = this->device->getDriverHandle()->getSvmAllocsManager();
    auto submittedAsPack = svmAllocsManager->submitIndirectAllocationsAsPack(*(this->csr));

    if (!submittedAsPack) {
        lockForIndirect = this->device->getDriverHandle()->getSvmAllocsManager()->obtainOwnership();
        NEO::ResidencyContainer residencyAllocations;
        svmAllocsManager->addInternalAllocationsToResidencyContainer(neoDevice->getRootDeviceIndex(),
                                                                     residencyAllocations,
                                                                     unifiedMemoryControls.generateMask());
        makeResidentAndMigrate(performMigration, residencyAllocations);
    }
}

void CommandQueueImp::makeResidentAndMigrate(bool performMigration, const NEO::ResidencyContainer &residencyContainer) {
    for (auto alloc : residencyContainer) {
        alloc->prepareHostPtrForResidency(csr);
        csr->makeResident(*alloc);
        if (performMigration &&
            (alloc->getAllocationType() == NEO::AllocationType::svmGpu ||
             alloc->getAllocationType() == NEO::AllocationType::svmCpu)) {
            auto pageFaultManager = device->getDriverHandle()->getMemoryManager()->getPageFaultManager();
            pageFaultManager->moveAllocationToGpuDomain(reinterpret_cast<void *>(alloc->getGpuAddress()));
        }
    }
}

ze_result_t CommandQueueImp::getOrdinal(uint32_t *pOrdinal) {
    *pOrdinal = desc.ordinal;
    return ZE_RESULT_SUCCESS;
}

ze_result_t CommandQueueImp::getIndex(uint32_t *pIndex) {
    *pIndex = desc.index;
    return ZE_RESULT_SUCCESS;
}

QueueProperties CommandQueue::extractQueueProperties(const ze_command_queue_desc_t &desc) {
    QueueProperties queueProperties = {};

    auto baseProperties = static_cast<const ze_base_desc_t *>(desc.pNext);

    while (baseProperties) {
        if (static_cast<uint32_t>(baseProperties->stype) == ZEX_INTEL_STRUCTURE_TYPE_QUEUE_ALLOCATE_MSIX_HINT_EXP_PROPERTIES) {
            queueProperties.interruptHint = static_cast<const zex_intel_queue_allocate_msix_hint_exp_desc_t *>(desc.pNext)->uniqueMsix;
        } else if (auto syncDispatchMode = getSyncDispatchMode(baseProperties)) {
            if (syncDispatchMode.has_value()) {
                queueProperties.synchronizedDispatchMode = syncDispatchMode.value();
            }
        } else if (static_cast<uint32_t>(baseProperties->stype) == ZEX_INTEL_STRUCTURE_TYPE_QUEUE_COPY_OPERATIONS_OFFLOAD_HINT_EXP_PROPERTIES) {
            queueProperties.copyOffloadHint = static_cast<const zex_intel_queue_copy_operations_offload_hint_exp_desc_t *>(desc.pNext)->copyOffloadEnabled;
        } else if (static_cast<uint32_t>(baseProperties->stype) == ZE_STRUCTURE_TYPE_QUEUE_PRIORITY_DESC) {
            queueProperties.priorityLevel = static_cast<const ze_queue_priority_desc_t *>(desc.pNext)->priority;
        }

        baseProperties = static_cast<const ze_base_desc_t *>(baseProperties->pNext);
    }

    return queueProperties;
}

void CommandQueueImp::makeResidentForResidencyContainer(const NEO::ResidencyContainer &residencyContainer) {
    for (auto alloc : residencyContainer) {
        alloc->prepareHostPtrForResidency(csr);
        csr->makeResident(*alloc);
    }
}

} // namespace L0

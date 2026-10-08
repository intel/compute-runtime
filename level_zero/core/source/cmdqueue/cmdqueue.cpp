/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdqueue/cmdqueue.h"

#include "shared/source/assert_handler/assert_handler.h"
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
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/utilities/buffer_pool_allocator.inl"
#include "shared/source/utilities/pool_allocators.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_cmdlist_execution_context.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_cmdlist_execution_internal_options.h"
#include "level_zero/core/source/cmdqueue/counters_cross_sync_definitions.h"
#include "level_zero/core/source/cmdqueue/internal_queue_throttle_ext.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/source/helpers/properties_parser.h"
#include "level_zero/core/source/kernel/kernel.h"

namespace L0 {

CommandQueueAllocatorFn commandQueueFactory[NEO::maxCoreEnumValue] = {};

bool CommandQueue::frontEndTrackingEnabled() const {
    return NEO::debugManager.flags.AllowPatchingVfeStateInCommandLists.get() || this->frontEndStateTracking;
}

void CommandQueue::saveTagAndTaskCountForCommandLists(uint32_t numCommandLists, ze_command_list_handle_t *commandListHandles,
                                                      NEO::GraphicsAllocation *tagGpuAllocation, TaskCountType submittedTaskCount) {
    if (this->saveWaitForPreamble) {
        for (uint32_t i = 0; i < numCommandLists; i++) {
            auto commandList = CommandList::fromHandle(commandListHandles[i]);
            commandList->saveLatestTagAndTaskCount(tagGpuAllocation, submittedTaskCount);
        }
    }
}

CommandQueue::CommandQueue(Device *device, NEO::CommandStreamReceiver *csr, const ze_command_queue_desc_t *desc)
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

ze_result_t CommandQueue::destroy() {
    if (copyOffloadQueue) {
        if (const auto copyOffloadTaskCount = copyOffloadQueue->getTaskCount(); copyOffloadTaskCount != 0) {
            copyOffloadQueue->getCsr()->waitForCompletionWithTimeout(NEO::WaitParams{false, false, false, NEO::TimeoutControls::maxTimeout}, copyOffloadTaskCount);
        }
        copyOffloadQueue->destroy();
        copyOffloadQueue = nullptr;
    }

    unregisterCsrClient();

    if (csrQueueOwnershipTaken) {
        csrQueueOwnershipTaken = false;
        csr->releaseQueueOwnership();
    }

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

ze_result_t CommandQueue::initialize(bool copyOnly, bool isInternal, bool immediateCmdListQueue) {
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
        if (!immediateCmdListQueue && NEO::Debugger::isDebugEnabled(internalUsage) && device->getL0Debugger()) {
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
        this->dispatchCmdListBatchBufferAsPrimary = L0GfxCoreHelper::dispatchCmdListBatchBufferAsPrimary(true);
        auto &compilerProductHelper = rootDeviceEnvironment.getHelper<NEO::CompilerProductHelper>();
        this->heaplessModeEnabled = compilerProductHelper.isHeaplessModeEnabled(hwInfo);
        this->saveWaitForPreamble = device->getGfxCoreHelper().getContextGroupContextsCount() > 1;
    }
    return returnValue;
}

ze_result_t CommandQueue::createCopyOffloadQueue() {
    std::lock_guard<std::mutex> lock(this->copyOffloadQueueCreationMutex);
    if (this->copyOffloadQueue != nullptr) {
        return ZE_RESULT_SUCCESS;
    }

    const auto ordinal = device->getCopyEngineOrdinal();

    NEO::CommandStreamReceiver *copyCsr = nullptr;
    bool queueOwnershipTaken = false;
    ze_result_t returnValue = device->getCsrForOrdinalAndIndex(&copyCsr, ordinal, 0, desc.priority, std::nullopt, 0u, &queueOwnershipTaken);
    if (returnValue != ZE_RESULT_SUCCESS) {
        return returnValue;
    }

    ze_command_queue_desc_t copyQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    copyQueueDesc.ordinal = ordinal;
    copyQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    copyQueueDesc.priority = desc.priority;

    auto newCopyOffloadQueue = CommandQueue::create(device, copyCsr, &copyQueueDesc, true, true, false, returnValue);
    if (newCopyOffloadQueue == nullptr) {
        if (queueOwnershipTaken) {
            copyCsr->releaseQueueOwnership();
        }
        return returnValue;
    }
    if (queueOwnershipTaken) {
        newCopyOffloadQueue->takeCsrQueueOwnership();
    }
    this->copyOffloadQueue = newCopyOffloadQueue;
    return ZE_RESULT_SUCCESS;
}

ze_result_t CommandQueue::ensureCopyOffloadQueue(uint32_t numCommandLists, ze_command_list_handle_t *phCommandLists) {
    if (this->copyOffloadQueue != nullptr) {
        return ZE_RESULT_SUCCESS;
    }
    for (auto i = 0u; i < numCommandLists; i++) {
        if (CommandList::fromHandle(phCommandLists[i])->getCopyOffloadSubCmdList() != nullptr) {
            // copy queue creation initializes direct submission, which locks direct submission controller.
            // Controller thread locks csrs while holding it, so this can't be done under ownership of this queue's csr.
            return createCopyOffloadQueue();
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t CommandQueue::dispatchCopyOffloadCmdList(CommandList *commandList, const CommandListExecutionContext &ctx) {
    auto copyOffloadCmdList = commandList->getCopyOffloadSubCmdList();
    if (copyOffloadCmdList == nullptr) {
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t returnValue = ZE_RESULT_SUCCESS;

    if (this->copyOffloadQueue == nullptr) {
        // copy queue is expected to be created by ensureCopyOffloadQueue() before csr ownership is taken
        DEBUG_BREAK_IF(true);
        returnValue = createCopyOffloadQueue();
        if (returnValue != ZE_RESULT_SUCCESS) {
            return returnValue;
        }
    }

    CommandListExecutionInternalOptions internalOptions = {};
    internalOptions.performMigration = ctx.isMigrationRequested;
    CountersCrossSyncContainer patchPreambleCounterSync;

    copyOffloadCmdList->setupPatchPreambleEnabled(ctx.patchPreambleEnabled);
    if (ctx.patchPreambleEnabled) {
        // In-order counter is reset by compute stream patch preamble, so copy offload stream could observe values from previous execution.
        // Copy offload stream starts after compute stream patch preamble signals patch preamble counter for current execution.
        UNRECOVERABLE_IF(ctx.patchPreambleRequiredCounter == 0 || ctx.patchPreambleRequiredDevicePostSyncGpuAddress == 0);
        NEO::GraphicsAllocation *counterHostAllocation = nullptr;
        uint64_t counterHostGpuAddress = 0;
        NEO::GraphicsAllocation *counterDeviceAllocation = nullptr;
        uint64_t counterDeviceGpuAddress = 0;
        patchPreambleCounter.getPatchPreambleNodeData(counterHostAllocation, counterHostGpuAddress, counterDeviceAllocation, counterDeviceGpuAddress);
        UNRECOVERABLE_IF(counterDeviceAllocation == nullptr);

        patchPreambleCounterSync.patchPreambleCrossSyncList.push_back({.counter = ctx.patchPreambleRequiredCounter,
                                                                       .deviceGpuAddress = ctx.patchPreambleRequiredDevicePostSyncGpuAddress,
                                                                       .deviceGraphicsAllocation = counterDeviceAllocation,
                                                                       .appendedCommandListToSyncBefore = copyOffloadCmdList});
        internalOptions.countersCrossSyncContainer = &patchPreambleCounterSync;

        // compute stream patch preamble may patch commands in copy offload stream
        for (auto cmdBuffer : copyOffloadCmdList->getCmdContainer().getCmdBufferAllocations()) {
            this->csr->makeResident(*cmdBuffer);
        }
    }

    auto copyOffloadCmdListHandle = copyOffloadCmdList->toHandle();
    return this->copyOffloadQueue->executeCommandLists(1, &copyOffloadCmdListHandle, nullptr, internalOptions);
}

void CommandQueue::downloadAllocations() {
    csr->downloadAllocations(true);

    // destinations of operations offloaded from regular cmd lists are resident only on copy queue csr
    if (copyOffloadQueue) {
        copyOffloadQueue->getCsr()->downloadAllocations(true);
    }
}

NEO::WaitStatus CommandQueue::reserveLinearStreamSize(size_t size) {
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

NEO::SubmissionStatus CommandQueue::submitBatchBuffer(size_t offset, void *endingCmdPtr,
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

ze_result_t CommandQueue::synchronize(uint64_t timeout) {
    if ((timeout == std::numeric_limits<uint64_t>::max()) && useKmdWaitFunction) {
        auto &waitPair = buffers.getCurrentFlushStamp();
        const auto waitStatus = csr->waitForTaskCountWithKmdNotifyFallback(waitPair.first, waitPair.second, false, NEO::QueueThrottle::MEDIUM);
        if (waitStatus == NEO::WaitStatus::gpuHang) {
            postSyncOperations(true);
            return ZE_RESULT_ERROR_DEVICE_LOST;
        }
        if (csr->isTbxMode()) {
            downloadAllocations();
        }
        postSyncOperations(false);

        return ZE_RESULT_SUCCESS;
    } else {
        return synchronizeByPollingForTaskCount(timeout);
    }
}

ze_result_t CommandQueue::synchronizeByPollingForTaskCount(uint64_t timeoutNanoseconds) {
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

    if (csr->isTbxMode()) {
        downloadAllocations();
    }
    postSyncOperations(false);
    getCsr()->pollForAubCompletion();

    return ZE_RESULT_SUCCESS;
}

void CommandQueue::printKernelsPrintfOutput(bool hangDetected) {
    for (auto &kernelWeakPtr : this->printfKernelContainer) {
        std::lock_guard<std::mutex> lock(this->getDevice()->printfKernelMutex);
        if (!kernelWeakPtr.expired()) {
            kernelWeakPtr.lock()->printPrintfOutput(hangDetected);
        }
    }
    this->printfKernelContainer.clear();
}

void CommandQueue::checkAssert() {
    bool valueExpected = true;
    bool hadAssert = cmdListWithAssertExecuted.compare_exchange_strong(valueExpected, false);

    if (hadAssert) {
        UNRECOVERABLE_IF(device->getNEODevice()->getRootDeviceEnvironment().assertHandler.get() == nullptr);
        device->getNEODevice()->getRootDeviceEnvironment().assertHandler->printAssertAndAbort();
    }
}

void CommandQueue::postSyncOperations(bool hangDetected) {
    printKernelsPrintfOutput(hangDetected);
    checkAssert();

    if (NEO::Debugger::isDebugEnabled(internalUsage) && device->getL0Debugger() && NEO::debugManager.flags.DebuggerLogBitmask.get()) {
        device->getL0Debugger()->printTrackedAddresses(csr->getOsContext().getContextId());
    }

    unregisterCsrClient();
}

CommandQueue *CommandQueue::create(Device *device, NEO::CommandStreamReceiver *csr,
                                   const ze_command_queue_desc_t *desc, bool isCopyOnly, bool isInternal, bool immediateCmdListQueue, ze_result_t &returnValue) {
    CommandQueueAllocatorFn allocator = nullptr;
    auto gfxCoreFamily = device->getNEODevice()->getRenderCoreFamily();
    if (gfxCoreFamily < NEO::maxCoreEnumValue) {
        allocator = commandQueueFactory[gfxCoreFamily];
    }

    CommandQueue *commandQueue = nullptr;
    returnValue = ZE_RESULT_ERROR_UNINITIALIZED;
    if (!allocator) {
        return nullptr;
    }

    commandQueue = (*allocator)(device, csr, desc);
    returnValue = commandQueue->initialize(isCopyOnly, isInternal, immediateCmdListQueue);
    if (returnValue != ZE_RESULT_SUCCESS) {
        commandQueue->destroy();
        commandQueue = nullptr;
        return nullptr;
    }

    csr->initializeResourcesAndDirectSubmission(device->getDevicePreemptionMode());
    if (commandQueue->cmdListHeapAddressModel == NEO::HeapAddressModel::globalStateless) {
        csr->createGlobalStatelessHeap();
    }

    return commandQueue;
}

void CommandQueue::unregisterCsrClient() {
    this->csr->unregisterClient(this);
    this->csrClientRegistered = false;
}

void CommandQueue::registerCsrClient() {
    if (this->csrClientRegistered) {
        return;
    }
    this->csr->registerClient(this);
    this->csrClientRegistered = true;
}

ze_result_t CommandBufferManager::initialize(Device *device, size_t sizeRequested) {
    size_t alignedSize = alignUp<size_t>(sizeRequested, MemoryConstants::pageSize64k);

    auto &rootDeviceEnvironment = device->getNEODevice()->getRootDeviceEnvironment();
    auto &productHelper = rootDeviceEnvironment.getHelper<NEO::ProductHelper>();

    if (NEO::CommandBufferPoolAllocator::isEnabled(productHelper)) {
        auto &poolAllocator = device->getNEODevice()->getCommandBufferPoolAllocator();
        buffers[BufferAllocation::first] = poolAllocator.allocate(alignedSize);
        buffers[BufferAllocation::second] = poolAllocator.allocate(alignedSize);
    }

    NEO::AllocationProperties properties{device->getRootDeviceIndex(), true, alignedSize,
                                         NEO::AllocationType::commandBuffer,
                                         (device->getNEODevice()->getNumGenericSubDevices() > 1u) /* multiOsContextCapable */,
                                         false,
                                         device->getNEODevice()->getDeviceBitfield()};

    if (!buffers[BufferAllocation::first]) {
        buffers[BufferAllocation::first] = device->obtainReusableAllocation(alignedSize, NEO::AllocationType::commandBuffer);
        if (!buffers[BufferAllocation::first]) {
            buffers[BufferAllocation::first] = device->getNEODevice()->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
        }
    }

    if (!buffers[BufferAllocation::second]) {
        buffers[BufferAllocation::second] = device->obtainReusableAllocation(alignedSize, NEO::AllocationType::commandBuffer);
        if (!buffers[BufferAllocation::second]) {
            buffers[BufferAllocation::second] = device->getNEODevice()->getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
        }
    }

    if (!buffers[BufferAllocation::first] || !buffers[BufferAllocation::second]) {
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    flushId[BufferAllocation::first] = std::make_pair(0u, 0u);
    flushId[BufferAllocation::second] = std::make_pair(0u, 0u);

    return ZE_RESULT_SUCCESS;
}

void CommandBufferManager::destroy(Device *device) {
    auto &poolAllocator = device->getNEODevice()->getCommandBufferPoolAllocator();

    if (auto firstBA = buffers[BufferAllocation::first]) {
        if (firstBA->getParentAllocation() &&
            poolAllocator.isPoolBuffer(firstBA->getParentAllocation())) {
            DEBUG_BREAK_IF(!firstBA->isView());
            poolAllocator.free(firstBA);
        } else {
            device->storeReusableAllocation(*firstBA);
        }
        buffers[BufferAllocation::first] = nullptr;
    }

    if (auto secondBA = buffers[BufferAllocation::second]) {
        if (secondBA->getParentAllocation() &&
            poolAllocator.isPoolBuffer(secondBA->getParentAllocation())) {
            DEBUG_BREAK_IF(!secondBA->isView());
            poolAllocator.free(secondBA);
        } else {
            device->storeReusableAllocation(*secondBA);
        }
        buffers[BufferAllocation::second] = nullptr;
    }
}

NEO::WaitStatus CommandBufferManager::switchBuffers(NEO::CommandStreamReceiver *csr) {
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

void CommandQueue::handleIndirectAllocationResidency(UnifiedMemoryControls unifiedMemoryControls, std::unique_lock<std::mutex> &lockForIndirect, bool performMigration) {
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

void CommandQueue::makeResidentAndMigrate(bool performMigration, const NEO::ResidencyContainer &residencyContainer) {
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

ze_result_t CommandQueue::getOrdinal(uint32_t *pOrdinal) {
    *pOrdinal = desc.ordinal;
    return ZE_RESULT_SUCCESS;
}

ze_result_t CommandQueue::getIndex(uint32_t *pIndex) {
    *pIndex = desc.index;
    return ZE_RESULT_SUCCESS;
}

ze_result_t CommandQueue::getFlags(ze_command_queue_flags_t *pFlags) {
    *pFlags = desc.flags;
    return ZE_RESULT_SUCCESS;
}

ze_result_t CommandQueue::getMode(ze_command_queue_mode_t *pMode) {
    *pMode = desc.mode;
    return ZE_RESULT_SUCCESS;
}

ze_result_t CommandQueue::getPriority(ze_command_queue_priority_t *pPriority) {
    *pPriority = desc.priority;
    return ZE_RESULT_SUCCESS;
}

QueueProperties CommandQueue::extractQueueProperties(const ze_command_queue_desc_t &desc) {
    QueueProperties queueProperties = {};

    auto baseProperties = static_cast<const ze_base_desc_t *>(desc.pNext);

    if ((desc.flags & ZE_COMMAND_QUEUE_FLAG_COPY_OFFLOAD_HINT) == ZE_COMMAND_QUEUE_FLAG_COPY_OFFLOAD_HINT) {
        queueProperties.copyOffloadHint = true;
    }

    while (baseProperties) {
        if (auto syncDispatchMode = getSyncDispatchMode(baseProperties)) {
            if (syncDispatchMode.has_value()) {
                queueProperties.synchronizedDispatchMode = syncDispatchMode.value();
            }
        } else if (static_cast<uint32_t>(baseProperties->stype) == ZEX_INTEL_STRUCTURE_TYPE_QUEUE_COPY_OPERATIONS_OFFLOAD_HINT_EXP_PROPERTIES) {
            queueProperties.copyOffloadHint = static_cast<const zex_intel_queue_copy_operations_offload_hint_exp_desc_t *>(desc.pNext)->copyOffloadEnabled;
        } else if (static_cast<uint32_t>(baseProperties->stype) == ZE_STRUCTURE_TYPE_QUEUE_PRIORITY_DESC) {
            queueProperties.priorityLevel = static_cast<const ze_queue_priority_desc_t *>(desc.pNext)->priority;
        } else if (static_cast<uint32_t>(baseProperties->stype) == static_cast<uint32_t>(ZE_STRUCTURE_TYPE_QUEUE_THROTTLE_EXT_DESC)) {
            auto throttleDesc = reinterpret_cast<const ze_queue_throttle_ext_desc_t *>(baseProperties);
            queueProperties.throttle = throttleDesc->throttle;
        }

        baseProperties = static_cast<const ze_base_desc_t *>(baseProperties->pNext);
    }

    return queueProperties;
}

void CommandQueue::makeResidentForResidencyContainer(const NEO::ResidencyContainer &residencyContainer) {
    for (auto alloc : residencyContainer) {
        alloc->prepareHostPtrForResidency(csr);
        csr->makeResident(*alloc);
    }
}

} // namespace L0

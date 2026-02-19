/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist/cmdlist.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/cpu_copy_helper.h"
#include "shared/source/helpers/engine_control.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/prefetch_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/utilities/tag_allocator.h"

#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/device/bcs_split.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/source/kernel/kernel_imp.h"
#include "level_zero/core/source/module/module_imp.h"
#include "level_zero/experimental/source/graph/graph.h"
#include "level_zero/tools/source/metrics/metric.h"

namespace L0 {

CommandList::CommandList(uint32_t numIddsPerBlock) : commandContainer(numIddsPerBlock) {}

CommandList::~CommandList() {
    if (cmdQImmediate) {
        cmdQImmediate->destroy();
    }
    if (cmdQImmediateCopyOffload) {
        cmdQImmediateCopyOffload->destroy();
    }
    removeDeallocationContainerData();
    if (!isImmediateType()) {
        removeHostPtrAllocations();
    }
    removeMemoryPrefetchAllocations();
    printfKernelContainer.clear();
    if (captureTarget && (false == captureTarget->wasPreallocated())) {
        delete captureTarget;
    }
}

void CommandList::storePrintfKernel(Kernel *kernel) {
    auto it = std::find_if(this->printfKernelContainer.begin(), this->printfKernelContainer.end(), [&kernel](const auto &kernelWeakPtr) { return kernelWeakPtr.lock().get() == kernel; });

    if (it == this->printfKernelContainer.end()) {
        auto module = static_cast<const ModuleImp *>(&static_cast<KernelImp *>(kernel)->getParentModule());
        this->printfKernelContainer.push_back(module->getPrintfKernelWeakPtr(kernel->toHandle()));
    }
}

void CommandList::removeHostPtrAllocations() {
    auto memoryManager = device ? device->getNEODevice()->getMemoryManager() : nullptr;

    bool restartDirectSubmission = !this->hostPtrMap.empty() && memoryManager && device->getNEODevice()->getRootDeviceEnvironment().getProductHelper().restartDirectSubmissionForHostptrFree();
    if (restartDirectSubmission) {
        const auto &engines = memoryManager->getRegisteredEngines(device->getRootDeviceIndex());
        for (const auto &engine : engines) {
            engine.commandStreamReceiver->stopDirectSubmission(false, true);
        }
    }

    for (auto &allocation : hostPtrMap) {
        UNRECOVERABLE_IF(memoryManager == nullptr);
        memoryManager->freeGraphicsMemory(allocation.second);
    }

    if (restartDirectSubmission) {
        const auto &engines = memoryManager->getRegisteredEngines(device->getRootDeviceIndex());
        for (const auto &engine : engines) {
            if (engine.commandStreamReceiver->isAnyDirectSubmissionEnabled()) {
                engine.commandStreamReceiver->flushTagUpdate();
            }
        }
    }

    hostPtrMap.clear();
}

void CommandList::removeMemoryPrefetchAllocations() {
    if (this->performMemoryPrefetch) {
        auto prefetchManager = this->device->getDriverHandle()->getMemoryManager()->getPrefetchManager();
        if (prefetchManager) {
            prefetchManager->removeAllocations(prefetchContext);
        }
        performMemoryPrefetch = false;
    }
}

void CommandList::storeFillPatternResourcesForReuse() {
    for (auto &patternAlloc : this->patternAllocations) {
        device->storeReusableAllocation(*patternAlloc);
    }
    this->patternAllocations.clear();
    for (auto &patternTag : this->patternTags) {
        patternTag->returnTag();
    }
    this->patternTags.clear();
}

NEO::GraphicsAllocation *CommandList::getAllocationFromHostPtrMap(const void *buffer, uint64_t bufferSize, bool copyOffload) {
    auto allocation = hostPtrMap.lower_bound(buffer);
    if (allocation != hostPtrMap.end()) {
        if (buffer == allocation->first && ptrOffset(allocation->first, allocation->second->getUnderlyingBufferSize()) >= ptrOffset(buffer, bufferSize)) {
            return allocation->second;
        }
    }
    if (allocation != hostPtrMap.begin()) {
        allocation--;
        if (ptrOffset(allocation->first, allocation->second->getUnderlyingBufferSize()) >= ptrOffset(buffer, bufferSize)) {
            return allocation->second;
        }
    }
    if (isImmediateType()) {
        auto csr = getCsr(copyOffload);
        auto allocation = csr->getInternalAllocationStorage()->obtainTemporaryAllocationWithPtr(bufferSize, buffer, NEO::AllocationType::externalHostPtr);
        if (allocation != nullptr) {
            auto alloc = allocation.get();
            alloc->incrementHostPtrTaskCountAssignment();
            csr->getInternalAllocationStorage()->storeAllocationWithTaskCount(std::move(allocation), NEO::AllocationUsage::TEMPORARY_ALLOCATION, csr->peekTaskCount());
            return alloc;
        }
    }
    return nullptr;
}

NEO::GraphicsAllocation *CommandList::getHostPtrAlloc(const void *buffer, uint64_t bufferSize, bool hostCopyAllowed, bool copyOffload) {
    NEO::GraphicsAllocation *alloc = getAllocationFromHostPtrMap(buffer, bufferSize, copyOffload);
    if (alloc) {
        return alloc;
    }
    alloc = device->allocateMemoryFromHostPtr(buffer, bufferSize, hostCopyAllowed);
    if (alloc == nullptr) {
        return nullptr;
    }
    if (isImmediateType()) {
        alloc->incrementHostPtrTaskCountAssignment();
        auto csr = getCsr(copyOffload);
        csr->getInternalAllocationStorage()->storeAllocationWithTaskCount(std::unique_ptr<NEO::GraphicsAllocation>(alloc), NEO::AllocationUsage::TEMPORARY_ALLOCATION, csr->peekTaskCount());
    } else if (alloc->getAllocationType() == NEO::AllocationType::externalHostPtr) {
        hostPtrMap.insert(std::make_pair(buffer, alloc));
    } else {
        commandContainer.getDeallocationContainer().push_back(alloc);
    }
    return alloc;
}

void CommandList::removeDeallocationContainerData() {
    auto memoryManager = device ? device->getNEODevice()->getMemoryManager() : nullptr;

    auto container = commandContainer.getDeallocationContainer();
    for (auto &deallocation : container) {
        DEBUG_BREAK_IF(deallocation == nullptr);
        UNRECOVERABLE_IF(memoryManager == nullptr);
        NEO::SvmAllocationData *allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(reinterpret_cast<void *>(deallocation->getGpuAddress()));
        if (allocData) {
            device->getDriverHandle()->getSvmAllocsManager()->removeSVMAlloc(*allocData);
        }
        if (!((deallocation->getAllocationType() == NEO::AllocationType::internalHeap) ||
              (deallocation->getAllocationType() == NEO::AllocationType::linearStream))) {
            memoryManager->freeGraphicsMemory(deallocation);
            eraseDeallocationContainerEntry(deallocation);
        }
    }
}

void CommandList::eraseDeallocationContainerEntry(NEO::GraphicsAllocation *allocation) {
    std::vector<NEO::GraphicsAllocation *>::iterator allocErase;
    auto container = &commandContainer.getDeallocationContainer();

    allocErase = std::find(container->begin(), container->end(), allocation);
    if (allocErase != container->end()) {
        container->erase(allocErase);
    }
}

void CommandList::eraseResidencyContainerEntry(NEO::GraphicsAllocation *allocation) {
    std::vector<NEO::GraphicsAllocation *>::iterator allocErase;
    auto container = &commandContainer.getResidencyContainer();

    allocErase = std::find(container->begin(), container->end(), allocation);
    if (allocErase != container->end()) {
        container->erase(allocErase);
    }
}

void CommandList::migrateSharedAllocations() {
    auto driverHandle = device->getDriverHandle();
    std::lock_guard<std::mutex> lock(driverHandle->sharedMakeResidentAllocationsLock);
    auto pageFaultManager = driverHandle->getMemoryManager()->getPageFaultManager();
    for (auto &alloc : driverHandle->sharedMakeResidentAllocations) {
        pageFaultManager->moveAllocationToGpuDomain(reinterpret_cast<void *>(alloc.second->getGpuAddress()));
    }
    if (this->unifiedMemoryControls.indirectSharedAllocationsAllowed) {
        auto pageFaultManager = device->getDriverHandle()->getMemoryManager()->getPageFaultManager();
        pageFaultManager->moveAllocationsWithinUMAllocsManagerToGpuDomain(this->device->getDriverHandle()->getSvmAllocsManager());
    }
}

bool CommandList::isTimestampEventForMultiTile(Event *signalEvent) {
    if (this->partitionCount > 1 && signalEvent && signalEvent->isEventTimestampFlagSet()) {
        return true;
    }

    return false;
}

bool CommandList::setupTimestampEventForMultiTile(Event *signalEvent) {
    if (isTimestampEventForMultiTile(signalEvent)) {
        signalEvent->setPacketsInUse(this->partitionCount);
        return true;
    }
    return false;
}

void CommandList::synchronizeEventList(uint32_t numWaitEvents, ze_event_handle_t *waitEventList) {
    for (uint32_t i = 0; i < numWaitEvents; i++) {
        Event *event = Event::fromHandle(waitEventList[i]);
        event->hostSynchronize(std::numeric_limits<uint64_t>::max());
    }
}

NEO::CommandStreamReceiver *CommandList::getCsr(bool copyOffload) const {
    auto queue = isDualStreamCopyOffloadOperation(copyOffload) ? this->cmdQImmediateCopyOffload : this->cmdQImmediate;

    return queue->getCsr();
}

void CommandList::registerWalkerWithProfilingEnqueued(Event *event) {
    if (this->shouldRegisterEnqueuedWalkerWithProfiling && event && event->isEventTimestampFlagSet()) {
        this->isWalkerWithProfilingEnqueued = true;
    }
}

ze_result_t CommandList::setKernelState(Kernel *kernel, const ze_group_size_t groupSizes, void **arguments) {
    if (kernel == nullptr) {
        return ZE_RESULT_ERROR_INVALID_NULL_HANDLE;
    }

    auto result = kernel->setGroupSize(groupSizes.groupSizeX, groupSizes.groupSizeY, groupSizes.groupSizeZ);

    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    auto &args = kernel->getImmutableData()->getDescriptor().payloadMappings.explicitArgs;

    if (args.size() > 0 && !arguments) {
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    auto lock = static_cast<KernelImp *>(kernel)->getParentModule().getDevice()->getDriverHandle()->getSvmAllocsManager()->obtainReadContainerLock();

    for (auto i = 0u; i < args.size(); i++) {

        auto &arg = args[i];
        auto argSize = sizeof(void *);
        auto argValue = arguments[i];

        switch (arg.type) {
        case NEO::ArgDescriptor::argTPointer:
            if (arg.getTraits().getAddressQualifier() == NEO::KernelArgMetadata::AddrLocal) {
                argSize = *reinterpret_cast<const size_t *>(argValue);
                argValue = nullptr;
            }
            break;
        case NEO::ArgDescriptor::argTValue:
            argSize = std::numeric_limits<size_t>::max();
            break;
        default:
            break;
        }
        result = kernel->setArgumentValue(i, argSize, argValue);
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }
    }
    return ZE_RESULT_SUCCESS;
}

uint32_t CommandList::getLimitIsaPrefetchSize() {
    constexpr size_t defaultLimitValue = MemoryConstants::kiloByte;

    uint32_t retrievedLimitValue = NEO::debugManager.flags.LimitIsaPrefetchSize.getIfNotDefault(static_cast<uint32_t>(defaultLimitValue));
    return retrievedLimitValue;
}

void CommandList::executeCleanupCallbacks() {
    std::vector<CleanupCallbackT> callbacksToExecute;
    callbacksToExecute.swap(this->cleanupCallbacks);

    for (auto &callback : callbacksToExecute) {
        callback.first(callback.second);
    }
}

bool CommandList::verifyMemory(const void *allocationPtr,
                               const void *expectedData,
                               size_t sizeOfComparison,
                               uint32_t comparisonMode) const {
    return getCsr(false)->expectMemory(allocationPtr, expectedData, sizeOfComparison, comparisonMode);
}

void CommandList::setPatchingPreamble(bool patching) {
    if (isImmediateType()) {
        cmdQImmediate->setPatchingPreamble(patching);
    }
}

void CommandList::setupPatchPreambleEnabled() {
    int32_t enableRegularCmdListPatchPreamble = NEO::debugManager.flags.ForceEnableRegularCmdListPatchPreamble.get();
    if (enableRegularCmdListPatchPreamble != -1) {
        this->patchPreambleEnabled = !!(enableRegularCmdListPatchPreamble);
    }
}

CommandListAllocatorFn commandListFactory[NEO::maxProductEnumValue] = {};
CommandListAllocatorFn commandListFactoryImmediate[NEO::maxProductEnumValue] = {};

ze_result_t CommandList::destroy() {
    if (this->isBcsSplitEnabled()) {
        destroyRecordedBcsSplitResources();
        this->device->bcsSplit->releaseResources();
    }

    if (this->cmdQImmediate && !this->isSyncModeQueue) {
        this->hostSynchronize(std::numeric_limits<uint64_t>::max());
    }

    if (!isImmediateType() &&
        !isCopyOnly(false) &&
        this->stateBaseAddressTracking &&
        this->cmdListHeapAddressModel == NEO::HeapAddressModel::privateHeaps) {

        auto surfaceStateHeap = this->commandContainer.getIndirectHeap(NEO::HeapType::surfaceState);
        if (surfaceStateHeap) {
            auto heapAllocation = surfaceStateHeap->getGraphicsAllocation();

            auto rootDeviceIndex = device->getRootDeviceIndex();
            auto &deviceEngines = device->getNEODevice()->getMemoryManager()->getRegisteredEngines(rootDeviceIndex);
            for (auto &engine : deviceEngines) {
                if (NEO::EngineHelpers::isComputeEngine(engine.getEngineType())) {
                    auto contextId = engine.osContext->getContextId();
                    if (heapAllocation->isUsedByOsContext(contextId) && engine.osContext->isInitialized() && heapAllocation->getTaskCount(contextId) > 0) {
                        engine.commandStreamReceiver->sendRenderStateCacheFlush();
                    }
                }
            }
        }
    }

    executeCleanupCallbacks();

    delete this;
    return ZE_RESULT_SUCCESS;
}

ze_result_t CommandList::appendMetricMemoryBarrier() {
    return device->getMetricDeviceContext().appendMetricMemoryBarrier(*this);
}

ze_result_t CommandList::appendMetricStreamerMarker(zet_metric_streamer_handle_t hMetricStreamer,
                                                    uint32_t value) {
    return MetricStreamer::fromHandle(hMetricStreamer)->appendStreamerMarker(*this, value);
}

ze_result_t CommandList::appendMetricQueryBegin(zet_metric_query_handle_t hMetricQuery) {
    if (isImmediateType()) {
        this->device->activateMetricGroups();
    }

    return MetricQuery::fromHandle(hMetricQuery)->appendBegin(*this);
}

ze_result_t CommandList::appendMetricQueryEnd(zet_metric_query_handle_t hMetricQuery, ze_event_handle_t hSignalEvent,
                                              uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {
    return MetricQuery::fromHandle(hMetricQuery)->appendEnd(*this, hSignalEvent, numWaitEvents, phWaitEvents);
}

CommandList *CommandList::create(uint32_t productFamily, Device *device, NEO::EngineGroupType engineGroupType,
                                 ze_command_list_flags_t flags, ze_result_t &returnValue,
                                 bool internalUsage) {
    CommandListAllocatorFn allocator = nullptr;
    if (productFamily < NEO::maxProductEnumValue) {
        allocator = commandListFactory[productFamily];
    }

    CommandList *commandList = nullptr;
    returnValue = ZE_RESULT_ERROR_UNINITIALIZED;

    if (allocator) {
        commandList = (*allocator)(CommandList::defaultNumIddsPerBlock);
        commandList->internalUsage = internalUsage;
        returnValue = commandList->initialize(device, engineGroupType, flags);
        if (returnValue != ZE_RESULT_SUCCESS) {
            commandList->destroy();
            commandList = nullptr;
        }
    }

    return commandList;
}

ze_result_t CommandList::getDeviceHandle(ze_device_handle_t *phDevice) {
    *phDevice = getDevice()->toHandle();
    return ZE_RESULT_SUCCESS;
}

ze_result_t CommandList::getContextHandle(ze_context_handle_t *phContext) {
    *phContext = getCmdListContext();
    return ZE_RESULT_SUCCESS;
}

uint32_t CommandList::getOrdinal() const {
    UNRECOVERABLE_IF(ordinal == std::nullopt);
    return ordinal.value();
}

ze_result_t CommandList::getImmediateIndex(uint32_t *pIndex) {
    if (isImmediateType()) {
        return cmdQImmediate->getIndex(pIndex);
    }
    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
}

ze_result_t CommandList::isImmediate(ze_bool_t *pIsImmediate) {
    *pIsImmediate = isImmediateType();
    return ZE_RESULT_SUCCESS;
}

CommandList *CommandList::createImmediate(uint32_t productFamily, Device *device,
                                          const ze_command_queue_desc_t *desc,
                                          bool internalUsage, NEO::EngineGroupType engineGroupType,
                                          ze_result_t &returnValue) {
    return createImmediate(productFamily, device, desc, internalUsage, engineGroupType, nullptr, returnValue);
}

CommandList *CommandList::createImmediate(uint32_t productFamily, Device *device,
                                          const ze_command_queue_desc_t *desc,
                                          bool internalUsage, NEO::EngineGroupType engineGroupType, NEO::CommandStreamReceiver *csr,
                                          ze_result_t &returnValue) {

    ze_command_queue_desc_t cmdQdesc = *desc;

    int32_t overrideImmediateCmdListSyncMode = NEO::debugManager.flags.OverrideImmediateCmdListSynchronousMode.get();
    if (overrideImmediateCmdListSyncMode != -1) {
        cmdQdesc.mode = static_cast<ze_command_queue_mode_t>(overrideImmediateCmdListSyncMode);
    }
    CommandListAllocatorFn allocator = nullptr;
    if (productFamily < NEO::maxProductEnumValue) {
        allocator = commandListFactoryImmediate[productFamily];
    }

    CommandList *commandList = nullptr;
    returnValue = ZE_RESULT_ERROR_UNINITIALIZED;

    if (!allocator) {
        return nullptr;
    }

    auto queueProperties = CommandQueue::extractQueueProperties(cmdQdesc);

    const auto &hwInfo = device->getHwInfo();
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto &productHelper = device->getProductHelper();

    if (!csr) {
        if (internalUsage) {
            if (NEO::EngineHelper::isCopyOnlyEngineType(engineGroupType) && device->getActiveDevice()->getInternalCopyEngine()) {
                csr = device->getActiveDevice()->getInternalCopyEngine()->commandStreamReceiver;
            } else {
                auto internalEngine = device->getActiveDevice()->getInternalEngine();
                csr = internalEngine.commandStreamReceiver;
                engineGroupType = device->getInternalEngineGroupType();
            }
        } else {
            if (queueProperties.interruptHint && !productHelper.isInterruptSupported()) {
                returnValue = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
                return commandList;
            }

            returnValue = device->getCsrForOrdinalAndIndex(&csr, cmdQdesc.ordinal, cmdQdesc.index, cmdQdesc.priority, queueProperties.priorityLevel, queueProperties.interruptHint);
            if (returnValue != ZE_RESULT_SUCCESS) {
                return commandList;
            }
        }
    }

    UNRECOVERABLE_IF(nullptr == csr);

    commandList = (*allocator)(CommandList::commandListimmediateIddsPerBlock);
    commandList->internalUsage = internalUsage;
    commandList->cmdListType = CommandListType::typeImmediate;
    commandList->isSyncModeQueue = (cmdQdesc.mode == ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS);
    if (NEO::debugManager.flags.MakeEachEnqueueBlocking.get()) {
        commandList->isSyncModeQueue |= true;
    }

    if (!internalUsage) {
        auto &rootDeviceEnvironment = device->getNEODevice()->getRootDeviceEnvironment();
        bool enabledCmdListSharing = !NEO::EngineHelper::isCopyOnlyEngineType(engineGroupType);
        commandList->immediateCmdListHeapSharing = L0GfxCoreHelper::enableImmediateCmdListHeapSharing(rootDeviceEnvironment, enabledCmdListSharing);
    }
    csr->initializeResources(false, device->getDevicePreemptionMode());
    csr->initDirectSubmission();

    auto commandQueue = CommandQueue::create(productFamily, device, csr, &cmdQdesc, NEO::EngineHelper::isCopyOnlyEngineType(engineGroupType), internalUsage, true, returnValue);
    if (!commandQueue) {
        commandList->destroy();
        commandList = nullptr;
        return commandList;
    }

    commandList->cmdQImmediate = commandQueue;

    returnValue = commandList->initialize(device, engineGroupType, 0);

    if ((cmdQdesc.flags & ZE_COMMAND_QUEUE_FLAG_IN_ORDER) || (NEO::debugManager.flags.ForceInOrderImmediateCmdListExecution.get() == 1)) {
        commandList->enableInOrderExecution();
        commandList->flags |= ZE_COMMAND_LIST_FLAG_IN_ORDER;
    }

    if (queueProperties.synchronizedDispatchMode != NEO::SynchronizedDispatchMode::disabled) {
        if (commandList->isInOrderExecutionEnabled()) {
            commandList->enableSynchronizedDispatch(queueProperties.synchronizedDispatchMode);
        } else {
            returnValue = ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
    }

    if (returnValue != ZE_RESULT_SUCCESS) {
        commandList->destroy();
        commandList = nullptr;
        return commandList;
    }

    commandList->isTbxMode = csr->isTbxMode();
    commandList->commandListPreemptionMode = device->getDevicePreemptionMode();

    commandList->copyThroughLockedPtrEnabled = gfxCoreHelper.copyThroughLockedPtrEnabled(hwInfo, productHelper);
    commandList->isSmallBarConfigPresent = NEO::isSmallBarConfigPresent(device->getOsInterface());
    auto isBcsPreferredForCopyOffload = NEO::debugManager.flags.EnableBlitterForEnqueueOperations.getIfNotDefault(productHelper.blitEnqueuePreferred(false));
    const bool cmdListSupportsCopyOffload = commandList->isInOrderExecutionEnabled() && !gfxCoreHelper.crossEngineCacheFlushRequired() && isBcsPreferredForCopyOffload;

    if ((NEO::debugManager.flags.ForceCopyOperationOffloadForComputeCmdList.get() == 1 || queueProperties.copyOffloadHint) && cmdListSupportsCopyOffload) {
        commandList->enableCopyOperationOffload();
    }

    commandList->enableBcsSplit();

    return commandList;
}

void CommandList::enableBcsSplit() {
    if (device->getNEODevice()->isBcsSplitSupported() && !internalUsage && !isBcsSplitEnabled()) {
        if (isImmediateType()) {
            this->bcsSplitMode = getDevice()->bcsSplit->setupDevice(getCsr(false), isCopyOffloadEnabled()) ? BcsSplitParams::BcsSplitMode::immediate : BcsSplitParams::BcsSplitMode::disabled;
        } else if (isInOrderExecutionEnabled() && device->getProductHelper().useAdditionalBlitProperties() && (NEO::debugManager.flags.SplitBcsForRegularCmdList.get() != 0)) {
            auto csr = getDevice()->getNEODevice()->getDefaultEngine().commandStreamReceiver;
            auto isCopyEnabled = isCopyOnly(true);
            bcsSplitMode = getDevice()->bcsSplit->setupDevice(csr, isCopyEnabled) ? BcsSplitParams::BcsSplitMode::recorded : BcsSplitParams::BcsSplitMode::disabled;
        }
    }
}

void CommandList::enableCopyOperationOffload() {
    if (isCopyOnly(false) || !device->tryGetCopyEngineOrdinal().has_value()) {
        return;
    }

    this->copyOffloadMode = device->getL0GfxCoreHelper().getDefaultCopyOffloadMode(device->getProductHelper().useAdditionalBlitProperties());

    if (this->copyOffloadMode != CopyOffloadModes::dualStream || !isImmediateType()) {
        // No need to create internal bcs queue
        return;
    }

    auto &computeOsContext = getCsr(false)->getOsContext();

    ze_command_queue_priority_t immediateQueuePriority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    if (computeOsContext.isHighPriority()) {
        immediateQueuePriority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_HIGH;
    } else if (computeOsContext.isLowPriority()) {
        immediateQueuePriority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW;
    }

    ze_command_queue_mode_t immediateQueueMode = this->isSyncModeQueue ? ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS : ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    NEO::CommandStreamReceiver *copyCsr = nullptr;
    uint32_t ordinal = device->getCopyEngineOrdinal();

    device->getCsrForOrdinalAndIndex(&copyCsr, ordinal, 0, immediateQueuePriority, std::nullopt, false);
    UNRECOVERABLE_IF(!copyCsr);

    ze_command_queue_desc_t copyQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    copyQueueDesc.ordinal = ordinal;
    copyQueueDesc.mode = immediateQueueMode;
    copyQueueDesc.priority = immediateQueuePriority;

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto offloadCommandQueue = CommandQueue::create(device->getHwInfo().platform.eProductFamily, device, copyCsr, &copyQueueDesc, true, false, true, returnValue);
    UNRECOVERABLE_IF(!offloadCommandQueue);

    this->cmdQImmediateCopyOffload = offloadCommandQueue;
}

void CommandList::setStreamPropertiesDefaultSettings(NEO::StreamProperties &streamProperties) {
    if (this->stateComputeModeTracking) {
        streamProperties.stateComputeMode.setPropertiesPerContext(cmdListDefaultCoherency, this->commandListPreemptionMode, true, std::nullopt);
    }

    streamProperties.frontEndState.setPropertiesDisableOverdispatch(cmdListDefaultDisableOverdispatch, true);
    streamProperties.pipelineSelect.setPropertiesModeSelected(cmdListDefaultPipelineSelectModeSelected, true);
}

void CommandList::enableInOrderExecution() {
    UNRECOVERABLE_IF(inOrderExecInfo.get());

    auto deviceCounterNode = this->device->getDeviceInOrderCounterAllocator()->getTag();
    NEO::TagNodeBase *hostCounterNode = nullptr;

    auto &gfxCoreHelper = device->getGfxCoreHelper();

    if (gfxCoreHelper.duplicatedInOrderCounterStorageEnabled(device->getNEODevice()->getRootDeviceEnvironment())) {
        hostCounterNode = this->device->getHostInOrderCounterAllocator()->getTag();
    }

    inOrderExecInfo = NEO::InOrderExecInfo::create(deviceCounterNode, hostCounterNode, *this->device->getNEODevice(), this->partitionCount, !isImmediateType());
    if (isImmediateType()) {
        inOrderExecInfo->setSimulationUploadCsr(getCsr(false));
        inOrderExecInfo->uploadAllocationsToSimulation();
    }
}

void CommandList::storeReferenceTsToMappedEvents(bool isClearEnabled) {
    if (mappedTsEventList.size()) {
        uint64_t currentCpuTimeStamp = 0;
        device->getNEODevice()->getOSTime()->getCpuTime(&currentCpuTimeStamp);
        for (auto &event : mappedTsEventList) {
            event->setReferenceTs(currentCpuTimeStamp);
        }

        if (isClearEnabled) {
            mappedTsEventList.clear();
        }
    }
}

void CommandList::addToMappedEventList(Event *event) {
    if (event && event->hasKernelMappedTsCapability) {
        if (std::find(mappedTsEventList.begin(), mappedTsEventList.end(), event) == mappedTsEventList.end()) {
            mappedTsEventList.push_back(event);
        }
    }
}

void CommandList::addRegularCmdListSubmissionCounter() {
    if (isInOrderExecutionEnabled()) {
        inOrderExecInfo->addRegularCmdListSubmissionCounter(1);
    }
}

bool CommandList::inOrderCmdsPatchingEnabled() const {
    return (!isImmediateType() && NEO::debugManager.flags.EnableInOrderRegularCmdListPatching.get() == 1);
}

void CommandList::clearInOrderExecCounterAllocation() {
    if (isInOrderExecutionEnabled()) {
        inOrderExecInfo->initializeAllocationsFromHost();
    }
}

size_t CommandList::getInOrderExecDeviceRequiredSize() const {
    size_t size = 0;
    if (isInOrderExecutionEnabled()) {
        size = inOrderExecInfo->getDeviceNodeWriteSize();
    }
    return size;
}

uint64_t CommandList::getInOrderExecDeviceGpuAddress() const {
    uint64_t gpuAddress = 0;
    if (isInOrderExecutionEnabled()) {
        gpuAddress = inOrderExecInfo->getDeviceNodeGpuAddress();
    }
    return gpuAddress;
}

size_t CommandList::getInOrderExecHostRequiredSize() const {
    size_t size = 0;
    if (isInOrderExecutionEnabled()) {
        size = inOrderExecInfo->getHostNodeWriteSize();
    }
    return size;
}

uint64_t CommandList::getInOrderExecHostGpuAddress() const {
    uint64_t gpuAddress = 0;
    if (isInOrderExecutionEnabled()) {
        gpuAddress = inOrderExecInfo->getHostNodeGpuAddress();
    }
    return gpuAddress;
}

void CommandList::enableSynchronizedDispatch(NEO::SynchronizedDispatchMode mode) {
    if (!device->isImplicitScalingCapable() || this->synchronizedDispatchMode != NEO::SynchronizedDispatchMode::disabled) {
        return;
    }

    this->synchronizedDispatchMode = mode;

    if (mode == NEO::SynchronizedDispatchMode::full) {
        this->syncDispatchQueueId = device->getNextSyncDispatchQueueId();
    } else if (mode == NEO::SynchronizedDispatchMode::limited) {
        // Limited mode does not acquire new token during execution. It only checks if token is already acquired by full sync dispatch.
        device->ensureSyncDispatchTokenAllocation();
    }
}

void CommandList::setInterruptEventsCsr(NEO::CommandStreamReceiver &csr) {
    for (auto &event : interruptEvents) {
        event->setCsr(&csr, true);
    }
}

void CommandList::setForSubCopyBcsSplit() {
    copyOffloadMode = CopyOffloadModes::disabled;
    bcsSplitMode = BcsSplitParams::BcsSplitMode::disabled;
}

void CommandList::resetBcsSplitEvents(bool release) {
    device->bcsSplit->events.resetAggregatedEventsStateForRecordedSubmission(this->eventsForRecordedBcsSplit, !release);

    if (release) {
        this->eventsForRecordedBcsSplit.clear();
    }
}

void CommandList::dispatchRecordedBcsSplit() {
    if (this->bcsSplitMode != BcsSplitParams::BcsSplitMode::recorded) {
        return;
    }

    resetBcsSplitEvents(false);

    device->bcsSplit->dispatchRecordedCmdLists(this->subCmdListsForRecordedBcsSplit);
}

BcsSplitParams::CmdListsForSplitContainer CommandList::getRegularCmdListsForSplit(size_t totalTransferSize, size_t perEngineMaxSize, size_t splitQueuesCount) {
    const size_t maxEnginesToUse = std::max(totalTransferSize / perEngineMaxSize, size_t(1));
    const auto engineCount = std::min(splitQueuesCount, maxEnginesToUse);

    ensureSubCmdLists(engineCount);

    return {subCmdListsForRecordedBcsSplit.begin(), subCmdListsForRecordedBcsSplit.begin() + engineCount};
}

void CommandList::ensureSubCmdLists(size_t count) {
    if (subCmdListsForRecordedBcsSplit.size() >= count) {
        return;
    }

    const auto currentSize = subCmdListsForRecordedBcsSplit.size();

    for (size_t i = currentSize; i < count; i++) {
        auto splitCmdList = device->bcsSplit->cmdLists[i];
        auto subCmdListDevice = splitCmdList->getDevice();

        ze_result_t returnValue = ZE_RESULT_SUCCESS;

        auto subCmdList = CommandList::create(device->getHwInfo().platform.eProductFamily, subCmdListDevice, splitCmdList->getEngineGroupType(), ZE_COMMAND_LIST_FLAG_IN_ORDER, returnValue, true);
        UNRECOVERABLE_IF(returnValue != ZE_RESULT_SUCCESS);

        subCmdList->forceDisableInOrderWaits();

        subCmdListsForRecordedBcsSplit.push_back(subCmdList);
    }
}

void CommandList::storeEventsForBcsSplit(const BcsSplitParams::MarkerEvent *markerEvent) {
    eventsForRecordedBcsSplit.push_back(markerEvent);
}

void CommandList::destroyRecordedBcsSplitResources() {
    for (auto &subCmdList : this->subCmdListsForRecordedBcsSplit) {
        subCmdList->destroy();
    }
    this->subCmdListsForRecordedBcsSplit.clear();
    resetBcsSplitEvents(true);
}

} // namespace L0

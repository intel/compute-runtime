/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/memory_manager.h"

#include "shared/source/ail/ail_configuration.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/blit_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string_helpers.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/allocations_list.h"
#include "shared/source/memory_manager/compression_selector.h"
#include "shared/source/memory_manager/deferrable_allocation_deletion.h"
#include "shared/source/memory_manager/deferred_deleter.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/memory_manager/host_ptr_manager.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/local_memory_usage.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/memory_manager/multi_graphics_allocation.h"
#include "shared/source/memory_manager/prefetch_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/utilities/logger_neo_only.h"

namespace NEO {
uint32_t MemoryManager::maxOsContextCount = 0u;

MemoryManager::MemoryManager(ExecutionEnvironment &executionEnvironment) : executionEnvironment(executionEnvironment), hostPtrManager(std::make_unique<HostPtrManager>()),
                                                                           multiContextResourceDestructor(std::make_unique<DeferredDeleter>()) {

    bool anyLocalMemorySupported = false;
    const auto rootEnvCount = executionEnvironment.rootDeviceEnvironments.size();

    defaultEngineIndex.resize(rootEnvCount);
    checkIsaPlacementOnceFlags = std::make_unique<std::once_flag[]>(rootEnvCount);
    isaInLocalMemory.resize(rootEnvCount);
    allRegisteredEngines.resize(rootEnvCount + 1);
    secondaryEngines.resize(rootEnvCount + 1);
    localMemAllocsSize = std::make_unique<std::atomic<size_t>[]>(rootEnvCount);
    sysMemAllocsSize.store(0u);

    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < rootEnvCount; ++rootDeviceIndex) {
        auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex];
        auto hwInfo = rootDeviceEnvironment.getHardwareInfo();
        auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
        internalLocalMemoryUsageBankSelector.emplace_back(new LocalMemoryUsageBankSelector(GfxCoreHelper::getSubDevicesCount(hwInfo)));
        externalLocalMemoryUsageBankSelector.emplace_back(new LocalMemoryUsageBankSelector(GfxCoreHelper::getSubDevicesCount(hwInfo)));
        this->localMemorySupported.push_back(gfxCoreHelper.getEnableLocalMemory(*hwInfo));
        this->enable64kbpages.push_back(OSInterface::osEnabled64kbPages && !!debugManager.flags.Enable64kbpages.get());

        gfxPartitions.push_back(std::make_unique<GfxPartition>(reservedCpuAddressRange));

        anyLocalMemorySupported |= this->localMemorySupported[rootDeviceIndex];

        auto globalHeap = ApiSpecificConfig::getGlobalBindlessHeapConfiguration(rootDeviceEnvironment.getReleaseHelper());
        heapAssigners.push_back(std::make_unique<HeapAssigner>(globalHeap));
        localMemAllocsSize[rootDeviceIndex].store(0u);
    }

    if (anyLocalMemorySupported || debugManager.isTbxPageFaultManagerEnabled()) {
        pageFaultManager = CpuPageFaultManager::create();
        if (anyLocalMemorySupported) {
            prefetchManager = PrefetchManager::create();
        }
    }

    if (debugManager.flags.EnableMultiStorageResources.get() != -1) {
        supportsMultiStorageResources = !!debugManager.flags.EnableMultiStorageResources.get();
    }

    if (debugManager.flags.UseSingleListForTemporaryAllocations.get() != 0) {
        singleTemporaryAllocationsList = true;
        temporaryAllocations = std::make_unique<AllocationsList>(AllocationUsage::TEMPORARY_ALLOCATION);
    }
    lastGpuHangCheck = std::chrono::high_resolution_clock::now();
}

void MemoryManager::storeTemporaryAllocation(std::unique_ptr<GraphicsAllocation> &&gfxAllocation, uint32_t osContextId, TaskCountType taskCount) {
    gfxAllocation->updateTaskCount(taskCount, osContextId);
    temporaryAllocations->pushTailOne(*gfxAllocation.release());
}

void MemoryManager::cleanTemporaryAllocations(const CommandStreamReceiver &csr, TaskCountType waitedTaskCount) {
    auto lock = getHostPtrManager()->obtainOwnership();

    GraphicsAllocation *currentAlloc = temporaryAllocations->detachNodes();

    IDList<GraphicsAllocation, false, true> allocationsLeft;

    while (currentAlloc != nullptr) {
        const auto waitedOsContextId = csr.getOsContext().getContextId();
        auto *nextAlloc = currentAlloc->next;
        bool freeAllocation = false;

        if (currentAlloc->hostPtrTaskCountAssignment == 0) {
            if (currentAlloc->isUsedByOsContext(waitedOsContextId)) {
                if (currentAlloc->getTaskCount(waitedOsContextId) <= waitedTaskCount) {
                    if (!currentAlloc->isUsedByManyOsContexts() || !allocInUse(*currentAlloc)) {
                        freeAllocation = true;
                    }
                }
            } else if (!allocInUse(*currentAlloc)) {
                freeAllocation = true;
            }
        }

        if (freeAllocation) {
            freeGraphicsMemory(currentAlloc);
        } else {
            allocationsLeft.pushTailOne(*currentAlloc);
        }

        currentAlloc = nextAlloc;
    }

    if (!allocationsLeft.peekIsEmpty()) {
        temporaryAllocations->splice(*allocationsLeft.detachNodes());
    }
}

std::unique_ptr<GraphicsAllocation> MemoryManager::obtainTemporaryAllocationWithPtr(CommandStreamReceiver *csr, size_t requiredSize, const void *requiredPtr, AllocationType allocationType) {
    return temporaryAllocations->detachAllocation(requiredSize, requiredPtr, csr, allocationType);
}

MemoryManager::~MemoryManager() {
    for (auto &engineContainer : secondaryEngines) {
        for (auto &engine : engineContainer) {
            DEBUG_BREAK_IF(true);
            engine.osContext->decRefInternal();
        }
        engineContainer.clear();
    }
    secondaryEngines.clear();

    for (auto &engineContainer : allRegisteredEngines) {
        for (auto &engine : engineContainer) {
            engine.osContext->decRefInternal();
        }
        engineContainer.clear();
    }
    allRegisteredEngines.clear();
    if (reservedMemory) {
        MemoryManager::alignedFreeWrapper(reservedMemory);
    }
}

bool MemoryManager::isLimitedGPU(uint32_t rootDeviceIndex) {
    return peek32bit() && !peekExecutionEnvironment().rootDeviceEnvironments[rootDeviceIndex]->isFullRangeSvm();
}

bool MemoryManager::isLimitedGPUOnType(uint32_t rootDeviceIndex, AllocationType type) {
    return isLimitedGPU(rootDeviceIndex) &&
           (type != AllocationType::mapAllocation) &&
           (type != AllocationType::image);
}

void *MemoryManager::alignedMallocWrapper(size_t bytes, size_t alignment) {
    return ::alignedMalloc(bytes, alignment);
}

void MemoryManager::alignedFreeWrapper(void *ptr) {
    ::alignedFree(ptr);
}

GmmHelper *MemoryManager::getGmmHelper(uint32_t rootDeviceIndex) {
    return executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getGmmHelper();
}

AddressRange MemoryManager::reserveCpuAddressWithZeroBaseRetry(const uint64_t requiredStartAddress, size_t size) {
    auto addressRange = reserveCpuAddress(requiredStartAddress, size);
    if ((addressRange.address == 0) && (requiredStartAddress != 0)) {
        addressRange = reserveCpuAddress(0, size);
    }
    return addressRange;
}

HeapIndex MemoryManager::selectInternalHeap(bool useLocalMemory) {
    return useLocalMemory ? HeapIndex::heapInternalDeviceMemory : HeapIndex::heapInternal;
}
HeapIndex MemoryManager::selectExternalHeap(bool useLocalMemory) {
    return useLocalMemory ? HeapIndex::heapExternalDeviceMemory : HeapIndex::heapExternal;
}

inline MemoryManager::AllocationStatus MemoryManager::registerSysMemAlloc(GraphicsAllocation *allocation) {
    this->sysMemAllocsSize += allocation->getUnderlyingBufferSize();
    return AllocationStatus::Success;
}

inline MemoryManager::AllocationStatus MemoryManager::registerLocalMemAlloc(GraphicsAllocation *allocation, uint32_t rootDeviceIndex) {
    this->localMemAllocsSize[rootDeviceIndex] += allocation->getUnderlyingBufferSize();
    return AllocationStatus::Success;
}

void MemoryManager::zeroCpuMemoryIfRequested(const AllocationData &allocationData, void *cpuPtr, size_t size) {
    if (allocationData.flags.zeroMemory) {
        memset(cpuPtr, 0, size);
    }
}

void *MemoryManager::allocateSystemMemory(size_t size, size_t alignment) {
    // Establish a minimum alignment of 16bytes.
    constexpr size_t minAlignment = 16;
    alignment = std::max(alignment, minAlignment);
    auto restrictions = getAlignedMallocRestrictions();
    void *ptr = alignedMallocWrapper(size, alignment);

    if (restrictions == nullptr || restrictions->minAddress == 0) {
        return ptr;
    }

    if (restrictions->minAddress > reinterpret_cast<uintptr_t>(ptr) && ptr != nullptr) {
        StackVec<void *, 100> invalidMemVector;
        invalidMemVector.push_back(ptr);
        do {
            ptr = alignedMallocWrapper(size, alignment);
            if (restrictions->minAddress > reinterpret_cast<uintptr_t>(ptr) && ptr != nullptr) {
                invalidMemVector.push_back(ptr);
            } else {
                break;
            }
        } while (1);
        for (auto &it : invalidMemVector) {
            alignedFreeWrapper(it);
        }
    }

    return ptr;
}

GraphicsAllocation *MemoryManager::allocateGraphicsMemoryWithHostPtr(const AllocationData &allocationData) {
    if (deferredDeleter) {
        deferredDeleter->drain(true, false);
    }
    GraphicsAllocation *graphicsAllocation = nullptr;
    auto osStorage = hostPtrManager->prepareOsStorageForAllocation(*this, allocationData.size, allocationData.hostPtr, allocationData.rootDeviceIndex);
    if (osStorage.fragmentCount > 0) {
        graphicsAllocation = createGraphicsAllocation(osStorage, allocationData);
        if (graphicsAllocation == nullptr) {
            hostPtrManager->releaseHandleStorage(allocationData.rootDeviceIndex, osStorage);
            cleanOsHandles(osStorage, allocationData.rootDeviceIndex);
        }
    }
    return graphicsAllocation;
}

GraphicsAllocation *MemoryManager::allocateGraphicsMemoryForImageFromHostPtr(const AllocationData &allocationData) {
    bool copyRequired = isCopyRequired(*allocationData.imgInfo, allocationData.hostPtr);

    if (allocationData.hostPtr && !copyRequired) {
        return allocateGraphicsMemoryWithHostPtr(allocationData);
    }
    return nullptr;
}

void MemoryManager::cleanGraphicsMemoryCreatedFromHostPtr(GraphicsAllocation *graphicsAllocation) {
    hostPtrManager->releaseHandleStorage(graphicsAllocation->getRootDeviceIndex(), graphicsAllocation->fragmentsStorage);
    cleanOsHandles(graphicsAllocation->fragmentsStorage, graphicsAllocation->getRootDeviceIndex());
}

void *MemoryManager::createMultiGraphicsAllocationInSystemMemoryPool(RootDeviceIndicesContainer &rootDeviceIndices, AllocationProperties &properties, MultiGraphicsAllocation &multiGraphicsAllocation, void *ptr) {
    properties.flags.forceSystemMemory = true;
    for (auto &rootDeviceIndex : rootDeviceIndices) {
        if (multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex)) {
            continue;
        }
        properties.rootDeviceIndex = rootDeviceIndex;
        properties.flags.isUSMHostAllocation = true;

        if (isLimitedRange(properties.rootDeviceIndex)) {
            properties.flags.isUSMHostAllocation = false;
            DEBUG_BREAK_IF(rootDeviceIndices.size() > 1);
        }

        if (!ptr) {
            auto graphicsAllocation = allocateGraphicsMemoryWithProperties(properties);
            if (!graphicsAllocation) {
                return nullptr;
            }
            multiGraphicsAllocation.addAllocation(graphicsAllocation);
            ptr = reinterpret_cast<void *>(graphicsAllocation->getUnderlyingBuffer());
        } else {
            properties.flags.allocateMemory = false;

            auto graphicsAllocation = createGraphicsAllocationFromExistingStorage(properties, ptr, multiGraphicsAllocation);

            if (!graphicsAllocation) {
                for (auto &gpuAllocation : multiGraphicsAllocation.getGraphicsAllocations()) {
                    freeGraphicsMemory(gpuAllocation);
                }
                return nullptr;
            }
            multiGraphicsAllocation.addAllocation(graphicsAllocation);
        }
    }

    return ptr;
}

GraphicsAllocation *MemoryManager::createGraphicsAllocationFromExistingStorage(AllocationProperties &properties, void *ptr, MultiGraphicsAllocation &multiGraphicsAllocation) {
    return allocateGraphicsMemoryWithProperties(properties, ptr);
}

void MemoryManager::freeSystemMemory(void *ptr) {
    ::alignedFree(ptr);
}

void MemoryManager::freeGraphicsMemory(GraphicsAllocation *gfxAllocation) {
    freeGraphicsMemory(gfxAllocation, false);
}

void MemoryManager::freeGraphicsMemory(GraphicsAllocation *gfxAllocation, bool isImportedAllocation) {
    if (!gfxAllocation) {
        return;
    }
    bool rootEnvAvailable = executionEnvironment.rootDeviceEnvironments.size() > 0;
    uint32_t rootDevIdx = gfxAllocation->getRootDeviceIndex();

    if (rootEnvAvailable) {
        if (executionEnvironment.rootDeviceEnvironments[rootDevIdx]->getBindlessHeapsHelper() != nullptr) {
            executionEnvironment.rootDeviceEnvironments[rootDevIdx]->getBindlessHeapsHelper()->releaseSSToReusePool(gfxAllocation->getBindlessInfo());
        }

        if (this->peekExecutionEnvironment().rootDeviceEnvironments[rootDevIdx]->memoryOperationsInterface) {
            this->peekExecutionEnvironment().rootDeviceEnvironments[rootDevIdx]->memoryOperationsInterface->free(nullptr, *gfxAllocation);
        }
    }

    const bool hasFragments = gfxAllocation->fragmentsStorage.fragmentCount != 0;
    const bool isLocked = gfxAllocation->isLocked();
    DEBUG_BREAK_IF(hasFragments && isLocked);

    if (!hasFragments) {
        handleFenceCompletion(gfxAllocation);
    }
    if (isLocked) {
        freeAssociatedResourceImpl(*gfxAllocation);
    }
    DBG_LOG(ResidencyDebugEnable, "Residency:", __FUNCTION__, "Free allocation, gpu address = ", std::hex, gfxAllocation->getGpuAddress());

    getLocalMemoryUsageBankSelector(gfxAllocation->getAllocationType(), rootDevIdx)->freeOnBanks(gfxAllocation->storageInfo.getMemoryBanks(), gfxAllocation->getUnderlyingBufferSize());
    freeGraphicsMemoryImpl(gfxAllocation, isImportedAllocation);
}

// if not in use destroy in place
// if in use pass to temporary allocation list that is cleaned on blocking calls
void MemoryManager::checkGpuUsageAndDestroyGraphicsAllocations(GraphicsAllocation *gfxAllocation) {
    if (gfxAllocation->isUsed()) {
        if (gfxAllocation->isUsedByManyOsContexts()) {
            multiContextResourceDestructor->deferDeletion(new DeferrableAllocationDeletion{*this, *gfxAllocation});
            multiContextResourceDestructor->drain(false, false);
            return;
        }
        for (auto &engine : getRegisteredEngines(gfxAllocation->getRootDeviceIndex())) {
            auto osContextId = engine.osContext->getContextId();
            auto allocationTaskCount = gfxAllocation->getTaskCount(osContextId);
            if (gfxAllocation->isUsedByOsContext(osContextId) &&
                engine.commandStreamReceiver->getTagAllocation() != nullptr &&
                allocationTaskCount > *engine.commandStreamReceiver->getTagAddress()) {
                engine.commandStreamReceiver->getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(gfxAllocation),
                                                                                              DEFERRED_DEALLOCATION);
                return;
            }
        }
    }
    freeGraphicsMemory(gfxAllocation);
}

uint64_t MemoryManager::getInternalHeapBaseAddress(uint32_t rootDeviceIndex, bool useLocalMemory) {
    return getGfxPartition(rootDeviceIndex)->getHeapBase(selectInternalHeap(useLocalMemory));
}
uint64_t MemoryManager::getExternalHeapBaseAddress(uint32_t rootDeviceIndex, bool useLocalMemory) {
    return getGfxPartition(rootDeviceIndex)->getHeapBase(selectExternalHeap(useLocalMemory));
}

bool MemoryManager::isLimitedRange(uint32_t rootDeviceIndex) {
    return getGfxPartition(rootDeviceIndex)->isLimitedRange();
}

void MemoryManager::waitForDeletions() {
    if (deferredDeleter) {
        deferredDeleter->drain(false, false);
    }
    deferredDeleter.reset(nullptr);
}
bool MemoryManager::isAsyncDeleterEnabled() const {
    return asyncDeleterEnabled;
}

bool MemoryManager::isLocalMemorySupported(uint32_t rootDeviceIndex) const {
    return localMemorySupported[rootDeviceIndex];
}

bool MemoryManager::peek64kbPagesEnabled(uint32_t rootDeviceIndex) const {
    return enable64kbpages[rootDeviceIndex];
}

bool MemoryManager::isMemoryBudgetExhausted() const {
    return false;
}

void MemoryManager::updateLatestContextIdForRootDevice(uint32_t rootDeviceIndex) {
    // rootDeviceIndexToContextId map would contain the first entry for context for each rootDevice
    auto entry = rootDeviceIndexToContextId.insert(std::pair<uint32_t, uint32_t>(rootDeviceIndex, latestContextId));
    if (entry.second == false) {
        if (latestContextId == std::numeric_limits<uint32_t>::max()) {
            // If we are here, it means we are reinitializing the contextId.
            latestContextId = entry.first->second;
        }
    }
}

uint32_t MemoryManager::getFirstContextIdForRootDevice(uint32_t rootDeviceIndex) {
    auto entry = rootDeviceIndexToContextId.find(rootDeviceIndex);
    if (entry != rootDeviceIndexToContextId.end()) {
        return entry->second + 1;
    }
    return 0;
}

void MemoryManager::initUsmReuseLimits() {
    const auto systemSharedMemorySize = this->getSystemSharedMemory(0u);
    auto fractionOfTotalMemoryForReuse = 0.02;
    if (debugManager.flags.ExperimentalEnableHostAllocationCache.get() != -1) {
        fractionOfTotalMemoryForReuse = 0.01 * std::min(100, debugManager.flags.ExperimentalEnableHostAllocationCache.get());
    }
    auto maxAllocationsSavedForReuseSize = static_cast<uint64_t>(fractionOfTotalMemoryForReuse * systemSharedMemorySize);

    auto limitAllocationsReuseThreshold = static_cast<uint64_t>(0.8 * systemSharedMemorySize);
    const auto limitFlagValue = debugManager.flags.ExperimentalUSMAllocationReuseLimitThreshold.get();
    if (limitFlagValue != -1) {
        if (limitFlagValue == 0) {
            limitAllocationsReuseThreshold = UsmReuseInfo::notLimited;
        } else {
            const auto fractionOfTotalMemoryToLimitReuse = limitFlagValue / 100.0;
            limitAllocationsReuseThreshold = static_cast<uint64_t>(fractionOfTotalMemoryToLimitReuse * systemSharedMemorySize);
        }
    }
    this->usmReuseInfo.init(maxAllocationsSavedForReuseSize, limitAllocationsReuseThreshold);
}

OsContext *MemoryManager::createAndRegisterOsContext(CommandStreamReceiver *commandStreamReceiver,
                                                     const EngineDescriptor &engineDescriptor) {
    auto rootDeviceIndex = commandStreamReceiver->getRootDeviceIndex();
    updateLatestContextIdForRootDevice(rootDeviceIndex);

    auto contextId = ++latestContextId;
    auto osContext = OsContext::create(peekExecutionEnvironment().rootDeviceEnvironments[rootDeviceIndex]->osInterface.get(), rootDeviceIndex, contextId, engineDescriptor);
    osContext->incRefInternal();

    UNRECOVERABLE_IF(rootDeviceIndex != osContext->getRootDeviceIndex());

    allRegisteredEngines[rootDeviceIndex].emplace_back(commandStreamReceiver, osContext);

    return osContext;
}

OsContext *MemoryManager::createAndRegisterSecondaryOsContext(const OsContext *primaryContext, CommandStreamReceiver *commandStreamReceiver,
                                                              const EngineDescriptor &engineDescriptor) {
    auto rootDeviceIndex = commandStreamReceiver->getRootDeviceIndex();

    updateLatestContextIdForRootDevice(rootDeviceIndex);

    auto contextId = ++latestContextId;
    auto osContext = OsContext::create(peekExecutionEnvironment().rootDeviceEnvironments[rootDeviceIndex]->osInterface.get(), rootDeviceIndex, contextId, engineDescriptor);
    osContext->incRefInternal();

    osContext->setPrimaryContext(primaryContext);

    UNRECOVERABLE_IF(rootDeviceIndex != osContext->getRootDeviceIndex());

    secondaryEngines[rootDeviceIndex].emplace_back(commandStreamReceiver, osContext);
    allRegisteredEngines[rootDeviceIndex].emplace_back(commandStreamReceiver, osContext);

    return osContext;
}

void MemoryManager::releaseSecondaryOsContexts(uint32_t rootDeviceIndex) {

    auto &engineContainer = secondaryEngines[rootDeviceIndex];

    for (auto &engine : engineContainer) {
        engine.osContext->decRefInternal();
    }
    engineContainer.clear();
}

bool MemoryManager::getAllocationData(AllocationData &allocationData, const AllocationProperties &properties, const void *hostPtr, const StorageInfo &storageInfo) {
    UNRECOVERABLE_IF(hostPtr == nullptr && !properties.flags.allocateMemory);
    UNRECOVERABLE_IF(properties.allocationType == AllocationType::unknown);

    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[properties.rootDeviceIndex];
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto &helper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    auto &productHelper = rootDeviceEnvironment.getProductHelper();

    if (storageInfo.getMemoryBanks() == 0) {
        allocationData.flags.useSystemMemory = true;
    }

    bool allow64KbPages = false;
    bool allow32Bit = false;
    bool forcePin = properties.flags.forcePin;
    bool mayRequireL3Flush = false;

    switch (properties.allocationType) {
    case AllocationType::buffer:
    case AllocationType::bufferHostMemory:
    case AllocationType::constantSurface:
    case AllocationType::globalSurface:
    case AllocationType::printfSurface:
    case AllocationType::privateSurface:
    case AllocationType::scratchSurface:
    case AllocationType::workPartitionSurface:
    case AllocationType::writeCombined:
    case AllocationType::assertBuffer:
        allow64KbPages = true;
        allow32Bit = true;
    default:
        break;
    }

    switch (properties.allocationType) {
    case AllocationType::svmGpu:
    case AllocationType::svmZeroCopy:
    case AllocationType::gpuTimestampDeviceBuffer:
    case AllocationType::preemption:
    case AllocationType::syncDispatchToken:
        allow64KbPages = true;
    default:
        break;
    }

    switch (properties.allocationType) {
    case AllocationType::buffer:
    case AllocationType::bufferHostMemory:
    case AllocationType::writeCombined:
        forcePin = true;
    default:
        break;
    }

    switch (properties.allocationType) {
    case AllocationType::buffer:
    case AllocationType::bufferHostMemory:
    case AllocationType::externalHostPtr:
    case AllocationType::globalSurface:
    case AllocationType::image:
    case AllocationType::mapAllocation:
    case AllocationType::sharedBuffer:
    case AllocationType::sharedImage:
    case AllocationType::sharedResourceCopy:
    case AllocationType::svmCpu:
    case AllocationType::svmGpu:
    case AllocationType::svmZeroCopy:
    case AllocationType::writeCombined:
        mayRequireL3Flush = true;
    default:
        break;
    }

    switch (properties.allocationType) {
    case AllocationType::commandBuffer:
    case AllocationType::ringBuffer:
    case AllocationType::semaphoreBuffer:
    case AllocationType::bufferHostMemory:
    case AllocationType::externalHostPtr:
    case AllocationType::fillPattern:
    case AllocationType::mapAllocation:
    case AllocationType::mcs:
    case AllocationType::profilingTagBuffer:
    case AllocationType::svmCpu:
    case AllocationType::svmZeroCopy:
    case AllocationType::tagBuffer:
    case AllocationType::globalFence:
    case AllocationType::internalHostMemory:
    case AllocationType::debugContextSaveArea:
    case AllocationType::debugSbaTrackingBuffer:
    case AllocationType::swTagBuffer:
    case AllocationType::hostFunction:
        allocationData.flags.useSystemMemory = true;
    default:
        break;
    }

    if (GraphicsAllocation::isIsaAllocationType(properties.allocationType)) {
        allocationData.flags.useSystemMemory = helper.useSystemMemoryPlacementForISA(hwInfo);
    }

    switch (properties.allocationType) {
    case AllocationType::commandBuffer:
    case AllocationType::ringBuffer:
        allocationData.flags.resource48Bit = helper.is48ResourceNeededForCmdBuffer();
        break;
    case AllocationType::deferredTasksList:
    case AllocationType::image:
    case AllocationType::indirectObjectHeap:
    case AllocationType::instructionHeap:
    case AllocationType::internalHeap:
    case AllocationType::kernelIsa:
    case AllocationType::kernelIsaInternal:
    case AllocationType::linearStream:
    case AllocationType::mcs:
    case AllocationType::preemption:
    case AllocationType::scratchSurface:
    case AllocationType::workPartitionSurface:
    case AllocationType::sharedImage:
    case AllocationType::sharedResourceCopy:
    case AllocationType::surfaceStateHeap:
    case AllocationType::timestampPacketTagBuffer:
    case AllocationType::debugModuleArea:
    case AllocationType::gpuTimestampDeviceBuffer:
    case AllocationType::semaphoreBuffer:
    case AllocationType::syncDispatchToken:
        allocationData.flags.resource48Bit = true;
        break;
    default:
        allocationData.flags.resource48Bit = properties.flags.resource48Bit;
    }
    allocationData.forceKMDAllocation = properties.forceKMDAllocation;
    allocationData.makeGPUVaDifferentThanCPUPtr = properties.makeGPUVaDifferentThanCPUPtr;
    allocationData.flags.shareable = properties.flags.shareable;
    allocationData.flags.shareableWithoutNTHandle = properties.flags.shareableWithoutNTHandle;
    allocationData.flags.isUSMDeviceMemory = properties.flags.isUSMDeviceAllocation;
    allocationData.flags.requiresCpuAccess = GraphicsAllocation::isCpuAccessRequired(properties.allocationType);
    allocationData.flags.allocateMemory = properties.flags.allocateMemory;
    allocationData.flags.allow32Bit = allow32Bit;
    allocationData.flags.allow64kbPages = allow64KbPages;
    allocationData.flags.forcePin = forcePin;
    allocationData.flags.uncacheable = properties.flags.uncacheable;
    allocationData.flags.flushL3 =
        (mayRequireL3Flush ? properties.flags.flushL3RequiredForRead | properties.flags.flushL3RequiredForWrite : 0u);
    allocationData.flags.preferCompressed = properties.flags.preferCompressed;
    allocationData.flags.preferCompressed |= CompressionSelector::preferCompressedAllocation(properties);
    allocationData.flags.multiOsContextCapable = properties.flags.multiOsContextCapable;
    allocationData.flags.cantBeReadOnly = properties.flags.cantBeReadOnly;
    allocationData.usmInitialPlacement = properties.usmInitialPlacement;

    if (properties.allocationType == AllocationType::commandBuffer && rootDeviceEnvironment.debugger.get() && rootDeviceEnvironment.debugger->getSingleAddressSpaceSbaTracking()) {
        allocationData.flags.cantBeReadOnly = true;
    }

    if (GraphicsAllocation::isDebugSurfaceAllocationType(properties.allocationType) ||
        GraphicsAllocation::isConstantOrGlobalSurfaceAllocationType(properties.allocationType)) {
        allocationData.flags.zeroMemory = 1;
    }

    if (properties.allocationType == AllocationType::debugModuleArea) {
        allocationData.flags.use32BitFrontWindow = true;
    } else {
        allocationData.flags.use32BitFrontWindow = properties.flags.use32BitFrontWindow;
    }

    allocationData.hostPtr = hostPtr;
    if (GraphicsAllocation::isKernelIsaAllocationType(properties.allocationType) &&
        !properties.isaPaddingIncluded) {
        allocationData.size = properties.size + helper.getPaddingForISAAllocation();
    } else {
        allocationData.size = properties.size;
    }
    allocationData.type = properties.allocationType;
    allocationData.storageInfo = storageInfo;
    allocationData.alignment = properties.alignment ? properties.alignment : MemoryConstants::preferredAlignment;
    allocationData.imgInfo = properties.imgInfo;

    if (allocationData.flags.allocateMemory) {
        allocationData.hostPtr = nullptr;
    }

    allocationData.gpuAddress = properties.gpuAddress;
    allocationData.osContext = properties.osContext;
    allocationData.rootDeviceIndex = properties.rootDeviceIndex;
    allocationData.useMmapObject = properties.useMmapObject;

    helper.setExtraAllocationData(allocationData, properties, rootDeviceEnvironment);
    allocationData.flags.useSystemMemory |= properties.flags.forceSystemMemory;

    overrideAllocationData(allocationData, properties);
    allocationData.flags.isUSMHostAllocation = properties.flags.isUSMHostAllocation;

    allocationData.storageInfo.systemMemoryPlacement = allocationData.flags.useSystemMemory;
    allocationData.storageInfo.systemMemoryForced = properties.flags.forceSystemMemory;
    allocationData.allocationMethod = getPreferredAllocationMethod(properties);

    bool useLocalPreferredForCacheableBuffers = productHelper.useLocalPreferredForCacheableBuffers();
    if (debugManager.flags.UseLocalPreferredForCacheableBuffers.get() != -1) {
        useLocalPreferredForCacheableBuffers = debugManager.flags.UseLocalPreferredForCacheableBuffers.get() == 1;
    }

    switch (properties.allocationType) {
    case AllocationType::buffer:
    case AllocationType::svmGpu:
    case AllocationType::image:
        if (false == allocationData.flags.uncacheable && useLocalPreferredForCacheableBuffers) {
            if ((usmDeviceAllocationMode == LocalMemAllocationMode::hwDefault) && !allocationData.flags.preferCompressed) {
                allocationData.storageInfo.localOnlyRequired = false;
            }
            allocationData.storageInfo.systemMemoryPlacement = false;
        }
        break;
    default:
        break;
    }

    return true;
}

GfxMemoryAllocationMethod MemoryManager::getPreferredAllocationMethod(const AllocationProperties &allocationProperties) const {
    return GfxMemoryAllocationMethod::notDefined;
}

GraphicsAllocation *MemoryManager::allocatePhysicalGraphicsMemory(const AllocationProperties &properties) {
    AllocationData allocationData;
    GraphicsAllocation *allocation = nullptr;
    getAllocationData(allocationData, properties, nullptr, createStorageInfoFromProperties(properties));

    AllocationStatus status = AllocationStatus::Error;
    if (allocationData.flags.isUSMDeviceMemory) {
        if (this->localMemorySupported[allocationData.rootDeviceIndex]) {
            allocation = allocatePhysicalLocalDeviceMemory(allocationData, status);
            if (allocation) {
                getLocalMemoryUsageBankSelector(properties.allocationType, properties.rootDeviceIndex)->reserveOnBanks(allocationData.storageInfo.getMemoryBanks(), allocation->getUnderlyingBufferSize());
                status = this->registerLocalMemAlloc(allocation, properties.rootDeviceIndex);
            }
        } else {
            allocation = allocatePhysicalDeviceMemory(allocationData, status);
            if (allocation) {
                status = this->registerSysMemAlloc(allocation);
            }
        }
    } else {
        allocation = allocatePhysicalHostMemory(allocationData, status);
        if (allocation) {
            status = this->registerSysMemAlloc(allocation);
        }
    }
    if (allocation && status != AllocationStatus::Success) {
        freeGraphicsMemory(allocation);
        allocation = nullptr;
    }
    if (!allocation) {
        return nullptr;
    }

    logAllocation(fileLoggerInstance(), allocation, this);
    registerAllocationInOs(allocation);
    return allocation;
}

GraphicsAllocation *MemoryManager::allocateGraphicsMemoryInPreferredPool(const AllocationProperties &properties, const void *hostPtr) {
    AllocationData allocationData;
    getAllocationData(allocationData, properties, hostPtr, createStorageInfoFromProperties(properties));

    AllocationStatus status = AllocationStatus::Error;
    GraphicsAllocation *allocation = allocateGraphicsMemoryInDevicePool(allocationData, status);
    if (allocation) {
        getLocalMemoryUsageBankSelector(properties.allocationType, properties.rootDeviceIndex)->reserveOnBanks(allocationData.storageInfo.getMemoryBanks(), allocation->getUnderlyingBufferSize());
        status = this->registerLocalMemAlloc(allocation, properties.rootDeviceIndex);
    }
    if (!allocation && status == AllocationStatus::RetryInNonDevicePool) {
        allocation = allocateGraphicsMemory(allocationData);
        if (allocation) {
            status = this->registerSysMemAlloc(allocation);
        }
    }
    if (allocation && status != AllocationStatus::Success) {
        freeGraphicsMemory(allocation);
        allocation = nullptr;
    }
    if (!allocation) {
        return nullptr;
    }
    allocation->checkAllocationTypeReadOnlyRestrictions(allocationData);

    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[properties.rootDeviceIndex];
    auto &productHelper = rootDeviceEnvironment.getProductHelper();
    if (productHelper.supportReadOnlyAllocations() &&
        !productHelper.isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, *allocation) &&
        allocation->canBeReadOnly()) {
        allocation->setAsReadOnly();
    }

    logAllocation(fileLoggerInstance(), allocation, this);
    registerAllocationInOs(allocation);
    return allocation;
}

GraphicsAllocation *MemoryManager::allocateInternalGraphicsMemoryWithHostCopy(uint32_t rootDeviceIndex,
                                                                              DeviceBitfield bitField,
                                                                              const void *ptr,
                                                                              size_t size) {
    NEO::AllocationProperties copyProperties{rootDeviceIndex,
                                             size,
                                             NEO::AllocationType::internalHostMemory,
                                             bitField};
    copyProperties.alignment = MemoryConstants::pageSize;
    auto allocation = this->allocateGraphicsMemoryWithProperties(copyProperties);
    if (allocation) {
        memcpy_s(allocation->getUnderlyingBuffer(), allocation->getUnderlyingBufferSize(), ptr, size);
    }
    return allocation;
}

bool MemoryManager::mapAuxGpuVA(GraphicsAllocation *graphicsAllocation) {
    bool ret = false;
    for (auto &engine : getRegisteredEngines(graphicsAllocation->getRootDeviceIndex())) {
        if (engine.commandStreamReceiver->pageTableManager.get()) {
            ret = engine.commandStreamReceiver->pageTableManager->updateAuxTable(graphicsAllocation->getGpuAddress(), graphicsAllocation->getDefaultGmm(), true);
            if (!ret) {
                break;
            }
        }
    }
    return ret;
}

GraphicsAllocation *MemoryManager::allocateGraphicsMemory(const AllocationData &allocationData) {
    auto ail = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getAILConfigurationHelper();

    if (allocationData.type == AllocationType::externalHostPtr &&
        allocationData.hostPtr &&
        this->getDeferredDeleter() &&
        (!ail || ail->drainHostptrs())) {
        this->getDeferredDeleter()->drain(true, true);
    }

    if (allocationData.type == AllocationType::image || allocationData.type == AllocationType::sharedResourceCopy) {
        UNRECOVERABLE_IF(allocationData.imgInfo == nullptr);
        return allocateGraphicsMemoryForImage(allocationData);
    }
    if (allocationData.flags.shareable || allocationData.flags.isUSMDeviceMemory) {
        return allocateMemoryByKMD(allocationData);
    }
    if (((false == allocationData.flags.isUSMHostAllocation) || (nullptr == allocationData.hostPtr)) &&
        (useNonSvmHostPtrAlloc(allocationData.type, allocationData.rootDeviceIndex) || isNonSvmBuffer(allocationData.hostPtr, allocationData.type, allocationData.rootDeviceIndex))) {
        auto allocation = allocateGraphicsMemoryForNonSvmHostPtr(allocationData);
        if (allocation) {
            allocation->setFlushL3Required(allocationData.flags.flushL3);
        }
        return allocation;
    }
    bool use32Allocator = heapAssigners[allocationData.rootDeviceIndex]->use32BitHeap(allocationData.type);

    bool isAllocationOnLimitedGPU = isLimitedGPUOnType(allocationData.rootDeviceIndex, allocationData.type);
    if (use32Allocator || isAllocationOnLimitedGPU ||
        (force32bitAllocations && allocationData.flags.allow32Bit && is64bit)) {
        return allocate32BitGraphicsMemoryImpl(allocationData);
    }
    if (allocationData.flags.isUSMHostAllocation && allocationData.hostPtr) {
        return allocateUSMHostGraphicsMemory(allocationData);
    }
    if (allocationData.hostPtr) {
        return allocateGraphicsMemoryWithHostPtr(allocationData);
    }
    if (allocationData.gpuAddress) {
        return allocateGraphicsMemoryWithGpuVa(allocationData);
    }
    if (peek64kbPagesEnabled(allocationData.rootDeviceIndex) && allocationData.flags.allow64kbPages) {
        return allocateGraphicsMemory64kb(allocationData);
    }
    return allocateGraphicsMemoryWithAlignment(allocationData);
}

GraphicsAllocation *MemoryManager::allocateGraphicsMemoryForImage(const AllocationData &allocationData) {
    auto gmm = std::make_unique<Gmm>(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmHelper(), *allocationData.imgInfo,
                                     allocationData.storageInfo, allocationData.flags.preferCompressed);

    // AllocationData needs to be reconfigured for System Memory paths
    AllocationData allocationDataWithSize = allocationData;
    allocationDataWithSize.size = allocationData.imgInfo->size;

    auto hostPtrAllocation = allocateGraphicsMemoryForImageFromHostPtr(allocationDataWithSize);

    if (hostPtrAllocation) {
        hostPtrAllocation->setDefaultGmm(gmm.release());
        return hostPtrAllocation;
    }

    return allocateGraphicsMemoryForImageImpl(allocationDataWithSize, std::move(gmm));
}

bool MemoryManager::isExternalAllocation(AllocationType allocationType) {
    if (allocationType == AllocationType::buffer ||
        allocationType == AllocationType::bufferHostMemory ||
        allocationType == AllocationType::externalHostPtr ||
        allocationType == AllocationType::fillPattern ||
        allocationType == AllocationType::image ||
        allocationType == AllocationType::mapAllocation ||
        allocationType == AllocationType::sharedBuffer ||
        allocationType == AllocationType::sharedImage ||
        allocationType == AllocationType::sharedResourceCopy ||
        allocationType == AllocationType::svmCpu ||
        allocationType == AllocationType::svmGpu ||
        allocationType == AllocationType::svmZeroCopy ||
        allocationType == AllocationType::unifiedSharedMemory ||
        allocationType == AllocationType::writeCombined) {
        return true;
    }
    return false;
}

LocalMemoryUsageBankSelector *MemoryManager::getLocalMemoryUsageBankSelector(AllocationType allocationType, uint32_t rootDeviceIndex) {
    if (isExternalAllocation(allocationType)) {
        return externalLocalMemoryUsageBankSelector[rootDeviceIndex].get();
    }
    return internalLocalMemoryUsageBankSelector[rootDeviceIndex].get();
}

const EngineControl *MemoryManager::getRegisteredEngineForCsr(CommandStreamReceiver *commandStreamReceiver) {
    const EngineControl *engineCtrl = nullptr;
    for (auto &engine : getRegisteredEngines(commandStreamReceiver->getRootDeviceIndex())) {
        if (engine.commandStreamReceiver == commandStreamReceiver) {
            engineCtrl = &engine;
            break;
        }
    }
    return engineCtrl;
}

void MemoryManager::unregisterEngineForCsr(CommandStreamReceiver *commandStreamReceiver) {
    auto &registeredEngines = allRegisteredEngines[commandStreamReceiver->getRootDeviceIndex()];
    auto numRegisteredEngines = registeredEngines.size();
    for (auto i = 0u; i < numRegisteredEngines; i++) {
        if (registeredEngines[i].commandStreamReceiver == commandStreamReceiver) {
            registeredEngines[i].osContext->decRefInternal();
            std::swap(registeredEngines[i], registeredEngines[numRegisteredEngines - 1]);
            registeredEngines.pop_back();
            return;
        }
    }
}

void *MemoryManager::lockResource(GraphicsAllocation *graphicsAllocation) {
    if (!graphicsAllocation) {
        return nullptr;
    }
    if (graphicsAllocation->isLocked()) {
        return graphicsAllocation->getLockedPtr();
    }
    auto retVal = lockResourceImpl(*graphicsAllocation);

    if (!retVal) {
        return nullptr;
    }

    graphicsAllocation->lock(retVal);
    return retVal;
}

void MemoryManager::unlockResource(GraphicsAllocation *graphicsAllocation) {
    if (!graphicsAllocation) {
        return;
    }
    DEBUG_BREAK_IF(!graphicsAllocation->isLocked());
    unlockResourceImpl(*graphicsAllocation);
    graphicsAllocation->unlock();
}

HeapIndex MemoryManager::selectHeap(const GraphicsAllocation *allocation, bool hasPointer, bool isFullRangeSVM, bool useFrontWindow) {
    if (allocation) {
        if (heapAssigners[allocation->getRootDeviceIndex()]->useInternal32BitHeap(allocation->getAllocationType())) {
            return useFrontWindow ? HeapAssigner::mapInternalWindowIndex(selectInternalHeap(allocation->isAllocatedInLocalMemoryPool())) : selectInternalHeap(allocation->isAllocatedInLocalMemoryPool());
        }
        if (allocation->is32BitAllocation() || heapAssigners[allocation->getRootDeviceIndex()]->useExternal32BitHeap(allocation->getAllocationType())) {
            return useFrontWindow ? HeapAssigner::mapExternalWindowIndex(selectExternalHeap(allocation->isAllocatedInLocalMemoryPool()))
                                  : selectExternalHeap(allocation->isAllocatedInLocalMemoryPool());
        }
    }
    if (isFullRangeSVM) {
        if (hasPointer) {
            return HeapIndex::heapSvm;
        }
        if (allocation && allocation->getDefaultGmm()->gmmResourceInfo->is64KBPageSuitable()) {
            return HeapIndex::heapStandard64KB;
        }
        return HeapIndex::heapStandard;
    }
    // Limited range allocation goes to STANDARD heap
    return HeapIndex::heapStandard;
}

bool MemoryManager::copyMemoryToAllocation(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy) {
    if (!graphicsAllocation->getUnderlyingBuffer()) {
        return false;
    }

    for (auto i = 0u; i < graphicsAllocation->storageInfo.getNumBanks(); ++i) {
        memcpy_s(ptrOffset(static_cast<uint8_t *>(graphicsAllocation->getUnderlyingBuffer()) + i * graphicsAllocation->getUnderlyingBufferSize(), destinationOffset),
                 (graphicsAllocation->getUnderlyingBufferSize() - destinationOffset), memoryToCopy, sizeToCopy);
        if (!GraphicsAllocation::isDebugSurfaceAllocationType(graphicsAllocation->getAllocationType())) {
            break;
        }
    }
    return true;
}

bool MemoryManager::copyMemoryToAllocationBanks(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy, DeviceBitfield handleMask) {
    DEBUG_BREAK_IF(graphicsAllocation->storageInfo.getNumBanks() > 1 && handleMask.count() > 0);

    memcpy_s(ptrOffset(static_cast<uint8_t *>(graphicsAllocation->getUnderlyingBuffer()), destinationOffset),
             (graphicsAllocation->getUnderlyingBufferSize() - destinationOffset), memoryToCopy, sizeToCopy);
    return true;
}
void MemoryManager::waitForEnginesCompletion(GraphicsAllocation &graphicsAllocation) {
    for (auto &engine : getRegisteredEngines(graphicsAllocation.getRootDeviceIndex())) {
        auto osContextId = engine.osContext->getContextId();
        auto allocationTaskCount = graphicsAllocation.getTaskCount(osContextId);
        if (graphicsAllocation.isUsedByOsContext(osContextId) &&
            engine.commandStreamReceiver->getTagAllocation() != nullptr &&
            allocationTaskCount > *engine.commandStreamReceiver->getTagAddress()) {
            engine.commandStreamReceiver->waitForCompletionWithTimeout(WaitParams{false, false, false, TimeoutControls::maxTimeout}, allocationTaskCount);
        }
    }
}

bool MemoryManager::allocInUse(GraphicsAllocation &graphicsAllocation) {
    uint32_t numEnginesChecked = 0;
    const uint32_t numContextsToCheck = graphicsAllocation.getNumRegisteredContexts();

    for (auto &engine : getRegisteredEngines(graphicsAllocation.getRootDeviceIndex())) {
        auto osContextId = engine.osContext->getContextId();
        auto allocationTaskCount = graphicsAllocation.getTaskCount(osContextId);

        if (graphicsAllocation.isUsedByOsContext(osContextId)) {
            numEnginesChecked++;
            if (engine.commandStreamReceiver->checkGpuHangDetected(std::chrono::high_resolution_clock::now(), this->lastGpuHangCheck)) {
                return false;
            }
            if (engine.commandStreamReceiver->getTagAddress() && (allocationTaskCount > *engine.commandStreamReceiver->getTagAddress())) {
                return true;
            }
        }

        if (numEnginesChecked == numContextsToCheck) {
            return false;
        }
    }
    return false;
}

void MemoryManager::cleanTemporaryAllocationListOnAllEngines(bool waitForCompletion) {
    for (auto &engineContainer : allRegisteredEngines) {
        for (auto &engine : engineContainer) {
            auto csr = engine.commandStreamReceiver;

            if (waitForCompletion) {
                csr->waitForCompletionWithTimeout(WaitParams{false, false, false, 0}, csr->peekLatestSentTaskCount());
            }
            csr->getInternalAllocationStorage()->cleanAllocationList(*csr->getTagAddress(), AllocationUsage::TEMPORARY_ALLOCATION);

            if (isSingleTemporaryAllocationsListEnabled() && (temporaryAllocations->peekIsEmpty() || !waitForCompletion)) {
                return;
            }
        }
    }
}

void *MemoryManager::getReservedMemory(size_t size, size_t alignment) {
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    if (!reservedMemory) {
        reservedMemory = allocateSystemMemory(size, alignment);
    }
    return reservedMemory;
}

bool MemoryManager::isHostPointerTrackingEnabled(uint32_t rootDeviceIndex) {
    if (debugManager.flags.EnableHostPtrTracking.get() != -1) {
        return !!debugManager.flags.EnableHostPtrTracking.get();
    }
    return is32bit;
}

bool MemoryManager::useNonSvmHostPtrAlloc(AllocationType allocationType, uint32_t rootDeviceIndex) {
    bool isExternalHostPtrAlloc = (allocationType == AllocationType::externalHostPtr);
    bool isMapAlloc = (allocationType == AllocationType::mapAllocation);

    if (forceNonSvmForExternalHostPtr && isExternalHostPtrAlloc) {
        return true;
    }

    bool isNonSvmPtrCapable = ((!peekExecutionEnvironment().rootDeviceEnvironments[rootDeviceIndex]->isFullRangeSvm() || !isHostPointerTrackingEnabled(rootDeviceIndex)) & !is32bit);

    return isNonSvmPtrCapable && (isExternalHostPtrAlloc || isMapAlloc);
}

bool MemoryManager::isCopyRequired(ImageInfo &imgInfo, const void *hostPtr) {
    if (!hostPtr) {
        return false;
    }

    size_t imageWidth = imgInfo.imgDesc.imageWidth;
    size_t imageHeight = 1;
    size_t imageDepth = 1;
    size_t imageCount = 1;

    switch (imgInfo.imgDesc.imageType) {
    case ImageType::image3D:
        imageDepth = imgInfo.imgDesc.imageDepth;
        [[fallthrough]];
    case ImageType::image2D:
    case ImageType::image2DArray:
        imageHeight = imgInfo.imgDesc.imageHeight;
        break;
    default:
        break;
    }

    auto hostPtrRowPitch = imgInfo.imgDesc.imageRowPitch ? imgInfo.imgDesc.imageRowPitch : imageWidth * imgInfo.surfaceFormat->imageElementSizeInBytes;
    auto hostPtrSlicePitch = imgInfo.imgDesc.imageSlicePitch ? imgInfo.imgDesc.imageSlicePitch : hostPtrRowPitch * imgInfo.imgDesc.imageHeight;

    size_t pointerPassedSize = hostPtrRowPitch * imageHeight * imageDepth * imageCount;
    auto alignedSizePassedPointer = alignSizeWholePage(const_cast<void *>(hostPtr), pointerPassedSize);
    auto alignedSizeRequiredForAllocation = alignSizeWholePage(const_cast<void *>(hostPtr), imgInfo.size);

    // Passed pointer doesn't have enough memory, copy is needed
    bool copyRequired = (alignedSizeRequiredForAllocation > alignedSizePassedPointer) |
                        (imgInfo.rowPitch != hostPtrRowPitch) |
                        (imgInfo.slicePitch != hostPtrSlicePitch) |
                        ((reinterpret_cast<uintptr_t>(hostPtr) & (MemoryConstants::cacheLineSize - 1)) != 0) |
                        !imgInfo.linearStorage;

    return copyRequired;
}

void MemoryManager::overrideAllocationData(AllocationData &allocationData, const AllocationProperties &properties) {
    if (debugManager.flags.ForceSystemMemoryPlacement.get()) {
        UNRECOVERABLE_IF(properties.allocationType == AllocationType::unknown);
        if ((1llu << (static_cast<int64_t>(properties.allocationType) - 1)) & debugManager.flags.ForceSystemMemoryPlacement.get()) {
            allocationData.flags.useSystemMemory = true;
        }
    }

    if (debugManager.flags.ForceNonSystemMemoryPlacement.get()) {
        UNRECOVERABLE_IF(properties.allocationType == AllocationType::unknown);
        if ((1llu << (static_cast<int64_t>(properties.allocationType) - 1)) & debugManager.flags.ForceNonSystemMemoryPlacement.get()) {
            allocationData.flags.useSystemMemory = false;
        }
    }

    int32_t directRingPlacement = debugManager.flags.DirectSubmissionBufferPlacement.get();
    int32_t directRingAddressing = debugManager.flags.DirectSubmissionBufferAddressing.get();
    if (properties.allocationType == AllocationType::ringBuffer) {
        if (directRingPlacement != -1) {
            if (directRingPlacement == 0) {
                allocationData.flags.requiresCpuAccess = true;
                allocationData.flags.useSystemMemory = false;
            } else {
                allocationData.flags.requiresCpuAccess = false;
                allocationData.flags.useSystemMemory = true;
            }
        }

        if (directRingAddressing != -1) {
            if (directRingAddressing == 0) {
                allocationData.flags.resource48Bit = false;
            } else {
                allocationData.flags.resource48Bit = true;
            }
        }
    }
    int32_t directSemaphorePlacement = debugManager.flags.DirectSubmissionSemaphorePlacement.get();
    int32_t directSemaphoreAddressing = debugManager.flags.DirectSubmissionSemaphoreAddressing.get();
    if (properties.allocationType == AllocationType::semaphoreBuffer) {
        if (directSemaphorePlacement != -1) {
            if (directSemaphorePlacement == 0) {
                allocationData.flags.requiresCpuAccess = true;
                allocationData.flags.useSystemMemory = false;
            } else {
                allocationData.flags.requiresCpuAccess = false;
                allocationData.flags.useSystemMemory = true;
            }
        }
        if (directSemaphoreAddressing != -1) {
            if (directSemaphoreAddressing == 0) {
                allocationData.flags.resource48Bit = false;
            } else {
                allocationData.flags.resource48Bit = true;
            }
        }
    }
}

bool MemoryManager::isAllocationTypeToCapture(AllocationType type) const {
    switch (type) {
    case AllocationType::scratchSurface:
    case AllocationType::privateSurface:
    case AllocationType::linearStream:
    case AllocationType::internalHeap:
        return true;
    default:
        break;
    }
    return false;
}

bool MemoryManager::isLocalMemoryUsedForIsa(uint32_t rootDeviceIndex) {
    std::call_once(checkIsaPlacementOnceFlags[rootDeviceIndex], [&] {
        AllocationProperties properties = {rootDeviceIndex, 0x1000, AllocationType::kernelIsa, 1};
        AllocationData data;
        getAllocationData(data, properties, nullptr, createStorageInfoFromProperties(properties));
        isaInLocalMemory[rootDeviceIndex] = !data.flags.useSystemMemory;
    });

    return isaInLocalMemory[rootDeviceIndex];
}

bool MemoryManager::isKernelBinaryReuseEnabled() {
    auto reuseBinaries = false;

    if (debugManager.flags.ReuseKernelBinaries.get() != -1) {
        reuseBinaries = debugManager.flags.ReuseKernelBinaries.get();
    }

    return reuseBinaries;
}

OsContext *MemoryManager::getDefaultEngineContext(uint32_t rootDeviceIndex, DeviceBitfield subdevicesBitfield) {
    OsContext *defaultContext = nullptr;
    for (auto &engine : getRegisteredEngines(rootDeviceIndex)) {
        auto osContext = engine.osContext;
        if (osContext->isDefaultContext() && osContext->getDeviceBitfield() == subdevicesBitfield) {
            defaultContext = osContext;
            break;
        }
    }
    if (!defaultContext) {
        defaultContext = getRegisteredEngines(rootDeviceIndex)[defaultEngineIndex[rootDeviceIndex]].osContext;
    }
    return defaultContext;
}

bool MemoryManager::allocateBindlessSlot(GraphicsAllocation *allocation) {
    auto bindlessHelper = peekExecutionEnvironment().rootDeviceEnvironments[allocation->getRootDeviceIndex()]->getBindlessHeapsHelper();

    if (bindlessHelper && allocation->getBindlessOffset() == std::numeric_limits<uint64_t>::max()) {
        auto &gfxCoreHelper = peekExecutionEnvironment().rootDeviceEnvironments[allocation->getRootDeviceIndex()]->getHelper<GfxCoreHelper>();
        const auto isImage = allocation->getAllocationType() == AllocationType::image || allocation->getAllocationType() == AllocationType::sharedImage;
        auto surfStateCount = isImage ? NEO::BindlessImageSlot::max : 1;
        auto surfaceStateSize = surfStateCount * gfxCoreHelper.getRenderSurfaceStateSize();

        auto surfaceStateInfo = bindlessHelper->allocateSSInHeap(surfaceStateSize, allocation, NEO::BindlessHeapsHelper::globalSsh);
        if (surfaceStateInfo.heapAllocation == nullptr) {
            return false;
        }
        allocation->setBindlessInfo(surfaceStateInfo);
    }
    return true;
}

bool MemoryTransferHelper::transferMemoryToAllocation(bool useBlitter, const Device &device, GraphicsAllocation *dstAllocation, size_t dstOffset, const void *srcMemory, size_t srcSize) {
    if (useBlitter) {
        if (BlitHelperFunctions::blitMemoryToAllocation(device, dstAllocation, dstOffset, srcMemory, {srcSize, 1, 1}) == BlitOperationResult::success) {
            return true;
        }
    }
    return device.getMemoryManager()->copyMemoryToAllocation(dstAllocation, dstOffset, srcMemory, srcSize);
}
bool MemoryTransferHelper::transferMemoryToAllocationBanks(bool useBlitter, const Device &device, GraphicsAllocation *dstAllocation, size_t dstOffset, const void *srcMemory,
                                                           size_t srcSize, DeviceBitfield dstMemoryBanks) {
    auto blitSuccess = false;

    if (useBlitter) {
        blitSuccess = BlitHelper::blitMemoryToAllocationBanks(device, dstAllocation, dstOffset, srcMemory, {srcSize, 1, 1}, dstMemoryBanks) == BlitOperationResult::success;
    }
    if (!blitSuccess) {
        return device.getMemoryManager()->copyMemoryToAllocationBanks(dstAllocation, dstOffset, srcMemory, srcSize, dstMemoryBanks);
    }
    return true;
}

uint64_t MemoryManager::adjustToggleBitFlagForGpuVa(AllocationType inputAllocationType, uint64_t gpuAddress) {
    if (debugManager.flags.ToggleBitIn57GpuVa.get() != "unk") {
        auto toggleBitIn57GpuVaEntries = StringHelpers::split(debugManager.flags.ToggleBitIn57GpuVa.get(), ",");

        for (const auto &entry : toggleBitIn57GpuVaEntries) {
            auto subEntries = StringHelpers::split(entry, ":");
            UNRECOVERABLE_IF(subEntries.size() < 2u);
            uint32_t allocationType = StringHelpers::toUint32t(subEntries[0]);
            uint32_t bitNumber = StringHelpers::toUint32t(subEntries[1]);

            UNRECOVERABLE_IF(allocationType >= static_cast<uint32_t>(AllocationType::count));
            UNRECOVERABLE_IF(bitNumber >= 56);
            if (allocationType == static_cast<uint32_t>(inputAllocationType)) {
                if (isBitSet(gpuAddress, bitNumber)) {
                    gpuAddress &= ~(1ull << bitNumber);
                } else {
                    gpuAddress |= 1ull << bitNumber;
                }
            }
        }
    }
    return gpuAddress;
}

void MemoryManager::addCustomHeapAllocatorConfig(AllocationType allocationType, bool isFrontWindowPool, const CustomHeapAllocatorConfig &config) {
    customHeapAllocators[{allocationType, isFrontWindowPool}] = config;
}

std::optional<std::reference_wrapper<CustomHeapAllocatorConfig>> MemoryManager::getCustomHeapAllocatorConfig(AllocationType allocationType, bool isFrontWindowPool) {
    auto it = customHeapAllocators.find({allocationType, isFrontWindowPool});
    if (it != customHeapAllocators.end()) {
        return it->second;
    }
    return std::nullopt;
}

void MemoryManager::removeCustomHeapAllocatorConfig(AllocationType allocationType, bool isFrontWindowPool) {
    customHeapAllocators.erase({allocationType, isFrontWindowPool});
}

bool MemoryManager::getLocalOnlyRequired(AllocationType allocationType, const ProductHelper &productHelper, const ReleaseHelper *releaseHelper, bool preferCompressed) const {
    const bool enabledForRelease{!releaseHelper || releaseHelper->isLocalOnlyAllowed()};

    if (allocationType == AllocationType::buffer || allocationType == AllocationType::svmGpu) {
        return productHelper.getStorageInfoLocalOnlyFlag(usmDeviceAllocationMode, enabledForRelease);
    }
    return (preferCompressed ? enabledForRelease : false);
}
} // namespace NEO

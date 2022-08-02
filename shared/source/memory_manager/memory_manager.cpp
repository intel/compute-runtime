/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/memory_manager.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/heap_assigner.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/memory_manager/compression_selector.h"
#include "shared/source/memory_manager/deferrable_allocation_deletion.h"
#include "shared/source/memory_manager/deferred_deleter.h"
#include "shared/source/memory_manager/host_ptr_manager.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/prefetch_manager.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/stackvec.h"

#include <algorithm>

namespace NEO {
uint32_t MemoryManager::maxOsContextCount = 0u;

MemoryManager::MemoryManager(ExecutionEnvironment &executionEnvironment) : executionEnvironment(executionEnvironment), hostPtrManager(std::make_unique<HostPtrManager>()),
                                                                           multiContextResourceDestructor(std::make_unique<DeferredDeleter>()) {

    bool anyLocalMemorySupported = false;
    const auto rootEnvCount = executionEnvironment.rootDeviceEnvironments.size();

    defaultEngineIndex.resize(rootEnvCount);
    checkIsaPlacementOnceFlags = std::make_unique<std::once_flag[]>(rootEnvCount);
    isaInLocalMemory.resize(rootEnvCount);

    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < rootEnvCount; ++rootDeviceIndex) {
        auto hwInfo = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
        internalLocalMemoryUsageBankSelector.emplace_back(new LocalMemoryUsageBankSelector(HwHelper::getSubDevicesCount(hwInfo)));
        externalLocalMemoryUsageBankSelector.emplace_back(new LocalMemoryUsageBankSelector(HwHelper::getSubDevicesCount(hwInfo)));
        this->localMemorySupported.push_back(HwHelper::get(hwInfo->platform.eRenderCoreFamily).getEnableLocalMemory(*hwInfo));
        this->enable64kbpages.push_back(OSInterface::osEnabled64kbPages && hwInfo->capabilityTable.ftr64KBpages && !!DebugManager.flags.Enable64kbpages.get());

        gfxPartitions.push_back(std::make_unique<GfxPartition>(reservedCpuAddressRange));

        anyLocalMemorySupported |= this->localMemorySupported[rootDeviceIndex];
        isLocalMemoryUsedForIsa(rootDeviceIndex);
    }

    if (anyLocalMemorySupported) {
        pageFaultManager = PageFaultManager::create();
        prefetchManager = PrefetchManager::create();
    }

    if (DebugManager.flags.EnableMultiStorageResources.get() != -1) {
        supportsMultiStorageResources = !!DebugManager.flags.EnableMultiStorageResources.get();
    }
}

MemoryManager::~MemoryManager() {
    for (auto &engine : registeredEngines) {
        engine.osContext->decRefInternal();
    }
    registeredEngines.clear();
    if (reservedMemory) {
        MemoryManager::alignedFreeWrapper(reservedMemory);
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
        deferredDeleter->drain(true);
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
                for (auto gpuAllocation : multiGraphicsAllocation.getGraphicsAllocations()) {
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
    if (ApiSpecificConfig::getBindlessConfiguration() && executionEnvironment.rootDeviceEnvironments[gfxAllocation->getRootDeviceIndex()]->getBindlessHeapsHelper() != nullptr) {
        executionEnvironment.rootDeviceEnvironments[gfxAllocation->getRootDeviceIndex()]->getBindlessHeapsHelper()->placeSSAllocationInReuseVectorOnFreeMemory(gfxAllocation);
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

    getLocalMemoryUsageBankSelector(gfxAllocation->getAllocationType(), gfxAllocation->getRootDeviceIndex())->freeOnBanks(gfxAllocation->storageInfo.getMemoryBanks(), gfxAllocation->getUnderlyingBufferSize());
    freeGraphicsMemoryImpl(gfxAllocation, isImportedAllocation);
}

//if not in use destroy in place
//if in use pass to temporary allocation list that is cleaned on blocking calls
void MemoryManager::checkGpuUsageAndDestroyGraphicsAllocations(GraphicsAllocation *gfxAllocation) {
    if (gfxAllocation->isUsed()) {
        if (gfxAllocation->isUsedByManyOsContexts()) {
            multiContextResourceDestructor->deferDeletion(new DeferrableAllocationDeletion{*this, *gfxAllocation});
            multiContextResourceDestructor->drain(false);
            return;
        }
        for (auto &engine : getRegisteredEngines()) {
            auto osContextId = engine.osContext->getContextId();
            auto allocationTaskCount = gfxAllocation->getTaskCount(osContextId);
            if (gfxAllocation->isUsedByOsContext(osContextId) &&
                allocationTaskCount > *engine.commandStreamReceiver->getTagAddress()) {
                engine.commandStreamReceiver->getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(gfxAllocation),
                                                                                              DEFERRED_DEALLOCATION);
                return;
            }
        }
    }
    freeGraphicsMemory(gfxAllocation);
}

void MemoryManager::waitForDeletions() {
    if (deferredDeleter) {
        deferredDeleter->drain(false);
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

OsContext *MemoryManager::createAndRegisterOsContext(CommandStreamReceiver *commandStreamReceiver,
                                                     const EngineDescriptor &engineDescriptor) {
    auto rootDeviceIndex = commandStreamReceiver->getRootDeviceIndex();
    updateLatestContextIdForRootDevice(rootDeviceIndex);

    auto contextId = ++latestContextId;
    auto osContext = OsContext::create(peekExecutionEnvironment().rootDeviceEnvironments[rootDeviceIndex]->osInterface.get(), contextId, engineDescriptor);
    osContext->incRefInternal();

    registeredEngines.emplace_back(commandStreamReceiver, osContext);

    return osContext;
}

bool MemoryManager::getAllocationData(AllocationData &allocationData, const AllocationProperties &properties, const void *hostPtr, const StorageInfo &storageInfo) {
    UNRECOVERABLE_IF(hostPtr == nullptr && !properties.flags.allocateMemory);
    UNRECOVERABLE_IF(properties.allocationType == AllocationType::UNKNOWN);

    auto hwInfo = executionEnvironment.rootDeviceEnvironments[properties.rootDeviceIndex]->getHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo->platform.eRenderCoreFamily);

    bool allow64KbPages = false;
    bool allow32Bit = false;
    bool forcePin = properties.flags.forcePin;
    bool mayRequireL3Flush = false;

    switch (properties.allocationType) {
    case AllocationType::BUFFER:
    case AllocationType::BUFFER_HOST_MEMORY:
    case AllocationType::CONSTANT_SURFACE:
    case AllocationType::GLOBAL_SURFACE:
    case AllocationType::PIPE:
    case AllocationType::PRINTF_SURFACE:
    case AllocationType::PRIVATE_SURFACE:
    case AllocationType::SCRATCH_SURFACE:
    case AllocationType::WORK_PARTITION_SURFACE:
    case AllocationType::WRITE_COMBINED:
        allow64KbPages = true;
        allow32Bit = true;
    default:
        break;
    }

    switch (properties.allocationType) {
    case AllocationType::SVM_GPU:
    case AllocationType::SVM_ZERO_COPY:
    case AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER:
    case AllocationType::PREEMPTION:
        allow64KbPages = true;
    default:
        break;
    }

    switch (properties.allocationType) {
    case AllocationType::BUFFER:
    case AllocationType::BUFFER_HOST_MEMORY:
    case AllocationType::WRITE_COMBINED:
        forcePin = true;
    default:
        break;
    }

    switch (properties.allocationType) {
    case AllocationType::BUFFER:
    case AllocationType::BUFFER_HOST_MEMORY:
    case AllocationType::EXTERNAL_HOST_PTR:
    case AllocationType::GLOBAL_SURFACE:
    case AllocationType::IMAGE:
    case AllocationType::MAP_ALLOCATION:
    case AllocationType::PIPE:
    case AllocationType::SHARED_BUFFER:
    case AllocationType::SHARED_IMAGE:
    case AllocationType::SHARED_RESOURCE_COPY:
    case AllocationType::SVM_CPU:
    case AllocationType::SVM_GPU:
    case AllocationType::SVM_ZERO_COPY:
    case AllocationType::WRITE_COMBINED:
        mayRequireL3Flush = true;
    default:
        break;
    }

    switch (properties.allocationType) {
    case AllocationType::COMMAND_BUFFER:
    case AllocationType::RING_BUFFER:
    case AllocationType::SEMAPHORE_BUFFER:
    case AllocationType::BUFFER_HOST_MEMORY:
    case AllocationType::EXTERNAL_HOST_PTR:
    case AllocationType::FILL_PATTERN:
    case AllocationType::MAP_ALLOCATION:
    case AllocationType::MCS:
    case AllocationType::PROFILING_TAG_BUFFER:
    case AllocationType::SHARED_CONTEXT_IMAGE:
    case AllocationType::SVM_CPU:
    case AllocationType::SVM_ZERO_COPY:
    case AllocationType::TAG_BUFFER:
    case AllocationType::GLOBAL_FENCE:
    case AllocationType::INTERNAL_HOST_MEMORY:
    case AllocationType::TIMESTAMP_PACKET_TAG_BUFFER:
    case AllocationType::DEBUG_CONTEXT_SAVE_AREA:
    case AllocationType::DEBUG_SBA_TRACKING_BUFFER:
    case AllocationType::SW_TAG_BUFFER:
        allocationData.flags.useSystemMemory = true;
    default:
        break;
    }

    if (GraphicsAllocation::isIsaAllocationType(properties.allocationType)) {
        allocationData.flags.useSystemMemory = hwHelper.useSystemMemoryPlacementForISA(*hwInfo);
    }

    switch (properties.allocationType) {
    case AllocationType::COMMAND_BUFFER:
    case AllocationType::IMAGE:
    case AllocationType::INDIRECT_OBJECT_HEAP:
    case AllocationType::INSTRUCTION_HEAP:
    case AllocationType::INTERNAL_HEAP:
    case AllocationType::KERNEL_ISA:
    case AllocationType::KERNEL_ISA_INTERNAL:
    case AllocationType::LINEAR_STREAM:
    case AllocationType::MCS:
    case AllocationType::PREEMPTION:
    case AllocationType::SCRATCH_SURFACE:
    case AllocationType::WORK_PARTITION_SURFACE:
    case AllocationType::SHARED_CONTEXT_IMAGE:
    case AllocationType::SHARED_IMAGE:
    case AllocationType::SHARED_RESOURCE_COPY:
    case AllocationType::SURFACE_STATE_HEAP:
    case AllocationType::TIMESTAMP_PACKET_TAG_BUFFER:
    case AllocationType::DEBUG_MODULE_AREA:
    case AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER:
    case AllocationType::RING_BUFFER:
    case AllocationType::SEMAPHORE_BUFFER:
        allocationData.flags.resource48Bit = true;
        break;
    default:
        allocationData.flags.resource48Bit = properties.flags.resource48Bit;
    }

    allocationData.flags.shareable = properties.flags.shareable;
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
    allocationData.flags.preferCompressed |= CompressionSelector::preferCompressedAllocation(properties, *hwInfo);
    allocationData.flags.multiOsContextCapable = properties.flags.multiOsContextCapable;
    allocationData.usmInitialPlacement = properties.usmInitialPlacement;

    if (properties.allocationType == AllocationType::DEBUG_CONTEXT_SAVE_AREA ||
        properties.allocationType == AllocationType::DEBUG_SBA_TRACKING_BUFFER) {
        allocationData.flags.zeroMemory = 1;
    }

    if (properties.allocationType == AllocationType::DEBUG_MODULE_AREA) {
        allocationData.flags.use32BitFrontWindow = true;
    } else {
        allocationData.flags.use32BitFrontWindow = properties.flags.use32BitFrontWindow;
    }

    allocationData.hostPtr = hostPtr;
    if (properties.allocationType == AllocationType::KERNEL_ISA ||
        properties.allocationType == AllocationType::KERNEL_ISA_INTERNAL) {
        allocationData.size = properties.size + hwHelper.getPaddingForISAAllocation();
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

    allocationData.flags.crossRootDeviceAccess = properties.flags.crossRootDeviceAccess;
    allocationData.flags.useSystemMemory |= properties.flags.crossRootDeviceAccess;

    hwHelper.setExtraAllocationData(allocationData, properties, *hwInfo);
    allocationData.flags.useSystemMemory |= properties.flags.forceSystemMemory;

    overrideAllocationData(allocationData, properties);
    allocationData.flags.isUSMHostAllocation = properties.flags.isUSMHostAllocation;

    allocationData.storageInfo.systemMemoryPlacement = allocationData.flags.useSystemMemory;

    return true;
}

GraphicsAllocation *MemoryManager::allocateGraphicsMemoryInPreferredPool(const AllocationProperties &properties, const void *hostPtr) {
    AllocationData allocationData;
    getAllocationData(allocationData, properties, hostPtr, createStorageInfoFromProperties(properties));

    AllocationStatus status = AllocationStatus::Error;
    GraphicsAllocation *allocation = allocateGraphicsMemoryInDevicePool(allocationData, status);
    if (allocation) {
        getLocalMemoryUsageBankSelector(properties.allocationType, properties.rootDeviceIndex)->reserveOnBanks(allocationData.storageInfo.getMemoryBanks(), allocation->getUnderlyingBufferSize());
        this->registerLocalMemAlloc(allocation, properties.rootDeviceIndex);
    }
    if (!allocation && status == AllocationStatus::RetryInNonDevicePool) {
        allocation = allocateGraphicsMemory(allocationData);
        if (allocation) {
            this->registerSysMemAlloc(allocation);
        }
    }

    if (!allocation) {
        return nullptr;
    }

    fileLoggerInstance().logAllocation(allocation);
    registerAllocationInOs(allocation);
    return allocation;
}

GraphicsAllocation *MemoryManager::allocateInternalGraphicsMemoryWithHostCopy(uint32_t rootDeviceIndex,
                                                                              DeviceBitfield bitField,
                                                                              const void *ptr,
                                                                              size_t size) {
    NEO::AllocationProperties copyProperties{rootDeviceIndex,
                                             size,
                                             NEO::AllocationType::INTERNAL_HOST_MEMORY,
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
    for (auto engine : registeredEngines) {
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
    if (allocationData.type == AllocationType::IMAGE || allocationData.type == AllocationType::SHARED_RESOURCE_COPY) {
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
    bool use32Allocator = heapAssigner.use32BitHeap(allocationData.type);

    bool isAllocationOnLimitedGPU = isLimitedGPUOnType(allocationData.rootDeviceIndex, allocationData.type);
    if (use32Allocator || isAllocationOnLimitedGPU ||
        (force32bitAllocations && allocationData.flags.allow32Bit && is64bit)) {
        auto hwInfo = executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getHardwareInfo();
        bool useLocalMem = heapAssigner.useExternal32BitHeap(allocationData.type) ? HwInfoConfig::get(hwInfo->platform.eProductFamily)->heapInLocalMem(*hwInfo) : false;
        return allocate32BitGraphicsMemoryImpl(allocationData, useLocalMem);
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

EngineControlContainer &MemoryManager::getRegisteredEngines() {
    return registeredEngines;
}

bool MemoryManager::isExternalAllocation(AllocationType allocationType) {
    if (allocationType == AllocationType::BUFFER ||
        allocationType == AllocationType::BUFFER_HOST_MEMORY ||
        allocationType == AllocationType::EXTERNAL_HOST_PTR ||
        allocationType == AllocationType::FILL_PATTERN ||
        allocationType == AllocationType::IMAGE ||
        allocationType == AllocationType::MAP_ALLOCATION ||
        allocationType == AllocationType::PIPE ||
        allocationType == AllocationType::SHARED_BUFFER ||
        allocationType == AllocationType::SHARED_CONTEXT_IMAGE ||
        allocationType == AllocationType::SHARED_IMAGE ||
        allocationType == AllocationType::SHARED_RESOURCE_COPY ||
        allocationType == AllocationType::SVM_CPU ||
        allocationType == AllocationType::SVM_GPU ||
        allocationType == AllocationType::SVM_ZERO_COPY ||
        allocationType == AllocationType::UNIFIED_SHARED_MEMORY ||
        allocationType == AllocationType::WRITE_COMBINED) {
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

EngineControl *MemoryManager::getRegisteredEngineForCsr(CommandStreamReceiver *commandStreamReceiver) {
    EngineControl *engineCtrl = nullptr;
    for (auto &engine : registeredEngines) {
        if (engine.commandStreamReceiver == commandStreamReceiver) {
            engineCtrl = &engine;
            break;
        }
    }
    return engineCtrl;
}

void MemoryManager::unregisterEngineForCsr(CommandStreamReceiver *commandStreamReceiver) {
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
        if (heapAssigner.useInternal32BitHeap(allocation->getAllocationType())) {
            return useFrontWindow ? HeapAssigner::mapInternalWindowIndex(selectInternalHeap(allocation->isAllocatedInLocalMemoryPool())) : selectInternalHeap(allocation->isAllocatedInLocalMemoryPool());
        }
        if (allocation->is32BitAllocation() || heapAssigner.useExternal32BitHeap(allocation->getAllocationType())) {
            return useFrontWindow ? HeapAssigner::mapExternalWindowIndex(selectExternalHeap(allocation->isAllocatedInLocalMemoryPool()))
                                  : selectExternalHeap(allocation->isAllocatedInLocalMemoryPool());
        }
    }
    if (isFullRangeSVM) {
        if (hasPointer) {
            return HeapIndex::HEAP_SVM;
        }
        if (allocation && allocation->getDefaultGmm()->gmmResourceInfo->is64KBPageSuitable()) {
            return HeapIndex::HEAP_STANDARD64KB;
        }
        return HeapIndex::HEAP_STANDARD;
    }
    // Limited range allocation goes to STANDARD heap
    return HeapIndex::HEAP_STANDARD;
}

bool MemoryManager::copyMemoryToAllocation(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy) {
    if (!graphicsAllocation->getUnderlyingBuffer()) {
        return false;
    }
    for (auto i = 0u; i < graphicsAllocation->storageInfo.getNumBanks(); ++i) {
        memcpy_s(ptrOffset(static_cast<uint8_t *>(graphicsAllocation->getUnderlyingBuffer()) + i * graphicsAllocation->getUnderlyingBufferSize(), destinationOffset),
                 (graphicsAllocation->getUnderlyingBufferSize() - destinationOffset), memoryToCopy, sizeToCopy);
        if (graphicsAllocation->getAllocationType() != AllocationType::DEBUG_CONTEXT_SAVE_AREA &&
            graphicsAllocation->getAllocationType() != AllocationType::DEBUG_SBA_TRACKING_BUFFER) {
            break;
        }
    }
    return true;
}

bool MemoryManager::copyMemoryToAllocationBanks(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy, DeviceBitfield handleMask) {
    memcpy_s(ptrOffset(static_cast<uint8_t *>(graphicsAllocation->getUnderlyingBuffer()), destinationOffset),
             (graphicsAllocation->getUnderlyingBufferSize() - destinationOffset), memoryToCopy, sizeToCopy);
    return true;
}
void MemoryManager::waitForEnginesCompletion(GraphicsAllocation &graphicsAllocation) {
    for (auto &engine : getRegisteredEngines()) {
        auto osContextId = engine.osContext->getContextId();
        auto allocationTaskCount = graphicsAllocation.getTaskCount(osContextId);
        if (graphicsAllocation.isUsedByOsContext(osContextId) &&
            engine.commandStreamReceiver->getTagAllocation() != nullptr &&
            allocationTaskCount > *engine.commandStreamReceiver->getTagAddress()) {
            engine.commandStreamReceiver->waitForCompletionWithTimeout(WaitParams{false, false, TimeoutControls::maxTimeout}, allocationTaskCount);
        }
    }
}

void MemoryManager::cleanTemporaryAllocationListOnAllEngines(bool waitForCompletion) {
    for (auto &engine : getRegisteredEngines()) {
        auto csr = engine.commandStreamReceiver;
        if (waitForCompletion) {
            csr->waitForCompletionWithTimeout(WaitParams{false, false, 0}, csr->peekLatestSentTaskCount());
        }
        csr->getInternalAllocationStorage()->cleanAllocationList(*csr->getTagAddress(), AllocationUsage::TEMPORARY_ALLOCATION);
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
    if (DebugManager.flags.EnableHostPtrTracking.get() != -1) {
        return !!DebugManager.flags.EnableHostPtrTracking.get();
    }
    return (peekExecutionEnvironment().rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo()->capabilityTable.hostPtrTrackingEnabled | is32bit);
}

bool MemoryManager::useNonSvmHostPtrAlloc(AllocationType allocationType, uint32_t rootDeviceIndex) {
    bool isExternalHostPtrAlloc = (allocationType == AllocationType::EXTERNAL_HOST_PTR);
    bool isMapAlloc = (allocationType == AllocationType::MAP_ALLOCATION);

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
    case ImageType::Image3D:
        imageDepth = imgInfo.imgDesc.imageDepth;
        [[fallthrough]];
    case ImageType::Image2D:
    case ImageType::Image2DArray:
        imageHeight = imgInfo.imgDesc.imageHeight;
        break;
    default:
        break;
    }

    auto hostPtrRowPitch = imgInfo.imgDesc.imageRowPitch ? imgInfo.imgDesc.imageRowPitch : imageWidth * imgInfo.surfaceFormat->ImageElementSizeInBytes;
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
    if (DebugManager.flags.ForceSystemMemoryPlacement.get()) {
        if ((1llu << (static_cast<int64_t>(properties.allocationType) - 1)) & DebugManager.flags.ForceSystemMemoryPlacement.get()) {
            allocationData.flags.useSystemMemory = true;
        }
    }

    if (DebugManager.flags.ForceNonSystemMemoryPlacement.get()) {
        if ((1llu << (static_cast<int64_t>(properties.allocationType) - 1)) & DebugManager.flags.ForceNonSystemMemoryPlacement.get()) {
            allocationData.flags.useSystemMemory = false;
        }
    }

    int32_t directRingPlacement = DebugManager.flags.DirectSubmissionBufferPlacement.get();
    int32_t directRingAddressing = DebugManager.flags.DirectSubmissionBufferAddressing.get();
    if (properties.allocationType == AllocationType::RING_BUFFER) {
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
    int32_t directSemaphorePlacement = DebugManager.flags.DirectSubmissionSemaphorePlacement.get();
    int32_t directSemaphoreAddressing = DebugManager.flags.DirectSubmissionSemaphoreAddressing.get();
    if (properties.allocationType == AllocationType::SEMAPHORE_BUFFER) {
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
    case AllocationType::SCRATCH_SURFACE:
    case AllocationType::PRIVATE_SURFACE:
    case AllocationType::LINEAR_STREAM:
    case AllocationType::INTERNAL_HEAP:
        return true;
    default:
        break;
    }
    return false;
}

bool MemoryManager::isLocalMemoryUsedForIsa(uint32_t rootDeviceIndex) {
    std::call_once(checkIsaPlacementOnceFlags[rootDeviceIndex], [&] {
        AllocationProperties properties = {rootDeviceIndex, 0x1000, AllocationType::KERNEL_ISA, 1};
        AllocationData data;
        getAllocationData(data, properties, nullptr, StorageInfo());
        isaInLocalMemory[rootDeviceIndex] = !data.flags.useSystemMemory;
    });

    return isaInLocalMemory[rootDeviceIndex];
}

bool MemoryTransferHelper::transferMemoryToAllocation(bool useBlitter, const Device &device, GraphicsAllocation *dstAllocation, size_t dstOffset, const void *srcMemory, size_t srcSize) {
    if (useBlitter) {
        if (BlitHelperFunctions::blitMemoryToAllocation(device, dstAllocation, dstOffset, srcMemory, {srcSize, 1, 1}) == BlitOperationResult::Success) {
            return true;
        }
    }
    return device.getMemoryManager()->copyMemoryToAllocation(dstAllocation, dstOffset, srcMemory, srcSize);
}
bool MemoryTransferHelper::transferMemoryToAllocationBanks(const Device &device, GraphicsAllocation *dstAllocation, size_t dstOffset, const void *srcMemory,
                                                           size_t srcSize, DeviceBitfield dstMemoryBanks) {
    auto blitSuccess = BlitHelper::blitMemoryToAllocationBanks(device, dstAllocation, dstOffset, srcMemory, {srcSize, 1, 1}, dstMemoryBanks) == BlitOperationResult::Success;

    if (!blitSuccess) {
        return device.getMemoryManager()->copyMemoryToAllocationBanks(dstAllocation, dstOffset, srcMemory, srcSize, dstMemoryBanks);
    }
    return true;
}
} // namespace NEO

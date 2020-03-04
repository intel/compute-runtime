/*
 * Copyright (C) 2017-2020 Intel Corporation
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
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/memory_manager/deferrable_allocation_deletion.h"
#include "shared/source/memory_manager/deferred_deleter.h"
#include "shared/source/memory_manager/host_ptr_manager.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/compiler_support.h"
#include "shared/source/utilities/stackvec.h"

#include <algorithm>

namespace NEO {
uint32_t MemoryManager::maxOsContextCount = 0u;

MemoryManager::MemoryManager(ExecutionEnvironment &executionEnvironment) : executionEnvironment(executionEnvironment), hostPtrManager(std::make_unique<HostPtrManager>()),
                                                                           multiContextResourceDestructor(std::make_unique<DeferredDeleter>()) {

    bool anyLocalMemorySupported = false;

    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < executionEnvironment.rootDeviceEnvironments.size(); ++rootDeviceIndex) {
        auto hwInfo = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
        localMemoryUsageBankSelector.emplace_back(new LocalMemoryUsageBankSelector(HwHelper::getSubDevicesCount(hwInfo)));
        this->localMemorySupported.push_back(HwHelper::get(hwInfo->platform.eRenderCoreFamily).getEnableLocalMemory(*hwInfo));
        this->enable64kbpages.push_back(OSInterface::osEnabled64kbPages && hwInfo->capabilityTable.ftr64KBpages);
        if (DebugManager.flags.Enable64kbpages.get() > -1) {
            this->enable64kbpages[rootDeviceIndex] = DebugManager.flags.Enable64kbpages.get() != 0;
        }

        gfxPartitions.push_back(std::make_unique<GfxPartition>());

        anyLocalMemorySupported |= this->localMemorySupported[rootDeviceIndex];
    }

    if (anyLocalMemorySupported) {
        pageFaultManager = PageFaultManager::create();
    }
}

MemoryManager::~MemoryManager() {
    for (auto &engine : registeredEngines) {
        engine.osContext->decRefInternal();
    }
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
    hostPtrManager->releaseHandleStorage(graphicsAllocation->fragmentsStorage);
    cleanOsHandles(graphicsAllocation->fragmentsStorage, graphicsAllocation->getRootDeviceIndex());
}

GraphicsAllocation *MemoryManager::createGraphicsAllocationWithPadding(GraphicsAllocation *inputGraphicsAllocation, size_t sizeWithPadding) {
    return createPaddedAllocation(inputGraphicsAllocation, sizeWithPadding);
}

GraphicsAllocation *MemoryManager::createPaddedAllocation(GraphicsAllocation *inputGraphicsAllocation, size_t sizeWithPadding) {
    return allocateGraphicsMemoryWithProperties({inputGraphicsAllocation->getRootDeviceIndex(), sizeWithPadding, GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY});
}

void MemoryManager::freeSystemMemory(void *ptr) {
    ::alignedFree(ptr);
}

void MemoryManager::freeGraphicsMemory(GraphicsAllocation *gfxAllocation) {
    if (!gfxAllocation) {
        return;
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

    localMemoryUsageBankSelector[gfxAllocation->getRootDeviceIndex()]->freeOnBanks(gfxAllocation->storageInfo.getMemoryBanks(), gfxAllocation->getUnderlyingBufferSize());
    freeGraphicsMemoryImpl(gfxAllocation);
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
                                                                                              TEMPORARY_ALLOCATION);
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

OsContext *MemoryManager::createAndRegisterOsContext(CommandStreamReceiver *commandStreamReceiver, aub_stream::EngineType engineType,
                                                     DeviceBitfield deviceBitfield, PreemptionMode preemptionMode,
                                                     bool lowPriority, bool internalEngine, bool rootDevice) {
    auto contextId = ++latestContextId;
    auto osContext = OsContext::create(peekExecutionEnvironment().rootDeviceEnvironments[commandStreamReceiver->getRootDeviceIndex()]->osInterface.get(),
                                       contextId, deviceBitfield, engineType, preemptionMode,
                                       lowPriority, internalEngine, rootDevice);
    UNRECOVERABLE_IF(!osContext->isInitialized());
    osContext->incRefInternal();

    registeredEngines.emplace_back(commandStreamReceiver, osContext);

    return osContext;
}

bool MemoryManager::getAllocationData(AllocationData &allocationData, const AllocationProperties &properties, const void *hostPtr, const StorageInfo &storageInfo) {
    UNRECOVERABLE_IF(hostPtr == nullptr && !properties.flags.allocateMemory);
    UNRECOVERABLE_IF(properties.allocationType == GraphicsAllocation::AllocationType::UNKNOWN);

    bool allow64KbPages = false;
    bool allow32Bit = false;
    bool forcePin = properties.flags.forcePin;
    bool mayRequireL3Flush = false;

    switch (properties.allocationType) {
    case GraphicsAllocation::AllocationType::BUFFER:
    case GraphicsAllocation::AllocationType::BUFFER_COMPRESSED:
    case GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY:
    case GraphicsAllocation::AllocationType::CONSTANT_SURFACE:
    case GraphicsAllocation::AllocationType::GLOBAL_SURFACE:
    case GraphicsAllocation::AllocationType::PIPE:
    case GraphicsAllocation::AllocationType::PRINTF_SURFACE:
    case GraphicsAllocation::AllocationType::PRIVATE_SURFACE:
    case GraphicsAllocation::AllocationType::SCRATCH_SURFACE:
    case GraphicsAllocation::AllocationType::WRITE_COMBINED:
        allow64KbPages = true;
        allow32Bit = true;
    default:
        break;
    }

    switch (properties.allocationType) {
    case GraphicsAllocation::AllocationType::SVM_GPU:
    case GraphicsAllocation::AllocationType::SVM_ZERO_COPY:
        allow64KbPages = true;
    default:
        break;
    }

    switch (properties.allocationType) {
    case GraphicsAllocation::AllocationType::BUFFER:
    case GraphicsAllocation::AllocationType::BUFFER_COMPRESSED:
    case GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY:
    case GraphicsAllocation::AllocationType::WRITE_COMBINED:
        forcePin = true;
    default:
        break;
    }

    switch (properties.allocationType) {
    case GraphicsAllocation::AllocationType::BUFFER:
    case GraphicsAllocation::AllocationType::BUFFER_COMPRESSED:
    case GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY:
    case GraphicsAllocation::AllocationType::EXTERNAL_HOST_PTR:
    case GraphicsAllocation::AllocationType::GLOBAL_SURFACE:
    case GraphicsAllocation::AllocationType::IMAGE:
    case GraphicsAllocation::AllocationType::MAP_ALLOCATION:
    case GraphicsAllocation::AllocationType::PIPE:
    case GraphicsAllocation::AllocationType::SHARED_BUFFER:
    case GraphicsAllocation::AllocationType::SHARED_IMAGE:
    case GraphicsAllocation::AllocationType::SHARED_RESOURCE_COPY:
    case GraphicsAllocation::AllocationType::SVM_CPU:
    case GraphicsAllocation::AllocationType::SVM_GPU:
    case GraphicsAllocation::AllocationType::SVM_ZERO_COPY:
    case GraphicsAllocation::AllocationType::WRITE_COMBINED:
        mayRequireL3Flush = true;
    default:
        break;
    }

    switch (properties.allocationType) {
    case GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY:
    case GraphicsAllocation::AllocationType::DEVICE_QUEUE_BUFFER:
    case GraphicsAllocation::AllocationType::EXTERNAL_HOST_PTR:
    case GraphicsAllocation::AllocationType::FILL_PATTERN:
    case GraphicsAllocation::AllocationType::MAP_ALLOCATION:
    case GraphicsAllocation::AllocationType::MCS:
    case GraphicsAllocation::AllocationType::PREEMPTION:
    case GraphicsAllocation::AllocationType::PROFILING_TAG_BUFFER:
    case GraphicsAllocation::AllocationType::SHARED_CONTEXT_IMAGE:
    case GraphicsAllocation::AllocationType::SVM_CPU:
    case GraphicsAllocation::AllocationType::SVM_ZERO_COPY:
    case GraphicsAllocation::AllocationType::TAG_BUFFER:
    case GraphicsAllocation::AllocationType::GLOBAL_FENCE:
    case GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY:
        allocationData.flags.useSystemMemory = true;
    default:
        break;
    }

    allocationData.flags.shareable = properties.flags.shareable;
    allocationData.flags.requiresCpuAccess = GraphicsAllocation::isCpuAccessRequired(properties.allocationType);
    allocationData.flags.allocateMemory = properties.flags.allocateMemory;
    allocationData.flags.allow32Bit = allow32Bit;
    allocationData.flags.allow64kbPages = allow64KbPages;
    allocationData.flags.forcePin = forcePin;
    allocationData.flags.uncacheable = properties.flags.uncacheable;
    allocationData.flags.flushL3 =
        (mayRequireL3Flush ? properties.flags.flushL3RequiredForRead | properties.flags.flushL3RequiredForWrite : 0u);
    allocationData.flags.preferRenderCompressed = GraphicsAllocation::AllocationType::BUFFER_COMPRESSED == properties.allocationType;
    allocationData.flags.multiOsContextCapable = properties.flags.multiOsContextCapable;

    allocationData.hostPtr = hostPtr;
    allocationData.size = properties.size;
    allocationData.type = properties.allocationType;
    allocationData.storageInfo = storageInfo;
    allocationData.alignment = properties.alignment ? properties.alignment : MemoryConstants::preferredAlignment;
    allocationData.imgInfo = properties.imgInfo;

    if (allocationData.flags.allocateMemory) {
        allocationData.hostPtr = nullptr;
    }

    allocationData.rootDeviceIndex = properties.rootDeviceIndex;
    allocationData.flags.resource48Bit = properties.flags.resource48Bit;

    return true;
}

GraphicsAllocation *MemoryManager::allocateGraphicsMemoryInPreferredPool(const AllocationProperties &properties, const void *hostPtr) {
    AllocationData allocationData;
    getAllocationData(allocationData, properties, hostPtr, createStorageInfoFromProperties(properties));
    overrideAllocationData(allocationData, properties);

    AllocationStatus status = AllocationStatus::Error;
    GraphicsAllocation *allocation = allocateGraphicsMemoryInDevicePool(allocationData, status);
    if (allocation) {
        localMemoryUsageBankSelector[properties.rootDeviceIndex]->reserveOnBanks(allocationData.storageInfo.getMemoryBanks(), allocation->getUnderlyingBufferSize());
    }
    if (!allocation && status == AllocationStatus::RetryInNonDevicePool) {
        allocation = allocateGraphicsMemory(allocationData);
    }
    FileLoggerInstance().logAllocation(allocation);
    return allocation;
}

bool MemoryManager::mapAuxGpuVA(GraphicsAllocation *graphicsAllocation) {
    auto index = graphicsAllocation->getRootDeviceIndex();
    if (executionEnvironment.rootDeviceEnvironments[index]->pageTableManager.get()) {
        return executionEnvironment.rootDeviceEnvironments[index]->pageTableManager->updateAuxTable(graphicsAllocation->getGpuAddress(), graphicsAllocation->getDefaultGmm(), true);
    }
    return false;
}

GraphicsAllocation *MemoryManager::allocateGraphicsMemory(const AllocationData &allocationData) {
    if (allocationData.type == GraphicsAllocation::AllocationType::IMAGE || allocationData.type == GraphicsAllocation::AllocationType::SHARED_RESOURCE_COPY) {
        UNRECOVERABLE_IF(allocationData.imgInfo == nullptr);
        return allocateGraphicsMemoryForImage(allocationData);
    }
    if (allocationData.flags.shareable) {
        return allocateShareableMemory(allocationData);
    }
    if (useNonSvmHostPtrAlloc(allocationData.type, allocationData.rootDeviceIndex)) {
        auto allocation = allocateGraphicsMemoryForNonSvmHostPtr(allocationData);
        if (allocation) {
            allocation->setFlushL3Required(allocationData.flags.flushL3);
        }
        return allocation;
    }
    if (useInternal32BitAllocator(allocationData.type) ||
        (force32bitAllocations && allocationData.flags.allow32Bit && is64bit)) {
        return allocate32BitGraphicsMemoryImpl(allocationData);
    }
    if (allocationData.hostPtr) {
        return allocateGraphicsMemoryWithHostPtr(allocationData);
    }
    if (peek64kbPagesEnabled(allocationData.rootDeviceIndex) && allocationData.flags.allow64kbPages) {
        return allocateGraphicsMemory64kb(allocationData);
    }
    return allocateGraphicsMemoryWithAlignment(allocationData);
}

GraphicsAllocation *MemoryManager::allocateGraphicsMemoryForImage(const AllocationData &allocationData) {
    auto gmm = std::make_unique<Gmm>(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmClientContext(), *allocationData.imgInfo, allocationData.storageInfo);

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

HeapIndex MemoryManager::selectHeap(const GraphicsAllocation *allocation, bool hasPointer, bool isFullRangeSVM) {
    if (allocation) {
        if (useInternal32BitAllocator(allocation->getAllocationType())) {
            return HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY;
        }
        if (allocation->is32BitAllocation()) {
            return HeapIndex::HEAP_EXTERNAL;
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

bool MemoryManager::copyMemoryToAllocation(GraphicsAllocation *graphicsAllocation, const void *memoryToCopy, size_t sizeToCopy) {
    if (!graphicsAllocation->getUnderlyingBuffer()) {
        return false;
    }
    memcpy_s(graphicsAllocation->getUnderlyingBuffer(), graphicsAllocation->getUnderlyingBufferSize(), memoryToCopy, sizeToCopy);
    return true;
}

void MemoryManager::waitForEnginesCompletion(GraphicsAllocation &graphicsAllocation) {
    for (auto &engine : getRegisteredEngines()) {
        auto osContextId = engine.osContext->getContextId();
        auto allocationTaskCount = graphicsAllocation.getTaskCount(osContextId);
        if (graphicsAllocation.isUsedByOsContext(osContextId) &&
            allocationTaskCount > *engine.commandStreamReceiver->getTagAddress()) {
            engine.commandStreamReceiver->waitForCompletionWithTimeout(false, TimeoutControls::maxTimeout, allocationTaskCount);
        }
    }
}

void MemoryManager::cleanTemporaryAllocationListOnAllEngines(bool waitForCompletion) {
    for (auto &engine : getRegisteredEngines()) {
        auto csr = engine.commandStreamReceiver;
        if (waitForCompletion) {
            csr->waitForCompletionWithTimeout(false, 0, csr->peekLatestSentTaskCount());
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

bool MemoryManager::useNonSvmHostPtrAlloc(GraphicsAllocation::AllocationType allocationType, uint32_t rootDeviceIndex) {
    bool isExternalHostPtrAlloc = (allocationType == GraphicsAllocation::AllocationType::EXTERNAL_HOST_PTR);
    bool isMapAlloc = (allocationType == GraphicsAllocation::AllocationType::MAP_ALLOCATION);

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
        CPP_ATTRIBUTE_FALLTHROUGH;
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
    int32_t directRingPlacement = DebugManager.flags.DirectSubmissionBufferPlacement.get();
    if (properties.allocationType == GraphicsAllocation::AllocationType::RING_BUFFER &&
        directRingPlacement != -1) {
        if (directRingPlacement == 0) {
            allocationData.flags.requiresCpuAccess = true;
            allocationData.flags.useSystemMemory = false;
        } else {
            allocationData.flags.requiresCpuAccess = false;
            allocationData.flags.useSystemMemory = true;
        }
    }
    int32_t directSemaphorePlacement = DebugManager.flags.DirectSubmissionSemaphorePlacement.get();
    if (properties.allocationType == GraphicsAllocation::AllocationType::SEMAPHORE_BUFFER &&
        directSemaphorePlacement != -1) {
        if (directSemaphorePlacement == 0) {
            allocationData.flags.requiresCpuAccess = true;
            allocationData.flags.useSystemMemory = false;
        } else {
            allocationData.flags.requiresCpuAccess = false;
            allocationData.flags.useSystemMemory = true;
        }
    }
}
} // namespace NEO

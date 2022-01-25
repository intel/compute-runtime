/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/context/context_imp.h"

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/helpers/allocation_extensions.h"
#include "level_zero/core/source/helpers/properties_parser.h"
#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/core/source/image/image.h"
#include "level_zero/core/source/memory/memory_operations_helper.h"

namespace L0 {

ze_result_t ContextImp::destroy() {
    delete this;

    return ZE_RESULT_SUCCESS;
}

ze_result_t ContextImp::getStatus() {
    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(this->driverHandle);
    for (auto device : driverHandleImp->devices) {
        DeviceImp *deviceImp = static_cast<DeviceImp *>(device);
        if (deviceImp->resourcesReleased) {
            return ZE_RESULT_ERROR_DEVICE_LOST;
        }
    }
    return ZE_RESULT_SUCCESS;
}

DriverHandle *ContextImp::getDriverHandle() {
    return this->driverHandle;
}

ContextImp::ContextImp(DriverHandle *driverHandle) {
    this->driverHandle = static_cast<DriverHandleImp *>(driverHandle);
}

void ContextImp::addDeviceAndSubDevices(Device *device) {
    this->devices.insert(std::make_pair(device->toHandle(), device));
    DeviceImp *deviceImp = static_cast<DeviceImp *>(device);
    for (auto subDevice : deviceImp->subDevices) {
        this->addDeviceAndSubDevices(subDevice);
    }
}

ze_result_t ContextImp::allocHostMem(const ze_host_mem_alloc_desc_t *hostDesc,
                                     size_t size,
                                     size_t alignment,
                                     void **ptr) {
    if (NEO::DebugManager.flags.ForceExtendedUSMBufferSize.get() >= 1) {
        size += (MemoryConstants::pageSize * NEO::DebugManager.flags.ForceExtendedUSMBufferSize.get());
    }

    bool relaxedSizeAllowed = NEO::DebugManager.flags.AllowUnrestrictedSize.get();
    if (hostDesc->pNext) {
        const ze_base_desc_t *extendedDesc = reinterpret_cast<const ze_base_desc_t *>(hostDesc->pNext);
        if (extendedDesc->stype == ZE_STRUCTURE_TYPE_RELAXED_ALLOCATION_LIMITS_EXP_DESC) {
            const ze_relaxed_allocation_limits_exp_desc_t *relaxedLimitsDesc =
                reinterpret_cast<const ze_relaxed_allocation_limits_exp_desc_t *>(extendedDesc);
            if (!(relaxedLimitsDesc->flags & ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE)) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
            relaxedSizeAllowed = true;
        }
    }

    if (relaxedSizeAllowed == false &&
        (size > this->driverHandle->devices[0]->getNEODevice()->getDeviceInfo().maxMemAllocSize)) {
        *ptr = nullptr;
        return ZE_RESULT_ERROR_UNSUPPORTED_SIZE;
    }

    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::HOST_UNIFIED_MEMORY,
                                                                           this->rootDeviceIndices,
                                                                           this->deviceBitfields);

    if (hostDesc->flags & ZE_HOST_MEM_ALLOC_FLAG_BIAS_UNCACHED) {
        unifiedMemoryProperties.allocationFlags.flags.locallyUncachedResource = 1;
    }

    auto usmPtr = this->driverHandle->svmAllocsManager->createHostUnifiedMemoryAllocation(size,
                                                                                          unifiedMemoryProperties);
    if (usmPtr == nullptr) {
        return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }

    *ptr = usmPtr;

    return ZE_RESULT_SUCCESS;
}

bool ContextImp::isDeviceDefinedForThisContext(Device *inDevice) {
    return (this->getDevices().find(inDevice->toHandle()) != this->getDevices().end());
}

ze_result_t ContextImp::allocDeviceMem(ze_device_handle_t hDevice,
                                       const ze_device_mem_alloc_desc_t *deviceDesc,
                                       size_t size,
                                       size_t alignment, void **ptr) {
    if (NEO::DebugManager.flags.ForceExtendedUSMBufferSize.get() >= 1) {
        size += (MemoryConstants::pageSize * NEO::DebugManager.flags.ForceExtendedUSMBufferSize.get());
    }

    auto device = Device::fromHandle(hDevice);
    if (isDeviceDefinedForThisContext(device) == false) {
        return ZE_RESULT_ERROR_DEVICE_LOST;
    }

    StructuresLookupTable lookupTable = {};

    lookupTable.relaxedSizeAllowed = NEO::DebugManager.flags.AllowUnrestrictedSize.get();
    auto parseResult = prepareL0StructuresLookupTable(lookupTable, deviceDesc->pNext);

    if (parseResult != ZE_RESULT_SUCCESS) {
        return parseResult;
    }

    auto neoDevice = device->getNEODevice();
    auto rootDeviceIndex = neoDevice->getRootDeviceIndex();
    auto deviceBitfields = this->driverHandle->deviceBitfields;

    deviceBitfields[rootDeviceIndex] = neoDevice->getDeviceBitfield();

    if (lookupTable.isSharedHandle) {
        if (lookupTable.sharedHandleType.isDMABUFHandle) {
            ze_ipc_memory_flags_t flags = {};
            *ptr = getMemHandlePtr(hDevice, lookupTable.sharedHandleType.fd, flags);
            if (nullptr == *ptr) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
        } else {
            UNRECOVERABLE_IF(!lookupTable.sharedHandleType.isNTHandle);
            *ptr = this->driverHandle->importNTHandle(hDevice, lookupTable.sharedHandleType.ntHnadle);
            if (*ptr == nullptr) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
        }
        return ZE_RESULT_SUCCESS;
    }

    if (lookupTable.relaxedSizeAllowed == false &&
        (size > neoDevice->getDeviceInfo().maxMemAllocSize)) {
        *ptr = nullptr;
        return ZE_RESULT_ERROR_UNSUPPORTED_SIZE;
    }

    uint64_t globalMemSize = neoDevice->getDeviceInfo().globalMemSize;

    uint32_t numSubDevices = neoDevice->getNumGenericSubDevices();
    if ((!device->isImplicitScalingCapable()) && (numSubDevices > 1)) {
        globalMemSize = globalMemSize / numSubDevices;
    }
    if (lookupTable.relaxedSizeAllowed && (size > globalMemSize)) {
        *ptr = nullptr;
        return ZE_RESULT_ERROR_UNSUPPORTED_SIZE;
    }

    deviceBitfields[rootDeviceIndex] = neoDevice->getDeviceBitfield();
    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::DEVICE_UNIFIED_MEMORY, this->driverHandle->rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.allocationFlags.flags.shareable = isShareableMemory(deviceDesc->pNext, static_cast<uint32_t>(lookupTable.exportMemory), neoDevice);
    unifiedMemoryProperties.device = neoDevice;
    unifiedMemoryProperties.allocationFlags.flags.compressedHint = isAllocationSuitableForCompression(lookupTable, *device, size);

    if (deviceDesc->flags & ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED) {
        unifiedMemoryProperties.allocationFlags.flags.locallyUncachedResource = 1;
    }

    void *usmPtr =
        this->driverHandle->svmAllocsManager->createUnifiedMemoryAllocation(size, unifiedMemoryProperties);
    if (usmPtr == nullptr) {
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    *ptr = usmPtr;

    return ZE_RESULT_SUCCESS;
}

ze_result_t ContextImp::allocSharedMem(ze_device_handle_t hDevice,
                                       const ze_device_mem_alloc_desc_t *deviceDesc,
                                       const ze_host_mem_alloc_desc_t *hostDesc,
                                       size_t size,
                                       size_t alignment,
                                       void **ptr) {
    if (NEO::DebugManager.flags.ForceExtendedUSMBufferSize.get() >= 1) {
        size += (MemoryConstants::pageSize * NEO::DebugManager.flags.ForceExtendedUSMBufferSize.get());
    }

    auto device = this->devices.begin()->second;
    if (hDevice != nullptr) {
        device = Device::fromHandle(hDevice);
    }
    auto neoDevice = device->getNEODevice();

    bool relaxedSizeAllowed = NEO::DebugManager.flags.AllowUnrestrictedSize.get();
    if (deviceDesc->pNext) {
        const ze_base_desc_t *extendedDesc = reinterpret_cast<const ze_base_desc_t *>(deviceDesc->pNext);
        if (extendedDesc->stype == ZE_STRUCTURE_TYPE_RELAXED_ALLOCATION_LIMITS_EXP_DESC) {
            const ze_relaxed_allocation_limits_exp_desc_t *relaxedLimitsDesc =
                reinterpret_cast<const ze_relaxed_allocation_limits_exp_desc_t *>(extendedDesc);
            if (!(relaxedLimitsDesc->flags & ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE)) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
            relaxedSizeAllowed = true;
        }
    }

    if (relaxedSizeAllowed == false &&
        (size > neoDevice->getDeviceInfo().maxMemAllocSize)) {
        *ptr = nullptr;
        return ZE_RESULT_ERROR_UNSUPPORTED_SIZE;
    }

    uint64_t globalMemSize = neoDevice->getDeviceInfo().globalMemSize;

    uint32_t numSubDevices = neoDevice->getNumGenericSubDevices();
    if ((!device->isImplicitScalingCapable()) && (numSubDevices > 1)) {
        globalMemSize = globalMemSize / numSubDevices;
    }
    if (relaxedSizeAllowed &&
        (size > globalMemSize)) {
        *ptr = nullptr;
        return ZE_RESULT_ERROR_UNSUPPORTED_SIZE;
    }

    auto deviceBitfields = this->deviceBitfields;
    NEO::Device *unifiedMemoryPropertiesDevice = nullptr;
    if (hDevice) {
        device = Device::fromHandle(hDevice);
        if (isDeviceDefinedForThisContext(device) == false) {
            return ZE_RESULT_ERROR_DEVICE_LOST;
        }

        neoDevice = device->getNEODevice();
        auto rootDeviceIndex = neoDevice->getRootDeviceIndex();
        unifiedMemoryPropertiesDevice = neoDevice;
        deviceBitfields[rootDeviceIndex] = neoDevice->getDeviceBitfield();
    }

    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::SHARED_UNIFIED_MEMORY,
                                                                           this->rootDeviceIndices,
                                                                           deviceBitfields);
    unifiedMemoryProperties.device = unifiedMemoryPropertiesDevice;

    if (deviceDesc->flags & ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED) {
        unifiedMemoryProperties.allocationFlags.flags.locallyUncachedResource = 1;
    }

    if (deviceDesc->flags & ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_INITIAL_PLACEMENT) {
        unifiedMemoryProperties.allocationFlags.allocFlags.usmInitialPlacementGpu = 1;
    }

    if (hostDesc->flags & ZE_HOST_MEM_ALLOC_FLAG_BIAS_INITIAL_PLACEMENT) {
        unifiedMemoryProperties.allocationFlags.allocFlags.usmInitialPlacementCpu = 1;
    }

    auto usmPtr =
        this->driverHandle->svmAllocsManager->createSharedUnifiedMemoryAllocation(size,
                                                                                  unifiedMemoryProperties,
                                                                                  static_cast<void *>(neoDevice->getSpecializedDevice<L0::Device>()));

    if (usmPtr == nullptr) {
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    *ptr = usmPtr;

    return ZE_RESULT_SUCCESS;
}

ze_result_t ContextImp::freeMem(const void *ptr) {
    return this->freeMem(ptr, false);
}

ze_result_t ContextImp::freeMem(const void *ptr, bool blocking) {
    auto allocation = this->driverHandle->svmAllocsManager->getSVMAlloc(ptr);
    if (allocation == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    for (auto pairDevice : this->devices) {
        DeviceImp *deviceImp = static_cast<DeviceImp *>(pairDevice.second);

        std::unique_lock<NEO::SpinLock> lock(deviceImp->peerAllocationsMutex);

        auto iter = deviceImp->peerAllocations.allocations.find(ptr);
        if (iter != deviceImp->peerAllocations.allocations.end()) {
            auto peerAllocData = &iter->second;
            auto peerAlloc = peerAllocData->gpuAllocations.getDefaultGraphicsAllocation();
            auto peerPtr = reinterpret_cast<void *>(peerAlloc->getGpuAddress());
            this->driverHandle->svmAllocsManager->freeSVMAlloc(peerPtr, blocking);
            deviceImp->peerAllocations.allocations.erase(iter);
        }
    }

    this->driverHandle->svmAllocsManager->freeSVMAlloc(const_cast<void *>(ptr), blocking);
    if (this->driverHandle->svmAllocsManager->getSvmMapOperation(ptr)) {
        this->driverHandle->svmAllocsManager->removeSvmMapOperation(ptr);
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t ContextImp::freeMemExt(const ze_memory_free_ext_desc_t *pMemFreeDesc,
                                   void *ptr) {

    if (pMemFreeDesc->freePolicy == ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_BLOCKING_FREE) {
        return this->freeMem(ptr, true);
    }
    if (pMemFreeDesc->freePolicy == ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_DEFER_FREE) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    return this->freeMem(ptr, false);
}

ze_result_t ContextImp::makeMemoryResident(ze_device_handle_t hDevice, void *ptr, size_t size) {
    Device *device = L0::Device::fromHandle(hDevice);
    NEO::Device *neoDevice = device->getNEODevice();
    auto allocation = device->getDriverHandle()->getDriverSystemMemoryAllocation(
        ptr,
        size,
        neoDevice->getRootDeviceIndex(),
        nullptr);
    if (allocation == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    NEO::MemoryOperationsHandler *memoryOperationsIface = neoDevice->getRootDeviceEnvironment().memoryOperationsInterface.get();
    auto success = memoryOperationsIface->makeResident(neoDevice, ArrayRef<NEO::GraphicsAllocation *>(&allocation, 1));
    ze_result_t res = changeMemoryOperationStatusToL0ResultType(success);

    if (ZE_RESULT_SUCCESS == res) {
        auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
        if (allocData && allocData->memoryType == InternalMemoryType::SHARED_UNIFIED_MEMORY) {
            DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(device->getDriverHandle());
            std::lock_guard<std::mutex> lock(driverHandleImp->sharedMakeResidentAllocationsLock);
            driverHandleImp->sharedMakeResidentAllocations.insert({ptr, allocation});
        }
    }

    return res;
}

ze_result_t ContextImp::evictMemory(ze_device_handle_t hDevice, void *ptr, size_t size) {
    Device *device = L0::Device::fromHandle(hDevice);
    NEO::Device *neoDevice = device->getNEODevice();
    auto allocation = device->getDriverHandle()->getDriverSystemMemoryAllocation(
        ptr,
        size,
        neoDevice->getRootDeviceIndex(),
        nullptr);
    if (allocation == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    {
        DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(device->getDriverHandle());
        std::lock_guard<std::mutex> lock(driverHandleImp->sharedMakeResidentAllocationsLock);
        driverHandleImp->sharedMakeResidentAllocations.erase(ptr);
    }

    NEO::MemoryOperationsHandler *memoryOperationsIface = neoDevice->getRootDeviceEnvironment().memoryOperationsInterface.get();
    auto success = memoryOperationsIface->evict(neoDevice, *allocation);
    return changeMemoryOperationStatusToL0ResultType(success);
}

ze_result_t ContextImp::makeImageResident(ze_device_handle_t hDevice, ze_image_handle_t hImage) {
    auto alloc = Image::fromHandle(hImage)->getAllocation();

    NEO::Device *neoDevice = L0::Device::fromHandle(hDevice)->getNEODevice();
    NEO::MemoryOperationsHandler *memoryOperationsIface = neoDevice->getRootDeviceEnvironment().memoryOperationsInterface.get();
    auto success = memoryOperationsIface->makeResident(neoDevice, ArrayRef<NEO::GraphicsAllocation *>(&alloc, 1));
    return changeMemoryOperationStatusToL0ResultType(success);
}
ze_result_t ContextImp::evictImage(ze_device_handle_t hDevice, ze_image_handle_t hImage) {
    auto alloc = Image::fromHandle(hImage)->getAllocation();

    NEO::Device *neoDevice = L0::Device::fromHandle(hDevice)->getNEODevice();
    NEO::MemoryOperationsHandler *memoryOperationsIface = neoDevice->getRootDeviceEnvironment().memoryOperationsInterface.get();
    auto success = memoryOperationsIface->evict(neoDevice, *alloc);
    return changeMemoryOperationStatusToL0ResultType(success);
}

ze_result_t ContextImp::getMemAddressRange(const void *ptr,
                                           void **pBase,
                                           size_t *pSize) {
    NEO::SvmAllocationData *allocData = this->driverHandle->svmAllocsManager->getSVMAlloc(ptr);
    if (allocData) {
        NEO::GraphicsAllocation *alloc;
        alloc = allocData->gpuAllocations.getDefaultGraphicsAllocation();
        if (pBase) {
            uint64_t *allocBase = reinterpret_cast<uint64_t *>(pBase);
            *allocBase = alloc->getGpuAddress();
        }

        if (pSize) {
            *pSize = allocData->size;
        }

        return ZE_RESULT_SUCCESS;
    }
    return ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t ContextImp::closeIpcMemHandle(const void *ptr) {
    return this->freeMem(ptr);
}

ze_result_t ContextImp::getIpcMemHandle(const void *ptr,
                                        ze_ipc_mem_handle_t *pIpcHandle) {
    NEO::SvmAllocationData *allocData = this->driverHandle->svmAllocsManager->getSVMAlloc(ptr);
    if (allocData) {
        uint64_t handle = allocData->gpuAllocations.getDefaultGraphicsAllocation()->peekInternalHandle(this->driverHandle->getMemoryManager());
        memcpy_s(reinterpret_cast<void *>(pIpcHandle->data),
                 sizeof(ze_ipc_mem_handle_t),
                 &handle,
                 sizeof(handle));

        return ZE_RESULT_SUCCESS;
    }
    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
}

ze_result_t ContextImp::openIpcMemHandle(ze_device_handle_t hDevice,
                                         ze_ipc_mem_handle_t pIpcHandle,
                                         ze_ipc_memory_flags_t flags,
                                         void **ptr) {
    uint64_t handle = 0u;
    memcpy_s(&handle,
             sizeof(handle),
             reinterpret_cast<void *>(pIpcHandle.data),
             sizeof(handle));

    *ptr = getMemHandlePtr(hDevice, handle, flags);
    if (nullptr == *ptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t EventPoolImp::closeIpcHandle() {
    return this->destroy();
}

ze_result_t EventPoolImp::getIpcHandle(ze_ipc_event_pool_handle_t *pIpcHandle) {
    // L0 uses a vector of ZE_MAX_IPC_HANDLE_SIZE bytes to send the IPC handle, i.e.
    // char data[ZE_MAX_IPC_HANDLE_SIZE];
    // First four bytes (which is of size sizeof(int)) of it contain the file descriptor
    // associated with the dma-buf,
    // Rest is payload to communicate extra info to the other processes.
    // For the event pool, this contains:
    // - the number of events the pool has.
    // - the id for the device used during pool creation

    uint64_t handle = this->eventPoolAllocations->getDefaultGraphicsAllocation()->peekInternalHandle(this->context->getDriverHandle()->getMemoryManager());

    memcpy_s(pIpcHandle->data, sizeof(int), &handle, sizeof(int));

    memcpy_s(pIpcHandle->data + sizeof(int), sizeof(this->numEvents), &this->numEvents, sizeof(this->numEvents));

    uint32_t rootDeviceIndex = this->getDevice()->getRootDeviceIndex();
    memcpy_s(pIpcHandle->data + sizeof(int) + sizeof(this->numEvents),
             sizeof(rootDeviceIndex), &rootDeviceIndex, sizeof(rootDeviceIndex));

    return ZE_RESULT_SUCCESS;
}

ze_result_t ContextImp::openEventPoolIpcHandle(ze_ipc_event_pool_handle_t hIpc,
                                               ze_event_pool_handle_t *phEventPool) {
    uint64_t handle = 0u;
    memcpy_s(&handle, sizeof(int), hIpc.data, sizeof(int));

    size_t numEvents = 0;
    memcpy_s(&numEvents, sizeof(numEvents), hIpc.data + sizeof(int), sizeof(numEvents));

    uint32_t rootDeviceIndex = std::numeric_limits<uint32_t>::max();
    memcpy_s(&rootDeviceIndex, sizeof(rootDeviceIndex),
             hIpc.data + sizeof(int) + sizeof(numEvents), sizeof(rootDeviceIndex));

    Device *device = this->devices.begin()->second;
    auto neoDevice = device->getNEODevice();
    NEO::osHandle osHandle = static_cast<NEO::osHandle>(handle);
    auto &hwHelper = device->getHwHelper();
    const uint32_t eventAlignment = static_cast<uint32_t>(hwHelper.getTimestampPacketAllocatorAlignment());
    uint32_t eventSize = static_cast<uint32_t>(alignUp(EventPacketsCount::eventPackets * hwHelper.getSingleTimestampPacketSize(), eventAlignment));
    size_t alignedSize = alignUp<size_t>(numEvents * eventSize, MemoryConstants::pageSize64k);
    NEO::AllocationProperties unifiedMemoryProperties{rootDeviceIndex,
                                                      alignedSize,
                                                      NEO::AllocationType::BUFFER_HOST_MEMORY,
                                                      systemMemoryBitfield};

    unifiedMemoryProperties.subDevicesBitfield = neoDevice->getDeviceBitfield();
    auto memoryManager = this->getDriverHandle()->getMemoryManager();
    NEO::GraphicsAllocation *alloc = memoryManager->createGraphicsAllocationFromSharedHandle(osHandle,
                                                                                             unifiedMemoryProperties,
                                                                                             false,
                                                                                             true);

    if (alloc == nullptr) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    ze_event_pool_desc_t desc = {};
    auto eventPool = new EventPoolImp(&desc);
    eventPool->context = this;
    eventPool->eventPoolAllocations =
        std::make_unique<NEO::MultiGraphicsAllocation>(static_cast<uint32_t>(this->rootDeviceIndices.size()));
    eventPool->eventPoolAllocations->addAllocation(alloc);
    eventPool->eventPoolPtr = reinterpret_cast<void *>(alloc->getUnderlyingBuffer());
    eventPool->devices.push_back(device);
    eventPool->isImportedIpcPool = true;
    eventPool->setEventSize(eventSize);
    eventPool->setEventAlignment(eventAlignment);

    for (auto currDeviceIndex : this->rootDeviceIndices) {
        if (currDeviceIndex == rootDeviceIndex) {
            continue;
        }

        unifiedMemoryProperties.rootDeviceIndex = currDeviceIndex;
        unifiedMemoryProperties.flags.isUSMHostAllocation = true;
        unifiedMemoryProperties.flags.forceSystemMemory = true;
        unifiedMemoryProperties.flags.allocateMemory = false;
        auto graphicsAllocation = memoryManager->createGraphicsAllocationFromExistingStorage(unifiedMemoryProperties,
                                                                                             eventPool->eventPoolPtr,
                                                                                             eventPool->getAllocation());
        if (!graphicsAllocation) {
            for (auto gpuAllocation : eventPool->getAllocation().getGraphicsAllocations()) {
                memoryManager->freeGraphicsMemory(gpuAllocation);
            }
            memoryManager->freeGraphicsMemory(alloc);

            delete eventPool;

            return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
        }
        eventPool->eventPoolAllocations->addAllocation(graphicsAllocation);
    }

    *phEventPool = eventPool;

    return ZE_RESULT_SUCCESS;
}

ze_result_t ContextImp::getMemAllocProperties(const void *ptr,
                                              ze_memory_allocation_properties_t *pMemAllocProperties,
                                              ze_device_handle_t *phDevice) {
    auto alloc = driverHandle->svmAllocsManager->getSVMAlloc(ptr);
    if (nullptr == alloc) {
        pMemAllocProperties->type = ZE_MEMORY_TYPE_UNKNOWN;
        return ZE_RESULT_SUCCESS;
    }

    pMemAllocProperties->type = Context::parseUSMType(alloc->memoryType);
    pMemAllocProperties->pageSize = alloc->pageSizeForAlignment;
    pMemAllocProperties->id = alloc->getAllocId();

    if (phDevice != nullptr) {
        if (alloc->device == nullptr) {
            *phDevice = nullptr;
        } else {
            auto device = static_cast<NEO::Device *>(alloc->device)->getSpecializedDevice<DeviceImp>();
            DEBUG_BREAK_IF(device == nullptr);
            *phDevice = device->toHandle();
        }
    }

    return handleAllocationExtensions(alloc->gpuAllocations.getDefaultGraphicsAllocation(),
                                      pMemAllocProperties->type,
                                      pMemAllocProperties->pNext,
                                      driverHandle);
}

ze_result_t ContextImp::getImageAllocProperties(Image *image, ze_image_allocation_ext_properties_t *pAllocProperties) {
    NEO::GraphicsAllocation *alloc = image->getAllocation();

    if (alloc == nullptr) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    pAllocProperties->id = 0;

    return handleAllocationExtensions(alloc, ZE_MEMORY_TYPE_DEVICE, pAllocProperties->pNext, driverHandle);
}

ze_result_t ContextImp::createModule(ze_device_handle_t hDevice,
                                     const ze_module_desc_t *desc,
                                     ze_module_handle_t *phModule,
                                     ze_module_build_log_handle_t *phBuildLog) {
    return L0::Device::fromHandle(hDevice)->createModule(desc, phModule, phBuildLog, ModuleType::User);
}

ze_result_t ContextImp::createSampler(ze_device_handle_t hDevice,
                                      const ze_sampler_desc_t *pDesc,
                                      ze_sampler_handle_t *phSampler) {
    return L0::Device::fromHandle(hDevice)->createSampler(pDesc, phSampler);
}

ze_result_t ContextImp::createCommandQueue(ze_device_handle_t hDevice,
                                           const ze_command_queue_desc_t *desc,
                                           ze_command_queue_handle_t *commandQueue) {
    return L0::Device::fromHandle(hDevice)->createCommandQueue(desc, commandQueue);
}

ze_result_t ContextImp::createCommandList(ze_device_handle_t hDevice,
                                          const ze_command_list_desc_t *desc,
                                          ze_command_list_handle_t *commandList) {
    return L0::Device::fromHandle(hDevice)->createCommandList(desc, commandList);
}

ze_result_t ContextImp::createCommandListImmediate(ze_device_handle_t hDevice,
                                                   const ze_command_queue_desc_t *desc,
                                                   ze_command_list_handle_t *commandList) {
    return L0::Device::fromHandle(hDevice)->createCommandListImmediate(desc, commandList);
}

ze_result_t ContextImp::activateMetricGroups(zet_device_handle_t hDevice,
                                             uint32_t count,
                                             zet_metric_group_handle_t *phMetricGroups) {
    return L0::Device::fromHandle(hDevice)->activateMetricGroupsDeferred(count, phMetricGroups);
}

ze_result_t ContextImp::reserveVirtualMem(const void *pStart,
                                          size_t size,
                                          void **pptr) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t ContextImp::freeVirtualMem(const void *ptr,
                                       size_t size) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t ContextImp::queryVirtualMemPageSize(ze_device_handle_t hDevice,
                                                size_t size,
                                                size_t *pagesize) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t ContextImp::createPhysicalMem(ze_device_handle_t hDevice,
                                          ze_physical_mem_desc_t *desc,
                                          ze_physical_mem_handle_t *phPhysicalMemory) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t ContextImp::destroyPhysicalMem(ze_physical_mem_handle_t hPhysicalMemory) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t ContextImp::mapVirtualMem(const void *ptr,
                                      size_t size,
                                      ze_physical_mem_handle_t hPhysicalMemory,
                                      size_t offset,
                                      ze_memory_access_attribute_t access) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t ContextImp::unMapVirtualMem(const void *ptr,
                                        size_t size) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t ContextImp::setVirtualMemAccessAttribute(const void *ptr,
                                                     size_t size,
                                                     ze_memory_access_attribute_t access) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t ContextImp::getVirtualMemAccessAttribute(const void *ptr,
                                                     size_t size,
                                                     ze_memory_access_attribute_t *access,
                                                     size_t *outSize) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t ContextImp::createEventPool(const ze_event_pool_desc_t *desc,
                                        uint32_t numDevices,
                                        ze_device_handle_t *phDevices,
                                        ze_event_pool_handle_t *phEventPool) {
    ze_result_t result;
    EventPool *eventPool = EventPool::create(this->driverHandle, this, numDevices, phDevices, desc, result);

    if (eventPool == nullptr) {
        return result;
    }

    *phEventPool = eventPool->toHandle();

    return ZE_RESULT_SUCCESS;
}

ze_result_t ContextImp::createImage(ze_device_handle_t hDevice,
                                    const ze_image_desc_t *desc,
                                    ze_image_handle_t *phImage) {
    return L0::Device::fromHandle(hDevice)->createImage(desc, phImage);
}

bool ContextImp::isAllocationSuitableForCompression(const StructuresLookupTable &structuresLookupTable, Device &device, size_t allocSize) {
    auto &hwInfo = device.getHwInfo();
    auto &hwHelper = device.getHwHelper();
    auto &l0HwHelper = L0HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    if (!l0HwHelper.usmCompressionSupported(hwInfo) || !hwHelper.isBufferSizeSuitableForCompression(allocSize, hwInfo) || structuresLookupTable.uncompressedHint) {
        return false;
    }

    if (l0HwHelper.forceDefaultUsmCompressionSupport()) {
        return true;
    }

    return structuresLookupTable.compressedHint;
}

} // namespace L0

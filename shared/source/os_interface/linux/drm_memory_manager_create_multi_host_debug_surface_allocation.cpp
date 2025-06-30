/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/memory_manager/memory_pool.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/os_context_linux.h"

namespace NEO {

DrmAllocation *DrmMemoryManager::createMultiHostDebugSurfaceAllocation(const AllocationData &allocationData) {
    if (!isAligned<MemoryConstants::pageSize>(allocationData.size)) {
        return nullptr;
    }
    const auto memoryPool = [&]() -> MemoryPool {
        UNRECOVERABLE_IF(!this->isLocalMemorySupported(allocationData.rootDeviceIndex));
        return MemoryPool::localMemory;
    }();
    auto numTiles = allocationData.storageInfo.getNumBanks();
    auto sizePerTile = allocationData.size;
    auto hostSizeToAllocate = numTiles * sizePerTile;

    auto &drm = this->getDrm(allocationData.rootDeviceIndex);

    auto cpuBasePointer = this->mmapFunction(0, hostSizeToAllocate, PROT_NONE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (cpuBasePointer == MAP_FAILED) {
        return nullptr;
    }

    auto gpuAddress = allocationData.gpuAddress;
    bool addressReserved = false;
    if (gpuAddress == 0) {
        gpuAddress = acquireGpuRange(sizePerTile, allocationData.rootDeviceIndex, HeapIndex::heapStandard);
        addressReserved = true;
    } else {
        gpuAddress = allocationData.gpuAddress;
    }

    auto gmmHelper = getGmmHelper(allocationData.rootDeviceIndex);
    auto canonizedGpuAddress = gmmHelper->canonize(gpuAddress);
    auto allocation = std::make_unique<DrmAllocation>(allocationData.rootDeviceIndex, numTiles, allocationData.type,
                                                      nullptr /*bo*/, cpuBasePointer, canonizedGpuAddress, sizePerTile, memoryPool);

    auto destroyBos = [&allocation]() {
        auto bufferObjects = allocation->getBOs();
        for (auto bufferObject : bufferObjects) {
            if (bufferObject) {
                delete bufferObject;
            }
        }
    };

    allocation->storageInfo = allocationData.storageInfo;
    allocation->setFlushL3Required(true);
    allocation->setUncacheable(true);
    allocation->setMmapPtr(cpuBasePointer);
    allocation->setMmapSize(hostSizeToAllocate);

    auto osContextLinux = static_cast<OsContextLinux *>(allocationData.osContext);
    allocation->setOsContext(osContextLinux);

    if (addressReserved) {
        allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuAddress), sizePerTile);
    }

    for (auto tile = 0u, currentBank = 0u; tile < numTiles; ++tile, ++currentBank) {
        while (!allocationData.storageInfo.memoryBanks.test(currentBank)) {
            ++currentBank;
        }
        PRINT_DEBUGGER_INFO_LOG("Inside for loop allocation tile %u", tile);
        auto boHostPtrPerTile = static_cast<uint8_t *>(cpuBasePointer) + tile * sizePerTile;
        std::unique_ptr<BufferObject, BufferObject::Deleter> bo(this->createBufferObjectInMemoryRegion(allocationData.rootDeviceIndex, nullptr, allocationData.type,
                                                                                                       gpuAddress, sizePerTile, 0u, maxOsContextCount, -1,
                                                                                                       false /*System Memory Pool*/, allocationData.flags.isUSMHostAllocation));
        if (!bo) {
            this->munmapFunction(cpuBasePointer, hostSizeToAllocate);
            // Release previous buffer object for this bank if already allocated
            destroyBos();
            return nullptr;
        }

        auto ioctlHelper = drm.getIoctlHelper();
        uint64_t offset = 0;
        auto mmapOffsetWb = ioctlHelper->getDrmParamValue(DrmParam::mmapOffsetWb);
        if (!ioctlHelper->retrieveMmapOffsetForBufferObject(*bo, mmapOffsetWb, offset)) {
            this->munmapFunction(cpuBasePointer, hostSizeToAllocate);
            // Release previous buffer object for this bank if already allocated
            destroyBos();
            return nullptr;
        }

        [[maybe_unused]] auto retPtr = this->mmapFunction(boHostPtrPerTile, sizePerTile, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, drm.getFileDescriptor(), static_cast<off_t>(offset));
        DEBUG_BREAK_IF(retPtr != boHostPtrPerTile);

        allocation->getBufferObjectToModify(currentBank) = bo.release();
    }

    this->registerLocalMemAlloc(allocation.get(), allocationData.rootDeviceIndex);

    return allocation.release();
}

} // namespace NEO
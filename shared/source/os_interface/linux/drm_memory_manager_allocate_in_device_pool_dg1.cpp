/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/memory_info_impl.h"

#include "opencl/source/memory_manager/memory_banks.h"

namespace NEO {

BufferObject *createBufferObjectInMemoryRegion(Drm *drm, uint64_t gpuAddress, size_t size, uint32_t memoryBanks) {
    auto memoryInfo = static_cast<MemoryInfoImpl *>(drm->getMemoryInfo());
    if (!memoryInfo) {
        return nullptr;
    }

    auto regionClassAndInstance = memoryInfo->getMemoryRegionClassAndInstance(memoryBanks);
    if (regionClassAndInstance.memory_class == MemoryInfoImpl::invalidMemoryRegion()) {
        return nullptr;
    }

    drm_i915_gem_memory_class_instance memRegions{};
    memRegions.memory_class = regionClassAndInstance.memory_class;
    memRegions.memory_instance = regionClassAndInstance.memory_instance;

    drm_i915_gem_object_param regionParam{};
    regionParam.size = 1;
    regionParam.data = reinterpret_cast<uintptr_t>(&memRegions);
    regionParam.param = I915_OBJECT_PARAM | I915_PARAM_MEMORY_REGIONS;

    drm_i915_gem_create_ext_setparam setparamRegion{};
    setparamRegion.base.name = I915_GEM_CREATE_EXT_SETPARAM;
    setparamRegion.param = regionParam;

    drm_i915_gem_create_ext createExt{};
    createExt.size = size;
    createExt.extensions = reinterpret_cast<uintptr_t>(&setparamRegion);
    auto ret = drm->ioctl(DRM_IOCTL_I915_GEM_CREATE_EXT, &createExt);
    if (ret != 0) {
        return nullptr;
    }

    auto bo = new (std::nothrow) BufferObject(drm, createExt.handle, size);
    if (!bo) {
        return nullptr;
    }

    bo->setAddress(gpuAddress);

    return bo;
}

uint64_t getGpuAddress(GraphicsAllocation::AllocationType allocType, GfxPartition *gfxPartition, size_t &sizeAllocated, const void *hostPtr, bool resource48Bit) {
    uint64_t gpuAddress = 0;
    switch (allocType) {
    case GraphicsAllocation::AllocationType::SVM_GPU:
        gpuAddress = reinterpret_cast<uint64_t>(hostPtr);
        sizeAllocated = 0;
        break;
    case GraphicsAllocation::AllocationType::KERNEL_ISA:
    case GraphicsAllocation::AllocationType::INTERNAL_HEAP:
        gpuAddress = GmmHelper::canonize(gfxPartition->heapAllocate(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY, sizeAllocated));
        break;
    case GraphicsAllocation::AllocationType::WRITE_COMBINED:
        sizeAllocated = 0;
        break;
    default:
        auto heapIndex = HeapIndex::HEAP_STANDARD64KB;
        if ((gfxPartition->getHeapLimit(HeapIndex::HEAP_EXTENDED) > 0) && !resource48Bit) {
            heapIndex = HeapIndex::HEAP_EXTENDED;
        }
        gpuAddress = GmmHelper::canonize(gfxPartition->heapAllocate(heapIndex, sizeAllocated));
        break;
    }
    return gpuAddress;
}

bool createDrmAllocation(Drm *drm, DrmAllocation *allocation, uint64_t gpuAddress) {
    std::array<std::unique_ptr<BufferObject>, EngineLimits::maxHandleCount> bos{};
    auto &storageInfo = allocation->storageInfo;
    auto boAddress = gpuAddress;
    for (auto handleId = 0u; handleId < storageInfo.getNumBanks(); handleId++) {
        uint32_t memoryBanks = 1u;
        if (storageInfo.getNumBanks() > 1) {
            memoryBanks &= 1u << handleId;
        }
        auto boSize = alignUp(allocation->getGmm(handleId)->gmmResourceInfo->getSizeAllocation(), MemoryConstants::pageSize64k);
        bos[handleId] = std::unique_ptr<BufferObject>(createBufferObjectInMemoryRegion(drm, boAddress, boSize, memoryBanks));
        if (nullptr == bos[handleId]) {
            return false;
        }
        allocation->getBufferObjectToModify(handleId) = bos[handleId].get();
        if (storageInfo.multiStorage) {
            boAddress += boSize;
        }
    }
    for (auto &bo : bos) {
        bo.release();
    }
    return true;
}

GraphicsAllocation *DrmMemoryManager::allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) {
    status = AllocationStatus::RetryInNonDevicePool;
    if (!this->localMemorySupported[allocationData.rootDeviceIndex] ||
        allocationData.flags.useSystemMemory ||
        (allocationData.flags.allow32Bit && this->force32bitAllocations) ||
        allocationData.type == GraphicsAllocation::AllocationType::SHARED_RESOURCE_COPY) {
        return nullptr;
    }

    std::unique_ptr<Gmm> gmm;
    size_t sizeAligned = 0;
    auto numHandles = allocationData.storageInfo.getNumBanks();
    DEBUG_BREAK_IF(numHandles > 1);
    if (allocationData.type == GraphicsAllocation::AllocationType::IMAGE) {
        allocationData.imgInfo->useLocalMemory = true;
        gmm = std::make_unique<Gmm>(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmClientContext(), *allocationData.imgInfo, allocationData.storageInfo);
        sizeAligned = alignUp(allocationData.imgInfo->size, MemoryConstants::pageSize64k);
    } else {
        bool preferRenderCompressed = (allocationData.type == GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);
        if (allocationData.type == GraphicsAllocation::AllocationType::WRITE_COMBINED) {
            sizeAligned = alignUp(allocationData.size + MemoryConstants::pageSize64k, 2 * MemoryConstants::megaByte) + 2 * MemoryConstants::megaByte;
        } else {
            sizeAligned = alignUp(allocationData.size, MemoryConstants::pageSize64k);
        }
        gmm = std::make_unique<Gmm>(executionEnvironment.rootDeviceEnvironments[allocationData.rootDeviceIndex]->getGmmClientContext(), nullptr, sizeAligned, allocationData.flags.uncacheable,
                                    preferRenderCompressed, false, allocationData.storageInfo);
    }

    auto sizeAllocated = sizeAligned;
    auto gfxPartition = getGfxPartition(allocationData.rootDeviceIndex);
    auto gpuAddress = getGpuAddress(allocationData.type, gfxPartition, sizeAllocated, allocationData.hostPtr, allocationData.flags.resource48Bit);

    auto allocation = std::make_unique<DrmAllocation>(allocationData.rootDeviceIndex, numHandles, allocationData.type, nullptr, nullptr, gpuAddress, sizeAligned, MemoryPool::LocalMemory);
    allocation->setDefaultGmm(gmm.release());
    allocation->storageInfo = allocationData.storageInfo;
    allocation->setFlushL3Required(allocationData.flags.flushL3);
    allocation->setReservedAddressRange(reinterpret_cast<void *>(gpuAddress), sizeAllocated);

    if (!createDrmAllocation(&getDrm(allocationData.rootDeviceIndex), allocation.get(), gpuAddress)) {
        for (auto handleId = 0u; handleId < allocationData.storageInfo.getNumBanks(); handleId++) {
            delete allocation->getGmm(handleId);
        }
        gfxPartition->freeGpuAddressRange(GmmHelper::decanonize(gpuAddress), sizeAllocated);
        status = AllocationStatus::Error;
        return nullptr;
    }
    if (allocationData.type == GraphicsAllocation::AllocationType::WRITE_COMBINED) {
        auto cpuAddress = lockResource(allocation.get());
        auto alignedCpuAddress = alignDown(cpuAddress, 2 * MemoryConstants::megaByte);
        auto offset = ptrDiff(cpuAddress, alignedCpuAddress);
        allocation->setAllocationOffset(offset);
        allocation->setCpuPtrAndGpuAddress(cpuAddress, reinterpret_cast<uint64_t>(alignedCpuAddress));
        DEBUG_BREAK_IF(allocation->storageInfo.multiStorage);
        allocation->getBO()->setAddress(reinterpret_cast<uint64_t>(cpuAddress));
    }
    if (allocationData.flags.requiresCpuAccess) {
        auto cpuAddress = lockResource(allocation.get());
        allocation->setCpuPtrAndGpuAddress(cpuAddress, gpuAddress);
    }
    if (useInternal32BitAllocator(allocationData.type)) {
        allocation->setGpuBaseAddress(GmmHelper::canonize(getInternalHeapBaseAddress(allocationData.rootDeviceIndex, true)));
    }

    status = AllocationStatus::Success;
    return allocation.release();
}

void *DrmMemoryManager::lockResourceInLocalMemoryImpl(GraphicsAllocation &graphicsAllocation) {
    auto bo = static_cast<DrmAllocation &>(graphicsAllocation).getBO();
    if (graphicsAllocation.getAllocationType() == GraphicsAllocation::AllocationType::WRITE_COMBINED) {
        auto addr = lockResourceInLocalMemoryImpl(bo);
        auto alignedAddr = alignUp(addr, MemoryConstants::pageSize64k);
        auto notUsedSize = ptrDiff(alignedAddr, addr);
        munmapFunction(addr, notUsedSize);
        bo->setLockedAddress(alignedAddr);
        return bo->peekLockedAddress();
    }
    return lockResourceInLocalMemoryImpl(bo);
}

void *DrmMemoryManager::lockResourceInLocalMemoryImpl(BufferObject *bo) {
    if (bo == nullptr)
        return nullptr;

    drm_i915_gem_mmap_offset mmapOffset = {};
    mmapOffset.handle = bo->peekHandle();
    mmapOffset.flags = I915_MMAP_OFFSET_WC;
    auto rootDeviceIndex = this->getRootDeviceIndex(bo->drm);

    if (getDrm(rootDeviceIndex).ioctl(DRM_IOCTL_I915_GEM_MMAP_OFFSET, &mmapOffset) != 0) {
        return nullptr;
    }

    auto addr = mmapFunction(nullptr, bo->peekSize(), PROT_WRITE | PROT_READ, MAP_SHARED, getDrm(rootDeviceIndex).getFileDescriptor(), mmapOffset.offset);
    DEBUG_BREAK_IF(addr == nullptr);

    bo->setLockedAddress(addr);

    return bo->peekLockedAddress();
}

void DrmMemoryManager::unlockResourceInLocalMemoryImpl(BufferObject *bo) {
    if (bo == nullptr)
        return;

    auto ret = munmapFunction(bo->peekLockedAddress(), bo->peekSize());
    DEBUG_BREAK_IF(ret != 0);
    UNUSED_VARIABLE(ret);

    bo->setLockedAddress(nullptr);
}

bool DrmMemoryManager::copyMemoryToAllocation(GraphicsAllocation *graphicsAllocation, const void *memoryToCopy, size_t sizeToCopy) {
    if (graphicsAllocation->getUnderlyingBuffer()) {
        return MemoryManager::copyMemoryToAllocation(graphicsAllocation, memoryToCopy, sizeToCopy);
    }
    auto drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    for (auto handleId = 0u; handleId < graphicsAllocation->storageInfo.getNumBanks(); handleId++) {
        auto ptr = lockResourceInLocalMemoryImpl(drmAllocation->getBOs()[handleId]);
        if (!ptr) {
            return false;
        }
        memcpy_s(ptr, graphicsAllocation->getUnderlyingBufferSize(), memoryToCopy, sizeToCopy);
        this->unlockResourceInLocalMemoryImpl(drmAllocation->getBOs()[handleId]);
    }
    return true;
}

uint64_t DrmMemoryManager::getLocalMemorySize(uint32_t rootDeviceIndex) {
    auto memoryInfo = static_cast<MemoryInfoImpl *>(getDrm(rootDeviceIndex).getMemoryInfo());
    if (!memoryInfo) {
        return 0;
    }
    return memoryInfo->getMemoryRegionSize(MemoryBanks::Bank0);
}

} // namespace NEO

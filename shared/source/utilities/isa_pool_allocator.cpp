/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/isa_pool_allocator.h"

#include "shared/source/device/device.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/device_buffers_pool.inl"

namespace NEO {

ISAPool::ISAPool(Device *device, bool isBuiltin, size_t storageSize)
    : BaseType(device, nullptr), isBuiltin(isBuiltin) {
    DEBUG_BREAK_IF(device->getProductHelper().is2MBLocalMemAlignmentEnabled() &&
                   !isAligned(storageSize, MemoryConstants::pageSize2M));

    auto allocationType = isBuiltin ? NEO::AllocationType::kernelIsaInternal : NEO::AllocationType::kernelIsa;
    AllocationProperties allocProperties = {device->getRootDeviceIndex(),
                                            storageSize,
                                            allocationType,
                                            device->getDeviceBitfield()};
    allocProperties.isaPaddingIncluded = true;
    auto graphicsAllocation = this->memoryManager->allocateGraphicsMemoryWithProperties(allocProperties);
    this->initStorage(graphicsAllocation);
}

SharedIsaAllocation *ISAPool::allocateISA(size_t requestedSize) const {
    auto offset = static_cast<size_t>(this->chunkAllocator->allocate(requestedSize));
    if (offset == 0) {
        return nullptr;
    }
    return new SharedIsaAllocation{this->mainStorage.get(), offset - this->params.startingOffset, requestedSize, this->mtx.get()};
}

ISAPoolAllocator::ISAPoolAllocator(Device *device) : device(device) {
    initAllocParams();
}

/**
 * @brief This method allocates SharedIsaAllocation object for a single user (module or program).
 * In first step, it checks if requested size for the ISA is higher than default pool size
 * and creates new ISA pool if it is.
 * Next, it tries to allocate using existing pools.
 * If failed, all existing pools are drained and performs allocation again.
 * If failed, creates another ISA pool and tries to allocate again.
 *
 * @param[in] isBuiltin flag specifying whether ISA will be used for builtin kernels
 * @param[in] size size requested by the client.
 *
 * @return returns SharedIsaAllocation or nullptr if allocation didn't succeeded
 */
SharedIsaAllocation *ISAPoolAllocator::requestGraphicsAllocationForIsa(bool isBuiltin, size_t sizeWithPadding) {
    std::unique_lock lock(allocatorMtx);

    auto maxAllocationSize = getAllocationSize(isBuiltin);

    if (sizeWithPadding > maxAllocationSize) {
        addNewBufferPool(ISAPool(device, isBuiltin, alignToPoolSize(sizeWithPadding)));
    }

    return allocateFromPoolsWithGrowth(
        [&] { return tryAllocateISA(isBuiltin, sizeWithPadding); },
        [&] { addNewBufferPool(ISAPool(device, isBuiltin, alignToPoolSize(getAllocationSize(isBuiltin)))); });
}

/**
 * @brief This method releases SharedIsaAllocation.
 *
 * @param[in] sharedIsaAllocation SharedIsaAllocation to free.
 *
 * @note actual chunk is not released immediately, it's freed during drain call.
 */
void ISAPoolAllocator::freeSharedIsaAllocation(SharedIsaAllocation *sharedIsaAllocation) {
    std::unique_lock lock(allocatorMtx);
    tryFreeFromPoolBuffer(sharedIsaAllocation->getGraphicsAllocation(), sharedIsaAllocation->getOffset(), sharedIsaAllocation->getSize());
    delete sharedIsaAllocation;
}

/**
 * @brief This method iterates over existing pools and tries to allocate shared isa allocation
 * on one of them. It will use only pools with correct isa type.
 *
 * @param[in] isBuiltin flag specifying whether ISA will be used for builtin kernels
 * @param[in] size size requested by the user.
 *
 * @return returns SharedIsaAllocation or nullptr if allocation didn't succeeded
 */
SharedIsaAllocation *ISAPoolAllocator::tryAllocateISA(bool isBuiltin, size_t size) {
    for (auto &isaPoolParent : this->bufferPools) {
        auto &isaPool = static_cast<ISAPool &>(isaPoolParent);
        if (isaPool.isBuiltinPool() == isBuiltin) {
            auto sharedIsaAllocation = isaPool.allocateISA(size);
            if (sharedIsaAllocation != nullptr) {
                return sharedIsaAllocation;
            }
        }
    }
    return nullptr;
}

void ISAPoolAllocator::initAllocParams() {
    if (device->getProductHelper().is2MBLocalMemAlignmentEnabled()) {
        userAllocationSize = MemoryConstants::pageSize2M * 2;
        builtinAllocationSize = MemoryConstants::pageSize2M;
        poolAlignment = MemoryConstants::pageSize2M;
    } else {
        userAllocationSize = MemoryConstants::pageSize2M * 2;
        builtinAllocationSize = MemoryConstants::pageSize64k;
    }
}

size_t ISAPoolAllocator::alignToPoolSize(size_t size) const {
    return alignUp(size, poolAlignment);
}

} // namespace NEO

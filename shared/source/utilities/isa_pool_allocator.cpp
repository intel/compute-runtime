/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/isa_pool_allocator.h"

#include "shared/source/device/device.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/buffer_pool_allocator.inl"

namespace NEO {

ISAPool::ISAPool(Device *device, bool isBuiltin, size_t storageSize)
    : BaseType(device->getMemoryManager(), nullptr), device(device), isBuiltin(isBuiltin) {
    auto allocationType = isBuiltin ? NEO::AllocationType::kernelIsaInternal : NEO::AllocationType::kernelIsa;
    AllocationProperties allocProperties = {device->getRootDeviceIndex(),
                                            storageSize,
                                            allocationType,
                                            device->getDeviceBitfield()};
    allocProperties.isaPaddingIncluded = true;
    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(allocProperties);
    this->chunkAllocator.reset(new NEO::HeapAllocator(params.startingOffset,
                                                      graphicsAllocation ? graphicsAllocation->getUnderlyingBufferSize() : 0u,
                                                      MemoryConstants::pageSize,
                                                      0u));
    this->mainStorage.reset(graphicsAllocation);
    this->mtx = std::make_unique<std::mutex>();
    this->stackVec.push_back(graphicsAllocation);
}

ISAPool::ISAPool(ISAPool &&pool) noexcept : BaseType(std::move(pool)) {
    this->isBuiltin = pool.isBuiltin;
    mtx.reset(pool.mtx.release());
    this->stackVec = std::move(pool.stackVec);
    this->device = pool.device;
}

ISAPool::~ISAPool() {
    if (mainStorage) {
        device->getMemoryManager()->freeGraphicsMemory(mainStorage.release());
    }
}

SharedIsaAllocation *ISAPool::allocateISA(size_t requestedSize) const {
    auto offset = static_cast<size_t>(this->chunkAllocator->allocate(requestedSize));
    if (offset == 0) {
        return nullptr;
    }
    return new SharedIsaAllocation{this->mainStorage.get(), offset - params.startingOffset, requestedSize, mtx.get()};
}

const StackVec<NEO::GraphicsAllocation *, 1> &ISAPool::getAllocationsVector() {
    return stackVec;
}

ISAPoolAllocator::ISAPoolAllocator(Device *device) : device(device) {
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
        addNewBufferPool(ISAPool(device, isBuiltin, sizeWithPadding));
    }

    auto sharedIsaAllocation = tryAllocateISA(isBuiltin, sizeWithPadding);
    if (sharedIsaAllocation) {
        return sharedIsaAllocation;
    }

    drain();

    sharedIsaAllocation = tryAllocateISA(isBuiltin, sizeWithPadding);
    if (sharedIsaAllocation) {
        return sharedIsaAllocation;
    }

    addNewBufferPool(ISAPool(device, isBuiltin, getAllocationSize(isBuiltin)));
    return tryAllocateISA(isBuiltin, sizeWithPadding);
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

} // namespace NEO

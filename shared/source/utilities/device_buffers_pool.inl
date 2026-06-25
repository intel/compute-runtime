/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/buffer_pool_allocator.inl"
#include "shared/source/utilities/device_buffers_pool.h"
#include "shared/source/utilities/heap_allocator.h"

namespace NEO {

template <typename PoolT>
DeviceBuffersPool<PoolT>::DeviceBuffersPool(Device *device, OnChunkFreeCallback onChunkFreeCallback)
    : BaseType(device->getMemoryManager(), std::move(onChunkFreeCallback)), device(device) {}

template <typename PoolT>
DeviceBuffersPool<PoolT>::~DeviceBuffersPool() {
    if (this->mainStorage) {
        this->device->getMemoryManager()->freeGraphicsMemory(this->mainStorage.release());
    }
}

template <typename PoolT>
void DeviceBuffersPool<PoolT>::initStorage(GraphicsAllocation *graphicsAllocation) {
    this->chunkAllocator.reset(new HeapAllocator(this->params.startingOffset,
                                                 graphicsAllocation ? graphicsAllocation->getUnderlyingBufferSize() : 0u,
                                                 MemoryConstants::pageSize,
                                                 0u));
    this->mainStorage.reset(graphicsAllocation);
    this->mtx = std::make_unique<std::mutex>();
    this->poolAllocations.push_back(graphicsAllocation);
}

} // namespace NEO

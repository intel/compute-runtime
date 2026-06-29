/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/task_count_helper.h"
#include "shared/source/utilities/device_buffers_pool.h"
#include "shared/source/utilities/pool_allocator_traits.h"
#include "shared/source/utilities/shared_pool_allocation.h"
#include "shared/source/utilities/stackvec.h"

#include <memory>
#include <mutex>
#include <vector>

namespace NEO {

class Device;
class GraphicsAllocation;
class ProductHelper;

struct DeferredFreeContext {
    volatile TagAddressType *tagAddress = nullptr;
    volatile TagAddressType *ucTagAddress = nullptr;
    uint32_t contextId = 0;
    uint32_t tagOffset = 0;
    uint32_t partitionCount = 1;
};

struct DeferredChunk {
    GraphicsAllocation *parent = nullptr;
    GraphicsAllocation *view = nullptr;
    size_t offset = 0;
    size_t size = 0;
    TaskCountType taskCount = 0;
    uint32_t contextId = 0;
};

template <PoolTraits Traits>
class GenericPool : public DeviceBuffersPool<GenericPool<Traits>> {
    using BaseType = DeviceBuffersPool<GenericPool<Traits>>;

  public:
    GenericPool(Device *device, size_t poolSize);

    GenericPool(GenericPool &&pool) noexcept = default;
    GenericPool &operator=(GenericPool &&) = delete;

    ~GenericPool() override = default;

    [[nodiscard]] SharedPoolAllocation *allocate(size_t size);
};

template <PoolTraits Traits>
class GenericPoolAllocator : public AbstractBuffersAllocator<GenericPool<Traits>, GraphicsAllocation> {
  public:
    explicit GenericPoolAllocator(Device *device);

    [[nodiscard]] SharedPoolAllocation *allocate(size_t size);
    void free(SharedPoolAllocation *allocation);

    size_t getDefaultPoolSize() const { return Traits::defaultPoolSize; }

  private:
    SharedPoolAllocation *allocateFromPools(size_t size);
    size_t alignToPoolSize(size_t size) const;

    Device *device = nullptr;
    std::mutex allocatorMtx;
};

template <PoolTraits Traits>
class GenericViewPool : public DeviceBuffersPool<GenericViewPool<Traits>> {
    using BaseType = DeviceBuffersPool<GenericViewPool<Traits>>;

  public:
    GenericViewPool(Device *device, size_t poolSize);

    GenericViewPool(GenericViewPool &&pool) noexcept = default;
    GenericViewPool &operator=(GenericViewPool &&) = delete;

    ~GenericViewPool() override = default;

    [[nodiscard]] GraphicsAllocation *allocate(size_t size);
};

template <PoolTraits Traits>
class GenericViewPoolAllocator : public AbstractBuffersAllocator<GenericViewPool<Traits>, GraphicsAllocation> {
  public:
    explicit GenericViewPoolAllocator(Device *device);

    [[nodiscard]] GraphicsAllocation *allocate(size_t size);
    [[nodiscard]] GraphicsAllocation *allocate(size_t size, const DeferredFreeContext *ctx);
    void free(GraphicsAllocation *allocation);
    void free(GraphicsAllocation *allocation, TaskCountType taskCount, uint32_t contextId);

    void processDeferredFrees(const DeferredFreeContext *ctx);
    void releasePools();

    size_t getDefaultPoolSize() const {
        if constexpr (requires { Traits::getDefaultPoolSize(); }) {
            return Traits::getDefaultPoolSize();
        }
        return Traits::defaultPoolSize;
    }

    static bool isEnabled(const ProductHelper &productHelper) {
        if constexpr (requires { Traits::isEnabled(productHelper); }) {
            return Traits::isEnabled(productHelper);
        }
        return true;
    }

  protected:
    GraphicsAllocation *allocateFromPools(size_t size);
    size_t alignToPoolSize(size_t size) const;

    void processDeferredFreesUnlocked(const DeferredFreeContext *ctx);
    bool isChunkReady(const DeferredChunk &chunk, const DeferredFreeContext *ctx) const;
    void returnChunkToPool(const DeferredChunk &chunk);

    Device *device = nullptr;
    std::mutex allocatorMtx;
    std::vector<DeferredChunk> deferredChunks;
};

} // namespace NEO

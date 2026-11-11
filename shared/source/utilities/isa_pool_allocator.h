/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/utilities/device_buffers_pool.h"
#include "shared/source/utilities/shared_pool_allocation.h"

#include <mutex>
#include <type_traits>

namespace NEO {
class GraphicsAllocation;
class Device;

using SharedIsaAllocation = SharedPoolAllocation;

// Each shared GA is maintained by single ISAPool
class ISAPool : public DeviceBuffersPool<ISAPool> {
    using BaseType = DeviceBuffersPool<ISAPool>;

  public:
    ISAPool(ISAPool &&pool) noexcept = default;
    ISAPool &operator=(ISAPool &&other) = delete;
    ISAPool(Device *device, bool isBuiltin, size_t storageSize);
    ~ISAPool() override = default;

    SharedIsaAllocation *allocateISA(size_t requestedSize) const;
    bool isBuiltinPool() const { return isBuiltin; }

  private:
    bool isBuiltin;
};

class ISAPoolAllocator : public AbstractBuffersAllocator<ISAPool, GraphicsAllocation> {
  public:
    ISAPoolAllocator(Device *device);
    SharedIsaAllocation *requestGraphicsAllocationForIsa(bool isBuiltin, size_t sizeWithPadding);
    void freeSharedIsaAllocation(SharedIsaAllocation *sharedIsaAllocation);

  private:
    void initAllocParams();
    SharedIsaAllocation *tryAllocateISA(bool isBuiltin, size_t size);

    size_t getAllocationSize(bool isBuiltin) const {
        return isBuiltin ? builtinAllocationSize : userAllocationSize;
    }
    size_t alignToPoolSize(size_t size) const;

    Device *device;
    size_t userAllocationSize = MemoryConstants::pageSize2M * 2;
    size_t builtinAllocationSize = MemoryConstants::pageSize64k;
    size_t poolAlignment = 1u;
    std::mutex allocatorMtx;
};

static_assert(NEO::NonCopyable<ISAPool>);

static_assert(std::is_nothrow_move_constructible_v<ISAPool>, "Pools live in std::vector and have a deleted copy ctor, so vector reallocation"
                                                             "requires a noexcept move ctor - otherwise it fails to compile.");

} // namespace NEO

/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/buffer_pool_allocator.h"
#include "shared/source/utilities/stackvec.h"

#include <memory>
#include <mutex>

namespace NEO {

class Device;
class GraphicsAllocation;

// Common base for pools whose mainStorage is a single device GraphicsAllocation.
// Owns the device handle, the allocation vector and the per-pool mutex, and frees
// the backing allocation on destruction. Derived pools only provide their own
// allocate()/chunk-handle logic and feed the created allocation to initStorage().
template <typename PoolT>
struct DeviceBuffersPool : public AbstractBuffersPool<PoolT, GraphicsAllocation> {
    using BaseType = AbstractBuffersPool<PoolT, GraphicsAllocation>;
    using OnChunkFreeCallback = typename BaseType::OnChunkFreeCallback;

    DeviceBuffersPool(Device *device, OnChunkFreeCallback onChunkFreeCallback);
    DeviceBuffersPool(DeviceBuffersPool &&pool) noexcept = default;
    DeviceBuffersPool &operator=(DeviceBuffersPool &&other) = delete;
    ~DeviceBuffersPool() override;

    const StackVec<GraphicsAllocation *, 1> &getAllocationsVector() {
        return this->poolAllocations;
    }

  protected:
    void initStorage(GraphicsAllocation *graphicsAllocation);

    Device *device{nullptr};
    StackVec<GraphicsAllocation *, 1> poolAllocations;
    std::unique_ptr<std::mutex> mtx;
};

} // namespace NEO

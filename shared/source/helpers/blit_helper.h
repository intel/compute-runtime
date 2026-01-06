/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/device_bitfield.h"
#include "shared/source/helpers/vec.h"

#include <functional>

namespace NEO {

class Device;
class GraphicsAllocation;

enum class BlitOperationResult {
    unsupported,
    fail,
    success,
    gpuHang
};

namespace BlitHelperFunctions {
using BlitMemoryToAllocationFunc = std::function<BlitOperationResult(const Device &device,
                                                                     GraphicsAllocation *memory,
                                                                     size_t offset,
                                                                     const void *hostPtr,
                                                                     const Vec3<size_t> &size)>;
using BlitMemsetAllocationFunc = std::function<BlitOperationResult(const Device &device,
                                                                   GraphicsAllocation *memory,
                                                                   size_t offset,
                                                                   int value,
                                                                   size_t size)>;
extern BlitMemoryToAllocationFunc blitMemoryToAllocation;
extern BlitMemsetAllocationFunc blitMemsetAllocation;
} // namespace BlitHelperFunctions

struct BlitHelper {
    static BlitOperationResult blitMemoryToAllocation(const Device &device, GraphicsAllocation *memory, size_t offset, const void *hostPtr,
                                                      const Vec3<size_t> &size);
    static BlitOperationResult blitMemoryToAllocationBanks(const Device &device, GraphicsAllocation *memory, size_t offset, const void *hostPtr,
                                                           const Vec3<size_t> &size, DeviceBitfield memoryBanks);
    static BlitOperationResult blitMemsetAllocation(const Device &device, GraphicsAllocation *memory, size_t offset, int value,
                                                    size_t size);
    static BlitOperationResult blitMemsetAllocationBanks(const Device &device, GraphicsAllocation *memory, size_t offset, int value,
                                                         size_t size, DeviceBitfield memoryBanks);
};

} // namespace NEO

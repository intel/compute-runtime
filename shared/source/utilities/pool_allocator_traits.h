/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/allocation_properties.h"

#include <concepts>

namespace NEO {

class Device;
class ProductHelper;

template <typename T>
concept PoolTraits = requires(Device *d, size_t s) {
                         { T::allocationType } -> std::convertible_to<AllocationType>;
                         { T::maxAllocationSize } -> std::convertible_to<size_t>;
                         { T::defaultPoolSize } -> std::convertible_to<size_t>;
                         { T::poolAlignment } -> std::convertible_to<size_t>;
                         { T::createAllocationProperties(d, s) } -> std::same_as<AllocationProperties>;
                     };

struct TimestampPoolTraits {
    static constexpr AllocationType allocationType = AllocationType::gpuTimestampDeviceBuffer;
    static constexpr size_t maxAllocationSize = 2 * MemoryConstants::megaByte;
    static constexpr size_t defaultPoolSize = 4 * MemoryConstants::megaByte;
    static constexpr size_t poolAlignment = MemoryConstants::pageSize2M;

    static AllocationProperties createAllocationProperties(Device *device, size_t poolSize);
};

struct GlobalSurfacePoolTraits {
    static constexpr AllocationType allocationType = AllocationType::globalSurface;
    static constexpr size_t maxAllocationSize = 2 * MemoryConstants::megaByte;
    static constexpr size_t defaultPoolSize = 2 * MemoryConstants::megaByte;
    static constexpr size_t poolAlignment = MemoryConstants::pageSize2M;

    static AllocationProperties createAllocationProperties(Device *device, size_t poolSize);
};

struct ConstantSurfacePoolTraits {
    static constexpr AllocationType allocationType = AllocationType::constantSurface;
    static constexpr size_t maxAllocationSize = 2 * MemoryConstants::megaByte;
    static constexpr size_t defaultPoolSize = 2 * MemoryConstants::megaByte;
    static constexpr size_t poolAlignment = MemoryConstants::pageSize2M;

    static AllocationProperties createAllocationProperties(Device *device, size_t poolSize);
};

struct CommandBufferPoolTraits {
    static constexpr AllocationType allocationType = AllocationType::commandBuffer;
    static constexpr size_t maxAllocationSize = MemoryConstants::pageSize2M;
    static constexpr size_t defaultPoolSize = 4 * MemoryConstants::pageSize2M;
    static constexpr size_t poolAlignment = MemoryConstants::pageSize2M;

    static AllocationProperties createAllocationProperties(Device *device, size_t poolSize);
    static bool isEnabled(const ProductHelper &productHelper);
};

struct LinearStreamPoolTraits {
    static constexpr AllocationType allocationType = AllocationType::linearStream;
    static constexpr size_t maxAllocationSize = MemoryConstants::pageSize2M;
    static constexpr size_t defaultPoolSize = 2 * MemoryConstants::pageSize2M;
    static constexpr size_t poolAlignment = MemoryConstants::pageSize2M;

    static AllocationProperties createAllocationProperties(Device *device, size_t poolSize);
};

} // namespace NEO
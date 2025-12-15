/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/device_bitfield.h"
#include "shared/source/helpers/memory_properties_flags.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/unified_memory/unified_memory.h"
#include "shared/source/utilities/stackvec.h"

#include <cstdint>
#include <map>

namespace NEO {

struct UnifiedMemoryProperties {
    UnifiedMemoryProperties(InternalMemoryType memoryType,
                            size_t alignment,
                            const RootDeviceIndicesContainer &rootDeviceIndices,
                            const std::map<uint32_t, DeviceBitfield> &subdeviceBitfields) : memoryType(memoryType),
                                                                                            alignment(alignment),
                                                                                            rootDeviceIndices(rootDeviceIndices),
                                                                                            subdeviceBitfields(subdeviceBitfields){};
    uint32_t getRootDeviceIndex() const;
    InternalMemoryType memoryType = InternalMemoryType::notSpecified;
    MemoryProperties allocationFlags;
    Device *device = nullptr;
    size_t alignment;
    const RootDeviceIndicesContainer &rootDeviceIndices;
    const std::map<uint32_t, DeviceBitfield> &subdeviceBitfields;
    AllocationType requestedAllocationType = AllocationType::unknown;
    bool isInternalAllocation = false;
};

} // namespace NEO

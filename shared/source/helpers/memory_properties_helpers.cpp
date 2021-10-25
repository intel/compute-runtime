/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/memory_properties_helpers_base.inl"

namespace NEO {

void MemoryPropertiesHelper::fillPoliciesInProperties(AllocationProperties &allocationProperties, const MemoryProperties &memoryProperties, const HardwareInfo &hwInfo, bool deviceOnlyVisibilty) {
    fillCachePolicyInProperties(allocationProperties,
                                memoryProperties.flags.locallyUncachedResource,
                                memoryProperties.flags.readOnly,
                                deviceOnlyVisibilty,
                                0);
}

uint32_t MemoryPropertiesHelper::getCacheRegion(const MemoryProperties &memoryProperties) {
    return 0;
}

} // namespace NEO

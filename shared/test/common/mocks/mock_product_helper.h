/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

namespace NEO {

struct MockProductHelper : ProductHelperHw<IGFX_UNKNOWN> {
    using ProductHelper::setupPreemptionSurfaceSize;
    MockProductHelper() = default;

    ADDMETHOD_CONST_NOBASE(is48bResourceNeededForRayTracing, bool, true, ());
    ADDMETHOD_CONST_NOBASE(overrideAllocationCacheable, bool, false, (const AllocationData &allocationData));
    ADDMETHOD_NOBASE(configureHwInfoWddm, int, 0, (const HardwareInfo *inHwInfo, HardwareInfo *outHwInfo, const RootDeviceEnvironment &rootDeviceEnvironment));
    ADDMETHOD_CONST_NOBASE(supportReadOnlyAllocations, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isBlitCopyRequiredForLocalMemory, bool, true, (const RootDeviceEnvironment &rootDeviceEnvironment, const GraphicsAllocation &allocation));
    ADDMETHOD_CONST_NOBASE(isDeviceUsmAllocationReuseSupported, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isHostUsmAllocationReuseSupported, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isUsmPoolAllocatorSupported, bool, false, ());
};
} // namespace NEO

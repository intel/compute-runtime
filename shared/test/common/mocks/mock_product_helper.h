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
    MockProductHelper() = default;

    ADDMETHOD_CONST_NOBASE(is48bResourceNeededForRayTracing, bool, true, ());
    ADDMETHOD_CONST_NOBASE(overrideAllocationCacheable, bool, false, (const AllocationData &allocationData));
    ADDMETHOD_NOBASE(configureHwInfoWddm, int, 0, (const HardwareInfo *inHwInfo, HardwareInfo *outHwInfo, const RootDeviceEnvironment &rootDeviceEnvironment));
};
} // namespace NEO

/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

namespace NEO {
class MockReleaseHelper : public ReleaseHelper {
  public:
    MockReleaseHelper() : ReleaseHelper(0) {}
    ADDMETHOD_CONST_NOBASE(isAdjustWalkOrderAvailable, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isMatrixMultiplyAccumulateSupported, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isPipeControlPriorToNonPipelinedStateCommandsWARequired, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isProgramAllStateComputeCommandFieldsWARequired, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isPrefetchDisablingRequired, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isSplitMatrixMultiplyAccumulateSupported, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isBFloat16ConversionSupported, bool, false, ());
    ADDMETHOD_CONST_NOBASE(getProductMaxPreferredSlmSize, int, 0, (int preferredEnumValue));
    ADDMETHOD_CONST_NOBASE(getMediaFrequencyTileIndex, bool, false, (uint32_t & tileIndex));
    ADDMETHOD_CONST_NOBASE(isResolvingSubDeviceIDNeeded, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isCachingOnCpuAvailable, bool, false, ());
    ADDMETHOD_CONST_NOBASE(shouldAdjustDepth, bool, false, ());
    ADDMETHOD_CONST_NOBASE(getPreferredAllocationMethod, std::optional<GfxMemoryAllocationMethod>, GfxMemoryAllocationMethod::NotDefined;, (AllocationType allocationType));
};
} // namespace NEO
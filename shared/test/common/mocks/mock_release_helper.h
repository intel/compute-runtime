/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

namespace NEO {
class MockReleaseHelper : public ReleaseHelper {
  public:
    MockReleaseHelper() : ReleaseHelper(0) {}
    ADDMETHOD_CONST_NOBASE(isAdjustWalkOrderAvailable, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isMatrixMultiplyAccumulateSupported, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isPipeControlPriorToNonPipelinedStateCommandsWARequired, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isPipeControlPriorToPipelineSelectWaRequired, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isProgramAllStateComputeCommandFieldsWARequired, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isPrefetchDisablingRequired, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isSplitMatrixMultiplyAccumulateSupported, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isBFloat16ConversionSupported, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isAuxSurfaceModeOverrideRequired, bool, false, ());
    ADDMETHOD_CONST_NOBASE(getProductMaxPreferredSlmSize, int, 0, (int preferredEnumValue));
    ADDMETHOD_CONST_NOBASE(getMediaFrequencyTileIndex, bool, false, (uint32_t & tileIndex));
    ADDMETHOD_CONST_NOBASE(isResolvingSubDeviceIDNeeded, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isCachingOnCpuAvailable, bool, false, ());
    ADDMETHOD_CONST_NOBASE(shouldAdjustDepth, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isDirectSubmissionSupported, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isRcsExposureDisabled, bool, false, ());
    ADDMETHOD_CONST_NOBASE(getPreferredAllocationMethod, std::optional<GfxMemoryAllocationMethod>, std::nullopt, (AllocationType allocationType));
    ADDMETHOD_CONST_NOBASE(getSupportedNumGrfs, std::vector<uint32_t>, {128}, ());
    ADDMETHOD_CONST_NOBASE(isBindlessAddressingDisabled, bool, true, ());
    ADDMETHOD_CONST_NOBASE(getNumThreadsPerEu, uint32_t, 8u, ());
    ADDMETHOD_CONST_NOBASE(getThreadsPerEUConfigs, const ThreadsPerEUConfigs, {}, ());
};
} // namespace NEO

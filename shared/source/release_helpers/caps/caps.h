/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

struct Caps {
    bool adjustWalkOrderAvailable = false;
    bool auxSurfaceModeOverrideRequired = false;
    bool bFloat16ConversionSupported = false;
    bool bindlessAddressingDisabled = false;
    bool blitImageAllowedForDepthFormat = false;
    bool deviceConfigStringTileCountIncluded = false;
    bool deviceConfigStringXeCuSegmentIncluded = false;
    bool directSubmissionLightSupported = false;
    bool dotProductAccumulateSystolicSupported = false;
    bool dummyBlitWaRequired = false;
    bool globalBindlessAllocatorEnabled = false;
    bool localOnlyAllowed = false;
    bool numRtStacksPerDssFixedValue = false;
    bool pipeControlPriorToNonPipelinedStateCommandsBaseWARequired = false;
    bool pipeControlPriorToPipelineSelectWaRequired = false;
    bool postImageWriteFlushRequired = false;
    bool preImageReadFlushRequired = false;
    bool programAdditionalStallPriorToBarrierWithTimestamp = false;
    bool programAllStateComputeCommandFieldsWARequired = false;
    bool rayTracingSupported = false;
    bool rcsExposureDisabled = false;
    bool splitMatrixMultiplyAccumulateSupported = false;

    constexpr bool operator==(const Caps &) const = default;
};

} // namespace NEO

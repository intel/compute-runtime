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
    bool deviceConfigStringTileCountIncluded = false;
    bool deviceConfigStringXeCuSegmentIncluded = false;
    bool dotProductAccumulateSystolicSupported = false;
    bool globalBindlessAllocatorEnabled = false;
    bool pipeControlPriorToNonPipelinedStateCommandsBaseWARequired = false;
    bool pipeControlPriorToPipelineSelectWaRequired = false;
    bool programAllStateComputeCommandFieldsWARequired = false;
    bool rcsExposureDisabled = false;
    bool rayTracingSupported = false;
    bool splitMatrixMultiplyAccumulateSupported = false;

    constexpr bool operator==(const Caps &) const = default;
};

} // namespace NEO

/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/release_helpers/caps/caps.h"

#include <concepts>
#include <type_traits>

namespace NEO {

#define NEO_CAP_FIELDS(NEO_COPY_CAP_FUNC)                                        \
    NEO_COPY_CAP_FUNC(adjustWalkOrderAvailable)                                  \
    NEO_COPY_CAP_FUNC(auxSurfaceModeOverrideRequired)                            \
    NEO_COPY_CAP_FUNC(bFloat16ConversionSupported)                               \
    NEO_COPY_CAP_FUNC(blitImageAllowedForDepthFormat)                            \
    NEO_COPY_CAP_FUNC(bindlessAddressingDisabled)                                \
    NEO_COPY_CAP_FUNC(deviceConfigStringTileCountIncluded)                       \
    NEO_COPY_CAP_FUNC(deviceConfigStringXeCuSegmentIncluded)                     \
    NEO_COPY_CAP_FUNC(directSubmissionLightSupported)                            \
    NEO_COPY_CAP_FUNC(dotProductAccumulateSystolicSupported)                     \
    NEO_COPY_CAP_FUNC(dummyBlitWaRequired)                                       \
    NEO_COPY_CAP_FUNC(globalBindlessAllocatorEnabled)                            \
    NEO_COPY_CAP_FUNC(localOnlyAllowed)                                          \
    NEO_COPY_CAP_FUNC(numRtStacksPerDssFixedValue)                               \
    NEO_COPY_CAP_FUNC(pipeControlPriorToNonPipelinedStateCommandsBaseWARequired) \
    NEO_COPY_CAP_FUNC(pipeControlPriorToPipelineSelectWaRequired)                \
    NEO_COPY_CAP_FUNC(postImageWriteFlushRequired)                               \
    NEO_COPY_CAP_FUNC(preImageReadFlushRequired)                                 \
    NEO_COPY_CAP_FUNC(programAllStateComputeCommandFieldsWARequired)             \
    NEO_COPY_CAP_FUNC(programAdditionalStallPriorToBarrierWithTimestamp)         \
    NEO_COPY_CAP_FUNC(rcsExposureDisabled)                                       \
    NEO_COPY_CAP_FUNC(rayTracingSupported)                                       \
    NEO_COPY_CAP_FUNC(splitMatrixMultiplyAccumulateSupported)

template <typename SourceCaps>
constexpr Caps materializeCaps() {
    Caps result{};

#define NEO_COPY_CAP(CAP)                                                               \
    if constexpr (requires { SourceCaps::CAP; }) {                                      \
        using DestinationType = std::remove_cvref_t<decltype(result.CAP)>;              \
        static_assert(std::convertible_to<decltype(SourceCaps::CAP), DestinationType>); \
        result.CAP = SourceCaps::CAP;                                                   \
    }

    NEO_CAP_FIELDS(NEO_COPY_CAP)

#undef NEO_COPY_CAP

    return result;
}

} // namespace NEO

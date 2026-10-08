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
    NEO_COPY_CAP_FUNC(cacheLineSize)                                             \
    NEO_COPY_CAP_FUNC(commandBuffersPreallocatedPerCommandQueue)                 \
    NEO_COPY_CAP_FUNC(defaultMidthreadPreemptionDelayTimer)                      \
    NEO_COPY_CAP_FUNC(kernelBFloat16AtomicCapabilities)                          \
    NEO_COPY_CAP_FUNC(kernelFp16AtomicCapabilities)                              \
    NEO_COPY_CAP_FUNC(maxNumSamplers)                                            \
    NEO_COPY_CAP_FUNC(planarYuvMaxHeight)                                        \
    NEO_COPY_CAP_FUNC(preferredWorkgroupCountPerSubslice)                        \
    NEO_COPY_CAP_FUNC(rtasFormat)                                                \
    NEO_COPY_CAP_FUNC(stackSizePerRay)                                           \
                                                                                 \
    NEO_COPY_CAP_FUNC(max3dImageWidthOrHeight)                                   \
    NEO_COPY_CAP_FUNC(svmCpuAlignment)                                           \
                                                                                 \
    NEO_COPY_CAP_FUNC(adjustWalkOrderAvailable)                                  \
    NEO_COPY_CAP_FUNC(auxSurfaceModeOverrideRequired)                            \
    NEO_COPY_CAP_FUNC(availableSemaphore64)                                      \
    NEO_COPY_CAP_FUNC(bFloat16ConversionSupported)                               \
    NEO_COPY_CAP_FUNC(bindlessAddressingDisabled)                                \
    NEO_COPY_CAP_FUNC(blitImageAllowedForDepthFormat)                            \
    NEO_COPY_CAP_FUNC(block2DLoadSupported)                                      \
    NEO_COPY_CAP_FUNC(block2DStoreSupported)                                     \
    NEO_COPY_CAP_FUNC(cacheFlushPriorToImageReadRequired)                        \
    NEO_COPY_CAP_FUNC(deviceConfigStringTileCountIncluded)                       \
    NEO_COPY_CAP_FUNC(deviceConfigStringXeCuSegmentIncluded)                     \
    NEO_COPY_CAP_FUNC(directSubmissionLightSupported)                            \
    NEO_COPY_CAP_FUNC(dotProductAccumulateSystolicSupported)                     \
    NEO_COPY_CAP_FUNC(dummyBlitWaRequired)                                       \
    NEO_COPY_CAP_FUNC(forceEmuInt32DivRemSPRequired)                             \
    NEO_COPY_CAP_FUNC(ftrXe2Compression)                                         \
    NEO_COPY_CAP_FUNC(globalBindlessAllocatorEnabled)                            \
    NEO_COPY_CAP_FUNC(hvAlign4Required)                                          \
    NEO_COPY_CAP_FUNC(initBuiltinAsyncSupported)                                 \
    NEO_COPY_CAP_FUNC(initDeviceWithFirstSubmissionRequired)                     \
    NEO_COPY_CAP_FUNC(latePreemptionStartSupported)                              \
    NEO_COPY_CAP_FUNC(localOnlyAllowed)                                          \
    NEO_COPY_CAP_FUNC(matrixMultiplyAccumulateSupported)                         \
    NEO_COPY_CAP_FUNC(memSetExtendedPayloadSupported)                            \
    NEO_COPY_CAP_FUNC(numRtStacksPerDssFixedValue)                               \
    NEO_COPY_CAP_FUNC(pipeControlPriorToNonPipelinedStateCommandsBaseWARequired) \
    NEO_COPY_CAP_FUNC(pipeControlPriorToPipelineSelectWaRequired)                \
    NEO_COPY_CAP_FUNC(postImageWriteFlushRequired)                               \
    NEO_COPY_CAP_FUNC(preImageReadFlushRequired)                                 \
    NEO_COPY_CAP_FUNC(programAdditionalStallPriorToBarrierWithTimestamp)         \
    NEO_COPY_CAP_FUNC(programAllStateComputeCommandFieldsWARequired)             \
    NEO_COPY_CAP_FUNC(queryPeerAccess)                                           \
    NEO_COPY_CAP_FUNC(rayTracingSupported)                                       \
    NEO_COPY_CAP_FUNC(rayTracingWalkerAdjustmentRequired)                        \
    NEO_COPY_CAP_FUNC(rcsExposureDisabled)                                       \
    NEO_COPY_CAP_FUNC(reducedSurfaceStateSupported)                              \
    NEO_COPY_CAP_FUNC(scratchSpaceBasePointerInGrf)                              \
    NEO_COPY_CAP_FUNC(singleDispatchRequiredForMultiCCS)                         \
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

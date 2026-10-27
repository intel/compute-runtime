/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace NEO {

struct Caps {
    uint32_t cacheLineSize = 0u;
    uint32_t commandBuffersPreallocatedPerCommandQueue = 0u;
    uint32_t defaultMidthreadPreemptionDelayTimer = 0u; // STATE_COMPUTE_MODE field encoding, not microseconds
    int32_t defaultThreadArbitrationPolicy = 0;
    uint32_t implicitArgsVersion = 0u;
    uint32_t kernelBFloat16AtomicCapabilities = 0u;
    uint32_t kernelFp16AtomicCapabilities = 0u;
    uint32_t maxNumSamplers = 0u;
    uint32_t minimalSimdSize = 0u;
    uint32_t numCacheRegions = 0u;
    uint32_t planarYuvMaxHeight = 0u;
    uint32_t preferredWorkgroupCountPerSubslice = 0u;
    uint32_t rtasFormat = 0u;
    uint32_t shareMemoryWithoutNTHandle = 0u;
    uint32_t stackSizePerRay = 0u;

    size_t max3dImageWidthOrHeight = 0u;
    size_t svmCpuAlignment = 0u;

    bool adjustWalkOrderAvailable = false;
    bool auxSurfaceModeOverrideRequired = false;
    bool availableSemaphore64 = false;
    bool bFloat16ConversionSupported = false;
    bool bindlessAddressingDisabled = false;
    bool blitImageAllowedForDepthFormat = false;
    bool block2DLoadSupported = false;
    bool block2DStoreSupported = false;
    bool bufferPoolAllocatorSupported = false;
    bool cacheFlushPriorToImageReadRequired = false;
    bool deviceConfigStringTileCountIncluded = false;
    bool deviceConfigStringXeCuSegmentIncluded = false;
    bool directSubmissionLightSupported = false;
    bool dotProductAccumulateSystolicSupported = false;
    bool dummyBlitWaRequired = false;
    bool exposingSubdevicesAllowed = false;
    bool forceEmuInt32DivRemSPRequired = false;
    bool ftrXe2Compression = false;
    bool globalBindlessAllocatorEnabled = false;
    bool hvAlign4Required = false;
    bool initBuiltinAsyncSupported = false;
    bool initDeviceWithFirstSubmissionRequired = false;
    bool latePreemptionStartSupported = false;
    bool localOnlyAllowed = false;
    bool matrixMultiplyAccumulateSupported = false;
    bool memSetExtendedPayloadSupported = false;
    bool misalignedUserPtr2WayCoherent = false;
    bool newCoherencyModelSupported = false;
    bool newResidencyModelSupported = false;
    bool numRtStacksPerDssFixedValue = false;
    bool overrideGfxPartitionLayoutForWsl = false;
    bool pipeControlPriorToNonPipelinedStateCommandsBaseWARequired = false;
    bool pipeControlPriorToPipelineSelectWaRequired = false;
    bool postImageWriteFlushRequired = false;
    bool preImageReadFlushRequired = false;
    bool programAdditionalStallPriorToBarrierWithTimestamp = false;
    bool programAllStateComputeCommandFieldsWARequired = false;
    bool queryPeerAccess = false;
    bool rayTracingSupported = false;
    bool rayTracingWalkerAdjustmentRequired = false;
    bool rcsExposureDisabled = false;
    bool readOnlyAllocationsSupported = false;
    bool reducedSurfaceStateSupported = false;
    bool scratchSpaceBasePointerInGrf = false;
    bool sharedUsmAllocationReuseSupported = false;
    bool sharingWith3dOrMediaAllowed = false;
    bool singleDispatchRequiredForMultiCCS = false;
    bool splitMatrixMultiplyAccumulateSupported = false;
    bool storageInfoAdjustmentRequired = false;
    bool useLocalPreferredForCacheableBuffers = false;

    constexpr bool operator==(const Caps &) const = default;
};

} // namespace NEO

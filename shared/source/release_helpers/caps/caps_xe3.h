/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/thread_arbitration_policy.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_ip_version.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/release_helpers/caps/materialize_caps.h"

#include "platforms.h"

#include <optional>

namespace NEO {

struct CapsXe3Core {
    static constexpr uint32_t cacheLineSize = 256u;
    static constexpr uint32_t commandBuffersPreallocatedPerCommandQueue = 2u;
    static constexpr int32_t defaultThreadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobinAfterDependency;
    static constexpr uint32_t kernelFp16AtomicCapabilities = FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps;
    static constexpr uint32_t maxNumSamplers = 16u;
    static constexpr uint32_t minimalSimdSize = 16u;
    static constexpr uint32_t planarYuvMaxHeight = 16128u;
    static constexpr uint32_t rtasFormat = 2u;
    static constexpr uint32_t shareMemoryWithoutNTHandle = 1u;
    static constexpr uint32_t stackSizePerRay = 64u;

    static constexpr size_t max3dImageWidthOrHeight = 16384u;
    static constexpr size_t svmCpuAlignment = MemoryConstants::pageSize64k;

    static constexpr bool bFloat16ConversionSupported = true;
    static constexpr bool blitImageAllowedForDepthFormat = true;
    static constexpr bool block2DLoadSupported = true;
    static constexpr bool block2DStoreSupported = true;
    static constexpr bool bufferPoolAllocatorSupported = true;
    static constexpr bool cacheFlushPriorToImageReadRequired = true;
    static constexpr bool deviceConfigStringTileCountIncluded = true;
    static constexpr bool dotProductAccumulateSystolicSupported = true;
    static constexpr bool exposingSubdevicesAllowed = true;
    static constexpr bool globalBindlessAllocatorEnabled = true;
    static constexpr bool matrixMultiplyAccumulateSupported = true;
    static constexpr bool misalignedUserPtr2WayCoherent = true;
    static constexpr bool newCoherencyModelSupported = true;
    static constexpr bool newResidencyModelSupported = true;
    static constexpr bool numRtStacksPerDssFixedValue = true;
    static constexpr bool preImageReadFlushRequired = true;
    static constexpr bool rcsExposureDisabled = true;
    static constexpr bool scratchSpaceBasePointerInGrf = true;
    static constexpr bool sharingWith3dOrMediaAllowed = true;
};

struct CapsPtlH : CapsXe3Core {
    static constexpr bool rayTracingSupported = true;
    static constexpr bool sharedUsmAllocationReuseSupported = true;
};
struct CapsPtlHA0 : CapsPtlH {};
struct CapsPtlHB0 : CapsPtlH {
    static constexpr bool ftrXe2Compression = true;
};

struct CapsPtlU : CapsXe3Core {
    static constexpr bool ftrXe2Compression = true;
    static constexpr bool rayTracingSupported = true;
    static constexpr bool sharedUsmAllocationReuseSupported = true;
};

struct CapsWcl : CapsXe3Core {
    static constexpr bool ftrXe2Compression = true;
    static constexpr bool sharedUsmAllocationReuseSupported = true;
};

struct CapsNvlS : CapsXe3Core {
    static constexpr bool ftrXe2Compression = true;
};

struct CapsNvlU : CapsXe3Core {
    static constexpr bool ftrXe2Compression = true;
    static constexpr bool rayTracingSupported = true;
};

constexpr std::optional<Caps> resolveCapsPtlH(HardwareIpVersion ipVersion) {
    switch (ipVersion.value) {
    case AOT::PTL_H_A0:
        return materializeCaps<CapsPtlHA0>();
    case AOT::PTL_H_B0:
        return materializeCaps<CapsPtlHB0>();
    default:
        return std::nullopt;
    }
}

constexpr std::optional<Caps> resolveCapsPtlU(HardwareIpVersion ipVersion) {
    switch (ipVersion.value) {
    case AOT::PTL_U_A0:
        return materializeCaps<CapsPtlU>();
    default:
        return std::nullopt;
    }
}

constexpr std::optional<Caps> resolveCapsWcl(HardwareIpVersion ipVersion) {
    switch (ipVersion.value) {
    case AOT::WCL_A0:
    case AOT::WCL_A1:
        return materializeCaps<CapsWcl>();
    default:
        return std::nullopt;
    }
}

constexpr std::optional<Caps> resolveCapsNvlS(HardwareIpVersion ipVersion) {
    switch (ipVersion.value) {
    case AOT::NVL_S_A0:
    case AOT::NVL_S_B0:
        return materializeCaps<CapsNvlS>();
    default:
        return std::nullopt;
    }
}

constexpr std::optional<Caps> resolveCapsNvlU(HardwareIpVersion ipVersion) {
    switch (ipVersion.value) {
    case AOT::NVL_U_A0:
    case AOT::NVL_U_A1:
    case AOT::NVL_U_B0:
        return materializeCaps<CapsNvlU>();
    default:
        return std::nullopt;
    }
}

} // namespace NEO

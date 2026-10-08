/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_ip_version.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/release_helpers/caps/materialize_caps.h"

#include "platforms.h"

#include <optional>

namespace NEO {

struct CapsXeHpcCore {
    static constexpr uint32_t cacheLineSize = 64u;
    static constexpr uint32_t commandBuffersPreallocatedPerCommandQueue = 2u;
    static constexpr uint32_t kernelFp16AtomicCapabilities = FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps;
    static constexpr uint32_t planarYuvMaxHeight = 16128u;
    static constexpr uint32_t rtasFormat = 1u;

    static constexpr size_t max3dImageWidthOrHeight = 16384u;
    static constexpr size_t svmCpuAlignment = MemoryConstants::pageSize64k;

    static constexpr bool bFloat16ConversionSupported = true;
    static constexpr bool bindlessAddressingDisabled = true;
    static constexpr bool block2DLoadSupported = true;
    static constexpr bool block2DStoreSupported = true;
    static constexpr bool dummyBlitWaRequired = true;
    static constexpr bool initBuiltinAsyncSupported = true;
    static constexpr bool initDeviceWithFirstSubmissionRequired = true;
    static constexpr bool localOnlyAllowed = true;
    static constexpr bool numRtStacksPerDssFixedValue = true;
    static constexpr bool rayTracingSupported = true;
    static constexpr bool rcsExposureDisabled = true;
    static constexpr bool scratchSpaceBasePointerInGrf = true;
};

struct CapsPvc : CapsXeHpcCore {
    static constexpr bool dotProductAccumulateSystolicSupported = true;
    static constexpr bool matrixMultiplyAccumulateSupported = true;
};

struct CapsPvcVg : CapsXeHpcCore {};

constexpr std::optional<Caps> resolveCapsPvc(HardwareIpVersion ipVersion) {
    switch (ipVersion.value) {
    case AOT::PVC_XL_A0:
    case AOT::PVC_XL_A0P:
    case AOT::PVC_XT_A0:
    case AOT::PVC_XT_B0:
    case AOT::PVC_XT_B1:
    case AOT::PVC_XT_C0:
        return materializeCaps<CapsPvc>();
    default:
        return std::nullopt;
    }
}

constexpr std::optional<Caps> resolveCapsPvcVg(HardwareIpVersion ipVersion) {
    switch (ipVersion.value) {
    case AOT::PVC_XT_C0_VG:
        return materializeCaps<CapsPvcVg>();
    default:
        return std::nullopt;
    }
}

} // namespace NEO

/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_ip_version.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/release_helpers/caps/materialize_caps.h"

#include "neo_aot_platforms.h"

#include <optional>

namespace NEO {

struct CapsXeHpcCore {
    static constexpr uint32_t kernelFp16AtomicCapabilities = FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps;

    static constexpr bool bFloat16ConversionSupported = true;
    static constexpr bool bindlessAddressingDisabled = true;
    static constexpr bool dummyBlitWaRequired = true;
    static constexpr bool localOnlyAllowed = true;
    static constexpr bool numRtStacksPerDssFixedValue = true;
    static constexpr bool rayTracingSupported = true;
    static constexpr bool rcsExposureDisabled = true;
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

/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_ip_version.h"
#include "shared/source/release_helpers/caps/materialize_caps.h"

#include "neo_aot_platforms.h"

#include <optional>

namespace NEO {

struct CapsXeHpgCore {
    static constexpr bool bFloat16ConversionSupported = true;
    static constexpr bool dotProductAccumulateSystolicSupported = true;
    static constexpr bool globalBindlessAllocatorEnabled = true;
    static constexpr bool pipeControlPriorToNonPipelinedStateCommandsBaseWARequired = true;
    static constexpr bool programAllStateComputeCommandFieldsWARequired = true;
    static constexpr bool rcsExposureDisabled = true;
    static constexpr bool rayTracingSupported = true;
    static constexpr bool splitMatrixMultiplyAccumulateSupported = true;
};

struct CapsDg2G10 : CapsXeHpgCore {};
struct CapsDg2G11 : CapsXeHpgCore {};
struct CapsDg2G12 : CapsXeHpgCore {};

constexpr std::optional<Caps> resolveCapsDg2G10(HardwareIpVersion ipVersion) {
    switch (ipVersion.value) {
    case AOT::DG2_G10_A0:
    case AOT::DG2_G10_A1:
    case AOT::DG2_G10_B0:
    case AOT::DG2_G10_C0:
        return materializeCaps<CapsDg2G10>();
    default:
        return std::nullopt;
    }
}

constexpr std::optional<Caps> resolveCapsDg2G11(HardwareIpVersion ipVersion) {
    switch (ipVersion.value) {
    case AOT::DG2_G11_A0:
    case AOT::DG2_G11_B0:
    case AOT::DG2_G11_B1:
        return materializeCaps<CapsDg2G11>();
    default:
        return std::nullopt;
    }
}

constexpr std::optional<Caps> resolveCapsDg2G12(HardwareIpVersion ipVersion) {
    switch (ipVersion.value) {
    case AOT::DG2_G12_A0:
        return materializeCaps<CapsDg2G12>();
    default:
        return std::nullopt;
    }
}

} // namespace NEO

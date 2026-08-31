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

struct CapsXeLpgCore {
    static constexpr bool bFloat16ConversionSupported = true;
    static constexpr bool directSubmissionLightSupported = true;
    static constexpr bool globalBindlessAllocatorEnabled = true;
    static constexpr bool localOnlyAllowed = true;
    static constexpr bool numRtStacksPerDssFixedValue = true;
    static constexpr bool rayTracingSupported = true;
};

struct CapsMtlU : CapsXeLpgCore {
    static constexpr bool auxSurfaceModeOverrideRequired = true;
    static constexpr bool blitImageAllowedForDepthFormat = true;
    static constexpr bool dummyBlitWaRequired = true;
};
struct CapsMtlUA0 : CapsMtlU {
    static constexpr bool pipeControlPriorToNonPipelinedStateCommandsBaseWARequired = true;
    static constexpr bool programAllStateComputeCommandFieldsWARequired = true;
};
struct CapsMtlUB0 : CapsMtlU {};

struct CapsMtlH : CapsXeLpgCore {
    static constexpr bool auxSurfaceModeOverrideRequired = true;
    static constexpr bool blitImageAllowedForDepthFormat = true;
    static constexpr bool dummyBlitWaRequired = true;
};
struct CapsMtlHA0 : CapsMtlH {
    static constexpr bool pipeControlPriorToNonPipelinedStateCommandsBaseWARequired = true;
    static constexpr bool programAllStateComputeCommandFieldsWARequired = true;
};
struct CapsMtlHB0 : CapsMtlH {};

struct CapsArlH : CapsXeLpgCore {
    static constexpr bool adjustWalkOrderAvailable = true;
    static constexpr bool dotProductAccumulateSystolicSupported = true;
    static constexpr bool matrixMultiplyAccumulateSupported = true;
    static constexpr bool pipeControlPriorToPipelineSelectWaRequired = true;
};

constexpr std::optional<Caps> resolveCapsMtlU(HardwareIpVersion ipVersion) {
    switch (ipVersion.value) {
    case AOT::MTL_U_A0:
        return materializeCaps<CapsMtlUA0>();
    case AOT::MTL_U_B0:
        return materializeCaps<CapsMtlUB0>();
    default:
        return std::nullopt;
    }
}

constexpr std::optional<Caps> resolveCapsMtlH(HardwareIpVersion ipVersion) {
    switch (ipVersion.value) {
    case AOT::MTL_H_A0:
        return materializeCaps<CapsMtlHA0>();
    case AOT::MTL_H_B0:
        return materializeCaps<CapsMtlHB0>();
    default:
        return std::nullopt;
    }
}

constexpr std::optional<Caps> resolveCapsArlH(HardwareIpVersion ipVersion) {
    switch (ipVersion.value) {
    case AOT::ARL_H_A0:
    case AOT::ARL_H_B0:
        return materializeCaps<CapsArlH>();
    default:
        return std::nullopt;
    }
}

} // namespace NEO

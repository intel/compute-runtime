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

struct CapsXe3pCore {
    static constexpr uint32_t kernelBFloat16AtomicCapabilities = FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps | FpAtomicExtFlags::addAtomicCaps;
    static constexpr uint32_t kernelFp16AtomicCapabilities = FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps | FpAtomicExtFlags::addAtomicCaps;

    static constexpr bool bFloat16ConversionSupported = true;
    static constexpr bool bindlessAddressingDisabled = true;
    static constexpr bool blitImageAllowedForDepthFormat = true;
    static constexpr bool deviceConfigStringTileCountIncluded = true;
    static constexpr bool dotProductAccumulateSystolicSupported = true;
    static constexpr bool globalBindlessAllocatorEnabled = true;
    static constexpr bool matrixMultiplyAccumulateSupported = true;
    static constexpr bool postImageWriteFlushRequired = true;
    static constexpr bool rcsExposureDisabled = true;
};

struct CapsCri : CapsXe3pCore {
    static constexpr bool deviceConfigStringXeCuSegmentIncluded = true;
};
struct CapsNvlP : CapsXe3pCore {
    static constexpr bool ftrXe2Compression = true;
    static constexpr bool preImageReadFlushRequired = true;
    static constexpr bool rayTracingSupported = true;
};
struct CapsNvlPA0 : CapsNvlP {};
struct CapsNvlPB0 : CapsNvlP {
    static constexpr bool availableSemaphore64 = true;
};

constexpr std::optional<Caps> resolveCapsCri(HardwareIpVersion ipVersion) {
    switch (ipVersion.value) {
    case AOT::CRI_A0:
        return materializeCaps<CapsCri>();
    default:
        return std::nullopt;
    }
}

constexpr std::optional<Caps> resolveCapsNvlP(HardwareIpVersion ipVersion) {
    switch (ipVersion.value) {
    case AOT::NVL_P_A0:
        return materializeCaps<CapsNvlPA0>();
    case AOT::NVL_P_B0:
        return materializeCaps<CapsNvlPB0>();
    default:
        return std::nullopt;
    }
}

} // namespace NEO

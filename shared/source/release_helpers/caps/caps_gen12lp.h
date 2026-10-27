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

#include <optional>

namespace NEO {

struct CapsGen12Lp {
    static constexpr uint32_t cacheLineSize = 64u;
    static constexpr int32_t defaultThreadArbitrationPolicy = ThreadArbitrationPolicy::AgeBased;
    static constexpr uint32_t kernelFp16AtomicCapabilities = FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps;
    static constexpr uint32_t maxNumSamplers = 16u;
    static constexpr uint32_t minimalSimdSize = 8u;
    static constexpr uint32_t planarYuvMaxHeight = 16352u;

    static constexpr size_t max3dImageWidthOrHeight = 2048u;
    static constexpr size_t svmCpuAlignment = MemoryConstants::pageSize2M;

    static constexpr bool bFloat16ConversionSupported = true;
    static constexpr bool bindlessAddressingDisabled = true;
    static constexpr bool exposingSubdevicesAllowed = true;
    static constexpr bool hvAlign4Required = true;
    static constexpr bool localOnlyAllowed = true;
    static constexpr bool numRtStacksPerDssFixedValue = true;
    static constexpr bool scratchSpaceBasePointerInGrf = true;
    static constexpr bool sharingWith3dOrMediaAllowed = true;
};

struct CapsTgl : CapsGen12Lp {
    static constexpr uint32_t shareMemoryWithoutNTHandle = 1u;
};
struct CapsRkl : CapsGen12Lp {
    static constexpr bool forceEmuInt32DivRemSPRequired = true;
    static constexpr uint32_t shareMemoryWithoutNTHandle = 1u;
};
struct CapsAdlS : CapsGen12Lp {
    static constexpr uint32_t shareMemoryWithoutNTHandle = 1u;
};
struct CapsAdlP : CapsGen12Lp {
    static constexpr uint32_t shareMemoryWithoutNTHandle = 1u;
};
struct CapsAdlN : CapsGen12Lp {
    static constexpr uint32_t shareMemoryWithoutNTHandle = 1u;
};
struct CapsDg1 : CapsGen12Lp {
    static constexpr bool overrideGfxPartitionLayoutForWsl = true;
    static constexpr bool storageInfoAdjustmentRequired = true;
};

constexpr std::optional<Caps> resolveCapsTgl(HardwareIpVersion) {
    return materializeCaps<CapsTgl>();
}
constexpr std::optional<Caps> resolveCapsRkl(HardwareIpVersion) {
    return materializeCaps<CapsRkl>();
}
constexpr std::optional<Caps> resolveCapsAdlS(HardwareIpVersion) {
    return materializeCaps<CapsAdlS>();
}
constexpr std::optional<Caps> resolveCapsAdlP(HardwareIpVersion) {
    return materializeCaps<CapsAdlP>();
}
constexpr std::optional<Caps> resolveCapsAdlN(HardwareIpVersion) {
    return materializeCaps<CapsAdlN>();
}
constexpr std::optional<Caps> resolveCapsDg1(HardwareIpVersion) {
    return materializeCaps<CapsDg1>();
}

} // namespace NEO

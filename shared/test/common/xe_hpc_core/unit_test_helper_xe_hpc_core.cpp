/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/xe_hpc_core/hw_info.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/unit_test_helper.inl"
#include "shared/test/common/helpers/unit_test_helper_xehp_and_later.inl"

using Family = NEO::XE_HPC_COREFamily;

namespace NEO {

template <>
const AuxTranslationMode UnitTestHelper<Family>::requiredAuxTranslationMode = AuxTranslationMode::Blit;

template <>
uint64_t UnitTestHelper<Family>::getAtomicMemoryAddress(const Family::MI_ATOMIC &atomic) {
    return atomic.getMemoryAddress();
}

template <>
const bool UnitTestHelper<Family>::tiledImagesSupported = false;

template <>
const uint32_t UnitTestHelper<Family>::smallestTestableSimdSize = 16;

template <>
uint32_t UnitTestHelper<Family>::getAppropriateThreadArbitrationPolicy(int32_t policy) {
    using STATE_COMPUTE_MODE = typename Family::STATE_COMPUTE_MODE;
    switch (policy) {
    case ThreadArbitrationPolicy::RoundRobin:
        return STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_ROUND_ROBIN;
    case ThreadArbitrationPolicy::AgeBased:
        return STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_OLDEST_FIRST;
    case ThreadArbitrationPolicy::RoundRobinAfterDependency:
        return STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_STALL_BASED_ROUND_ROBIN;
    default:
        return STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_HW_DEFAULT;
    }
}

template <>
bool UnitTestHelper<Family>::requiresTimestampPacketsInSystemMemory(HardwareInfo &hwInfo) {
    return false;
}

template <>
bool UnitTestHelper<Family>::isAdditionalSynchronizationRequired() {
    return true;
}

template <>
bool UnitTestHelper<Family>::isAdditionalMiSemaphoreWaitRequired(const HardwareInfo &hwInfo) {
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    auto programGlobalFenceAsMiMemFenceCommandInCommandStream = hwInfoConfig.isGlobalFenceAsMiMemFenceCommandInCommandStreamRequired(hwInfo);
    if (DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.get() != -1) {
        programGlobalFenceAsMiMemFenceCommandInCommandStream = !!DebugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.get();
    }
    return !programGlobalFenceAsMiMemFenceCommandInCommandStream;
}

template <>
const bool UnitTestHelper<Family>::additionalMiFlushDwRequired = false;

template <>
uint32_t UnitTestHelper<Family>::getDebugModeRegisterOffset() {
    return 0x20d8;
}

template <>
uint32_t UnitTestHelper<Family>::getDebugModeRegisterValue() {
    return (1u << 5) | (1u << 21);
}

template <>
uint32_t UnitTestHelper<Family>::getTdCtlRegisterOffset() {
    return 0xe400;
}

template <>
uint32_t UnitTestHelper<Family>::getTdCtlRegisterValue() {
    return (1u << 7) | (1u << 4) | (1u << 2) | (1u << 0);
}

template struct UnitTestHelper<Family>;

} // namespace NEO

/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"
#include "shared/source/xe_hpg_core/hw_info.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/unit_test_helper.inl"
#include "shared/test/common/helpers/unit_test_helper_xe_hpg_and_xe_hpc.inl"
#include "shared/test/common/helpers/unit_test_helper_xehp_and_later.inl"

using Family = NEO::XeHpgCoreFamily;

namespace NEO {

template <>
uint64_t UnitTestHelper<Family>::getAtomicMemoryAddress(const typename Family::MI_ATOMIC &atomic) {
    return atomic.getMemoryAddress() | ((static_cast<uint64_t>(atomic.getMemoryAddressHigh())) << 32);
}

template <>
uint32_t UnitTestHelper<Family>::getAppropriateThreadArbitrationPolicy(int32_t policy) {
    return static_cast<uint32_t>(policy);
}

template <>
const bool UnitTestHelper<Family>::additionalMiFlushDwRequired = true;

template <>
bool UnitTestHelperWithHeap<Family>::getDisableFusionStateFromFrontEndCommand(const typename Family::FrontEndStateCommand &feCmd) {
    return feCmd.getFusedEuDispatch();
}

template <>
bool UnitTestHelper<Family>::isAdditionalMiSemaphoreWaitRequired(const RootDeviceEnvironment &rootDeviceEnvironment) {
    return false;
}

template <>
bool UnitTestHelper<Family>::isAdditionalSynchronizationRequired() {
    return false;
}

template <>
bool UnitTestHelper<Family>::requiresTimestampPacketsInSystemMemory(HardwareInfo &hwInfo) {
    return true;
}

template <>
void UnitTestHelper<Family>::skipStatePrefetch(GenCmdList::iterator &iter) {}

template struct UnitTestHelper<Family>;
template struct UnitTestHelperWithHeap<Family>;

template uint64_t UnitTestHelper<Family>::getWalkerActivePostSyncAddress<Family::COMPUTE_WALKER>(Family::COMPUTE_WALKER *walkerCmd);
} // namespace NEO

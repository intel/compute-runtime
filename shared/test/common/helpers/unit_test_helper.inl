/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.h"

#include "gtest/gtest.h"

namespace NEO {

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::isPageTableManagerSupported(const HardwareInfo &hwInfo) {
    return false;
}

template <typename GfxFamily>
inline uint32_t UnitTestHelper<GfxFamily>::getAppropriateThreadArbitrationPolicy(int32_t policy) {
    return static_cast<uint32_t>(policy);
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::isPipeControlWArequired(const HardwareInfo &hwInfo) {
    return false;
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::isAdditionalSynchronizationRequired() {
    return false;
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::isAdditionalMiSemaphoreWaitRequired(const HardwareInfo &hwInfo) {
    return false;
}

template <typename GfxFamily>
inline uint64_t UnitTestHelper<GfxFamily>::getAtomicMemoryAddress(const typename GfxFamily::MI_ATOMIC &atomic) {
    return atomic.getMemoryAddress() | ((static_cast<uint64_t>(atomic.getMemoryAddressHigh())) << 32);
}

template <typename GfxFamily>
inline bool UnitTestHelper<GfxFamily>::requiresTimestampPacketsInSystemMemory(HardwareInfo &hwInfo) {
    return true;
}

template <typename GfxFamily>
void UnitTestHelper<GfxFamily>::setExtraMidThreadPreemptionFlag(HardwareInfo &hwInfo, bool value) {
    hwInfo.featureTable.flags.ftrGpGpuMidThreadLevelPreempt = value;
}

template <typename GfxFamily>
const bool UnitTestHelper<GfxFamily>::tiledImagesSupported = true;

template <typename GfxFamily>
const uint32_t UnitTestHelper<GfxFamily>::smallestTestableSimdSize = 8;

template <typename GfxFamily>
const AuxTranslationMode UnitTestHelper<GfxFamily>::requiredAuxTranslationMode = AuxTranslationMode::Builtin;

template <typename GfxFamily>
const bool UnitTestHelper<GfxFamily>::useFullRowForLocalIdsGeneration = false;

template <typename GfxFamily>
const bool UnitTestHelper<GfxFamily>::additionalMiFlushDwRequired = false;

template <typename GfxFamily>
inline uint64_t UnitTestHelper<GfxFamily>::getPipeControlPostSyncAddress(const typename GfxFamily::PIPE_CONTROL &pipeControl) {
    uint64_t gpuAddress = pipeControl.getAddress();
    uint64_t gpuAddressHigh = pipeControl.getAddressHigh();

    return (gpuAddressHigh << 32) | gpuAddress;
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::timestampRegisterHighAddress() {
    return false;
}

template <typename GfxFamily>
void UnitTestHelper<GfxFamily>::validateSbaMocs(uint32_t expectedMocs, CommandStreamReceiver &csr) {
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;

    HardwareParse hwParse;
    hwParse.parseCommands<GfxFamily>(csr.getCS(0), 0);
    auto itorCmd = reverseFind<STATE_BASE_ADDRESS *>(hwParse.cmdList.rbegin(), hwParse.cmdList.rend());
    EXPECT_NE(hwParse.cmdList.rend(), itorCmd);
    auto sba = genCmdCast<STATE_BASE_ADDRESS *>(*itorCmd);
    EXPECT_NE(nullptr, sba);

    auto mocs = sba->getStatelessDataPortAccessMemoryObjectControlState();

    EXPECT_EQ(expectedMocs, mocs);
}

} // namespace NEO

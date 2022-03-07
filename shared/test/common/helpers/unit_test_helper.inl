/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

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

} // namespace NEO

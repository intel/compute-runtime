/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::isL3ConfigProgrammable() {
    return true;
};

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::evaluateDshUsage(size_t sizeBeforeEnqueue, size_t sizeAfterEnqueue, Kernel *kernel) {
    if (sizeBeforeEnqueue != sizeAfterEnqueue) {
        return true;
    }
    return false;
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::isPageTableManagerSupported(const HardwareInfo &hwInfo) {
    return false;
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::isTimestampPacketWriteSupported() {
    return false;
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::isExpectMemoryNotEqualSupported() {
    return false;
}

template <typename GfxFamily>
uint32_t UnitTestHelper<GfxFamily>::getDefaultSshUsage() {
    return 0;
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::evaluateGshAddressForScratchSpace(uint64_t usedScratchGpuAddress, uint64_t retrievedGshAddress) {
    return usedScratchGpuAddress == retrievedGshAddress;
}

template <typename GfxFamily>
bool UnitTestHelper<GfxFamily>::isPipeControlWArequired(const HardwareInfo &hwInfo) {
    return false;
}

template <typename GfxFamily>
inline uint64_t UnitTestHelper<GfxFamily>::getMemoryAddress(const typename GfxFamily::MI_ATOMIC &atomic) {
    return atomic.getMemoryAddress() | ((static_cast<uint64_t>(atomic.getMemoryAddressHigh())) << 32);
}

template <typename GfxFamily>
const bool UnitTestHelper<GfxFamily>::tiledImagesSupported = true;

} // namespace NEO

/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace OCLRT {

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
} // namespace OCLRT

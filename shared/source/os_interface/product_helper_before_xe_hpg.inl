/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper_hw.h"

namespace NEO {

template <PRODUCT_FAMILY gfxProduct>
uint64_t ProductHelperHw<gfxProduct>::getCrossDeviceSharedMemCapabilities() const {
    return 0;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::obtainBlitterPreference(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isBlitterFullySupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isNewResidencyModelSupported() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::heapInLocalMem(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
void ProductHelperHw<gfxProduct>::setCapabilityCoherencyFlag(const HardwareInfo &hwInfo, bool &coherencyFlag) const {
    coherencyFlag = true;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::canShareMemoryWithoutNTHandle() const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool ProductHelperHw<gfxProduct>::isTimestampWaitSupportedForEvents() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t ProductHelperHw<gfxProduct>::getCommandBuffersPreallocatedPerCommandQueue() const {
    return 0u;
}

} // namespace NEO

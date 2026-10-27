/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/product_helper_gen12lp.inl"
#include "shared/source/os_interface/product_helper.inl"
#include "shared/source/os_interface/product_helper_before_xe2.inl"

namespace NEO {
template <>
bool ProductHelperHw<gfxProduct>::isPageTableManagerSupported(const HardwareInfo &hwInfo) const {
    return hwInfo.capabilityTable.ftrRenderCompressedBuffers || hwInfo.capabilityTable.ftrRenderCompressedImages;
}

template <>
bool ProductHelperHw<gfxProduct>::isHostUsmPoolAllocatorSupported() const {
    return false;
}

template <>
bool ProductHelperHw<gfxProduct>::isDeviceUsmPoolAllocatorSupported() const {
    return false;
}

template <>
bool ProductHelperHw<gfxProduct>::isDeviceUsmAllocationReuseSupported() const {
    return false;
}

template <>
bool ProductHelperHw<gfxProduct>::isHostUsmAllocationReuseSupported() const {
    return false;
}

template <>
bool ProductHelperHw<gfxProduct>::isFlushBetweenBlitsRequired() const {
    return false;
}

} // namespace NEO

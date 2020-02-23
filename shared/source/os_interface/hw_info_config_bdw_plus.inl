/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {
template <PRODUCT_FAMILY gfxProduct>
uint64_t HwInfoConfigHw<gfxProduct>::getHostMemCapabilities() {
    return (UNIFIED_SHARED_MEMORY_ACCESS | UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS);
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t HwInfoConfigHw<gfxProduct>::getDeviceMemCapabilities() {
    return (UNIFIED_SHARED_MEMORY_ACCESS | UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS);
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t HwInfoConfigHw<gfxProduct>::getSingleDeviceSharedMemCapabilities() {
    return (UNIFIED_SHARED_MEMORY_ACCESS | UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS);
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t HwInfoConfigHw<gfxProduct>::getCrossDeviceSharedMemCapabilities() {
    return 0;
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t HwInfoConfigHw<gfxProduct>::getSharedSystemMemCapabilities() {
    return 0;
}

} // namespace NEO

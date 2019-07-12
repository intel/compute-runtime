/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/hw_info_config.h"

namespace NEO {
template <PRODUCT_FAMILY gfxProduct>
cl_unified_shared_memory_capabilities_intel HwInfoConfigHw<gfxProduct>::getHostMemCapabilities() {
    return (CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS_INTEL);
}

template <PRODUCT_FAMILY gfxProduct>
cl_unified_shared_memory_capabilities_intel HwInfoConfigHw<gfxProduct>::getDeviceMemCapabilities() {
    return (CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS_INTEL);
}

template <PRODUCT_FAMILY gfxProduct>
cl_unified_shared_memory_capabilities_intel HwInfoConfigHw<gfxProduct>::getSingleDeviceSharedMemCapabilities() {
    return (CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS_INTEL);
}

template <PRODUCT_FAMILY gfxProduct>
cl_unified_shared_memory_capabilities_intel HwInfoConfigHw<gfxProduct>::getCrossDeviceSharedMemCapabilities() {
    return 0;
}

template <PRODUCT_FAMILY gfxProduct>
cl_unified_shared_memory_capabilities_intel HwInfoConfigHw<gfxProduct>::getSharedSystemMemCapabilities() {
    return 0;
}

} // namespace NEO

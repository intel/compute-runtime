/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/ioctl_helper_xe_prelim.h"

#include "shared/source/os_interface/linux/xe/xedrm_prelim.h"

namespace NEO {

bool IoctlHelperXePrelim::isEuPerDssTopologyType(uint16_t topologyType) const {
    return topologyType == DRM_XE_TOPO_EU_PER_DSS ||
           topologyType == DRM_XE_TOPO_SIMD16_EU_PER_DSS;
}

bool IoctlHelperXePrelim::isL3BankTopologyType(uint16_t topologyType) const {
    return topologyType == DRM_XE_TOPO_L3_BANK;
}
} // namespace NEO

/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"
#include "shared/source/os_interface/linux/xe/xedrm.h"
namespace NEO {

bool IoctlHelperXe::isEuPerDssTopologyType(uint16_t topologyType) {
    return topologyType == DRM_XE_TOPO_EU_PER_DSS;
}
} // namespace NEO

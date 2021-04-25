/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"

#include "drm_query_flags.h"

namespace NEO {

bool Drm::queryTopology(const HardwareInfo &hwInfo, int &sliceCount, int &subSliceCount, int &euCount) {
    int32_t length;
    auto dataQuery = this->query(DRM_I915_QUERY_TOPOLOGY_INFO, DrmQueryItemFlags::topology, length);
    auto data = reinterpret_cast<drm_i915_query_topology_info *>(dataQuery.get());

    if (!data) {
        return false;
    }

    return translateTopologyInfo(data, sliceCount, subSliceCount, euCount);
}

bool Drm::isDebugAttachAvailable() {
    return false;
}

} // namespace NEO

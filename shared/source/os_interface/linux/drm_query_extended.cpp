/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"

#include "drm_query_flags.h"

namespace NEO {

bool Drm::queryTopology(const HardwareInfo &hwInfo, QueryTopologyData &topologyData) {
    int32_t length;
    auto dataQuery = this->query(DRM_I915_QUERY_TOPOLOGY_INFO, DrmQueryItemFlags::topology, length);
    auto data = reinterpret_cast<drm_i915_query_topology_info *>(dataQuery.get());

    if (!data) {
        return false;
    }

    topologyData.maxSliceCount = data->max_slices;
    topologyData.maxSubSliceCount = data->max_subslices;
    topologyData.maxEuCount = data->max_eus_per_subslice;

    TopologyMapping mapping;
    auto result = translateTopologyInfo(data, topologyData, mapping);
    this->topologyMap[0] = mapping;
    return result;
}

bool Drm::isDebugAttachAvailable() {
    return false;
}

} // namespace NEO

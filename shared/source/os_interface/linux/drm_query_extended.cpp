/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"

#include "drm_query_flags.h"

namespace NEO {

bool Drm::queryTopology(int &sliceCount, int &subSliceCount, int &euCount) {
    int32_t length;
    auto dataQuery = this->query(DRM_I915_QUERY_TOPOLOGY_INFO, DrmQueryItemFlags::topology, length);
    auto data = reinterpret_cast<drm_i915_query_topology_info *>(dataQuery.get());

    if (!data) {
        return false;
    }

    sliceCount = 0;
    subSliceCount = 0;
    euCount = 0;

    for (int x = 0; x < data->max_slices; x++) {
        bool isSliceEnable = (data->data[x / 8] >> (x % 8)) & 1;
        if (!isSliceEnable) {
            continue;
        }
        sliceCount++;
        for (int y = 0; y < data->max_subslices; y++) {
            bool isSubSliceEnabled = (data->data[data->subslice_offset + x * data->subslice_stride + y / 8] >> (y % 8)) & 1;
            if (!isSubSliceEnabled) {
                continue;
            }
            subSliceCount++;
            for (int z = 0; z < data->max_eus_per_subslice; z++) {
                bool isEUEnabled = (data->data[data->eu_offset + (x * data->max_subslices + y) * data->eu_stride + z / 8] >> (z % 8)) & 1;
                if (!isEUEnabled) {
                    continue;
                }
                euCount++;
            }
        }
    }

    return (sliceCount && subSliceCount && euCount);
}

} // namespace NEO

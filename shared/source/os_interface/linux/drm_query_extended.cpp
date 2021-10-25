/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/cache_info_impl.h"
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

bool Drm::querySystemInfo() {
    return false;
}

void Drm::setupSystemInfo(HardwareInfo *hwInfo, SystemInfo *sysInfo) {}

void Drm::setupCacheInfo(const HardwareInfo &hwInfo) {
    this->cacheInfo.reset(new CacheInfoImpl());
}

int Drm::bindBufferObject(OsContext *osContext, uint32_t vmHandleId, BufferObject *bo) {
    return 0;
}

int Drm::unbindBufferObject(OsContext *osContext, uint32_t vmHandleId, BufferObject *bo) {
    return 0;
}

void Drm::waitForBind(uint32_t vmHandleId) {
}

int Drm::waitUserFence(uint32_t ctx, uint64_t address, uint64_t value, ValueWidth dataWidth, int64_t timeout, uint16_t flags) {
    return 0;
}

bool Drm::isVmBindAvailable() {
    return this->bindAvailable;
}

uint32_t Drm::createDrmContextExt(drm_i915_gem_context_create_ext &gcc, uint32_t drmVmId, bool isSpecialContextRequested) {
    return ioctl(DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT, &gcc);
}

void Drm::appendDrmContextFlags(drm_i915_gem_context_create_ext &gcc, bool isSpecialContextRequested) {
}

} // namespace NEO

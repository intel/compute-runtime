/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/cache_info_impl.h"
#include "shared/source/os_interface/linux/drm_engine_mapper.h"
#include "shared/source/os_interface/linux/engine_info_impl.h"

#include "drm_neo.h"
#include "drm_query_flags.h"

#include <fstream>

namespace NEO {

int Drm::getMaxGpuFrequency(HardwareInfo &hwInfo, int &maxGpuFrequency) {
    maxGpuFrequency = 0;
    std::string clockSysFsPath = getSysFsPciPath();

    clockSysFsPath += "/gt_max_freq_mhz";

    std::ifstream ifs(clockSysFsPath.c_str(), std::ifstream::in);
    if (ifs.fail()) {
        return -1;
    }

    ifs >> maxGpuFrequency;
    ifs.close();
    return 0;
}

bool Drm::querySystemInfo() {
    return true;
}

bool Drm::queryEngineInfo() {
    auto length = 0;
    auto dataQuery = this->query(DRM_I915_QUERY_ENGINE_INFO, DrmQueryItemFlags::empty, length);
    auto data = reinterpret_cast<drm_i915_query_engine_info *>(dataQuery.get());
    if (data) {
        this->engineInfo.reset(new EngineInfoImpl(data->engines, data->num_engines));
        return true;
    }
    return false;
}

bool Drm::queryMemoryInfo() {
    return true;
}

unsigned int Drm::bindDrmContext(uint32_t drmContextId, uint32_t deviceIndex, aub_stream::EngineType engineType) {
    return DrmEngineMapper::engineNodeMap(engineType);
}

int Drm::bindBufferObject(OsContext *osContext, uint32_t vmHandleId, BufferObject *bo) {
    return 0;
}

int Drm::unbindBufferObject(OsContext *osContext, uint32_t vmHandleId, BufferObject *bo) {
    return 0;
}

void Drm::waitForBind(uint32_t vmHandleId) {
}

bool Drm::isVmBindAvailable() {
    return this->bindAvailable;
}

void Drm::appendDrmContextFlags(drm_i915_gem_context_create_ext &gcc, bool isDirectSubmission) {
}

void Drm::setupCacheInfo(const HardwareInfo &hwInfo) {
    this->cacheInfo.reset(new CacheInfoImpl());
}

} // namespace NEO

/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/drm_engine_mapper.h"
#include "shared/source/os_interface/linux/engine_info_impl.h"
#include "shared/source/os_interface/linux/memory_info_impl.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include "drm_neo.h"
#include "drm_query_flags.h"
#include "drm_tip.h"

#include <fstream>

namespace NEO {
class OsContext;

namespace IoctlHelper {
std::string getIoctlStringRemaining(unsigned long request) {
    return std::to_string(request);
}

std::string getIoctlParamStringRemaining(int param) {
    return std::to_string(param);
}

std::unique_ptr<uint8_t[]> translateDataQueryOnDrmTip(uint8_t *dataQuery, int32_t &length) {
    auto dataOnDrmTip = reinterpret_cast<DRM_TIP::drm_i915_query_memory_regions *>(dataQuery);
    auto lengthOnDrmTrip = static_cast<int32_t>(sizeof(DRM_TIP::drm_i915_query_memory_regions) + dataOnDrmTip->num_regions * sizeof(DRM_TIP::drm_i915_memory_region_info));
    if (length == lengthOnDrmTrip) {
        auto lengthTranslated = static_cast<int32_t>(sizeof(drm_i915_query_memory_regions) + dataOnDrmTip->num_regions * sizeof(drm_i915_memory_region_info));
        auto dataQueryTranslated = std::make_unique<uint8_t[]>(lengthTranslated);
        auto dataTranslated = reinterpret_cast<drm_i915_query_memory_regions *>(dataQueryTranslated.get());
        dataTranslated->num_regions = dataOnDrmTip->num_regions;
        for (uint32_t i = 0; i < dataTranslated->num_regions; i++) {
            dataTranslated->regions[i].region.memory_class = dataOnDrmTip->regions[i].region.memory_class;
            dataTranslated->regions[i].region.memory_instance = dataOnDrmTip->regions[i].region.memory_instance;
            dataTranslated->regions[i].probed_size = dataOnDrmTip->regions[i].probed_size;
            dataTranslated->regions[i].unallocated_size = dataOnDrmTip->regions[i].unallocated_size;
        }
        length = lengthTranslated;
        return dataQueryTranslated;
    }
    return nullptr;
}
} // namespace IoctlHelper

bool Drm::queryEngineInfo(bool isSysmanEnabled) {
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
    auto length = 0;
    auto dataQuery = this->query(DRM_I915_QUERY_MEMORY_REGIONS, DrmQueryItemFlags::empty, length);
    if (dataQuery) {
        auto dataQueryTranslated = IoctlHelper::translateDataQueryOnDrmTip(dataQuery.get(), length);
        if (dataQueryTranslated) {
            dataQuery.reset(dataQueryTranslated.release());
        }
        auto data = reinterpret_cast<drm_i915_query_memory_regions *>(dataQuery.get());
        DEBUG_BREAK_IF(static_cast<size_t>(length) != sizeof(drm_i915_query_memory_regions) + data->num_regions * sizeof(drm_i915_memory_region_info));
        this->memoryInfo.reset(new MemoryInfoImpl(data->regions, data->num_regions));
        return true;
    }
    return false;
}

unsigned int Drm::bindDrmContext(uint32_t drmContextId, uint32_t deviceIndex, aub_stream::EngineType engineType, bool engineInstancedDevice) {
    return DrmEngineMapper::engineNodeMap(engineType);
}

int Drm::createDrmVirtualMemory(uint32_t &drmVmId) {
    drm_i915_gem_vm_control ctl = {};
    auto ret = SysCalls::ioctl(getFileDescriptor(), DRM_IOCTL_I915_GEM_VM_CREATE, &ctl);
    if (ret == 0) {
        drmVmId = ctl.vm_id;
    }
    return ret;
}

} // namespace NEO

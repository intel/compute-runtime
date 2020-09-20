/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_engine_mapper.h"
#include "shared/source/os_interface/linux/engine_info_impl.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include "drm_neo.h"
#include "drm_query_flags.h"

#include <fstream>

namespace NEO {

namespace IoctlHelper {
std::string getIoctlStringRemaining(unsigned long request) {
    return std::to_string(request);
}

std::string getIoctlParamStringRemaining(int param) {
    return std::to_string(param);
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
    return true;
}

unsigned int Drm::bindDrmContext(uint32_t drmContextId, uint32_t deviceIndex, aub_stream::EngineType engineType, bool engineInstancedDevice) {
    return DrmEngineMapper::engineNodeMap(engineType);
}

int Drm::createDrmVirtualMemory(uint32_t &drmVmId) {
    drm_i915_gem_vm_control ctl = {};
    auto ret = SysCalls::ioctl(getFileDescriptor(), DRM_IOCTL_I915_GEM_VM_CREATE, &ctl);
    if (ret == 0) {
        if (ctl.vm_id == 0) {
            // 0 is reserved for invalid/unassigned ppgtt
            return -1;
        }
        drmVmId = ctl.vm_id;
    }
    return ret;
}

} // namespace NEO

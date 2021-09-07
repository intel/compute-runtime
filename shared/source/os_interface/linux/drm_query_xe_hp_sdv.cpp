/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/cache_info_impl.h"
#include "shared/source/os_interface/linux/drm_engine_mapper.h"
#include "shared/source/os_interface/linux/engine_info_impl.h"
#include "shared/source/os_interface/linux/memory_info_impl.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include "drm_neo.h"
#include "drm_query_flags.h"

#include <fstream>

namespace NEO {
class OsContext;

namespace IoctlHelper {
std::string getIoctlStringRemaining(unsigned long request) {
    switch (request) {
    default:
        return std::to_string(request);
    }
}

std::string getIoctlParamStringRemaining(int param) {
    switch (param) {
    default:
        return std::to_string(param);
    }
}
} // namespace IoctlHelper

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
    return false;
}

void Drm::setupSystemInfo(HardwareInfo *hwInfo, SystemInfo *sysInfo) {}

bool Drm::queryEngineInfo(bool isSysmanEnabled) {
    auto length = 0;

    auto enginesQuery = this->query(DRM_I915_QUERY_ENGINE_INFO, DrmQueryItemFlags::empty, length);
    auto engines = reinterpret_cast<drm_i915_query_engine_info *>(enginesQuery.get());
    if (!engines) {
        return false;
    }

    auto memoryInfoImpl = static_cast<MemoryInfoImpl *>(memoryInfo.get());

    if (!memoryInfoImpl) {
        return false;
    }

    auto &memoryRegions = memoryInfoImpl->getDrmRegionInfos();

    auto tileCount = 0u;
    std::vector<drm_i915_query_distance_info> distanceInfos;
    for (auto i = 0u; i < memoryRegions.size(); i++) {
        if (I915_MEMORY_CLASS_DEVICE == memoryRegions[i].region.memory_class) {
            tileCount++;
            drm_i915_query_distance_info distanceInfo{};
            distanceInfo.region.memory_class = I915_MEMORY_CLASS_DEVICE;
            distanceInfo.region.memory_instance = memoryRegions[i].region.memory_instance;

            for (auto j = 0u; j < engines->num_engines; j++) {
                auto engineInfo = engines->engines[j];
                switch (engineInfo.engine.engine_class) {
                case I915_ENGINE_CLASS_RENDER:
                case I915_ENGINE_CLASS_COPY:
                case I915_ENGINE_CLASS_COMPUTE:
                    distanceInfo.engine = engineInfo.engine;
                    distanceInfos.push_back(distanceInfo);
                    break;
                case I915_ENGINE_CLASS_VIDEO:
                case I915_ENGINE_CLASS_VIDEO_ENHANCE:
                    if (isSysmanEnabled == true) {
                        distanceInfo.engine = engineInfo.engine;
                        distanceInfos.push_back(distanceInfo);
                    }
                    break;
                default:
                    break;
                }
            }
        }
    }

    auto hwInfo = rootDeviceEnvironment.getMutableHardwareInfo();

    if (tileCount == 0u) {
        this->engineInfo.reset(new EngineInfoImpl(hwInfo, engines));
        return true;
    }

    std::vector<drm_i915_query_item> queryItems{distanceInfos.size()};
    for (auto i = 0u; i < distanceInfos.size(); i++) {
        queryItems[i].query_id = DRM_I915_QUERY_DISTANCE_INFO;
        queryItems[i].length = sizeof(drm_i915_query_distance_info);
        queryItems[i].flags = 0u;
        queryItems[i].data_ptr = reinterpret_cast<__u64>(&distanceInfos[i]);
    }

    drm_i915_query query{};
    query.items_ptr = reinterpret_cast<__u64>(queryItems.data());
    query.num_items = static_cast<uint32_t>(queryItems.size());
    auto ret = this->ioctl(DRM_IOCTL_I915_QUERY, &query);
    if (ret != 0) {
        return false;
    }

    const bool queryUnsupported = std::all_of(queryItems.begin(), queryItems.end(),
                                              [](const drm_i915_query_item &item) { return item.length == -EINVAL; });
    if (queryUnsupported) {
        DEBUG_BREAK_IF(tileCount != 1);
        this->engineInfo.reset(new EngineInfoImpl(hwInfo, engines));
        return true;
    }

    memoryInfoImpl->assignRegionsFromDistances(distanceInfos);

    auto &multiTileArchInfo = const_cast<GT_MULTI_TILE_ARCH_INFO &>(hwInfo->gtSystemInfo.MultiTileArchInfo);
    multiTileArchInfo.IsValid = true;
    multiTileArchInfo.TileCount = tileCount;
    multiTileArchInfo.TileMask = static_cast<uint8_t>(maxNBitValue(tileCount));

    this->engineInfo.reset(new EngineInfoImpl(hwInfo, tileCount, distanceInfos, queryItems, engines));
    return true;
}

bool Drm::queryEngineInfo() {
    return Drm::queryEngineInfo(false);
}

bool Drm::sysmanQueryEngineInfo() {
    return Drm::queryEngineInfo(true);
}

bool Drm::queryMemoryInfo() {
    auto length = 0;
    auto dataQuery = this->query(DRM_I915_QUERY_MEMORY_REGIONS, DrmQueryItemFlags::empty, length);
    auto data = reinterpret_cast<drm_i915_query_memory_regions *>(dataQuery.get());
    if (data) {
        this->memoryInfo.reset(new MemoryInfoImpl(data->regions, data->num_regions));
        return true;
    }
    return false;
}

unsigned int Drm::bindDrmContext(uint32_t drmContextId, uint32_t deviceIndex, aub_stream::EngineType engineType, bool engineInstancedDevice) {
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

int Drm::waitUserFence(uint32_t ctx, uint64_t address, uint64_t value, ValueWidth dataWidth, int64_t timeout, uint16_t flags) {
    return 0;
}

bool Drm::isVmBindAvailable() {
    return this->bindAvailable;
}

void Drm::appendDrmContextFlags(drm_i915_gem_context_create_ext &gcc, bool isSpecialContextRequested) {
}

void Drm::setupCacheInfo(const HardwareInfo &hwInfo) {
    this->cacheInfo.reset(new CacheInfoImpl());
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

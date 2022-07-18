/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/engine/linux/os_engine_imp.h"

#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/i915.h"

#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

constexpr auto I915_SAMPLE_BUSY = NEO::I915::drm_i915_pmu_engine_sample::I915_SAMPLE_BUSY;
static const std::multimap<__u16, zes_engine_group_t> i915ToEngineMap = {
    {static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER), ZES_ENGINE_GROUP_RENDER_SINGLE},
    {static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO), ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE},
    {static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO), ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE},
    {static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_COPY), ZES_ENGINE_GROUP_COPY_SINGLE},
    {static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO_ENHANCE), ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE}};

static const std::multimap<zes_engine_group_t, __u16> engineToI915Map = {
    {ZES_ENGINE_GROUP_RENDER_SINGLE, static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER)},
    {ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO)},
    {ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE, static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO)},
    {ZES_ENGINE_GROUP_COPY_SINGLE, static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_COPY)},
    {ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE, static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO_ENHANCE)}};

ze_result_t OsEngine::getNumEngineTypeAndInstances(std::set<std::pair<zes_engine_group_t, EngineInstanceSubDeviceId>> &engineGroupInstance, OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    NEO::Drm *pDrm = &pLinuxSysmanImp->getDrm();

    if (pDrm->sysmanQueryEngineInfo() == false) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    auto engineInfo = pDrm->getEngineInfo();
    for (auto itr = engineInfo->engines.begin(); itr != engineInfo->engines.end(); ++itr) {
        auto i915ToEngineMapRange = i915ToEngineMap.equal_range(static_cast<__u16>(itr->engine.engineClass));
        for (auto l0EngineEntryInMap = i915ToEngineMapRange.first; l0EngineEntryInMap != i915ToEngineMapRange.second; l0EngineEntryInMap++) {
            auto l0EngineType = l0EngineEntryInMap->second;
            engineGroupInstance.insert({l0EngineType, {static_cast<uint32_t>(itr->engine.engineInstance), 0}});
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxEngineImp::getActivity(zes_engine_stats_t *pStats) {
    if (fd < 0) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    uint64_t data[2] = {};
    if (pPmuInterface->pmuRead(static_cast<int>(fd), data, sizeof(data)) < 0) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    // In data[], First u64 is "active time", And second u64 is "timestamp". Both in nanoseconds
    pStats->activeTime = data[0] / microSecondsToNanoSeconds;
    pStats->timestamp = data[1] / microSecondsToNanoSeconds;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxEngineImp::getProperties(zes_engine_properties_t &properties) {
    properties.type = engineGroup;
    properties.onSubdevice = 0;
    properties.subdeviceId = subDeviceId;
    return ZE_RESULT_SUCCESS;
}

void LinuxEngineImp::init() {
    auto i915EngineClass = engineToI915Map.find(engineGroup);
    // I915_PMU_ENGINE_BUSY macro provides the perf type config which we want to listen to get the engine busyness.
    fd = pPmuInterface->pmuInterfaceOpen(I915_PMU_ENGINE_BUSY(i915EngineClass->second, engineInstance), -1, PERF_FORMAT_TOTAL_TIME_ENABLED);
}

bool LinuxEngineImp::isEngineModuleSupported() {
    if (fd < 0) {
        return false;
    }
    return true;
}

LinuxEngineImp::LinuxEngineImp(OsSysman *pOsSysman, zes_engine_group_t type, uint32_t engineInstance, uint32_t subDeviceId) : engineGroup(type), engineInstance(engineInstance), subDeviceId(subDeviceId) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pDrm = &pLinuxSysmanImp->getDrm();
    pDevice = pLinuxSysmanImp->getDeviceHandle();
    pPmuInterface = pLinuxSysmanImp->getPmuInterface();
    init();
}

OsEngine *OsEngine::create(OsSysman *pOsSysman, zes_engine_group_t type, uint32_t engineInstance, uint32_t subDeviceId) {
    LinuxEngineImp *pLinuxEngineImp = new LinuxEngineImp(pOsSysman, type, engineInstance, subDeviceId);
    return static_cast<OsEngine *>(pLinuxEngineImp);
}

} // namespace L0

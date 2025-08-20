/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/engine/linux/os_engine_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"

namespace L0 {

constexpr auto I915_SAMPLE_BUSY = NEO::I915::drm_i915_pmu_engine_sample::I915_SAMPLE_BUSY; // NOLINT(readability-identifier-naming)

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

ze_result_t LinuxEngineImp::getActivity(zes_engine_stats_t *pStats) {
    if (initStatus != ZE_RESULT_SUCCESS) {
        return initStatus;
    }
    uint64_t data[2] = {};
    auto ret = pPmuInterface->pmuRead(static_cast<int>(fdList[0].first), data, sizeof(data));
    if (ret < 0) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    // In data[], First u64 is "active time", And second u64 is "timestamp". Both in nanoseconds
    pStats->activeTime = data[0] / microSecondsToNanoSeconds;
    pStats->timestamp = data[1] / microSecondsToNanoSeconds;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxEngineImp::getProperties(zes_engine_properties_t &properties) {
    properties.type = engineGroup;
    properties.onSubdevice = onSubDevice;
    properties.subdeviceId = subDeviceId;
    return ZE_RESULT_SUCCESS;
}

void LinuxEngineImp::checkErrorNumberAndUpdateStatus() {
    if (errno == EMFILE || errno == ENFILE) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Engine Handles could not be created because system has run out of file handles. Suggested action is to increase the file handle limit. \n");
        initStatus = ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    } else {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():No valid Filedescriptors: Engine Module is not supported \n", __FUNCTION__);
        initStatus = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
}

void LinuxEngineImp::init() {
    auto i915EngineClass = engineToI915Map.find(engineGroup);
    if (i915EngineClass == engineToI915Map.end()) {
        checkErrorNumberAndUpdateStatus();
        return;
    }
    vfConfigs.clear();
    // I915_PMU_ENGINE_BUSY macro provides the perf type config which we want to listen to get the engine busyness.
    auto fd = pPmuInterface->pmuInterfaceOpen(I915_PMU_ENGINE_BUSY(i915EngineClass->second, engineInstance), -1, PERF_FORMAT_TOTAL_TIME_ENABLED);
    if (fd >= 0) {
        fdList.push_back(std::make_pair(fd, -1));
    } else {
        checkErrorNumberAndUpdateStatus();
    }
}

ze_result_t LinuxEngineImp::getActivityExt(uint32_t *pCount, zes_engine_stats_t *pStats) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

void LinuxEngineImp::cleanup() {
    for (auto &fdPair : fdList) {
        DEBUG_BREAK_IF(fdPair.first < 0);
        NEO::SysCalls::close(static_cast<int>(fdPair.first));
    }
    fdList.clear();
}

LinuxEngineImp::~LinuxEngineImp() {
    cleanup();
}

void LinuxEngineImp::getInstancesFromEngineInfo(NEO::EngineInfo *engineInfo, std::set<std::pair<zes_engine_group_t, EngineInstanceSubDeviceId>> &engineGroupInstance) {
    for (const auto &info : engineInfo->getEngineInfos()) {
        auto i915ToEngineMapRange = i915ToEngineMap.equal_range(static_cast<__u16>(info.engine.engineClass));
        for (auto l0EngineEntryInMap = i915ToEngineMapRange.first; l0EngineEntryInMap != i915ToEngineMapRange.second; l0EngineEntryInMap++) {
            auto l0EngineType = l0EngineEntryInMap->second;
            engineGroupInstance.insert({l0EngineType, {static_cast<uint32_t>(info.engine.engineInstance), 0}});
        }
    }
}

ze_result_t LinuxEngineImp::isEngineModuleSupported() {
    return initStatus;
}

LinuxEngineImp::LinuxEngineImp(OsSysman *pOsSysman, zes_engine_group_t type, uint32_t engineInstance, uint32_t subDeviceId, ze_bool_t onSubDevice) : engineGroup(type), engineInstance(engineInstance), subDeviceId(subDeviceId), onSubDevice(onSubDevice) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pDrm = &pLinuxSysmanImp->getDrm();
    pDevice = pLinuxSysmanImp->getDeviceHandle();
    pPmuInterface = pLinuxSysmanImp->getPmuInterface();
    init();
    if (initStatus != ZE_RESULT_SUCCESS) {
        cleanup();
    }
}

} // namespace L0

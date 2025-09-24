/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/i915_prelim.h"

#include "level_zero/tools/source/sysman/linux/pmu/pmu.h"
#include "level_zero/tools/source/sysman/vf_management/linux/os_vf_imp.h"

namespace L0 {

using NEO::PrelimI915::drm_i915_pmu_engine_sample::I915_SAMPLE_BUSY;

static const std::multimap<__u16, zes_engine_group_t> i915ToEngineMap = {
    {static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER), ZES_ENGINE_GROUP_RENDER_SINGLE},
    {static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO), ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE},
    {static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO), ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE},
    {static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_COPY), ZES_ENGINE_GROUP_COPY_SINGLE},
    {static_cast<__u16>(prelim_drm_i915_gem_engine_class::PRELIM_I915_ENGINE_CLASS_COMPUTE), ZES_ENGINE_GROUP_COMPUTE_SINGLE},
    {static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO_ENHANCE), ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE}};

static const std::multimap<zes_engine_group_t, __u16> engineToI915Map = {
    {ZES_ENGINE_GROUP_RENDER_SINGLE, static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER)},
    {ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO)},
    {ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE, static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO)},
    {ZES_ENGINE_GROUP_COPY_SINGLE, static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_COPY)},
    {ZES_ENGINE_GROUP_COMPUTE_SINGLE, static_cast<__u16>(prelim_drm_i915_gem_engine_class::PRELIM_I915_ENGINE_CLASS_COMPUTE)},
    {ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE, static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO_ENHANCE)}};

void LinuxVfImp::vfGetInstancesFromEngineInfo(NEO::EngineInfo *engineInfo, std::set<std::pair<zes_engine_group_t, uint32_t>> &engineGroupAndInstance) {

    auto engineTileMap = engineInfo->getEngineTileInfo();
    for (auto itr = engineTileMap.begin(); itr != engineTileMap.end(); itr++) {
        auto i915ToEngineMapRange = i915ToEngineMap.equal_range(static_cast<__u16>(itr->second.engineClass));
        for (auto l0EngineEntryInMap = i915ToEngineMapRange.first; l0EngineEntryInMap != i915ToEngineMapRange.second; l0EngineEntryInMap++) {
            auto l0EngineType = l0EngineEntryInMap->second;
            engineGroupAndInstance.insert({l0EngineType, static_cast<uint32_t>(itr->second.engineInstance)});
        }
    }
}

ze_result_t LinuxVfImp::vfEngineDataInit() {

    NEO::Drm *pDrm = &pLinuxSysmanImp->getDrm();
    PmuInterface *pPmuInterface = pLinuxSysmanImp->getPmuInterface();

    if (pDrm->sysmanQueryEngineInfo() == false) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():sysmanQueryEngineInfo is returning false and error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    auto engineInfo = pDrm->getEngineInfo();
    vfGetInstancesFromEngineInfo(engineInfo, engineGroupAndInstance);
    for (auto itr = engineGroupAndInstance.begin(); itr != engineGroupAndInstance.end(); itr++) {
        auto engineClass = engineToI915Map.find(itr->first);
        UNRECOVERABLE_IF(engineClass == engineToI915Map.end());
        uint64_t busyTicksConfig = ___PRELIM_I915_PMU_FN_EVENT(PRELIM_I915_PMU_ENGINE_BUSY_TICKS(engineClass->second, itr->second), static_cast<uint64_t>(vfId));
        int64_t busyTicksFd = pPmuInterface->pmuInterfaceOpen(busyTicksConfig, -1, PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_GROUP);
        if (busyTicksFd < 0) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Could not open Busy Ticks Handle and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
            cleanup();
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }

        uint64_t totalTicksConfig = ___PRELIM_I915_PMU_FN_EVENT(PRELIM_I915_PMU_ENGINE_TOTAL_TICKS(engineClass->second, itr->second), static_cast<uint64_t>(vfId));
        int64_t totalTicksFd = pPmuInterface->pmuInterfaceOpen(totalTicksConfig, static_cast<int32_t>(busyTicksFd), PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_GROUP);
        if (totalTicksFd < 0) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Could not open Total Ticks Handle and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
            NEO::SysCalls::close(static_cast<int>(busyTicksFd));
            cleanup();
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }

        EngineUtilsData pEngineUtilsData;
        pEngineUtilsData.engineType = itr->first;
        pEngineUtilsData.busyTicksFd = busyTicksFd;
        pEngineUtilsData.totalTicksFd = totalTicksFd;
        pEngineUtils.push_back(pEngineUtilsData);
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxVfImp::getEngineUtilization(uint32_t *pCount, zes_vf_util_engine_exp2_t *pEngineUtil) {

    ze_result_t result = ZE_RESULT_SUCCESS;
    std::call_once(initEngineDataOnce, [&]() {
        result = this->vfEngineDataInit();
    });

    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    uint32_t engineCount = static_cast<uint32_t>(pEngineUtils.size());
    if (engineCount == 0) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): The Total Engine Count Is Zero and hence returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    if (*pCount == 0) {
        *pCount = engineCount;
        return ZE_RESULT_SUCCESS;
    }

    if (*pCount > engineCount) {
        *pCount = engineCount;
    }

    if (pEngineUtil != nullptr) {
        PmuInterface *pPmuInterface = pLinuxSysmanImp->getPmuInterface();
        for (uint32_t i = 0; i < *pCount; i++) {
            uint64_t pmuData[4] = {};
            auto ret = pPmuInterface->pmuRead(static_cast<int>(pEngineUtils[i].busyTicksFd), pmuData, sizeof(pmuData));
            if (ret < 0) {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():pmuRead is returning value:%d and error:0x%x \n", __FUNCTION__, ret, ZE_RESULT_ERROR_UNKNOWN);
                *pCount = 0;
                return ZE_RESULT_ERROR_UNKNOWN;
            }
            pEngineUtil[i].vfEngineType = pEngineUtils[i].engineType;
            pEngineUtil[i].activeCounterValue = pmuData[2];
            pEngineUtil[i].samplingCounterValue = pmuData[3];
        }
    }

    return ZE_RESULT_SUCCESS;
}

} // namespace L0

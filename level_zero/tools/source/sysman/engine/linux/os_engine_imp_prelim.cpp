/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/i915_prelim.h"

#include "level_zero/tools/source/sysman/engine/linux/os_engine_imp.h"
#include "level_zero/tools/source/sysman/linux/fs_access.h"
#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"

namespace L0 {

constexpr std::string_view pathForNumberOfVfs = "device/sriov_numvfs";
using NEO::PrelimI915::drm_i915_pmu_engine_sample::I915_SAMPLE_BUSY;

static const std::multimap<__u16, zes_engine_group_t> i915ToEngineMapPrelim = {
    {static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER), ZES_ENGINE_GROUP_RENDER_SINGLE},
    {static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO), ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE},
    {static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO), ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE},
    {static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_COPY), ZES_ENGINE_GROUP_COPY_SINGLE},
    {static_cast<__u16>(prelim_drm_i915_gem_engine_class::PRELIM_I915_ENGINE_CLASS_COMPUTE), ZES_ENGINE_GROUP_COMPUTE_SINGLE},
    {static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO_ENHANCE), ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE}};

static const std::multimap<zes_engine_group_t, __u16> engineToI915MapPrelim = {
    {ZES_ENGINE_GROUP_RENDER_SINGLE, static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_RENDER)},
    {ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO)},
    {ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE, static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO)},
    {ZES_ENGINE_GROUP_COPY_SINGLE, static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_COPY)},
    {ZES_ENGINE_GROUP_COMPUTE_SINGLE, static_cast<__u16>(prelim_drm_i915_gem_engine_class::PRELIM_I915_ENGINE_CLASS_COMPUTE)},
    {ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE, static_cast<__u16>(drm_i915_gem_engine_class::I915_ENGINE_CLASS_VIDEO_ENHANCE)}};

zes_engine_group_t LinuxEngineImpPrelim::getGroupFromEngineType(zes_engine_group_t type) {
    if (type == ZES_ENGINE_GROUP_RENDER_SINGLE) {
        return ZES_ENGINE_GROUP_RENDER_ALL;
    }
    if (type == ZES_ENGINE_GROUP_COMPUTE_SINGLE) {
        return ZES_ENGINE_GROUP_COMPUTE_ALL;
    }
    if (type == ZES_ENGINE_GROUP_COPY_SINGLE) {
        return ZES_ENGINE_GROUP_COPY_ALL;
    }
    if (type == ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE || type == ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE || type == ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE) {
        return ZES_ENGINE_GROUP_MEDIA_ALL;
    }
    return ZES_ENGINE_GROUP_ALL;
}

static ze_result_t readBusynessFromGroupFd(PmuInterface *pPmuInterface, std::pair<int64_t, int64_t> &fdPair, zes_engine_stats_t *pStats) {
    uint64_t data[4] = {};

    auto ret = pPmuInterface->pmuRead(static_cast<int>(fdPair.first), data, sizeof(data));
    if (ret < 0) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():pmuRead is returning value:%d and error:0x%x \n", __FUNCTION__, ret, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    // In data[], First u64 is "active time", And second u64 is "timestamp". Both in ticks
    pStats->activeTime = data[2];
    pStats->timestamp = data[3] ? data[3] : SysmanDevice::getSysmanTimestamp();
    return ZE_RESULT_SUCCESS;
}

static ze_result_t openPmuHandlesForVfs(uint32_t numberOfVfs,
                                        PmuInterface *pPmuInterface,
                                        std::vector<std::pair<uint64_t, uint64_t>> &vfConfigs,
                                        std::vector<std::pair<int64_t, int64_t>> &fdList) {
    // +1 to include PF
    for (uint64_t i = 0; i < numberOfVfs + 1; i++) {
        int64_t fd[2] = {-1, -1};
        fd[0] = pPmuInterface->pmuInterfaceOpen(vfConfigs[i].first, -1,
                                                PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_GROUP);

        if (fd[0] >= 0) {
            fd[1] = pPmuInterface->pmuInterfaceOpen(vfConfigs[i].second, static_cast<int>(fd[0]),
                                                    PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_GROUP);
            if (fd[1] < 0) {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Could not open Total Active Ticks PMU Handle \n", __FUNCTION__);
                close(static_cast<int>(fd[0]));
                fd[0] = -1;
            }
        }

        if (fd[1] < 0) {
            if (errno == EMFILE || errno == ENFILE) {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Engine Handles could not be created because system has run out of file handles. Suggested action is to increase the file handle limit. \n");
                return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
            }
        }

        fdList.push_back(std::make_pair(fd[0], fd[1]));
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxEngineImpPrelim::getActivity(zes_engine_stats_t *pStats) {

    if (initStatus != ZE_RESULT_SUCCESS) {
        // Handles are not expected to be created in case of init failure
        DEBUG_BREAK_IF(true);
    }

    // read from global busyness fd
    return readBusynessFromGroupFd(pPmuInterface, fdList[0], pStats);
}

void LinuxEngineImpPrelim::cleanup() {
    for (auto &fdPair : fdList) {
        if (fdPair.first >= 0) {
            close(static_cast<int>(fdPair.first));
        }
        if (fdPair.second >= 0) {
            close(static_cast<int>(fdPair.second));
        }
    }
    fdList.clear();
    vfConfigs.clear();
    numberOfVfs = 0;
}

LinuxEngineImpPrelim::~LinuxEngineImpPrelim() {
    cleanup();
}

ze_result_t LinuxEngineImpPrelim::getActivityExt(uint32_t *pCount, zes_engine_stats_t *pStats) {

    if (numberOfVfs == 0) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    if (*pCount == 0) {
        // +1 for PF
        *pCount = numberOfVfs + 1;
        return ZE_RESULT_SUCCESS;
    }

    if (fdList.size() == 0) {
        DEBUG_BREAK_IF(true);
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): unexpected fdlist\n", __FUNCTION__);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    // Open only if not opened previously
    // fdList[0] has global busyness.
    // So check if PF and VF busyness were not updated
    if (fdList.size() == 1) {
        auto status = openPmuHandlesForVfs(numberOfVfs, pPmuInterface, vfConfigs, fdList);
        if (status != ZE_RESULT_SUCCESS) {
            // Clean up all vf fds added
            for (size_t i = 1; i < fdList.size(); i++) {
                auto &fdPair = fdList[i];
                if (fdPair.first >= 0) {
                    close(static_cast<int32_t>(fdPair.first));
                    close(static_cast<int32_t>(fdPair.second));
                }
                fdList.resize(1);
            }
            return status;
        }
    }

    *pCount = std::min(*pCount, numberOfVfs + 1);
    memset(pStats, 0, *pCount * sizeof(zes_engine_stats_t));
    for (uint32_t i = 0; i < *pCount; i++) {
        // +1 to consider the global busyness value
        const auto fdListIndex = i + 1;
        if (fdList[fdListIndex].first >= 0) {
            auto status = readBusynessFromGroupFd(pPmuInterface, fdList[fdListIndex], &pStats[i]);
            if (status != ZE_RESULT_SUCCESS) {
                return status;
            }
            continue;
        }
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxEngineImpPrelim::getProperties(zes_engine_properties_t &properties) {
    properties.subdeviceId = subDeviceId;
    properties.onSubdevice = onSubDevice;
    properties.type = engineGroup;
    return ZE_RESULT_SUCCESS;
}

void LinuxEngineImpPrelim::checkErrorNumberAndUpdateStatus() {
    if (errno == EMFILE || errno == ENFILE) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Engine Handles could not be created because system has run out of file handles. Suggested action is to increase the file handle limit. \n");
        initStatus = ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    } else {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():No valid Filedescriptors: Engine Module is not supported \n", __FUNCTION__);
        initStatus = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
}

void LinuxEngineImpPrelim::init() {
    uint64_t config = UINT64_MAX;
    switch (engineGroup) {
    case ZES_ENGINE_GROUP_ALL:
        config = __PRELIM_I915_PMU_ANY_ENGINE_GROUP_BUSY_TICKS(subDeviceId);
        break;
    case ZES_ENGINE_GROUP_COMPUTE_ALL:
    case ZES_ENGINE_GROUP_RENDER_ALL:
        config = __PRELIM_I915_PMU_RENDER_GROUP_BUSY_TICKS(subDeviceId);
        break;
    case ZES_ENGINE_GROUP_COPY_ALL:
        config = __PRELIM_I915_PMU_COPY_GROUP_BUSY_TICKS(subDeviceId);
        break;
    case ZES_ENGINE_GROUP_MEDIA_ALL:
        config = __PRELIM_I915_PMU_MEDIA_GROUP_BUSY_TICKS(subDeviceId);
        break;
    default:
        auto i915EngineClass = engineToI915MapPrelim.find(engineGroup);
        config = PRELIM_I915_PMU_ENGINE_BUSY_TICKS(i915EngineClass->second, engineInstance);
        break;
    }
    int64_t fd[2];

    // Fds for global busyness
    fd[0] = pPmuInterface->pmuInterfaceOpen(config, -1, PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_GROUP);
    if (fd[0] < 0) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Could not open Busy Ticks Handle \n", __FUNCTION__);
        checkErrorNumberAndUpdateStatus();
        return;
    }

    uint64_t totalTickConfig = 0u;
    bool isGroupEngineHandle = (engineGroup == ZES_ENGINE_GROUP_ALL || engineGroup == ZES_ENGINE_GROUP_MEDIA_ALL || engineGroup == ZES_ENGINE_GROUP_COMPUTE_ALL || engineGroup == ZES_ENGINE_GROUP_RENDER_ALL || engineGroup == ZES_ENGINE_GROUP_COPY_ALL);
    if (isGroupEngineHandle) {
        totalTickConfig = __PRELIM_I915_PMU_TOTAL_ACTIVE_TICKS(subDeviceId);
    } else {
        auto i915EngineClass = engineToI915MapPrelim.find(engineGroup);
        totalTickConfig = PRELIM_I915_PMU_ENGINE_TOTAL_TICKS(i915EngineClass->second, engineInstance);
    }

    fd[1] = pPmuInterface->pmuInterfaceOpen(totalTickConfig, static_cast<int>(fd[0]), PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_GROUP);

    if (fd[1] < 0) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Could not open Total Active Ticks Handle \n", __FUNCTION__);
        checkErrorNumberAndUpdateStatus();
        close(static_cast<int>(fd[0]));
        return;
    }

    fdList.push_back(std::make_pair(fd[0], fd[1]));

    auto status = pSysfsAccess->read(pathForNumberOfVfs.data(), numberOfVfs);
    if (status != ZE_RESULT_SUCCESS) {
        numberOfVfs = 0;
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():Reading Number Of Vfs Failed or number of Vfs == 0 \n", __FUNCTION__);
        return;
    }

    // Delay fd opening till actually needed
    for (uint64_t i = 0; i < numberOfVfs + 1; i++) {
        const uint64_t busyConfig = ___PRELIM_I915_PMU_FN_EVENT(config, i);
        auto i915EngineClass = engineToI915MapPrelim.find(engineGroup);
        const uint64_t totalConfig = ___PRELIM_I915_PMU_FN_EVENT(PRELIM_I915_PMU_ENGINE_TOTAL_TICKS(i915EngineClass->second, engineInstance), i);
        vfConfigs.push_back(std::make_pair(busyConfig, totalConfig));
    }
}

ze_result_t LinuxEngineImpPrelim::isEngineModuleSupported() {
    return initStatus;
}

void LinuxEngineImpPrelim::getInstancesFromEngineInfo(NEO::EngineInfo *engineInfo, std::set<std::pair<zes_engine_group_t, EngineInstanceSubDeviceId>> &engineGroupInstance) {
    for (const auto &info : engineInfo->getEngineInfos()) {
        uint32_t subDeviceId = engineInfo->getEngineTileIndex(info.engine);
        auto i915ToEngineMapRange = i915ToEngineMapPrelim.equal_range(static_cast<__u16>(info.engine.engineClass));
        for (auto l0EngineEntryInMap = i915ToEngineMapRange.first; l0EngineEntryInMap != i915ToEngineMapRange.second; l0EngineEntryInMap++) {
            auto l0EngineType = l0EngineEntryInMap->second;
            engineGroupInstance.insert({l0EngineType, {static_cast<uint32_t>(info.engine.engineInstance), subDeviceId}});
            engineGroupInstance.insert({LinuxEngineImpPrelim::getGroupFromEngineType(l0EngineType), {0u, subDeviceId}});
            engineGroupInstance.insert({ZES_ENGINE_GROUP_ALL, {0u, subDeviceId}});
        }
    }
}

LinuxEngineImpPrelim::LinuxEngineImpPrelim(OsSysman *pOsSysman, zes_engine_group_t type, uint32_t engineInstance, uint32_t subDeviceId, ze_bool_t onSubDevice) : engineGroup(type), engineInstance(engineInstance), subDeviceId(subDeviceId), onSubDevice(onSubDevice) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pDrm = &pLinuxSysmanImp->getDrm();
    pDevice = pLinuxSysmanImp->getDeviceHandle();
    pPmuInterface = pLinuxSysmanImp->getPmuInterface();
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    init();

    if (initStatus != ZE_RESULT_SUCCESS) {
        cleanup();
    }
}

} // namespace L0

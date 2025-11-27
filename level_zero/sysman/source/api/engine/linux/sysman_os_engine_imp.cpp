/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/engine/linux/sysman_os_engine_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/i915.h"

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/pmu/sysman_pmu_imp.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/linux/sysman_hw_device_id_linux.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"
#include "level_zero/sysman/source/sysman_const.h"

namespace L0 {
namespace Sysman {

void LinuxEngineImp::cleanup() {
    for (auto &fd : fdList) {
        DEBUG_BREAK_IF(fd < 0);
        NEO::SysCalls::close(static_cast<int>(fd));
    }
    fdList.clear();
}

LinuxEngineImp::~LinuxEngineImp() {
    cleanup();
}

zes_engine_group_t LinuxEngineImp::getGroupFromEngineType(zes_engine_group_t type) {
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

ze_result_t OsEngine::getNumEngineTypeAndInstances(MapOfEngineInfo &mapEngineInfo, OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    NEO::Drm *pDrm = pLinuxSysmanImp->getDrm();
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    auto pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();

    bool status = false;
    {
        auto hwDeviceId = pLinuxSysmanImp->getSysmanHwDeviceIdInstance();
        if (hwDeviceId.getFileDescriptor() < 0) {
            return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
        }
        status = pDrm->sysmanQueryEngineInfo();
    }

    if (status == false) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():sysmanQueryEngineInfo is returning false and error message:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    auto engineInfo = pDrm->getEngineInfo();
    auto engineTileMap = engineInfo->getEngineTileInfo();
    for (auto itr = engineTileMap.begin(); itr != engineTileMap.end(); ++itr) {
        uint32_t tileId = itr->first;
        auto engineGroupRange = engineClassToEngineGroup.equal_range(static_cast<uint16_t>(itr->second.engineClass));
        for (auto l0EngineEntryInMap = engineGroupRange.first; l0EngineEntryInMap != engineGroupRange.second; l0EngineEntryInMap++) {
            auto l0EngineType = l0EngineEntryInMap->second;

            // Addition of Single Engine Info in the map
            auto itrEngineGroupInfo = mapEngineInfo.find(l0EngineType);
            auto engineInstanceTileId = std::pair(static_cast<uint32_t>(itr->second.engineInstance), tileId);

            if (itrEngineGroupInfo != mapEngineInfo.end()) {
                itrEngineGroupInfo->second.insert({engineInstanceTileId});
            } else {
                SetOfEngineInstanceAndTileId engineInstancesAndTileIds = {engineInstanceTileId};
                mapEngineInfo[l0EngineType] = engineInstancesAndTileIds;
            }

            // Addition of Group Engine Info in the map
            if (pSysmanKmdInterface->isGroupEngineInterfaceAvailable() || pSysmanProductHelper->isAggregationOfSingleEnginesSupported()) {

                auto engineGroup = LinuxEngineImp::getGroupFromEngineType(l0EngineType);
                auto itrEngineGroupInfo = mapEngineInfo.find(engineGroup);
                if (itrEngineGroupInfo != mapEngineInfo.end()) {
                    itrEngineGroupInfo->second.insert({0u, tileId});
                } else {
                    SetOfEngineInstanceAndTileId engineInstancesAndTileIds = {{0u, tileId}};
                    mapEngineInfo[engineGroup] = engineInstancesAndTileIds;
                }

                itrEngineGroupInfo = mapEngineInfo.find(ZES_ENGINE_GROUP_ALL);
                if (itrEngineGroupInfo != mapEngineInfo.end()) {
                    itrEngineGroupInfo->second.insert({0u, tileId});
                } else {
                    SetOfEngineInstanceAndTileId engineInstancesAndTileIds = {{0u, tileId}};
                    mapEngineInfo[ZES_ENGINE_GROUP_ALL] = engineInstancesAndTileIds;
                }
            }
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxEngineImp::getActivity(zes_engine_stats_t *pStats) {
    return pSysmanKmdInterface->readBusynessFromGroupFd(pPmuInterface, fdList, pStats);
}

ze_result_t LinuxEngineImp::getProperties(zes_engine_properties_t &properties) {
    properties.type = engineGroupInfo.engineGroup;
    properties.onSubdevice = onSubDevice;
    properties.subdeviceId = engineGroupInfo.tileId;
    return ZE_RESULT_SUCCESS;
}

void LinuxEngineImp::init(MapOfEngineInfo &mapEngineInfo) {

    std::vector<uint64_t> pmuConfigs;
    const std::string sysmanDeviceDir = std::string(sysDevicesDir) + pSysmanKmdInterface->getSysmanDeviceDirName();

    if (isGroupEngineHandle(engineGroupInfo.engineGroup)) {
        initStatus = pSysmanKmdInterface->getPmuConfigsForGroupEngines(mapEngineInfo, sysmanDeviceDir, engineGroupInfo, pPmuInterface, pDrm, pmuConfigs);
    } else {
        initStatus = pSysmanKmdInterface->getPmuConfigsForSingleEngines(sysmanDeviceDir, engineGroupInfo, pPmuInterface, pDrm, pmuConfigs);
    }

    if (initStatus != ZE_RESULT_SUCCESS) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to fetch Pmu Configs \n", __FUNCTION__);
        return;
    } else {
        if (pmuConfigs.empty()) {
            initStatus = ZE_RESULT_ERROR_UNKNOWN;
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Pmu Configs Not Available \n", __FUNCTION__);
            return;
        } else {
            for (auto &config : pmuConfigs) {
                int64_t fd = -1;
                if (fdList.empty()) {
                    if (pmuConfigs.size() == 1) {
                        fd = pPmuInterface->pmuInterfaceOpen(config, -1, PERF_FORMAT_TOTAL_TIME_ENABLED);
                    } else {
                        fd = pPmuInterface->pmuInterfaceOpen(config, -1, PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_GROUP);
                    }
                    if (fd < 0) {
                        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Could not open Ticks Handle \n", __FUNCTION__);
                        initStatus = pSysmanKmdInterface->checkErrorNumberAndReturnStatus();
                        return;
                    }
                } else {
                    fd = pPmuInterface->pmuInterfaceOpen(config, static_cast<int>(fdList[0]), PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_GROUP);
                    if (fd < 0) {
                        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Could not open Ticks Handle \n", __FUNCTION__);
                        for (auto &fd : fdList) {
                            NEO::SysCalls::close(static_cast<int>(fd));
                        }
                        fdList.clear();
                        initStatus = pSysmanKmdInterface->checkErrorNumberAndReturnStatus();
                        return;
                    }
                }
                fdList.push_back(fd);
            }
            initStatus = ZE_RESULT_SUCCESS;
        }
    }
}

bool LinuxEngineImp::isEngineModuleSupported() {
    if (initStatus != ZE_RESULT_SUCCESS) {
        return false;
    }
    return true;
}

LinuxEngineImp::LinuxEngineImp(OsSysman *pOsSysman, MapOfEngineInfo &mapEngineInfo, zes_engine_group_t type, uint32_t engineInstance, uint32_t tileId, ze_bool_t onSubDevice) : onSubDevice(onSubDevice) {

    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    engineGroupInfo.engineGroup = type;
    engineGroupInfo.engineInstance = engineInstance;
    engineGroupInfo.tileId = tileId;
    pDrm = pLinuxSysmanImp->getDrm();
    pDevice = pLinuxSysmanImp->getSysmanDeviceImp();
    pPmuInterface = pLinuxSysmanImp->getPmuInterface();
    pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    init(mapEngineInfo);
    if (initStatus != ZE_RESULT_SUCCESS) {
        cleanup();
    }
}

std::unique_ptr<OsEngine> OsEngine::create(OsSysman *pOsSysman, MapOfEngineInfo &mapEngineInfo, zes_engine_group_t type, uint32_t engineInstance, uint32_t tileId, ze_bool_t onSubDevice) {
    std::unique_ptr<LinuxEngineImp> pLinuxEngineImp = std::make_unique<LinuxEngineImp>(pOsSysman, mapEngineInfo, type, engineInstance, tileId, onSubDevice);
    return pLinuxEngineImp;
}

} // namespace Sysman
} // namespace L0

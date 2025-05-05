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
    for (auto &fdPair : fdList) {
        DEBUG_BREAK_IF(fdPair.first < 0);
        close(static_cast<int>(fdPair.first));
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

ze_result_t OsEngine::getNumEngineTypeAndInstances(std::set<std::pair<zes_engine_group_t, EngineInstanceSubDeviceId>> &engineGroupInstance, OsSysman *pOsSysman) {
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
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():sysmanQueryEngineInfo is returning false and error message:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    auto engineInfo = pDrm->getEngineInfo();
    auto engineTileMap = engineInfo->getEngineTileInfo();
    for (auto itr = engineTileMap.begin(); itr != engineTileMap.end(); ++itr) {
        uint32_t tileId = itr->first;
        auto engineGroupRange = engineClassToEngineGroup.equal_range(static_cast<uint16_t>(itr->second.engineClass));
        for (auto l0EngineEntryInMap = engineGroupRange.first; l0EngineEntryInMap != engineGroupRange.second; l0EngineEntryInMap++) {
            auto l0EngineType = l0EngineEntryInMap->second;
            engineGroupInstance.insert({l0EngineType, {static_cast<uint32_t>(itr->second.engineInstance), tileId}});
            if (pSysmanKmdInterface->isGroupEngineInterfaceAvailable() || pSysmanProductHelper->isAggregationOfSingleEnginesSupported()) {
                engineGroupInstance.insert({LinuxEngineImp::getGroupFromEngineType(l0EngineType), {0u, tileId}});
                engineGroupInstance.insert({ZES_ENGINE_GROUP_ALL, {0u, tileId}});
            }
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxEngineImp::getActivity(zes_engine_stats_t *pStats) {

    auto pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
    if (!fdList.empty()) {
        return pSysmanKmdInterface->readBusynessFromGroupFd(pPmuInterface, fdList[0], pStats);
    } else if (pSysmanProductHelper->isAggregationOfSingleEnginesSupported()) {
        return pSysmanProductHelper->getGroupEngineBusynessFromSingleEngines(pLinuxSysmanImp, pStats, engineGroup);
    } else {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
}

ze_result_t LinuxEngineImp::getProperties(zes_engine_properties_t &properties) {
    properties.type = engineGroup;
    properties.onSubdevice = onSubDevice;
    properties.subdeviceId = tileId;
    return ZE_RESULT_SUCCESS;
}

void LinuxEngineImp::init() {
    initStatus = pSysmanKmdInterface->getEngineActivityFdListAndConfigPair(engineGroup, engineInstance, gtId, pPmuInterface, fdList, pmuConfigPair);
}

bool LinuxEngineImp::isEngineModuleSupported() {
    if (initStatus != ZE_RESULT_SUCCESS) {
        return false;
    }
    return true;
}

void LinuxEngineImp::getConfigPair(std::pair<uint64_t, uint64_t> &configPair) {
    configPair = pmuConfigPair;
    return;
}

LinuxEngineImp::LinuxEngineImp(OsSysman *pOsSysman, zes_engine_group_t type, uint32_t engineInstance, uint32_t tileId, ze_bool_t onSubDevice) : engineGroup(type), engineInstance(engineInstance), tileId(tileId), onSubDevice(onSubDevice) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pDrm = pLinuxSysmanImp->getDrm();
    pDevice = pLinuxSysmanImp->getSysmanDeviceImp();
    pPmuInterface = pLinuxSysmanImp->getPmuInterface();
    pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    if (!isGroupEngineHandle(type)) {
        auto engineClass = engineGroupToEngineClass.find(type);
        gtId = pDrm->getIoctlHelper()->getGtIdFromTileId(tileId, engineClass->second);
    }
    init();
    if (initStatus != ZE_RESULT_SUCCESS) {
        cleanup();
    }
}

std::unique_ptr<OsEngine> OsEngine::create(OsSysman *pOsSysman, zes_engine_group_t type, uint32_t engineInstance, uint32_t tileId, ze_bool_t onSubDevice) {
    std::unique_ptr<LinuxEngineImp> pLinuxEngineImp = std::make_unique<LinuxEngineImp>(pOsSysman, type, engineInstance, tileId, onSubDevice);
    return pLinuxEngineImp;
}

static int32_t getFdList(PmuInterface *const &pPmuInterface, std::vector<uint64_t> &engineConfigs, std::vector<int64_t> &fdList) {

    for (auto &engineConfig : engineConfigs) {
        int64_t fd = -1;
        if (fdList.empty()) {
            fd = pPmuInterface->pmuInterfaceOpen(engineConfig, -1, PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_GROUP);
            if (fd < 0) {
                return -1;
            }
        } else {
            fd = pPmuInterface->pmuInterfaceOpen(engineConfig, static_cast<int>(fdList[0]), PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_GROUP);
            if (fd < 0) {
                return -1;
            }
        }
        fdList.push_back(fd);
    }

    return 0;
}

static void closeFds(std::vector<int64_t> &fdList) {

    if (!fdList.empty()) {
        for (auto &fd : fdList) {
            DEBUG_BREAK_IF(fd < 0);
            close(static_cast<int>(fd));
        }
        fdList.clear();
    }
}

void OsEngine::initGroupEngineHandleGroupFd(OsSysman *pOsSysman) {

    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    auto pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();

    if (!pSysmanProductHelper->isAggregationOfSingleEnginesSupported()) {
        return;
    }

    auto pPmuInterface = pLinuxSysmanImp->getPmuInterface();
    auto pSysmanDeviceImp = pLinuxSysmanImp->getSysmanDeviceImp();

    std::vector<uint64_t> mediaEngineConfigs{};
    std::vector<uint64_t> computeEngineConfigs{};
    std::vector<uint64_t> copyEngineConfigs{};
    std::vector<uint64_t> renderEngineConfigs{};
    std::vector<uint64_t> allEnginesConfigs{};

    for (auto &engine : pSysmanDeviceImp->pEngineHandleContext->handleList) {

        zes_engine_properties_t engineProperties = {};
        engine->engineGetProperties(&engineProperties);

        if (engineProperties.type == ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE || engineProperties.type == ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE || engineProperties.type == ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE) {
            mediaEngineConfigs.push_back(engine->configPair.first);
            mediaEngineConfigs.push_back(engine->configPair.second);
        } else if (engineProperties.type == ZES_ENGINE_GROUP_COMPUTE_SINGLE) {
            computeEngineConfigs.push_back(engine->configPair.first);
            computeEngineConfigs.push_back(engine->configPair.second);
        } else if (engineProperties.type == ZES_ENGINE_GROUP_COPY_SINGLE) {
            copyEngineConfigs.push_back(engine->configPair.first);
            copyEngineConfigs.push_back(engine->configPair.second);
        } else if (engineProperties.type == ZES_ENGINE_GROUP_RENDER_SINGLE) {
            renderEngineConfigs.push_back(engine->configPair.first);
            renderEngineConfigs.push_back(engine->configPair.second);
        }
    }

    uint64_t allEngineGroupsSize = mediaEngineConfigs.size() + computeEngineConfigs.size() + copyEngineConfigs.size() + renderEngineConfigs.size();
    allEnginesConfigs.reserve(allEngineGroupsSize);

    allEnginesConfigs.insert(allEnginesConfigs.end(), mediaEngineConfigs.begin(), mediaEngineConfigs.end());
    allEnginesConfigs.insert(allEnginesConfigs.end(), computeEngineConfigs.begin(), computeEngineConfigs.end());
    allEnginesConfigs.insert(allEnginesConfigs.end(), copyEngineConfigs.begin(), copyEngineConfigs.end());
    allEnginesConfigs.insert(allEnginesConfigs.end(), renderEngineConfigs.begin(), renderEngineConfigs.end());

    std::vector<std::unique_ptr<L0::Sysman::Engine>>::iterator it = pSysmanDeviceImp->pEngineHandleContext->handleList.begin();
    while (it != pSysmanDeviceImp->pEngineHandleContext->handleList.end()) {

        int32_t ret = 0;
        zes_engine_properties_t engineProperties = {};
        (*it)->engineGetProperties(&engineProperties);

        if (engineProperties.type == ZES_ENGINE_GROUP_MEDIA_ALL) {
            ret = getFdList(pPmuInterface, mediaEngineConfigs, (*it)->fdList);
        } else if (engineProperties.type == ZES_ENGINE_GROUP_COMPUTE_ALL) {
            ret = getFdList(pPmuInterface, computeEngineConfigs, (*it)->fdList);
        } else if (engineProperties.type == ZES_ENGINE_GROUP_COPY_ALL) {
            ret = getFdList(pPmuInterface, copyEngineConfigs, (*it)->fdList);
        } else if (engineProperties.type == ZES_ENGINE_GROUP_RENDER_ALL) {
            ret = getFdList(pPmuInterface, renderEngineConfigs, (*it)->fdList);
        } else if (engineProperties.type == ZES_ENGINE_GROUP_ALL) {
            ret = getFdList(pPmuInterface, allEnginesConfigs, (*it)->fdList);
        }

        if (ret < 0) {
            closeFds((*it)->fdList);
            it = pSysmanDeviceImp->pEngineHandleContext->handleList.erase(it);
        } else {
            ++it;
        }
    }

    return;
}

void OsEngine::closeFdsForGroupEngineHandles(OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    auto pSysmanDeviceImp = pLinuxSysmanImp->getSysmanDeviceImp();

    for (auto &engine : pSysmanDeviceImp->pEngineHandleContext->handleList) {
        closeFds(engine->fdList);
    }
}

} // namespace Sysman
} // namespace L0

/*
 * Copyright (C) 2020-2024 Intel Corporation
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
#include "level_zero/sysman/source/shared/linux/sysman_hw_device_id_linux.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"
#include "level_zero/sysman/source/sysman_const.h"

namespace L0 {
namespace Sysman {

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
        uint32_t subDeviceId = itr->first;
        auto engineGroupRange = engineClassToEngineGroup.equal_range(static_cast<uint16_t>(itr->second.engineClass));
        for (auto l0EngineEntryInMap = engineGroupRange.first; l0EngineEntryInMap != engineGroupRange.second; l0EngineEntryInMap++) {
            auto l0EngineType = l0EngineEntryInMap->second;
            engineGroupInstance.insert({l0EngineType, {static_cast<uint32_t>(itr->second.engineInstance), subDeviceId}});
            if (pSysmanKmdInterface->isGroupEngineInterfaceAvailable()) {
                engineGroupInstance.insert({LinuxEngineImp::getGroupFromEngineType(l0EngineType), {0u, subDeviceId}});
                engineGroupInstance.insert({ZES_ENGINE_GROUP_ALL, {0u, subDeviceId}});
            }
        }
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxEngineImp::getActivity(zes_engine_stats_t *pStats) {
    if (fd < 0) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): as fileDescriptor value = %d it's returning error:0x%x \n", __FUNCTION__, fd, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    uint64_t data[2] = {};
    auto ret = pPmuInterface->pmuRead(static_cast<int>(fd), data, sizeof(data));
    if (ret < 0) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():pmuRead is returning value:%d and error:0x%x \n", __FUNCTION__, ret, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
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

void LinuxEngineImp::init() {
    fd = pSysmanKmdInterface->getEngineActivityFd(engineGroup, engineInstance, subDeviceId, pPmuInterface);
}

bool LinuxEngineImp::isEngineModuleSupported() {
    if (fd < 0) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): as fileDescriptor value = %d Engine Module is not supported \n", __FUNCTION__, fd);
        return false;
    }
    return true;
}

LinuxEngineImp::LinuxEngineImp(OsSysman *pOsSysman, zes_engine_group_t type, uint32_t engineInstance, uint32_t subDeviceId, ze_bool_t onSubDevice) : engineGroup(type), engineInstance(engineInstance), subDeviceId(subDeviceId), onSubDevice(onSubDevice) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pDrm = pLinuxSysmanImp->getDrm();
    pDevice = pLinuxSysmanImp->getSysmanDeviceImp();
    pPmuInterface = pLinuxSysmanImp->getPmuInterface();
    pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    init();
}

std::unique_ptr<OsEngine> OsEngine::create(OsSysman *pOsSysman, zes_engine_group_t type, uint32_t engineInstance, uint32_t subDeviceId, ze_bool_t onSubDevice) {
    std::unique_ptr<LinuxEngineImp> pLinuxEngineImp = std::make_unique<LinuxEngineImp>(pOsSysman, type, engineInstance, subDeviceId, onSubDevice);
    return pLinuxEngineImp;
}

} // namespace Sysman
} // namespace L0

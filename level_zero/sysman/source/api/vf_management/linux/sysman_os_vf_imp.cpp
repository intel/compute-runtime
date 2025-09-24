/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/vf_management/linux/sysman_os_vf_imp.h"

#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/utilities/directory.h"

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/pmu/sysman_pmu_imp.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"
#include "level_zero/sysman/source/sysman_const.h"

namespace L0 {
namespace Sysman {
static const std::string pathForVfTelemetryPrefix = "iov/vf";

ze_result_t LinuxVfImp::getVfBDFAddress(uint32_t vfIdMinusOne, zes_pci_address_t *address) {
    std::string pathForVfBdf = "device/virtfn";
    std::string vfRealPath = "";
    std::string vfPath = pathForVfBdf + std::to_string(vfIdMinusOne);
    ze_result_t result = pSysfsAccess->getRealPath(vfPath, vfRealPath);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get the real path and returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }
    std::size_t loc = vfRealPath.find_last_of("/");
    if (loc == std::string::npos) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get the last occurrence of '/' and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }
    std::string vfBdfString = vfRealPath.substr(loc + 1);

    constexpr int vfBdfTokensNum = 4;
    uint16_t domain = -1;
    uint8_t bus = -1, device = -1, function = -1;
    if (NEO::parseBdfString(vfBdfString, domain, bus, device, function) != vfBdfTokensNum) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get the correct token sum and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }
    address->domain = domain;
    address->bus = bus;
    address->device = device;
    address->function = function;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxVfImp::vfOsGetCapabilities(zes_vf_exp2_capabilities_t *pCapability) {

    const auto pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
    const auto pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();

    uint64_t vfLmemQuota = 0;
    ze_result_t result = pSysmanProductHelper->getVfLocalMemoryQuota(pSysfsAccess, vfLmemQuota, vfId);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    pCapability->vfDeviceMemSize = vfLmemQuota;

    pCapability->vfID = vfId;
    result = getVfBDFAddress((vfId - 1), &pCapability->address);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxVfImp::vfOsGetMemoryUtilization(uint32_t *pCount, zes_vf_util_mem_exp2_t *pMemUtil) {

    const auto pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
    if (!pSysmanProductHelper->isVfMemoryUtilizationSupported()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    uint64_t vfLmemUsed = 0;
    if (*pCount == 0) {
        *pCount = maxMemoryTypes;
        return ZE_RESULT_SUCCESS;
    }
    if (*pCount > maxMemoryTypes) {
        *pCount = maxMemoryTypes;
    }

    if (!vfOsGetLocalMemoryUsed(vfLmemUsed)) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    if (pMemUtil != nullptr) {
        for (uint32_t i = 0; i < *pCount; i++) {
            pMemUtil[i].vfMemLocation = ZES_MEM_LOC_DEVICE;
            pMemUtil[i].vfMemUtilized = vfLmemUsed;
        }
    }

    return ZE_RESULT_SUCCESS;
}

void LinuxVfImp::vfGetInstancesFromEngineInfo(NEO::Drm *pDrm) {

    auto engineTileMap = pDrm->getEngineInfo()->getEngineTileInfo();
    for (const auto &engine : engineTileMap) {
        uint32_t tileId = engine.first;
        uint32_t gtId = pDrm->getIoctlHelper()->getGtIdFromTileId(tileId, engine.second.engineClass);
        auto engineClassToEngineGroupRange = engineClassToEngineGroup.equal_range(static_cast<uint16_t>(engine.second.engineClass));
        for (auto l0EngineEntryInMap = engineClassToEngineGroupRange.first; l0EngineEntryInMap != engineClassToEngineGroupRange.second; l0EngineEntryInMap++) {
            auto l0EngineType = l0EngineEntryInMap->second;
            engineGroupInstance.insert({l0EngineType, {static_cast<uint32_t>(engine.second.engineInstance), gtId}});
        }
    }
}

ze_result_t LinuxVfImp::vfEngineDataInit() {

    const auto pDrm = pLinuxSysmanImp->getDrm();
    const auto pPmuInterface = pLinuxSysmanImp->getPmuInterface();
    const auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();

    auto hwDeviceId = pLinuxSysmanImp->getSysmanHwDeviceIdInstance();
    if (hwDeviceId.getFileDescriptor() < 0) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Could not get Device Id Fd and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    if (pDrm->sysmanQueryEngineInfo() == false) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():sysmanQueryEngineInfo is returning false and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    vfGetInstancesFromEngineInfo(pDrm);
    for (const auto &engine : engineGroupInstance) {
        auto engineClass = engineGroupToEngineClass.find(engine.first);
        UNRECOVERABLE_IF(engineClass == engineGroupToEngineClass.end());
        std::pair<uint64_t, uint64_t> configPair{UINT64_MAX, UINT64_MAX};
        auto result = pSysmanKmdInterface->getBusyAndTotalTicksConfigsForVf(pPmuInterface, vfId, engine.second.first, engineClass->second, engine.second.second, configPair);
        if (result != ZE_RESULT_SUCCESS) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get the busy config and total ticks config and returning error:0x%x \n", __FUNCTION__, result);
            cleanup();
            return result;
        }

        uint64_t busyTicksConfig = configPair.first;
        int64_t busyTicksFd = pPmuInterface->pmuInterfaceOpen(busyTicksConfig, -1, PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_GROUP);
        if (busyTicksFd < 0) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Could not open Busy Ticks Handle and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
            cleanup();
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }

        uint64_t totalTicksConfig = configPair.second;
        int64_t totalTicksFd = pPmuInterface->pmuInterfaceOpen(totalTicksConfig, static_cast<int32_t>(busyTicksFd), PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_GROUP);
        if (totalTicksFd < 0) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Could not open Total Ticks Handle and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
            NEO::SysCalls::close(static_cast<int>(busyTicksFd));
            cleanup();
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }

        EngineUtilsData pEngineUtilsData;
        pEngineUtilsData.engineType = engine.first;
        pEngineUtilsData.busyTicksFd = busyTicksFd;
        pEngineUtilsData.totalTicksFd = totalTicksFd;
        pEngineUtils.push_back(pEngineUtilsData);
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxVfImp::vfOsGetEngineUtilization(uint32_t *pCount, zes_vf_util_engine_exp2_t *pEngineUtil) {

    const auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    if (!pSysmanKmdInterface->isVfEngineUtilizationSupported()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    std::call_once(initEngineDataOnce, [this]() {
        this->vfEngineDataInit();
    });

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
        const auto pPmuInterface = pLinuxSysmanImp->getPmuInterface();
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

bool LinuxVfImp::vfOsGetLocalMemoryUsed(uint64_t &lMemUsed) {
    std::string pathForLmemUsed = "/telemetry/lmem_alloc_size";
    std::string pathForDeviceMemUsed = pathForVfTelemetryPrefix + std::to_string(vfId) + pathForLmemUsed;

    auto result = pSysfsAccess->read(pathForDeviceMemUsed.data(), lMemUsed);
    if (result != ZE_RESULT_SUCCESS) {
        lMemUsed = 0;
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read Local Memory Used with error 0x%x \n", __FUNCTION__, result);
        return false;
    }
    return true;
}

LinuxVfImp::LinuxVfImp(
    OsSysman *pOsSysman, uint32_t vfId) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    this->vfId = vfId;
}

void LinuxVfImp::cleanup() {
    for (const auto &pEngineUtilsData : pEngineUtils) {
        DEBUG_BREAK_IF(pEngineUtilsData.busyTicksFd < 0);
        NEO::SysCalls::close(static_cast<int>(pEngineUtilsData.busyTicksFd));
        DEBUG_BREAK_IF(pEngineUtilsData.totalTicksFd < 0);
        NEO::SysCalls::close(static_cast<int>(pEngineUtilsData.totalTicksFd));
    }
    pEngineUtils.clear();
}

LinuxVfImp::~LinuxVfImp() {
    cleanup();
}

std::unique_ptr<OsVf> OsVf::create(
    OsSysman *pOsSysman, uint32_t vfId) {
    std::unique_ptr<LinuxVfImp> pLinuxVfImp = std::make_unique<LinuxVfImp>(pOsSysman, vfId);
    return pLinuxVfImp;
}

uint32_t OsVf::getNumEnabledVfs(OsSysman *pOsSysman) {
    constexpr std::string_view pathForNumberOfVfs = "device/sriov_numvfs";
    uint32_t numberOfVfs = 0;
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    SysFsAccessInterface *pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    auto result = pSysfsAccess->read(pathForNumberOfVfs.data(), numberOfVfs);
    if (result != ZE_RESULT_SUCCESS) {
        numberOfVfs = 0;
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read Number Of Vfs with error 0x%x \n", __FUNCTION__, result);
    }
    return numberOfVfs;
}

} // namespace Sysman
} // namespace L0

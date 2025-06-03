/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/source/os_interface/linux/system_info.h"

#include "level_zero/sysman/source/api/ras/linux/ras_util/sysman_ras_util.h"
#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util.h"
#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/pmt/sysman_pmt.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"
#include "level_zero/sysman/source/sysman_const.h"

#include <algorithm>

namespace L0 {
namespace Sysman {

template <PRODUCT_FAMILY gfxProduct>
const std::map<std::string, std::map<std::string, uint64_t>> *SysmanProductHelperHw<gfxProduct>::getGuidToKeyOffsetMap() {
    return nullptr;
}

template <PRODUCT_FAMILY gfxProduct>
void SysmanProductHelperHw<gfxProduct>::getFrequencyStepSize(double *pStepSize) {
    *pStepSize = (50.0 / 3); // Step of 16.6666667 Mhz
}

template <PRODUCT_FAMILY gfxProduct>
ze_result_t SysmanProductHelperHw<gfxProduct>::getMemoryProperties(zes_mem_properties_t *pProperties, LinuxSysmanImp *pLinuxSysmanImp, NEO::Drm *pDrm, SysmanKmdInterface *pSysmanKmdInterface, uint32_t subDeviceId, bool isSubdevice) {
    auto pSysFsAccess = pSysmanKmdInterface->getSysFsAccess();

    pProperties->location = ZES_MEM_LOC_DEVICE;
    pProperties->type = ZES_MEM_TYPE_DDR;
    pProperties->onSubdevice = isSubdevice;
    pProperties->subdeviceId = subDeviceId;
    pProperties->busWidth = -1;
    pProperties->numChannels = -1;
    pProperties->physicalSize = 0;

    auto hwDeviceId = pLinuxSysmanImp->getSysmanHwDeviceIdInstance();
    auto status = pDrm->querySystemInfo();

    if (status) {
        auto memSystemInfo = pDrm->getSystemInfo();
        if (memSystemInfo != nullptr) {
            auto memType = memSystemInfo->getMemoryType();
            switch (memType) {
            case NEO::DeviceBlobConstants::MemoryType::hbm2e:
            case NEO::DeviceBlobConstants::MemoryType::hbm2:
            case NEO::DeviceBlobConstants::MemoryType::hbm3:
                pProperties->type = ZES_MEM_TYPE_HBM;
                break;
            case NEO::DeviceBlobConstants::MemoryType::lpddr4:
                pProperties->type = ZES_MEM_TYPE_LPDDR4;
                break;
            case NEO::DeviceBlobConstants::MemoryType::lpddr5:
                pProperties->type = ZES_MEM_TYPE_LPDDR5;
                break;
            case NEO::DeviceBlobConstants::MemoryType::gddr6:
                pProperties->type = ZES_MEM_TYPE_GDDR6;
                break;
            default:
                DEBUG_BREAK_IF(true);
                return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
            }

            if (pProperties->type == ZES_MEM_TYPE_HBM) {
                pProperties->numChannels = memSystemInfo->getNumHbmStacksPerTile() * memSystemInfo->getNumChannlesPerHbmStack();
            } else {
                pProperties->numChannels = memSystemInfo->getMaxMemoryChannels();
            }
        }
    }

    pProperties->busWidth = memoryBusWidth;
    pProperties->physicalSize = 0;

    if (pSysmanKmdInterface->isPhysicalMemorySizeSupported() == true) {
        if (isSubdevice) {
            std::string memval;
            std::string physicalSizeFile = pSysmanKmdInterface->getSysfsFilePathForPhysicalMemorySize(subDeviceId);
            ze_result_t result = pSysFsAccess->read(physicalSizeFile, memval);
            uint64_t intval = strtoull(memval.c_str(), nullptr, 16);
            if (ZE_RESULT_SUCCESS != result) {
                pProperties->physicalSize = 0u;
            } else {
                pProperties->physicalSize = intval;
            }
        }
    }

    return ZE_RESULT_SUCCESS;
}

template <PRODUCT_FAMILY gfxProduct>
ze_result_t SysmanProductHelperHw<gfxProduct>::getMemoryBandwidth(zes_mem_bandwidth_t *pBandwidth, LinuxSysmanImp *pLinuxSysmanImp, uint32_t subdeviceId) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

template <PRODUCT_FAMILY gfxProduct>
void SysmanProductHelperHw<gfxProduct>::getMemoryHealthIndicator(FirmwareUtil *pFwInterface, zes_mem_health_t *health) {
    *health = ZES_MEM_HEALTH_UNKNOWN;
}

template <PRODUCT_FAMILY gfxProduct>
void SysmanProductHelperHw<gfxProduct>::getMediaPerformanceFactorMultiplier(const double performanceFactor, double *pMultiplier) {
    if (performanceFactor > halfOfMaxPerformanceFactor) {
        *pMultiplier = 1;
    } else if (performanceFactor > minPerformanceFactor) {
        *pMultiplier = 0.5;
    } else {
        *pMultiplier = 0;
    }
}

template <PRODUCT_FAMILY gfxProduct>
bool SysmanProductHelperHw<gfxProduct>::isPerfFactorSupported() {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool SysmanProductHelperHw<gfxProduct>::isMemoryMaxTemperatureSupported() {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool SysmanProductHelperHw<gfxProduct>::isFrequencySetRangeSupported() {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
zes_freq_throttle_reason_flags_t SysmanProductHelperHw<gfxProduct>::getThrottleReasons(LinuxSysmanImp *pLinuxSysmanImp, uint32_t subdeviceId) {

    zes_freq_throttle_reason_flags_t throttleReasons = 0u;
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    auto pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    const std::string baseDir = pSysmanKmdInterface->getBasePath(subdeviceId);
    bool baseDirectoryExists = false;

    if (pSysfsAccess->directoryExists(baseDir)) {
        baseDirectoryExists = true;
    }

    uint32_t val = 0;
    std::string throttleReasonStatusFile = pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonStatus, subdeviceId, baseDirectoryExists);
    auto result = pSysfsAccess->read(throttleReasonStatusFile, val);
    if (ZE_RESULT_SUCCESS == result && val) {

        std::string throttleReasonPL1File = pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonPL1, subdeviceId, baseDirectoryExists);
        std::string throttleReasonPL2File = pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonPL2, subdeviceId, baseDirectoryExists);
        std::string throttleReasonPL4File = pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonPL4, subdeviceId, baseDirectoryExists);
        std::string throttleReasonThermalFile = pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonThermal, subdeviceId, baseDirectoryExists);

        if ((ZE_RESULT_SUCCESS == pSysfsAccess->read(throttleReasonPL1File, val)) && val) {
            throttleReasons |= ZES_FREQ_THROTTLE_REASON_FLAG_AVE_PWR_CAP;
        }
        if ((ZE_RESULT_SUCCESS == pSysfsAccess->read(throttleReasonPL2File, val)) && val) {
            throttleReasons |= ZES_FREQ_THROTTLE_REASON_FLAG_BURST_PWR_CAP;
        }
        if ((ZE_RESULT_SUCCESS == pSysfsAccess->read(throttleReasonPL4File, val)) && val) {
            throttleReasons |= ZES_FREQ_THROTTLE_REASON_FLAG_CURRENT_LIMIT;
        }
        if ((ZE_RESULT_SUCCESS == pSysfsAccess->read(throttleReasonThermalFile, val)) && val) {
            throttleReasons |= ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT;
        }
    } else {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read file %s, returning error 0x%x>\n", __func__, throttleReasonStatusFile.c_str(), result);
    }

    return throttleReasons;
}

template <PRODUCT_FAMILY gfxProduct>
ze_result_t SysmanProductHelperHw<gfxProduct>::getGlobalMaxTemperature(LinuxSysmanImp *pLinuxSysmanImp, double *pTemperature, uint32_t subdeviceId) {

    std::string telemDir = "";
    std::string guid = "";
    uint64_t telemOffset = 0;

    if (!pLinuxSysmanImp->getTelemData(subdeviceId, telemDir, guid, telemOffset)) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    std::map<std::string, uint64_t> keyOffsetMap;
    auto pGuidToKeyOffsetMap = this->getGuidToKeyOffsetMap();
    if (pGuidToKeyOffsetMap == nullptr) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    auto keyOffsetMapEntry = pGuidToKeyOffsetMap->find(guid);
    if (keyOffsetMapEntry == pGuidToKeyOffsetMap->end()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    keyOffsetMap = keyOffsetMapEntry->second;

    auto isValidTemperature = [](auto temperature) {
        if ((temperature > invalidMaxTemperature) || (temperature < invalidMinTemperature)) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): temperature:%f is not in valid limits \n", __FUNCTION__, temperature);
            return false;
        }
        return true;
    };

    auto getMaxTemperature = [&](auto temperature, auto numTemperatureEntries) {
        uint32_t maxTemperature = 0;
        for (uint32_t count = 0; count < numTemperatureEntries; count++) {
            uint32_t localTemperatureVal = (temperature >> (8 * count)) & 0xff;
            if (isValidTemperature(localTemperatureVal)) {
                if (localTemperatureVal > maxTemperature) {
                    maxTemperature = localTemperatureVal;
                }
            }
        }
        return maxTemperature;
    };

    std::string key = "SOC_TEMPERATURES";
    uint64_t socTemperature = 0;
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemDir, key, telemOffset, socTemperature)) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): readValue for SOC_TEMPERATURES returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_NOT_AVAILABLE);
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    uint32_t maxSocTemperature = getMaxTemperature(socTemperature, numSocTemperatureEntries);
    *pTemperature = static_cast<double>(maxSocTemperature);
    return ZE_RESULT_SUCCESS;
}

template <PRODUCT_FAMILY gfxProduct>
ze_result_t SysmanProductHelperHw<gfxProduct>::getGpuMaxTemperature(LinuxSysmanImp *pLinuxSysmanImp, double *pTemperature, uint32_t subdeviceId) {

    std::string telemDir = "";
    std::string guid = "";
    uint64_t telemOffset = 0;

    if (!pLinuxSysmanImp->getTelemData(subdeviceId, telemDir, guid, telemOffset)) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    std::map<std::string, uint64_t> keyOffsetMap;
    auto pGuidToKeyOffsetMap = this->getGuidToKeyOffsetMap();
    if (pGuidToKeyOffsetMap == nullptr) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    auto keyOffsetMapEntry = pGuidToKeyOffsetMap->find(guid);
    if (keyOffsetMapEntry == pGuidToKeyOffsetMap->end()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    keyOffsetMap = keyOffsetMapEntry->second;

    double gpuMaxTemperature = 0;
    uint64_t socTemperature = 0;
    std::string key = "SOC_TEMPERATURES";
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemDir, key, telemOffset, socTemperature)) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): readValue for SOC_TEMPERATURES returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_NOT_AVAILABLE);
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    gpuMaxTemperature = static_cast<double>(socTemperature & 0xff);
    *pTemperature = gpuMaxTemperature;
    return ZE_RESULT_SUCCESS;
}

template <PRODUCT_FAMILY gfxProduct>
ze_result_t SysmanProductHelperHw<gfxProduct>::getMemoryMaxTemperature(LinuxSysmanImp *pLinuxSysmanImp, double *pTemperature, uint32_t subdeviceId) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

template <PRODUCT_FAMILY gfxProduct>
RasInterfaceType SysmanProductHelperHw<gfxProduct>::getGtRasUtilInterface() {
    return RasInterfaceType::none;
}

template <PRODUCT_FAMILY gfxProduct>
RasInterfaceType SysmanProductHelperHw<gfxProduct>::getHbmRasUtilInterface() {
    return RasInterfaceType::none;
}

template <PRODUCT_FAMILY gfxProduct>
bool SysmanProductHelperHw<gfxProduct>::isRepairStatusSupported() {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
int32_t SysmanProductHelperHw<gfxProduct>::getPowerLimitValue(uint64_t value) {
    uint64_t val = value / milliFactor;
    return static_cast<int32_t>(val);
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t SysmanProductHelperHw<gfxProduct>::setPowerLimitValue(int32_t value) {
    uint64_t val = static_cast<uint64_t>(value) * milliFactor;
    return val;
}

template <PRODUCT_FAMILY gfxProduct>
zes_limit_unit_t SysmanProductHelperHw<gfxProduct>::getPowerLimitUnit() {
    return ZES_LIMIT_UNIT_POWER;
}

template <PRODUCT_FAMILY gfxProduct>
bool SysmanProductHelperHw<gfxProduct>::isPowerSetLimitSupported() {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
std::string SysmanProductHelperHw<gfxProduct>::getPackageCriticalPowerLimitFile() {
    return "power1_crit";
}

template <PRODUCT_FAMILY gfxProduct>
SysfsValueUnit SysmanProductHelperHw<gfxProduct>::getPackageCriticalPowerLimitNativeUnit() {
    return SysfsValueUnit::micro;
}

template <PRODUCT_FAMILY gfxProduct>
ze_result_t SysmanProductHelperHw<gfxProduct>::getPowerEnergyCounter(zes_power_energy_counter_t *pEnergy, LinuxSysmanImp *pLinuxSysmanImp, zes_power_domain_t powerDomain, uint32_t subdeviceId) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

template <PRODUCT_FAMILY gfxProduct>
bool SysmanProductHelperHw<gfxProduct>::isStandbySupported(SysmanKmdInterface *pSysmanKmdInterface) {
    return pSysmanKmdInterface->isStandbyModeControlAvailable();
}

template <PRODUCT_FAMILY gfxProduct>
void SysmanProductHelperHw<gfxProduct>::getDeviceSupportedFwTypes(FirmwareUtil *pFwInterface, std::vector<std::string> &fwTypes) {
    fwTypes.clear();
    pFwInterface->getDeviceSupportedFwTypes(fwTypes);
}

template <PRODUCT_FAMILY gfxProduct>
bool SysmanProductHelperHw<gfxProduct>::isLateBindingSupported() {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool SysmanProductHelperHw<gfxProduct>::isEccConfigurationSupported() {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool SysmanProductHelperHw<gfxProduct>::isUpstreamPortConnected() {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
ze_result_t SysmanProductHelperHw<gfxProduct>::getPciProperties(zes_pci_properties_t *pProperties) {
    pProperties->haveBandwidthCounters = false;
    pProperties->havePacketCounters = false;
    pProperties->haveReplayCounters = false;
    return ZE_RESULT_SUCCESS;
}

template <PRODUCT_FAMILY gfxProduct>
ze_result_t SysmanProductHelperHw<gfxProduct>::getPciStats(zes_pci_stats_t *pStats, LinuxSysmanImp *pLinuxSysmanImp) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
};

template <PRODUCT_FAMILY gfxProduct>
bool SysmanProductHelperHw<gfxProduct>::isZesInitSupported() {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool SysmanProductHelperHw<gfxProduct>::isAggregationOfSingleEnginesSupported() {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
ze_result_t SysmanProductHelperHw<gfxProduct>::getGroupEngineBusynessFromSingleEngines(LinuxSysmanImp *pLinuxSysmanImp, zes_engine_stats_t *pStats, zes_engine_group_t &engineGroup) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

template <PRODUCT_FAMILY gfxProduct>
bool SysmanProductHelperHw<gfxProduct>::isVfMemoryUtilizationSupported() {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
ze_result_t SysmanProductHelperHw<gfxProduct>::getVfLocalMemoryQuota(SysFsAccessInterface *pSysfsAccess, uint64_t &lMemQuota, const uint32_t &vfId) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace Sysman
} // namespace L0

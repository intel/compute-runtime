/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/pmt/sysman_pmt.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.inl"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"
#include "level_zero/zes_intel_gpu_sysman.h"

namespace L0 {
namespace Sysman {
constexpr static auto gfxProduct = IGFX_CRI;

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_xe_hp_and_later.inl"
#include "level_zero/sysman/source/shared/product_helper/sysman_os_agnostic_product_helper_xe2_and_later.inl"

static std::map<std::string, std::map<std::string, uint64_t>> guidToKeyOffsetMap = {
    {"0x1e2fa030",
     {{"VR_TEMPERATURE_0", 200},
      {"VR_TEMPERATURE_1", 204},
      {"VR_TEMPERATURE_2", 208},
      {"VR_TEMPERATURE_3", 212}}}};

template <>
bool SysmanProductHelperHw<gfxProduct>::isFrequencySetRangeSupported() {
    return false;
}

template <>
bool SysmanProductHelperHw<gfxProduct>::isPerfFactorSupported() {
    return false;
}

template <>
bool SysmanProductHelperHw<gfxProduct>::isStandbySupported(SysmanKmdInterface *pSysmanKmdInterface) {
    return false;
}

template <>
void SysmanProductHelperHw<gfxProduct>::getDeviceSupportedFwTypes(FirmwareUtil *pFwInterface, std::vector<std::string> &fwTypes) {
    fwTypes.clear();
    return;
}

template <>
bool SysmanProductHelperHw<gfxProduct>::isPowerSetLimitSupported() {
    return false;
}

template <>
int32_t SysmanProductHelperHw<gfxProduct>::getPowerMinLimit(const int32_t &defaultLimit) {
    return static_cast<int32_t>(defaultLimit * minPowerLimitFactor);
}

template <>
RasInterfaceType SysmanProductHelperHw<gfxProduct>::getGtRasUtilInterface() {
    return RasInterfaceType::netlink;
}

template <>
RasInterfaceType SysmanProductHelperHw<gfxProduct>::getHbmRasUtilInterface() {
    return RasInterfaceType::netlink;
}

static zes_freq_throttle_reason_flags_t getAggregatedThrottleReasons(const zes_intel_freq_throttle_detailed_reason_exp_flags_t &pDetailedThrottleReasons) {

    const zes_freq_throttle_reason_flags_t powerFlags =
        static_cast<zes_freq_throttle_reason_flags_t>(ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_CARD_PL1 |
                                                      ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_CARD_PL2 |
                                                      ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_CARD_PL4 |
                                                      ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_PACKAGE_PL1 |
                                                      ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_PACKAGE_PL2 |
                                                      ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_PACKAGE_PL4 |
                                                      ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_FAST_VMODE |
                                                      ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_ICCMAX);

    const zes_freq_throttle_reason_flags_t thermalFlags =
        static_cast<zes_freq_throttle_reason_flags_t>(ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_THERMAL_MEMORY |
                                                      ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_THERMAL_PROCHOT |
                                                      ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_THERMAL_RATL |
                                                      ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_THERMAL_SOC |
                                                      ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_THERMAL_SOC_AVG |
                                                      ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_THERMAL_VR);

    const zes_freq_throttle_reason_flags_t voltageFlags =
        static_cast<zes_freq_throttle_reason_flags_t>(ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_VOLTAGE_P0_FREQ);

    zes_freq_throttle_reason_flags_t aggregatedReasons = 0u;

    if (pDetailedThrottleReasons & powerFlags) {
        aggregatedReasons |= static_cast<zes_freq_throttle_reason_flags_t>(ZES_INTEL_FREQ_THROTTLE_AGGREGATED_REASON_EXP_FLAG_POWER);
    }
    if (pDetailedThrottleReasons & thermalFlags) {
        aggregatedReasons |= ZES_FREQ_THROTTLE_REASON_FLAG_THERMAL_LIMIT;
    }
    if (pDetailedThrottleReasons & voltageFlags) {
        aggregatedReasons |= static_cast<zes_freq_throttle_reason_flags_t>(ZES_INTEL_FREQ_THROTTLE_AGGREGATED_REASON_EXP_FLAG_VOLTAGE);
    }

    return aggregatedReasons;
}

static zes_intel_freq_throttle_detailed_reason_exp_flags_t getDetailedThrottleReasons(SysmanKmdInterface *pSysmanKmdInterface, SysFsAccessInterface *pSysfsAccess, uint32_t subdeviceId) {
    zes_intel_freq_throttle_detailed_reason_exp_flags_t detailedThrottleReasons = 0u;

    const std::string baseDir = pSysmanKmdInterface->getBasePath(subdeviceId);
    bool baseDirectoryExists = pSysfsAccess->directoryExists(baseDir);

    uint32_t reasonStatusVal = 0;
    std::string throttleReasonStatusFile = pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonStatus, subdeviceId, baseDirectoryExists);
    auto result = pSysfsAccess->read(throttleReasonStatusFile, reasonStatusVal);

    if (ZE_RESULT_SUCCESS != result) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read file %s, returning error 0x%x>\n", __func__, throttleReasonStatusFile.c_str(), result);
        return detailedThrottleReasons;
    }

    if (reasonStatusVal == 0) {
        return detailedThrottleReasons;
    }

    static constexpr std::pair<const char *, zes_intel_freq_throttle_detailed_reason_exp_flag_t> throttleReasonMap[] = {
        {"freq0/throttle/reason_psys_pl1", ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_CARD_PL1},
        {"freq0/throttle/reason_psys_pl2", ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_CARD_PL2},
        {"freq0/throttle/reason_psys_crit", ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_CARD_PL4},
        {"freq0/throttle/reason_pl1", ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_PACKAGE_PL1},
        {"freq0/throttle/reason_pl2", ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_PACKAGE_PL2},
        {"freq0/throttle/reason_pl4", ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_PACKAGE_PL4},
        {"freq0/throttle/reason_fastvmode", ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_FAST_VMODE},
        {"freq0/throttle/reason_iccmax", ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_ICCMAX},
        {"freq0/throttle/reason_soc_thermal", ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_THERMAL_SOC},
        {"freq0/throttle/reason_soc_avg_thermal", ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_THERMAL_SOC_AVG},
        {"freq0/throttle/reason_memory_thermal", ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_THERMAL_MEMORY},
        {"freq0/throttle/reason_vr_thermal", ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_THERMAL_VR},
        {"freq0/throttle/reason_ratl", ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_THERMAL_RATL},
        {"freq0/throttle/reason_prochot", ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_THERMAL_PROCHOT},
        {"freq0/throttle/reason_p0_freq", ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_VOLTAGE_P0_FREQ}};

    uint32_t detailedThrottleReasonVal = 0u;
    for (const auto &reasonPair : throttleReasonMap) {
        std::string filePath = baseDirectoryExists ? pSysmanKmdInterface->getBasePath(subdeviceId) + reasonPair.first : std::string("");
        detailedThrottleReasonVal = 0u;
        if ((pSysfsAccess->read(filePath, detailedThrottleReasonVal) == ZE_RESULT_SUCCESS) && detailedThrottleReasonVal) {
            detailedThrottleReasons |= reasonPair.second;
        }
    }

    return detailedThrottleReasons;
}

template <>
zes_freq_throttle_reason_flags_t SysmanProductHelperHw<gfxProduct>::getThrottleReasons(SysmanKmdInterface *pSysmanKmdInterface, SysFsAccessInterface *pSysfsAccess, uint32_t subdeviceId, void *pNext) {
    zes_intel_freq_throttle_detailed_reason_exp_flags_t detailedThrottleReasons = getDetailedThrottleReasons(pSysmanKmdInterface, pSysfsAccess, subdeviceId);
    zes_freq_throttle_reason_flags_t aggregatedReasons = getAggregatedThrottleReasons(detailedThrottleReasons);
    void *pCurrent = pNext;
    while (pCurrent) {
        auto pExpThrottleReason = reinterpret_cast<zes_base_properties_t *>(pCurrent);
        if (pExpThrottleReason->stype == ZES_INTEL_STRUCTURE_TYPE_FREQUENCY_THROTTLE_DETAILED_REASON_EXP) {
            auto pDetailedThrottleReason = reinterpret_cast<zes_intel_freq_throttle_detailed_reason_exp_t *>(pExpThrottleReason);
            pDetailedThrottleReason->detailedReasons = detailedThrottleReasons;
            break;
        }
        pCurrent = pExpThrottleReason->pNext;
    }
    return aggregatedReasons;
}

template <>
void SysmanProductHelperHw<gfxProduct>::getSupportedSensors(std::map<zes_temp_sensors_t, uint32_t> &supportedSensorTypeMap) {
    supportedSensorTypeMap[ZES_TEMP_SENSORS_GLOBAL] = 1;
    supportedSensorTypeMap[ZES_TEMP_SENSORS_GPU] = 1;
    supportedSensorTypeMap[ZES_TEMP_SENSORS_MEMORY] = 1;
    supportedSensorTypeMap[ZES_TEMP_SENSORS_VOLTAGE_REGULATOR] = 4;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getVoltageRegulatorTemperature(LinuxSysmanImp *pLinuxSysmanImp, double *pTemperature, uint32_t subdeviceId, uint32_t sensorIndex) {
    std::string telemDir = "";
    std::string guid = "";
    uint64_t telemOffset = 0;

    if (!pLinuxSysmanImp->getTelemData(subdeviceId, telemDir, guid, telemOffset)) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    telemOffset = 0;

    std::map<std::string, uint64_t> keyOffsetMap;
    auto keyOffsetMapEntry = guidToKeyOffsetMap.find(guid);
    if (keyOffsetMapEntry == guidToKeyOffsetMap.end()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    keyOffsetMap = keyOffsetMapEntry->second;

    // Build the key name based on sensor index
    std::string key = "VR_TEMPERATURE_" + std::to_string(sensorIndex);

    uint32_t vrTemperature = 0;
    if (!PlatformMonitoringTech::readValue(std::move(keyOffsetMap), telemDir, key, telemOffset, vrTemperature)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    switch (sensorIndex) {
    case 0:
        vrTemperature /= 10; // VR_TEMPERATURE_0 is reported in deci-degree celsius
        break;
    case 1:
    case 2:
    case 3:
        vrTemperature = vrTemperature & 0xFF; // VR_TEMPERATURE_1/2/3 is reported in U8.0 format
        break;
    default:
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    *pTemperature = static_cast<double>(vrTemperature);
    return ZE_RESULT_SUCCESS;
}

template <>
const std::map<std::string, std::map<std::string, uint64_t>> *SysmanProductHelperHw<gfxProduct>::getGuidToKeyOffsetMap() {
    return &guidToKeyOffsetMap;
}

template class SysmanProductHelperHw<gfxProduct>;

} // namespace Sysman
} // namespace L0

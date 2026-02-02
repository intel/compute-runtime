/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/pmt_util.h"

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

constexpr static uint32_t memoryMsuCount = 20;
constexpr static uint32_t busWidthPerChannelInBytes = 2; // 16 bits = 2 bytes
constexpr static uint32_t transactionSize = 64;
constexpr static uint32_t memoryBridgeCount = 2;

static std::map<std::string, std::map<std::string, uint64_t>> guidToKeyOffsetMap = {
    {"0x1e2fa030",
     {{"VR_TEMPERATURE_0", 200},
      {"VR_TEMPERATURE_1", 204},
      {"VR_TEMPERATURE_2", 208},
      {"VR_TEMPERATURE_3", 212},
      {"VRAM_BANDWIDTH", 56}}},
    {"0x5e2fa230",
     {{"MEMSS0_PERF_CTR_MB0_CFI_NUM_READ_REQ", 688},
      {"MEMSS1_PERF_CTR_MB0_CFI_NUM_READ_REQ", 768},
      {"MEMSS2_PERF_CTR_MB0_CFI_NUM_READ_REQ", 848},
      {"MEMSS3_PERF_CTR_MB0_CFI_NUM_READ_REQ", 928},
      {"MEMSS4_PERF_CTR_MB0_CFI_NUM_READ_REQ", 1008},
      {"MEMSS5_PERF_CTR_MB0_CFI_NUM_READ_REQ", 1088},
      {"MEMSS6_PERF_CTR_MB0_CFI_NUM_READ_REQ", 1168},
      {"MEMSS7_PERF_CTR_MB0_CFI_NUM_READ_REQ", 1248},
      {"MEMSS8_PERF_CTR_MB0_CFI_NUM_READ_REQ", 1328},
      {"MEMSS9_PERF_CTR_MB0_CFI_NUM_READ_REQ", 1408},
      {"MEMSS10_PERF_CTR_MB0_CFI_NUM_READ_REQ", 1488},
      {"MEMSS11_PERF_CTR_MB0_CFI_NUM_READ_REQ", 1568},
      {"MEMSS12_PERF_CTR_MB0_CFI_NUM_READ_REQ", 1648},
      {"MEMSS13_PERF_CTR_MB0_CFI_NUM_READ_REQ", 1728},
      {"MEMSS14_PERF_CTR_MB0_CFI_NUM_READ_REQ", 1808},
      {"MEMSS15_PERF_CTR_MB0_CFI_NUM_READ_REQ", 1888},
      {"MEMSS16_PERF_CTR_MB0_CFI_NUM_READ_REQ", 1968},
      {"MEMSS17_PERF_CTR_MB0_CFI_NUM_READ_REQ", 2048},
      {"MEMSS18_PERF_CTR_MB0_CFI_NUM_READ_REQ", 2128},
      {"MEMSS19_PERF_CTR_MB0_CFI_NUM_READ_REQ", 2208},
      {"MEMSS0_PERF_CTR_MB1_CFI_NUM_READ_REQ", 728},
      {"MEMSS1_PERF_CTR_MB1_CFI_NUM_READ_REQ", 808},
      {"MEMSS2_PERF_CTR_MB1_CFI_NUM_READ_REQ", 888},
      {"MEMSS3_PERF_CTR_MB1_CFI_NUM_READ_REQ", 968},
      {"MEMSS4_PERF_CTR_MB1_CFI_NUM_READ_REQ", 1048},
      {"MEMSS5_PERF_CTR_MB1_CFI_NUM_READ_REQ", 1128},
      {"MEMSS6_PERF_CTR_MB1_CFI_NUM_READ_REQ", 1208},
      {"MEMSS7_PERF_CTR_MB1_CFI_NUM_READ_REQ", 1288},
      {"MEMSS8_PERF_CTR_MB1_CFI_NUM_READ_REQ", 1368},
      {"MEMSS9_PERF_CTR_MB1_CFI_NUM_READ_REQ", 1448},
      {"MEMSS10_PERF_CTR_MB1_CFI_NUM_READ_REQ", 1528},
      {"MEMSS11_PERF_CTR_MB1_CFI_NUM_READ_REQ", 1608},
      {"MEMSS12_PERF_CTR_MB1_CFI_NUM_READ_REQ", 1688},
      {"MEMSS13_PERF_CTR_MB1_CFI_NUM_READ_REQ", 1768},
      {"MEMSS14_PERF_CTR_MB1_CFI_NUM_READ_REQ", 1848},
      {"MEMSS15_PERF_CTR_MB1_CFI_NUM_READ_REQ", 1928},
      {"MEMSS16_PERF_CTR_MB1_CFI_NUM_READ_REQ", 2008},
      {"MEMSS17_PERF_CTR_MB1_CFI_NUM_READ_REQ", 2088},
      {"MEMSS18_PERF_CTR_MB1_CFI_NUM_READ_REQ", 2168},
      {"MEMSS19_PERF_CTR_MB1_CFI_NUM_READ_REQ", 2248},
      {"MEMSS0_PERF_CTR_MB0_CFI_NUM_WRITE_REQ", 680},
      {"MEMSS1_PERF_CTR_MB0_CFI_NUM_WRITE_REQ", 760},
      {"MEMSS2_PERF_CTR_MB0_CFI_NUM_WRITE_REQ", 840},
      {"MEMSS3_PERF_CTR_MB0_CFI_NUM_WRITE_REQ", 920},
      {"MEMSS4_PERF_CTR_MB0_CFI_NUM_WRITE_REQ", 1000},
      {"MEMSS5_PERF_CTR_MB0_CFI_NUM_WRITE_REQ", 1080},
      {"MEMSS6_PERF_CTR_MB0_CFI_NUM_WRITE_REQ", 1160},
      {"MEMSS7_PERF_CTR_MB0_CFI_NUM_WRITE_REQ", 1240},
      {"MEMSS8_PERF_CTR_MB0_CFI_NUM_WRITE_REQ", 1320},
      {"MEMSS9_PERF_CTR_MB0_CFI_NUM_WRITE_REQ", 1400},
      {"MEMSS10_PERF_CTR_MB0_CFI_NUM_WRITE_REQ", 1480},
      {"MEMSS11_PERF_CTR_MB0_CFI_NUM_WRITE_REQ", 1560},
      {"MEMSS12_PERF_CTR_MB0_CFI_NUM_WRITE_REQ", 1640},
      {"MEMSS13_PERF_CTR_MB0_CFI_NUM_WRITE_REQ", 1720},
      {"MEMSS14_PERF_CTR_MB0_CFI_NUM_WRITE_REQ", 1800},
      {"MEMSS15_PERF_CTR_MB0_CFI_NUM_WRITE_REQ", 1880},
      {"MEMSS16_PERF_CTR_MB0_CFI_NUM_WRITE_REQ", 1960},
      {"MEMSS17_PERF_CTR_MB0_CFI_NUM_WRITE_REQ", 2040},
      {"MEMSS18_PERF_CTR_MB0_CFI_NUM_WRITE_REQ", 2120},
      {"MEMSS19_PERF_CTR_MB0_CFI_NUM_WRITE_REQ", 2200},
      {"MEMSS0_PERF_CTR_MB1_CFI_NUM_WRITE_REQ", 720},
      {"MEMSS1_PERF_CTR_MB1_CFI_NUM_WRITE_REQ", 800},
      {"MEMSS2_PERF_CTR_MB1_CFI_NUM_WRITE_REQ", 880},
      {"MEMSS3_PERF_CTR_MB1_CFI_NUM_WRITE_REQ", 960},
      {"MEMSS4_PERF_CTR_MB1_CFI_NUM_WRITE_REQ", 1040},
      {"MEMSS5_PERF_CTR_MB1_CFI_NUM_WRITE_REQ", 1120},
      {"MEMSS6_PERF_CTR_MB1_CFI_NUM_WRITE_REQ", 1200},
      {"MEMSS7_PERF_CTR_MB1_CFI_NUM_WRITE_REQ", 1280},
      {"MEMSS8_PERF_CTR_MB1_CFI_NUM_WRITE_REQ", 1360},
      {"MEMSS9_PERF_CTR_MB1_CFI_NUM_WRITE_REQ", 1440},
      {"MEMSS10_PERF_CTR_MB1_CFI_NUM_WRITE_REQ", 1520},
      {"MEMSS11_PERF_CTR_MB1_CFI_NUM_WRITE_REQ", 1600},
      {"MEMSS12_PERF_CTR_MB1_CFI_NUM_WRITE_REQ", 1680},
      {"MEMSS13_PERF_CTR_MB1_CFI_NUM_WRITE_REQ", 1760},
      {"MEMSS14_PERF_CTR_MB1_CFI_NUM_WRITE_REQ", 1840},
      {"MEMSS15_PERF_CTR_MB1_CFI_NUM_WRITE_REQ", 1920},
      {"MEMSS16_PERF_CTR_MB1_CFI_NUM_WRITE_REQ", 2000},
      {"MEMSS17_PERF_CTR_MB1_CFI_NUM_WRITE_REQ", 2080},
      {"MEMSS18_PERF_CTR_MB1_CFI_NUM_WRITE_REQ", 2160},
      {"MEMSS19_PERF_CTR_MB1_CFI_NUM_WRITE_REQ", 2240},
      {"NUM_OF_MEM_CHANNEL", 3660}}}};

static ze_result_t getErrorCode(ze_result_t result) {
    if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    return result;
}

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
int32_t SysmanProductHelperHw<gfxProduct>::getPowerMinLimit(const int32_t &defaultLimit) {
    return static_cast<int32_t>(defaultLimit * minPowerLimitFactor);
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getLimitsExp(SysmanKmdInterface *pSysmanKmdInterface, SysFsAccessInterface *pSysfsAccess, const std::map<std::string, std::pair<std::string, bool>> &powerLimitFiles, uint32_t *pLimit) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    uint64_t powerLimit = 0;

    const auto &[sustainedPowerLimitFile, sustainedPowerLimitFileExists] = powerLimitFiles.at("sustainedLimitFile");
    const auto &[burstPowerLimitFile, burstPowerLimitFileExists] = powerLimitFiles.at("burstLimitFile");

    // Return PL1 if enabled, otherwise return PL2
    if (sustainedPowerLimitFileExists) {
        result = pSysfsAccess->read(sustainedPowerLimitFile, powerLimit);
        if (ZE_RESULT_SUCCESS != result) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s and returning error:0x%x \n", __FUNCTION__, sustainedPowerLimitFile.c_str(), getErrorCode(result));
            return getErrorCode(result);
        }
    } else if (burstPowerLimitFileExists) {
        result = pSysfsAccess->read(burstPowerLimitFile, powerLimit);
        if (ZE_RESULT_SUCCESS != result) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s and returning error:0x%x \n", __FUNCTION__, burstPowerLimitFile.c_str(), getErrorCode(result));
            return getErrorCode(result);
        }
    } else {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): No power limit files exist for given power domain , returning unsupported feature\n", __FUNCTION__);
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }

    pSysmanKmdInterface->convertSysfsValueUnit(SysfsValueUnit::milli, SysfsValueUnit::micro, powerLimit, powerLimit);
    *pLimit = static_cast<uint32_t>(powerLimit);
    return ZE_RESULT_SUCCESS;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::setLimitsExp(SysmanKmdInterface *pSysmanKmdInterface, SysFsAccessInterface *pSysfsAccess, const std::map<std::string, std::pair<std::string, bool>> &powerLimitFiles, zes_power_domain_t powerDomain, const uint32_t limit) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    uint64_t val = static_cast<uint64_t>(limit);
    uint64_t convertedVal = 0;
    bool anyLimitSet = false;

    const auto &[sustainedPowerLimitFile, sustainedPowerLimitFileExists] = powerLimitFiles.at("sustainedLimitFile");
    const auto &[burstPowerLimitFile, burstPowerLimitFileExists] = powerLimitFiles.at("burstLimitFile");
    const auto &[criticalPowerLimitFile, criticalPowerLimitFileExists] = powerLimitFiles.at("criticalLimitFile");

    if (powerDomain == ZES_POWER_DOMAIN_CARD) {
        // Card domain: Apply to PL1 and PL2 if enabled, Apply x2 to PsysCRIT if enabled
        if (sustainedPowerLimitFileExists) {
            convertedVal = val;
            pSysmanKmdInterface->convertSysfsValueUnit(pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameCardSustainedPowerLimit), SysfsValueUnit::milli, convertedVal, convertedVal);
            result = pSysfsAccess->write(sustainedPowerLimitFile, convertedVal);
            if (ZE_RESULT_SUCCESS != result) {
                PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->write() failed to write into %s and returning error:0x%x \n", __FUNCTION__, sustainedPowerLimitFile.c_str(), getErrorCode(result));
                return getErrorCode(result);
            }
            anyLimitSet = true;
        }

        if (burstPowerLimitFileExists) {
            convertedVal = val;
            pSysmanKmdInterface->convertSysfsValueUnit(pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameCardBurstPowerLimit), SysfsValueUnit::milli, convertedVal, convertedVal);
            result = pSysfsAccess->write(burstPowerLimitFile, convertedVal);
            if (ZE_RESULT_SUCCESS != result) {
                PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->write() failed to write into %s and returning error:0x%x \n", __FUNCTION__, burstPowerLimitFile.c_str(), getErrorCode(result));
                return getErrorCode(result);
            }
            anyLimitSet = true;
        }

        if (criticalPowerLimitFileExists) {
            convertedVal = val * criticalLimitMultiplyFactor;
            pSysmanKmdInterface->convertSysfsValueUnit(pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameCardCriticalPowerLimit), SysfsValueUnit::milli, convertedVal, convertedVal);
            result = pSysfsAccess->write(criticalPowerLimitFile, convertedVal);
            if (ZE_RESULT_SUCCESS != result) {
                PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->write() failed to write into %s and returning error:0x%x \n", __FUNCTION__, criticalPowerLimitFile.c_str(), getErrorCode(result));
                return getErrorCode(result);
            }
            anyLimitSet = true;
        }
    } else if (powerDomain == ZES_POWER_DOMAIN_PACKAGE) {
        // Package domain: Apply to PL1 and PL2 if enabled
        if (sustainedPowerLimitFileExists) {
            convertedVal = val;
            pSysmanKmdInterface->convertSysfsValueUnit(pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNamePackageSustainedPowerLimit), SysfsValueUnit::milli, convertedVal, convertedVal);
            result = pSysfsAccess->write(sustainedPowerLimitFile, convertedVal);
            if (ZE_RESULT_SUCCESS != result) {
                PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->write() failed to write into %s and returning error:0x%x \n", __FUNCTION__, sustainedPowerLimitFile.c_str(), getErrorCode(result));
                return getErrorCode(result);
            }
            anyLimitSet = true;
        }

        if (burstPowerLimitFileExists) {
            convertedVal = val;
            pSysmanKmdInterface->convertSysfsValueUnit(pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNamePackageBurstPowerLimit), SysfsValueUnit::milli, convertedVal, convertedVal);
            result = pSysfsAccess->write(burstPowerLimitFile, convertedVal);
            if (ZE_RESULT_SUCCESS != result) {
                PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->write() failed to write into %s and returning error:0x%x \n", __FUNCTION__, burstPowerLimitFile.c_str(), getErrorCode(result));
                return getErrorCode(result);
            }
            anyLimitSet = true;
        }
    } else {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Power Limits for given domain do not exist , returning error:0x%x\n", __FUNCTION__, ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }

    if (!anyLimitSet) {
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }

    return ZE_RESULT_SUCCESS;
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

static ze_result_t getMemoryBandwidthCounterValues(const std::map<std::string, uint64_t> &keyOffsetMap, std::unordered_map<std::string, std::string> &keyTelemInfoMap,
                                                   zes_mem_bandwidth_t *pBandwidth) {

    uint64_t readCounter = 0;
    uint64_t writeCounter = 0;
    uint64_t telemOffset = 0;

    for (uint32_t i = 0; i < memoryMsuCount; i++) {
        uint64_t msuReadCounter = 0;
        uint64_t msuWriteCounter = 0;

        for (uint32_t mb = 0; mb < memoryBridgeCount; mb++) {
            std::string mbSuffix = "MB" + std::to_string(mb);

            std::string readKey = "MEMSS" + std::to_string(i) + "_PERF_CTR_" + mbSuffix + "_CFI_NUM_READ_REQ";
            uint64_t readCounterValue = 0;
            if (!PlatformMonitoringTech::readValue(keyOffsetMap, keyTelemInfoMap[readKey], readKey, telemOffset, readCounterValue)) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
            msuReadCounter += readCounterValue;

            std::string writeKey = "MEMSS" + std::to_string(i) + "_PERF_CTR_" + mbSuffix + "_CFI_NUM_WRITE_REQ";
            uint64_t writeCounterValue = 0;
            if (!PlatformMonitoringTech::readValue(keyOffsetMap, keyTelemInfoMap[writeKey], writeKey, telemOffset, writeCounterValue)) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
            msuWriteCounter += writeCounterValue;
        }

        readCounter += msuReadCounter;
        writeCounter += msuWriteCounter;
    }

    pBandwidth->readCounter = readCounter * transactionSize;
    pBandwidth->writeCounter = writeCounter * transactionSize;
    return ZE_RESULT_SUCCESS;
}

static ze_result_t getMemoryMaxBandwidth(const std::map<std::string, uint64_t> &keyOffsetMap, std::unordered_map<std::string, std::string> &keyTelemInfoMap,
                                         zes_mem_bandwidth_t *pBandwidth) {

    uint64_t telemOffset = 0;
    uint32_t maxBandwidth = 0;
    std::string key = "VRAM_BANDWIDTH";
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, keyTelemInfoMap[key], key, telemOffset, maxBandwidth)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    maxBandwidth = maxBandwidth >> 16;
    pBandwidth->maxBandwidth = static_cast<uint64_t>(maxBandwidth) * mbpsToBytesPerSec;

    return ZE_RESULT_SUCCESS;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getMemoryBandwidth(zes_mem_bandwidth_t *pBandwidth, LinuxSysmanImp *pLinuxSysmanImp, uint32_t subdeviceId) {

    std::string &rootPath = pLinuxSysmanImp->getPciRootPath();
    std::map<uint32_t, std::string> telemNodes;
    NEO::PmtUtil::getTelemNodesInPciPath(std::string_view(rootPath), telemNodes);
    if (telemNodes.empty()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    std::map<std::string, uint64_t> keyOffsetMap;
    std::unordered_map<std::string, std::string> keyTelemInfoMap;

    for (const auto &it : telemNodes) {
        std::string telemNodeDir = it.second;

        std::array<char, NEO::PmtUtil::guidStringSize> guidString = {};
        if (!NEO::PmtUtil::readGuid(telemNodeDir, guidString)) {
            continue;
        }

        auto keyOffsetMapIterator = guidToKeyOffsetMap.find(guidString.data());
        if (keyOffsetMapIterator == guidToKeyOffsetMap.end()) {
            continue;
        }

        const auto &tempKeyOffsetMap = keyOffsetMapIterator->second;
        for (const auto &[key, value] : tempKeyOffsetMap) {
            keyOffsetMap[key] = value;
            keyTelemInfoMap[key] = telemNodeDir;
        }
    }

    if (keyOffsetMap.empty()) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to find KeyOffsetMap, returning error 0x%x>\n", __func__, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    if (ZE_RESULT_SUCCESS != getMemoryBandwidthCounterValues(keyOffsetMap, keyTelemInfoMap, pBandwidth)) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get the Read and Write Counter Values, returning error 0x%x>\n", __func__, ZE_RESULT_ERROR_NOT_AVAILABLE);
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    if (ZE_RESULT_SUCCESS != getMemoryMaxBandwidth(keyOffsetMap, keyTelemInfoMap, pBandwidth)) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get the Max Bandwidth Value, returning error 0x%x>\n", __func__, ZE_RESULT_ERROR_NOT_AVAILABLE);
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    pBandwidth->timestamp = SysmanDevice::getSysmanTimestamp();

    return ZE_RESULT_SUCCESS;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getMemoryProperties(zes_mem_properties_t *pProperties, LinuxSysmanImp *pLinuxSysmanImp, NEO::Drm *pDrm, SysmanKmdInterface *pSysmanKmdInterface, uint32_t subDeviceId, bool isSubdevice) {

    std::string &rootPath = pLinuxSysmanImp->getPciRootPath();
    std::map<uint32_t, std::string> telemNodes;
    NEO::PmtUtil::getTelemNodesInPciPath(std::string_view(rootPath), telemNodes);
    if (telemNodes.empty()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    std::map<std::string, uint64_t> keyOffsetMap;
    std::unordered_map<std::string, std::string> keyTelemInfoMap;

    for (const auto &it : telemNodes) {
        std::string telemNodeDir = it.second;

        std::array<char, NEO::PmtUtil::guidStringSize> guidString = {};
        if (!NEO::PmtUtil::readGuid(telemNodeDir, guidString)) {
            continue;
        }

        auto keyOffsetMapIterator = guidToKeyOffsetMap.find(guidString.data());
        if (keyOffsetMapIterator == guidToKeyOffsetMap.end()) {
            continue;
        }

        const auto &tempKeyOffsetMap = keyOffsetMapIterator->second;
        for (const auto &[key, value] : tempKeyOffsetMap) {
            keyOffsetMap[key] = value;
            keyTelemInfoMap[key] = telemNodeDir;
        }
    }

    if (keyOffsetMap.empty()) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to find KeyOffsetMap, returning error 0x%x>\n", __func__, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    uint32_t numOfChannelsPerMsu = 0;
    std::string numOfChannelsKey = "NUM_OF_MEM_CHANNEL";
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, keyTelemInfoMap[numOfChannelsKey], numOfChannelsKey, 0, numOfChannelsPerMsu)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    pProperties->location = ZES_MEM_LOC_DEVICE;
    pProperties->type = static_cast<zes_mem_type_t>(ZES_INTEL_MEM_TYPE_LPDDR5X);
    pProperties->onSubdevice = isSubdevice;
    pProperties->subdeviceId = subDeviceId;
    pProperties->numChannels = numOfChannelsPerMsu * memoryMsuCount;
    pProperties->busWidth = pProperties->numChannels * busWidthPerChannelInBytes;
    pProperties->physicalSize = 0;
    return ZE_RESULT_SUCCESS;
}

template class SysmanProductHelperHw<gfxProduct>;

} // namespace Sysman
} // namespace L0

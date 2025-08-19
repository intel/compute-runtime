/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.inl"

namespace L0 {
namespace Sysman {

constexpr static auto gfxProduct = IGFX_DG2;

template <>
RasInterfaceType SysmanProductHelperHw<gfxProduct>::getGtRasUtilInterface() {
    return RasInterfaceType::pmu;
}

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_xe_hp_and_later.inl"

static std::map<std::string, std::map<std::string, uint64_t>> guidToKeyOffsetMap = {
    {"0x4f95", // For DG2 device
     {{"PACKAGE_ENERGY", 1032},
      {"SOC_TEMPERATURES", 56}}}, // SOC_TEMPERATURE contains GT_TEMP, DRAM_TEMP, SA_TEMP, DE_TEMP, PCIE_TEMP, TYPEC_TEMP
    {"0x4f9301",                  // For ATSM device
     {{"PACKAGE_ENERGY", 1032},
      {"SOC_TEMPERATURES", 56}}}, // SOC_TEMPERATURE contains GT_TEMP, DRAM_TEMP, SA_TEMP, DE_TEMP, PCIE_TEMP, TYPEC_TEMP
    {"0x4f9302",                  // For DG2 512EU / ATS-M1
     {{"PACKAGE_ENERGY", 1032},
      {"SOC_TEMPERATURES", 56},
      {"MC_CAPTURE_TIMESTAMP", 1088},
      {"IDI_READS[0]", 1096},
      {"IDI_READS[1]", 1104},
      {"IDI_READS[2]", 1112},
      {"IDI_READS[3]", 1120},
      {"IDI_READS[4]", 1128},
      {"IDI_READS[5]", 1136},
      {"IDI_READS[6]", 1144},
      {"IDI_READS[7]", 1152},
      {"IDI_READS[8]", 1160},
      {"IDI_READS[9]", 1168},
      {"IDI_READS[10]", 1176},
      {"IDI_READS[11]", 1184},
      {"IDI_READS[12]", 1192},
      {"IDI_READS[13]", 1200},
      {"IDI_READS[14]", 1208},
      {"IDI_READS[15]", 1216},
      {"IDI_WRITES[0]", 1224},
      {"IDI_WRITES[1]", 1232},
      {"IDI_WRITES[2]", 1240},
      {"IDI_WRITES[3]", 1248},
      {"IDI_WRITES[4]", 1256},
      {"IDI_WRITES[5]", 1264},
      {"IDI_WRITES[6]", 1272},
      {"IDI_WRITES[7]", 1280},
      {"IDI_WRITES[8]", 1288},
      {"IDI_WRITES[9]", 1296},
      {"IDI_WRITES[10]", 1304},
      {"IDI_WRITES[11]", 1312},
      {"IDI_WRITES[12]", 1320},
      {"IDI_WRITES[13]", 1328},
      {"IDI_WRITES[14]", 1336},
      {"IDI_WRITES[15]", 1344},
      {"DISPLAY_VC1_READS[0]", 1352},
      {"DISPLAY_VC1_READS[1]", 1360},
      {"DISPLAY_VC1_READS[2]", 1368},
      {"DISPLAY_VC1_READS[3]", 1376},
      {"DISPLAY_VC1_READS[4]", 1384},
      {"DISPLAY_VC1_READS[5]", 1392},
      {"DISPLAY_VC1_READS[6]", 1400},
      {"DISPLAY_VC1_READS[7]", 1408},
      {"DISPLAY_VC1_READS[8]", 1416},
      {"DISPLAY_VC1_READS[9]", 1424},
      {"DISPLAY_VC1_READS[10]", 1432},
      {"DISPLAY_VC1_READS[11]", 1440},
      {"DISPLAY_VC1_READS[12]", 1448},
      {"DISPLAY_VC1_READS[13]", 1456},
      {"DISPLAY_VC1_READS[14]", 1464},
      {"DISPLAY_VC1_READS[15]", 1472}}},
    {"0x4f9502", // For DG2 128EU / ATS-M3
     {{"PACKAGE_ENERGY", 1032},
      {"SOC_TEMPERATURES", 56},
      {"MC_CAPTURE_TIMESTAMP", 1088},
      {"IDI_READS[0]", 1096},
      {"IDI_READS[1]", 1104},
      {"IDI_READS[2]", 1112},
      {"IDI_READS[3]", 1120},
      {"IDI_READS[4]", 1128},
      {"IDI_READS[5]", 1136},
      {"IDI_READS[6]", 1144},
      {"IDI_READS[7]", 1152},
      {"IDI_READS[8]", 1160},
      {"IDI_READS[9]", 1168},
      {"IDI_READS[10]", 1176},
      {"IDI_READS[11]", 1184},
      {"IDI_READS[12]", 1192},
      {"IDI_READS[13]", 1200},
      {"IDI_READS[14]", 1208},
      {"IDI_READS[15]", 1216},
      {"IDI_WRITES[0]", 1224},
      {"IDI_WRITES[1]", 1232},
      {"IDI_WRITES[2]", 1240},
      {"IDI_WRITES[3]", 1248},
      {"IDI_WRITES[4]", 1256},
      {"IDI_WRITES[5]", 1264},
      {"IDI_WRITES[6]", 1272},
      {"IDI_WRITES[7]", 1280},
      {"IDI_WRITES[8]", 1288},
      {"IDI_WRITES[9]", 1296},
      {"IDI_WRITES[10]", 1304},
      {"IDI_WRITES[11]", 1312},
      {"IDI_WRITES[12]", 1320},
      {"IDI_WRITES[13]", 1328},
      {"IDI_WRITES[14]", 1336},
      {"IDI_WRITES[15]", 1344},
      {"DISPLAY_VC1_READS[0]", 1352},
      {"DISPLAY_VC1_READS[1]", 1360},
      {"DISPLAY_VC1_READS[2]", 1368},
      {"DISPLAY_VC1_READS[3]", 1376},
      {"DISPLAY_VC1_READS[4]", 1384},
      {"DISPLAY_VC1_READS[5]", 1392},
      {"DISPLAY_VC1_READS[6]", 1400},
      {"DISPLAY_VC1_READS[7]", 1408},
      {"DISPLAY_VC1_READS[8]", 1416},
      {"DISPLAY_VC1_READS[9]", 1424},
      {"DISPLAY_VC1_READS[10]", 1432},
      {"DISPLAY_VC1_READS[11]", 1440},
      {"DISPLAY_VC1_READS[12]", 1448},
      {"DISPLAY_VC1_READS[13]", 1456},
      {"DISPLAY_VC1_READS[14]", 1464},
      {"DISPLAY_VC1_READS[15]", 1472}}}};

template <>
const std::map<std::string, std::map<std::string, uint64_t>> *SysmanProductHelperHw<gfxProduct>::getGuidToKeyOffsetMap() {
    return &guidToKeyOffsetMap;
}

ze_result_t readMcChannelCounters(std::map<std::string, uint64_t> keyOffsetMap, uint64_t &readCounters, uint64_t &writeCounters, std::string telemDir, uint64_t telemOffset) {
    uint32_t numMcChannels = 16u;
    std::vector<std::string> nameOfCounters{"IDI_READS", "IDI_WRITES", "DISPLAY_VC1_READS"};
    std::vector<uint64_t> counterValues(3, 0);
    for (uint64_t counterIndex = 0; counterIndex < nameOfCounters.size(); counterIndex++) {
        for (uint32_t mcChannelIndex = 0; mcChannelIndex < numMcChannels; mcChannelIndex++) {
            uint64_t val = 0;
            std::string readCounterKey = nameOfCounters[counterIndex] + "[" + std::to_string(mcChannelIndex) + "]";
            if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemDir, readCounterKey, telemOffset, val)) {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for readCounterKey returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_NOT_AVAILABLE);
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
            counterValues[counterIndex] += val;
        }
    }

    constexpr uint64_t transactionSize = 32;
    readCounters = (counterValues[0] + counterValues[2]) * transactionSize;
    writeCounters = (counterValues[1]) * transactionSize;
    return ZE_RESULT_SUCCESS;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getMemoryBandwidth(zes_mem_bandwidth_t *pBandwidth, LinuxSysmanImp *pLinuxSysmanImp, uint32_t subdeviceId) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    auto pSysFsAccess = pSysmanKmdInterface->getSysFsAccess();
    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    pBandwidth->readCounter = 0;
    pBandwidth->writeCounter = 0;
    pBandwidth->timestamp = 0;
    pBandwidth->maxBandwidth = 0;

    std::string telemDir = "";
    std::string guid = "";
    uint64_t telemOffset = 0;

    if (!pLinuxSysmanImp->getTelemData(subdeviceId, telemDir, guid, telemOffset)) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    std::map<std::string, uint64_t> keyOffsetMap;
    auto keyOffsetMapEntry = guidToKeyOffsetMap.find(guid);
    if (keyOffsetMapEntry == guidToKeyOffsetMap.end()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    keyOffsetMap = keyOffsetMapEntry->second;

    result = readMcChannelCounters(std::move(keyOffsetMap), pBandwidth->readCounter, pBandwidth->writeCounter, std::move(telemDir), telemOffset);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readMcChannelCounters returning error:0x%x  \n", __FUNCTION__, result);
        return result;
    }
    pBandwidth->maxBandwidth = 0u;
    const std::string maxBwFile = "prelim_lmem_max_bw_Mbps";
    uint64_t maxBw = 0;
    result = pSysFsAccess->read(maxBwFile, maxBw);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():Sysfsread for maxBw returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }
    pBandwidth->maxBandwidth = maxBw * mbpsToBytesPerSecond;
    pBandwidth->timestamp = SysmanDevice::getSysmanTimestamp();
    return result;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getPowerEnergyCounter(zes_power_energy_counter_t *pEnergy, LinuxSysmanImp *pLinuxSysmanImp, zes_power_domain_t powerDomain, uint32_t subdeviceId) {

    std::string telemDir = "";
    std::string guid = "";
    uint64_t telemOffset = 0;

    if (!pLinuxSysmanImp->getTelemData(subdeviceId, telemDir, guid, telemOffset)) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    std::map<std::string, uint64_t> keyOffsetMap;
    if (!PlatformMonitoringTech::getKeyOffsetMap(this, std::move(guid), keyOffsetMap)) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    const std::string key("PACKAGE_ENERGY");
    uint64_t energyCounter = 0;
    constexpr uint64_t fixedPointToJoule = 1048576;
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemDir, key, telemOffset, energyCounter)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    // PMT will return energy counter in Q20 format(fixed point representation) where first 20 bits(from LSB) represent decimal part
    // and remaining integral part which is converted into joule by division with 1048576(2^20) and then converted into microjoules
    pEnergy->energy = (energyCounter / fixedPointToJoule) * convertJouleToMicroJoule;
    pEnergy->timestamp = SysmanDevice::getSysmanTimestamp();

    return ZE_RESULT_SUCCESS;
}

template <>
bool SysmanProductHelperHw<gfxProduct>::isEccConfigurationSupported() {
    return true;
}

template <>
bool SysmanProductHelperHw<gfxProduct>::isUpstreamPortConnected() {
    return true;
}

template <>
bool SysmanProductHelperHw<gfxProduct>::isVfMemoryUtilizationSupported() {
    return true;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getVfLocalMemoryQuota(SysFsAccessInterface *pSysfsAccess, uint64_t &lMemQuota, const uint32_t &vfId) {

    const std::string pathForLmemQuota = "/gt/lmem_quota";
    std::string pathForDeviceMemQuota = "iov/vf" + std::to_string(vfId) + pathForLmemQuota;

    auto result = pSysfsAccess->read(std::move(pathForDeviceMemQuota), lMemQuota);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read Local Memory Quota with error 0x%x \n", __FUNCTION__, result);
        return result;
    }
    return ZE_RESULT_SUCCESS;
}

template class SysmanProductHelperHw<gfxProduct>;

} // namespace Sysman
} // namespace L0

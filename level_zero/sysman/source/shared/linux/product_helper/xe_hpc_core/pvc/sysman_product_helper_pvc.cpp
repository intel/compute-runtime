/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/pmt_util.h"

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.inl"
#include "level_zero/sysman/source/sysman_const.h"

#include <algorithm>
#include <vector>

namespace L0 {
namespace Sysman {
constexpr static auto gfxProduct = IGFX_PVC;
static const uint32_t numHbmModules = 4;

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_xe_hp_and_later.inl"

static std::map<std::string, std::map<std::string, uint64_t>> guidToKeyOffsetMap = {
    {"0xb15a0edc", // For PVC device
     {{"HBM0MaxDeviceTemperature", 28},
      {"HBM1MaxDeviceTemperature", 36},
      {"TileMinTemperature", 40},
      {"TileMaxTemperature", 44},
      {"GTMinTemperature", 48},
      {"GTMaxTemperature", 52},
      {"VF0_VFID", 88},
      {"VF0_HBM0_READ", 92},
      {"VF0_HBM0_WRITE", 96},
      {"VF0_HBM1_READ", 104},
      {"VF0_HBM1_WRITE", 108},
      {"VF0_TIMESTAMP_L", 168},
      {"VF0_TIMESTAMP_H", 172},
      {"VF1_VFID", 176},
      {"VF1_HBM0_READ", 180},
      {"VF1_HBM0_WRITE", 184},
      {"VF1_HBM1_READ", 192},
      {"VF1_HBM1_WRITE", 196},
      {"VF1_TIMESTAMP_L", 256},
      {"VF1_TIMESTAMP_H", 260},
      {"HBM2MaxDeviceTemperature", 300},
      {"HBM3MaxDeviceTemperature", 308},
      {"VF0_HBM2_READ", 312},
      {"VF0_HBM2_WRITE", 316},
      {"VF0_HBM3_READ", 328},
      {"VF0_HBM3_WRITE", 332},
      {"VF1_HBM2_READ", 344},
      {"VF1_HBM2_WRITE", 348},
      {"VF1_HBM3_READ", 360},
      {"VF1_HBM3_WRITE", 364}}},
    {"0xb15a0edd", // For PVC device
     {{"HBM0MaxDeviceTemperature", 28},
      {"HBM1MaxDeviceTemperature", 36},
      {"TileMinTemperature", 40},
      {"TileMaxTemperature", 44},
      {"GTMinTemperature", 48},
      {"GTMaxTemperature", 52},
      {"VF0_VFID", 88},
      {"VF0_HBM0_READ", 92},
      {"VF0_HBM0_WRITE", 96},
      {"VF0_HBM1_READ", 104},
      {"VF0_HBM1_WRITE", 108},
      {"VF0_TIMESTAMP_L", 168},
      {"VF0_TIMESTAMP_H", 172},
      {"VF1_VFID", 176},
      {"VF1_HBM0_READ", 180},
      {"VF1_HBM0_WRITE", 184},
      {"VF1_HBM1_READ", 192},
      {"VF1_HBM1_WRITE", 196},
      {"VF1_TIMESTAMP_L", 256},
      {"VF1_TIMESTAMP_H", 260},
      {"HBM2MaxDeviceTemperature", 300},
      {"HBM3MaxDeviceTemperature", 308},
      {"VF0_HBM2_READ", 312},
      {"VF0_HBM2_WRITE", 316},
      {"VF0_HBM3_READ", 328},
      {"VF0_HBM3_WRITE", 332},
      {"VF1_HBM2_READ", 344},
      {"VF1_HBM2_WRITE", 348},
      {"VF1_HBM3_READ", 360},
      {"VF1_HBM3_WRITE", 364}}},
    {"0xb15a0ede", // For PVC device
     {{"HBM0MaxDeviceTemperature", 28},
      {"HBM1MaxDeviceTemperature", 36},
      {"TileMinTemperature", 40},
      {"TileMaxTemperature", 44},
      {"GTMinTemperature", 48},
      {"GTMaxTemperature", 52},
      {"VF0_VFID", 88},
      {"VF0_HBM0_READ", 92},
      {"VF0_HBM0_WRITE", 96},
      {"VF0_HBM1_READ", 104},
      {"VF0_HBM1_WRITE", 108},
      {"VF0_TIMESTAMP_L", 168},
      {"VF0_TIMESTAMP_H", 172},
      {"VF1_VFID", 176},
      {"VF1_HBM0_READ", 180},
      {"VF1_HBM0_WRITE", 184},
      {"VF1_HBM1_READ", 192},
      {"VF1_HBM1_WRITE", 196},
      {"VF1_TIMESTAMP_L", 256},
      {"VF1_TIMESTAMP_H", 260},
      {"HBM2MaxDeviceTemperature", 300},
      {"HBM3MaxDeviceTemperature", 308},
      {"VF0_HBM2_READ", 312},
      {"VF0_HBM2_WRITE", 316},
      {"VF0_HBM3_READ", 328},
      {"VF0_HBM3_WRITE", 332},
      {"VF1_HBM2_READ", 344},
      {"VF1_HBM2_WRITE", 348},
      {"VF1_HBM3_READ", 360},
      {"VF1_HBM3_WRITE", 364},
      {"VF0_HBM_READ_L", 384},
      {"VF0_HBM_READ_H", 388},
      {"VF0_HBM_WRITE_L", 392},
      {"VF0_HBM_WRITE_H", 396},
      {"VF1_HBM_READ_L", 400},
      {"VF1_HBM_READ_H", 404},
      {"VF1_HBM_WRITE_L", 408},
      {"VF1_HBM_WRITE_H", 412}}},
    {"0x41fe79a5", // For PVC root device
     {{"PPIN", 152},
      {"BoardNumber", 72}}}};

template <>
const std::map<std::string, std::map<std::string, uint64_t>> *SysmanProductHelperHw<gfxProduct>::getGuidToKeyOffsetMap() {
    return &guidToKeyOffsetMap;
}

void getHBMFrequency(SysmanKmdInterface *pSysmanKmdInterface, SysFsAccessInterface *pSysFsAccess, uint64_t &hbmFrequency, uint32_t subdeviceId, unsigned short stepping) {
    hbmFrequency = 0;
    if (stepping >= REVISION_B) {
        const std::string hbmRP0FreqFile = pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMaxMemoryFrequency, subdeviceId, true);
        uint64_t hbmFreqValue = 0;
        ze_result_t result = pSysFsAccess->read(hbmRP0FreqFile, hbmFreqValue);
        if (ZE_RESULT_SUCCESS == result) {
            hbmFrequency = hbmFreqValue * 1000 * 1000;
            return;
        }
    } else if (stepping == REVISION_A0) {
        hbmFrequency = 3.2 * gigaUnitTransferToUnitTransfer;
    }
}

ze_result_t getVFIDString(std::map<std::string, uint64_t> keyOffsetMap, std::string &vfID, std::string telemDir, uint64_t telemOffset) {
    uint32_t vf0VfIdVal = 0;
    std::string key = "VF0_VFID";
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemDir, key, telemOffset, vf0VfIdVal)) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for VF0_VFID is returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_NOT_AVAILABLE);
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint32_t vf1VfIdVal = 0;
    key = "VF1_VFID";
    if (!PlatformMonitoringTech::readValue(std::move(keyOffsetMap), telemDir, key, telemOffset, vf1VfIdVal)) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for VF1_VFID is returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_NOT_AVAILABLE);
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    if (((vf0VfIdVal == 0) && (vf1VfIdVal == 0)) ||
        ((vf0VfIdVal > 0) && (vf1VfIdVal > 0))) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s() VF0 returning 0x%x and VF1 returning 0x%x as both should not be the same \n", __FUNCTION__, vf0VfIdVal, vf1VfIdVal);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (vf0VfIdVal > 0) {
        vfID = "VF0";
    }
    if (vf1VfIdVal > 0) {
        vfID = "VF1";
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t getHBMBandwidth(std::map<std::string, uint64_t> keyOffsetMap, zes_mem_bandwidth_t *pBandwidth, LinuxSysmanImp *pLinuxSysmanImp, std::string telemDir, uint64_t telemOffset, uint32_t subdeviceId, unsigned short stepping) {

    pBandwidth->readCounter = 0;
    pBandwidth->writeCounter = 0;
    pBandwidth->timestamp = 0;
    pBandwidth->maxBandwidth = 0;
    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    std::string vfId = "";
    result = getVFIDString(keyOffsetMap, vfId, telemDir, telemOffset);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():getVFIDString returning error:0x%x while retrieving VFID string \n", __FUNCTION__, result);
        return result;
    }

    for (auto hbmModuleIndex = 0u; hbmModuleIndex < numHbmModules; hbmModuleIndex++) {
        uint32_t counterValue = 0;
        std::string readCounterKey = vfId + "_HBM" + std::to_string(hbmModuleIndex) + "_READ";
        if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemDir, readCounterKey, telemOffset, counterValue)) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for readCounterKey returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_NOT_AVAILABLE);
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        pBandwidth->readCounter += counterValue;

        counterValue = 0;
        std::string writeCounterKey = vfId + "_HBM" + std::to_string(hbmModuleIndex) + "_WRITE";
        if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemDir, writeCounterKey, telemOffset, counterValue)) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for writeCounterKey returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_NOT_AVAILABLE);
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        pBandwidth->writeCounter += counterValue;
    }

    constexpr uint64_t transactionSize = 32;
    pBandwidth->readCounter = pBandwidth->readCounter * transactionSize;
    pBandwidth->writeCounter = pBandwidth->writeCounter * transactionSize;
    pBandwidth->timestamp = SysmanDevice::getSysmanTimestamp();

    uint64_t hbmFrequency = 0;
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    auto pSysFsAccess = pSysmanKmdInterface->getSysFsAccess();
    getHBMFrequency(pSysmanKmdInterface, pSysFsAccess, hbmFrequency, subdeviceId, stepping);
    pBandwidth->maxBandwidth = memoryBusWidth * hbmFrequency * numHbmModules;
    return ZE_RESULT_SUCCESS;
}

ze_result_t getHBMBandwidth(zes_mem_bandwidth_t *pBandwidth, LinuxSysmanImp *pLinuxSysmanImp, uint32_t subdeviceId) {

    auto pDevice = pLinuxSysmanImp->getSysmanDeviceImp();
    auto &hwInfo = pDevice->getHardwareInfo();
    auto &productHelper = pDevice->getRootDeviceEnvironment().getHelper<NEO::ProductHelper>();
    auto stepping = productHelper.getSteppingFromHwRevId(hwInfo);
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

    if (guid != guid64BitMemoryCounters) {
        return getHBMBandwidth(std::move(keyOffsetMap), pBandwidth, pLinuxSysmanImp, telemDir, telemOffset, subdeviceId, stepping);
    }

    pBandwidth->readCounter = 0;
    pBandwidth->writeCounter = 0;
    pBandwidth->timestamp = 0;
    pBandwidth->maxBandwidth = 0;

    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    std::string vfId = "";

    result = getVFIDString(keyOffsetMap, vfId, telemDir, telemOffset);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():getVFIDString returning error:0x%x while retrieving VFID string \n", __FUNCTION__, result);
        return result;
    }

    uint32_t readCounterL = 0;
    std::string readCounterKey = vfId + "_HBM_READ_L";
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemDir, readCounterKey, telemOffset, readCounterL)) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for readCounterL returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_NOT_AVAILABLE);
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint32_t readCounterH = 0;
    readCounterKey = vfId + "_HBM_READ_H";
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemDir, readCounterKey, telemOffset, readCounterH)) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for readCounterH returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_NOT_AVAILABLE);
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    constexpr uint64_t transactionSize = 32;
    pBandwidth->readCounter = readCounterH;
    pBandwidth->readCounter = (pBandwidth->readCounter << 32) | static_cast<uint64_t>(readCounterL);
    pBandwidth->readCounter = (pBandwidth->readCounter * transactionSize);

    uint32_t writeCounterL = 0;
    std::string writeCounterKey = vfId + "_HBM_WRITE_L";
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemDir, writeCounterKey, telemOffset, writeCounterL)) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for writeCounterL returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_NOT_AVAILABLE);
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint32_t writeCounterH = 0;
    writeCounterKey = vfId + "_HBM_WRITE_H";
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemDir, writeCounterKey, telemOffset, writeCounterH)) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for writeCounterH returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_NOT_AVAILABLE);
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    pBandwidth->writeCounter = writeCounterH;
    pBandwidth->writeCounter = (pBandwidth->writeCounter << 32) | static_cast<uint64_t>(writeCounterL);
    pBandwidth->writeCounter = (pBandwidth->writeCounter * transactionSize);
    pBandwidth->timestamp = SysmanDevice::getSysmanTimestamp();

    uint64_t hbmFrequency = 0;
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    auto pSysFsAccess = pSysmanKmdInterface->getSysFsAccess();
    getHBMFrequency(pSysmanKmdInterface, pSysFsAccess, hbmFrequency, subdeviceId, stepping);
    pBandwidth->maxBandwidth = memoryBusWidth * hbmFrequency * numHbmModules;
    return ZE_RESULT_SUCCESS;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getMemoryBandwidth(zes_mem_bandwidth_t *pBandwidth, LinuxSysmanImp *pLinuxSysmanImp, uint32_t subdeviceId) {
    return getHBMBandwidth(pBandwidth, pLinuxSysmanImp, subdeviceId);
}

template <>
void SysmanProductHelperHw<gfxProduct>::getMemoryHealthIndicator(FirmwareUtil *pFwInterface, zes_mem_health_t *health) {
    *health = ZES_MEM_HEALTH_UNKNOWN;
    if (pFwInterface != nullptr) {
        pFwInterface->fwGetMemoryHealthIndicator(health);
    }
}

template <>
void SysmanProductHelperHw<gfxProduct>::getMediaPerformanceFactorMultiplier(const double performanceFactor, double *pMultiplier) {
    if (performanceFactor > halfOfMaxPerformanceFactor) {
        *pMultiplier = 1;
    } else {
        *pMultiplier = 0.5;
    }
}

template <>
bool SysmanProductHelperHw<gfxProduct>::isMemoryMaxTemperatureSupported() {
    return true;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getGlobalMaxTemperature(LinuxSysmanImp *pLinuxSysmanImp, double *pTemperature, uint32_t subdeviceId) {

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

    uint32_t globalMaxTemperature = 0;
    std::string key("TileMaxTemperature");
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemDir, key, telemOffset, globalMaxTemperature)) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for TileMaxTemperature returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_NOT_AVAILABLE);
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    *pTemperature = static_cast<double>(globalMaxTemperature);
    return ZE_RESULT_SUCCESS;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getGpuMaxTemperature(LinuxSysmanImp *pLinuxSysmanImp, double *pTemperature, uint32_t subdeviceId) {

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

    uint32_t gpuMaxTemperature = 0;
    std::string key("GTMaxTemperature");
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemDir, key, telemOffset, gpuMaxTemperature)) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for GTMaxTemperature returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_NOT_AVAILABLE);
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    *pTemperature = static_cast<double>(gpuMaxTemperature);
    return ZE_RESULT_SUCCESS;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getMemoryMaxTemperature(LinuxSysmanImp *pLinuxSysmanImp, double *pTemperature, uint32_t subdeviceId) {

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

    uint32_t numHbmModules = 4u;
    std::vector<uint32_t> maxDeviceTemperatureList;
    for (auto hbmModuleIndex = 0u; hbmModuleIndex < numHbmModules; hbmModuleIndex++) {
        uint32_t maxDeviceTemperature = 0;
        std::string key = "HBM" + std::to_string(hbmModuleIndex) + "MaxDeviceTemperature";
        if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemDir, key, telemOffset, maxDeviceTemperature)) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for %s returning error:0x%x \n", __FUNCTION__, key.c_str(), ZE_RESULT_ERROR_NOT_AVAILABLE);
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        maxDeviceTemperatureList.push_back(maxDeviceTemperature);
    }
    *pTemperature = static_cast<double>(*std::max_element(maxDeviceTemperatureList.begin(), maxDeviceTemperatureList.end()));
    return ZE_RESULT_SUCCESS;
}

template <>
RasInterfaceType SysmanProductHelperHw<gfxProduct>::getGtRasUtilInterface() {
    return RasInterfaceType::pmu;
}

template <>
RasInterfaceType SysmanProductHelperHw<gfxProduct>::getHbmRasUtilInterface() {
    return RasInterfaceType::gsc;
}

template <>
bool SysmanProductHelperHw<gfxProduct>::isRepairStatusSupported() {
    return true;
}

template <>
int32_t SysmanProductHelperHw<gfxProduct>::getPowerLimitValue(uint64_t value) {
    return static_cast<int32_t>(value);
}

template <>
uint64_t SysmanProductHelperHw<gfxProduct>::setPowerLimitValue(int32_t value) {
    return static_cast<uint64_t>(value);
}

template <>
zes_limit_unit_t SysmanProductHelperHw<gfxProduct>::getPowerLimitUnit() {
    return ZES_LIMIT_UNIT_CURRENT;
}

template <>
std::string SysmanProductHelperHw<gfxProduct>::getPackageCriticalPowerLimitFile() {
    return "curr1_crit";
}

template <>
SysfsValueUnit SysmanProductHelperHw<gfxProduct>::getPackageCriticalPowerLimitNativeUnit() {
    return SysfsValueUnit::milli;
}

template <>
bool SysmanProductHelperHw<gfxProduct>::isUpstreamPortConnected() {
    return true;
}

template class SysmanProductHelperHw<gfxProduct>;

} // namespace Sysman
} // namespace L0

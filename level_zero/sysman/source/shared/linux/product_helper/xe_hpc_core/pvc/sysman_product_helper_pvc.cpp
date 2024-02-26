/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

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

ze_result_t getVFIDString(std::string &vfID, PlatformMonitoringTech *pPmt) {
    uint32_t vf0VfIdVal = 0;
    std::string key = "VF0_VFID";
    auto result = pPmt->readValue(key, vf0VfIdVal);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for VF0_VFID is returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }

    uint32_t vf1VfIdVal = 0;
    key = "VF1_VFID";
    result = pPmt->readValue(key, vf1VfIdVal);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for VF1_VFID is returning error:0x%x \n", __FUNCTION__, result);
        return result;
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
    return result;
}

ze_result_t getHBMBandwidth(zes_mem_bandwidth_t *pBandwidth, PlatformMonitoringTech *pPmt, SysmanKmdInterface *pSysmanKmdInterface, SysFsAccessInterface *pSysFsAccess, uint32_t subdeviceId, unsigned short stepping) {
    pBandwidth->readCounter = 0;
    pBandwidth->writeCounter = 0;
    pBandwidth->timestamp = 0;
    pBandwidth->maxBandwidth = 0;
    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    std::string vfId = "";
    result = getVFIDString(vfId, pPmt);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():getVFIDString returning error:0x%x while retriving VFID string \n", __FUNCTION__, result);
        return result;
    }

    for (auto hbmModuleIndex = 0u; hbmModuleIndex < numHbmModules; hbmModuleIndex++) {
        uint32_t counterValue = 0;
        std::string readCounterKey = vfId + "_HBM" + std::to_string(hbmModuleIndex) + "_READ";
        result = pPmt->readValue(readCounterKey, counterValue);
        if (result != ZE_RESULT_SUCCESS) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for readCounterKey returning error:0x%x \n", __FUNCTION__, result);
            return result;
        }
        pBandwidth->readCounter += counterValue;

        counterValue = 0;
        std::string writeCounterKey = vfId + "_HBM" + std::to_string(hbmModuleIndex) + "_WRITE";
        result = pPmt->readValue(writeCounterKey, counterValue);
        if (result != ZE_RESULT_SUCCESS) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for writeCounterKey returning error:0x%x \n", __FUNCTION__, result);
            return result;
        }
        pBandwidth->writeCounter += counterValue;
    }

    constexpr uint64_t transactionSize = 32;
    pBandwidth->readCounter = pBandwidth->readCounter * transactionSize;
    pBandwidth->writeCounter = pBandwidth->writeCounter * transactionSize;
    pBandwidth->timestamp = SysmanDevice::getSysmanTimestamp();

    uint64_t hbmFrequency = 0;
    getHBMFrequency(pSysmanKmdInterface, pSysFsAccess, hbmFrequency, subdeviceId, stepping);

    pBandwidth->maxBandwidth = memoryBusWidth * hbmFrequency * numHbmModules;
    return result;
}

ze_result_t getHBMBandwidth(zes_mem_bandwidth_t *pBandwidth, PlatformMonitoringTech *pPmt, SysmanDeviceImp *pDevice, SysmanKmdInterface *pSysmanKmdInterface, uint32_t subdeviceId) {
    auto pSysFsAccess = pSysmanKmdInterface->getSysFsAccess();
    auto &hwInfo = pDevice->getHardwareInfo();
    auto &productHelper = pDevice->getRootDeviceEnvironment().getHelper<NEO::ProductHelper>();
    auto stepping = productHelper.getSteppingFromHwRevId(hwInfo);

    std::string guid = pPmt->getGuid();
    if (guid != guid64BitMemoryCounters) {
        return getHBMBandwidth(pBandwidth, pPmt, pSysmanKmdInterface, pSysFsAccess, subdeviceId, stepping);
    }

    pBandwidth->readCounter = 0;
    pBandwidth->writeCounter = 0;
    pBandwidth->timestamp = 0;
    pBandwidth->maxBandwidth = 0;

    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    std::string vfId = "";

    result = getVFIDString(vfId, pPmt);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():getVFIDString returning error:0x%x while retriving VFID string \n", __FUNCTION__, result);
        return result;
    }

    uint32_t readCounterL = 0;
    std::string readCounterKey = vfId + "_HBM_READ_L";
    result = pPmt->readValue(readCounterKey, readCounterL);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for readCounterL returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }

    uint32_t readCounterH = 0;
    readCounterKey = vfId + "_HBM_READ_H";
    result = pPmt->readValue(readCounterKey, readCounterH);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for readCounterH returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }

    constexpr uint64_t transactionSize = 32;
    pBandwidth->readCounter = readCounterH;
    pBandwidth->readCounter = (pBandwidth->readCounter << 32) | static_cast<uint64_t>(readCounterL);
    pBandwidth->readCounter = (pBandwidth->readCounter * transactionSize);

    uint32_t writeCounterL = 0;
    std::string writeCounterKey = vfId + "_HBM_WRITE_L";
    result = pPmt->readValue(writeCounterKey, writeCounterL);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for writeCounterL returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }

    uint32_t writeCounterH = 0;
    writeCounterKey = vfId + "_HBM_WRITE_H";
    result = pPmt->readValue(writeCounterKey, writeCounterH);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for writeCounterH returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }

    pBandwidth->writeCounter = writeCounterH;
    pBandwidth->writeCounter = (pBandwidth->writeCounter << 32) | static_cast<uint64_t>(writeCounterL);
    pBandwidth->writeCounter = (pBandwidth->writeCounter * transactionSize);
    pBandwidth->timestamp = SysmanDevice::getSysmanTimestamp();

    uint64_t hbmFrequency = 0;
    getHBMFrequency(pSysmanKmdInterface, pSysFsAccess, hbmFrequency, subdeviceId, stepping);

    pBandwidth->maxBandwidth = memoryBusWidth * hbmFrequency * numHbmModules;
    return result;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getMemoryBandwidth(zes_mem_bandwidth_t *pBandwidth, PlatformMonitoringTech *pPmt, SysmanDeviceImp *pDevice, SysmanKmdInterface *pSysmanKmdInterface, uint32_t subdeviceId) {
    return getHBMBandwidth(pBandwidth, pPmt, pDevice, pSysmanKmdInterface, subdeviceId);
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
ze_result_t SysmanProductHelperHw<gfxProduct>::getGlobalMaxTemperature(PlatformMonitoringTech *pPmt, double *pTemperature) {
    uint32_t globalMaxTemperature = 0;
    std::string key("TileMaxTemperature");
    ze_result_t result = pPmt->readValue(key, globalMaxTemperature);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Pmt->readvalue() for TileMaxTemperature is returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }
    *pTemperature = static_cast<double>(globalMaxTemperature);
    return result;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getGpuMaxTemperature(PlatformMonitoringTech *pPmt, double *pTemperature) {
    uint32_t gpuMaxTemperature = 0;
    std::string key("GTMaxTemperature");
    ze_result_t result = pPmt->readValue(key, gpuMaxTemperature);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Pmt->readvalue() for GTMaxTemperature is returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }
    *pTemperature = static_cast<double>(gpuMaxTemperature);
    return result;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getMemoryMaxTemperature(PlatformMonitoringTech *pPmt, double *pTemperature) {
    uint32_t numHbmModules = 4u;
    ze_result_t result = ZE_RESULT_SUCCESS;
    std::vector<uint32_t> maxDeviceTemperatureList;
    for (auto hbmModuleIndex = 0u; hbmModuleIndex < numHbmModules; hbmModuleIndex++) {
        uint32_t maxDeviceTemperature = 0;
        // To read HBM 0's max device temperature key would be HBM0MaxDeviceTemperature
        std::string key = "HBM" + std::to_string(hbmModuleIndex) + "MaxDeviceTemperature";
        result = pPmt->readValue(key, maxDeviceTemperature);
        if (result != ZE_RESULT_SUCCESS) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Pmt->readvalue() for %s is returning error:0x%x \n", __FUNCTION__, key.c_str(), result);
            return result;
        }
        maxDeviceTemperatureList.push_back(maxDeviceTemperature);
    }

    *pTemperature = static_cast<double>(*std::max_element(maxDeviceTemperatureList.begin(), maxDeviceTemperatureList.end()));
    return result;
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
bool SysmanProductHelperHw<gfxProduct>::isDiagnosticsSupported() {
    return true;
}

template class SysmanProductHelperHw<gfxProduct>;

} // namespace Sysman
} // namespace L0

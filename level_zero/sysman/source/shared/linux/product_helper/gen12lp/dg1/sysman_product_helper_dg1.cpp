/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.inl"
#include "level_zero/sysman/source/sysman_const.h"

#include <algorithm>

namespace L0 {
namespace Sysman {
constexpr static auto gfxProduct = IGFX_DG1;

static std::map<std::string, std::map<std::string, uint64_t>> guidToKeyOffsetMap = {
    {"0x490e01", // DG1 B stepping
     {{"COMPUTE_TEMPERATURES", 0x68},
      {"SOC_TEMPERATURES", 0x60},
      {"CORE_TEMPERATURES", 0x6c}}},
    {"0x490e", // DG1 A stepping
     {{"COMPUTE_TEMPERATURES", 0x68},
      {"SOC_TEMPERATURES", 0x60},
      {"CORE_TEMPERATURES", 0x6c}}}};

template <>
const std::map<std::string, std::map<std::string, uint64_t>> *SysmanProductHelperHw<gfxProduct>::getGuidToKeyOffsetMap() {
    return &guidToKeyOffsetMap;
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

    uint32_t maxComputeTemperature = 0;
    uint32_t maxCoreTemperature = 0;
    std::string key;

    uint32_t computeTemperature = 0;
    key = "COMPUTE_TEMPERATURES";
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemDir, key, telemOffset, computeTemperature)) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): readValue for COMPUTE_TEMPERATURES returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_NOT_AVAILABLE);
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    // Check max temperature among IA, GT and LLC sensors across COMPUTE_TEMPERATURES
    maxComputeTemperature = getMaxTemperature(computeTemperature, numComputeTemperatureEntries);

    uint32_t coreTemperature = 0;
    key = "CORE_TEMPERATURES";
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemDir, key, telemOffset, coreTemperature)) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): readValue for CORE_TEMPERATURES returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_NOT_AVAILABLE);
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    // Check max temperature among CORE0, CORE1, CORE2, CORE3 sensors across CORE_TEMPERATURES
    maxCoreTemperature = getMaxTemperature(coreTemperature, numCoreTemperatureEntries);

    uint64_t socTemperature = 0;
    key = "SOC_TEMPERATURES";
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemDir, key, telemOffset, socTemperature)) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): readValue for SOC_TEMPERATURES returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_NOT_AVAILABLE);
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    // Check max temperature among possible sensors like PCH or GT_TEMP, DRAM, SA, PSF, DE, PCIE, TYPEC across SOC_TEMPERATURES
    uint32_t maxSocTemperature = getMaxTemperature(socTemperature, numSocTemperatureEntries);

    *pTemperature = static_cast<double>(std::max({maxComputeTemperature, maxCoreTemperature, maxSocTemperature}));

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

    double gpuMaxTemperature = 0;
    uint32_t computeTemperature = 0;
    std::string key("COMPUTE_TEMPERATURES");
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemDir, key, telemOffset, computeTemperature)) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): readValue for COMPUTE_TEMPERATURES returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_NOT_AVAILABLE);
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    computeTemperature = (computeTemperature >> 8) & 0xff;
    gpuMaxTemperature = static_cast<double>(computeTemperature);

    *pTemperature = gpuMaxTemperature;
    return ZE_RESULT_SUCCESS;
}

template <>
RasInterfaceType SysmanProductHelperHw<gfxProduct>::getGtRasUtilInterface() {
    return RasInterfaceType::pmu;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getMemoryProperties(zes_mem_properties_t *pProperties, LinuxSysmanImp *pLinuxSysmanImp, NEO::Drm *pDrm, SysmanKmdInterface *pSysmanKmdInterface, uint32_t subDeviceId, bool isSubdevice) {
    pProperties->location = ZES_MEM_LOC_DEVICE;
    pProperties->type = ZES_MEM_TYPE_DDR;
    pProperties->onSubdevice = isSubdevice;
    pProperties->subdeviceId = subDeviceId;
    pProperties->busWidth = -1;
    pProperties->numChannels = -1;
    pProperties->physicalSize = 0;
    return ZE_RESULT_SUCCESS;
}

template <>
bool SysmanProductHelperHw<gfxProduct>::isUpstreamPortConnected() {
    return true;
}

template class SysmanProductHelperHw<gfxProduct>;

} // namespace Sysman
} // namespace L0

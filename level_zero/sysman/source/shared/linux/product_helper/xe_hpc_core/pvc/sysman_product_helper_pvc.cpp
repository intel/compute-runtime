/*
 * Copyright (C) 2023 Intel Corporation
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
    return RasInterfaceType::PMU;
}

template <>
RasInterfaceType SysmanProductHelperHw<gfxProduct>::getHbmRasUtilInterface() {
    return RasInterfaceType::GSC;
}

template class SysmanProductHelperHw<gfxProduct>;

} // namespace Sysman
} // namespace L0

/*
 * Copyright (C) 2023-2024 Intel Corporation
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

ze_result_t readMcChannelCounters(PlatformMonitoringTech *pPmt, uint64_t &readCounters, uint64_t &writeCounters) {
    uint32_t numMcChannels = 16u;
    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    std::vector<std::string> nameOfCounters{"IDI_READS", "IDI_WRITES", "DISPLAY_VC1_READS"};
    std::vector<uint64_t> counterValues(3, 0);
    for (uint64_t counterIndex = 0; counterIndex < nameOfCounters.size(); counterIndex++) {
        for (uint32_t mcChannelIndex = 0; mcChannelIndex < numMcChannels; mcChannelIndex++) {
            uint64_t val = 0;
            std::string readCounterKey = nameOfCounters[counterIndex] + "[" + std::to_string(mcChannelIndex) + "]";
            result = pPmt->readValue(readCounterKey, val);
            if (result != ZE_RESULT_SUCCESS) {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s():readValue for readCounterKey returning error:0x%x \n", __FUNCTION__, result);
                return result;
            }
            counterValues[counterIndex] += val;
        }
    }

    constexpr uint64_t transactionSize = 32;
    readCounters = (counterValues[0] + counterValues[2]) * transactionSize;
    writeCounters = (counterValues[1]) * transactionSize;
    return result;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getMemoryBandwidth(zes_mem_bandwidth_t *pBandwidth, PlatformMonitoringTech *pPmt, SysmanDeviceImp *pDevice, SysmanKmdInterface *pSysmanKmdInterface, uint32_t subdeviceId) {
    auto pSysFsAccess = pSysmanKmdInterface->getSysFsAccess();
    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    pBandwidth->readCounter = 0;
    pBandwidth->writeCounter = 0;
    pBandwidth->timestamp = 0;
    pBandwidth->maxBandwidth = 0;
    result = readMcChannelCounters(pPmt, pBandwidth->readCounter, pBandwidth->writeCounter);
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
bool SysmanProductHelperHw<gfxProduct>::isEccConfigurationSupported() {
    return true;
}

template class SysmanProductHelperHw<gfxProduct>;

} // namespace Sysman
} // namespace L0

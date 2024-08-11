/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/pmt_util.h"

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.inl"

namespace L0 {
namespace Sysman {
constexpr static auto gfxProduct = IGFX_BMG;

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_xe_hp_and_later.inl"

static std::map<std::string, std::map<std::string, uint64_t>> guidToKeyOffsetMap = {
    {"0x5e2F8210", // BMG OOBMSM Rev 15
     {{"SOC_THERMAL_SENSORS_TEMPERATURE_0_2_0_GTTMMADR[1]", 42},
      {"VRAM_TEMPERATURE_0_2_0_GTTMMADR", 43},
      {"reg_PCIESS_rx_bytecount_lsb", 70},
      {"reg_PCIESS_rx_bytecount_msb", 69},
      {"reg_PCIESS_tx_bytecount_lsb", 72},
      {"reg_PCIESS_tx_bytecount_msb", 71},
      {"reg_PCIESS_rx_pktcount_lsb", 74},
      {"reg_PCIESS_rx_pktcount_msb", 73},
      {"reg_PCIESS_tx_pktcount_lsb", 76},
      {"reg_PCIESS_tx_pktcount_msb", 75}}}};

template <>
const std::map<std::string, std::map<std::string, uint64_t>> *SysmanProductHelperHw<gfxProduct>::getGuidToKeyOffsetMap() {
    return &guidToKeyOffsetMap;
}

template <>
RasInterfaceType SysmanProductHelperHw<gfxProduct>::getGtRasUtilInterface() {
    return RasInterfaceType::netlink;
}

template <>
RasInterfaceType SysmanProductHelperHw<gfxProduct>::getHbmRasUtilInterface() {
    return RasInterfaceType::netlink;
}

template <>
bool SysmanProductHelperHw<gfxProduct>::isUpstreamPortConnected() {
    return true;
}

static ze_result_t getPciStatsValues(zes_pci_stats_t *pStats, std::map<std::string, uint64_t> &keyOffsetMap, const std::string &telemNodeDir, uint64_t telemOffset) {
    uint32_t rxCounterLsb = 0;
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemNodeDir, "reg_PCIESS_rx_bytecount_lsb", telemOffset, rxCounterLsb)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint32_t rxCounterMsb = 0;
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemNodeDir, "reg_PCIESS_rx_bytecount_msb", telemOffset, rxCounterMsb)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint64_t rxCounter = PACK_INTO_64BIT(rxCounterMsb, rxCounterLsb);

    uint32_t txCounterLsb = 0;
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemNodeDir, "reg_PCIESS_tx_bytecount_lsb", telemOffset, txCounterLsb)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint32_t txCounterMsb = 0;
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemNodeDir, "reg_PCIESS_tx_bytecount_msb", telemOffset, txCounterMsb)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint64_t txCounter = PACK_INTO_64BIT(txCounterMsb, txCounterLsb);

    uint32_t rxPacketCounterLsb = 0;
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemNodeDir, "reg_PCIESS_rx_pktcount_lsb", telemOffset, rxPacketCounterLsb)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint32_t rxPacketCounterMsb = 0;
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemNodeDir, "reg_PCIESS_rx_pktcount_msb", telemOffset, rxPacketCounterMsb)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint64_t rxPacketCounter = PACK_INTO_64BIT(rxPacketCounterMsb, rxPacketCounterLsb);

    uint32_t txPacketCounterLsb = 0;
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemNodeDir, "reg_PCIESS_tx_pktcount_lsb", telemOffset, txPacketCounterLsb)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint32_t txPacketCounterMsb = 0;
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemNodeDir, "reg_PCIESS_tx_pktcount_msb", telemOffset, txPacketCounterMsb)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint64_t txPacketCounter = PACK_INTO_64BIT(txPacketCounterMsb, txPacketCounterLsb);

    pStats->speed.gen = -1;
    pStats->speed.width = -1;
    pStats->speed.maxBandwidth = -1;
    pStats->replayCounter = 0;
    pStats->rxCounter = rxCounter;
    pStats->txCounter = txCounter;
    pStats->packetCounter = rxPacketCounter + txPacketCounter;
    pStats->timestamp = SysmanDevice::getSysmanTimestamp();

    return ZE_RESULT_SUCCESS;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getPciStats(zes_pci_stats_t *pStats, LinuxSysmanImp *pLinuxSysmanImp) {
    std::string &rootPath = pLinuxSysmanImp->getPciRootPath();
    std::map<uint32_t, std::string> telemNodes;
    NEO::PmtUtil::getTelemNodesInPciPath(std::string_view(rootPath), telemNodes);
    if (telemNodes.empty()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    ze_result_t result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    for (auto it : telemNodes) {
        std::string telemNodeDir = it.second;

        std::array<char, NEO::PmtUtil::guidStringSize> guidString = {};
        if (!NEO::PmtUtil::readGuid(telemNodeDir, guidString)) {
            continue;
        }

        auto keyOffsetMapIterator = guidToKeyOffsetMap.find(guidString.data());
        if (keyOffsetMapIterator == guidToKeyOffsetMap.end()) {
            continue;
        }

        uint64_t telemOffset = 0;
        if (!NEO::PmtUtil::readOffset(telemNodeDir, telemOffset)) {
            break;
        }

        result = getPciStatsValues(pStats, keyOffsetMapIterator->second, telemNodeDir, telemOffset);
        if (result == ZE_RESULT_SUCCESS) {
            break;
        }
    }

    return result;
};

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
    std::string key("SOC_THERMAL_SENSORS_TEMPERATURE_0_2_0_GTTMMADR[1]");
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemDir, key, telemOffset, gpuMaxTemperature)) {
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

    uint32_t memoryMaxTemperature = 0;
    std::string key("VRAM_TEMPERATURE_0_2_0_GTTMMADR");
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemDir, key, telemOffset, memoryMaxTemperature)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    memoryMaxTemperature &= 0xFFu; // Extract least significant 8 bits
    *pTemperature = static_cast<double>(memoryMaxTemperature);
    return ZE_RESULT_SUCCESS;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getGlobalMaxTemperature(LinuxSysmanImp *pLinuxSysmanImp, double *pTemperature, uint32_t subdeviceId) {
    double gpuMaxTemperature = 0;
    ze_result_t result = this->getGpuMaxTemperature(pLinuxSysmanImp, &gpuMaxTemperature, subdeviceId);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    double memoryMaxTemperature = 0;
    result = this->getMemoryMaxTemperature(pLinuxSysmanImp, &memoryMaxTemperature, subdeviceId);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    *pTemperature = std::max(gpuMaxTemperature, memoryMaxTemperature);
    return result;
}

template class SysmanProductHelperHw<gfxProduct>;

} // namespace Sysman
} // namespace L0

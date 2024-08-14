/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/pmt_util.h"

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

namespace L0 {
namespace Sysman {

static const std::string baseTelemSysfs("/sys/class/intel_pmt");

bool PlatformMonitoringTech::getKeyOffsetMap(SysmanProductHelper *pSysmanProductHelper, std::string guid, std::map<std::string, uint64_t> &keyOffsetMap) {
    auto pGuidToKeyOffsetMap = pSysmanProductHelper->getGuidToKeyOffsetMap();
    if (pGuidToKeyOffsetMap == nullptr) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get the pGuidToKeyOffsetMap \n", __FUNCTION__);
        return false;
    }
    auto keyOffsetMapEntry = pGuidToKeyOffsetMap->find(guid);
    if (keyOffsetMapEntry == pGuidToKeyOffsetMap->end()) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to find keyOffset in keyOffsetMap \n", __FUNCTION__);
        return false;
    }
    keyOffsetMap = keyOffsetMapEntry->second;
    return true;
}

bool PlatformMonitoringTech::getTelemOffsetAndTelemDir(LinuxSysmanImp *pLinuxSysmanImp, uint64_t &telemOffset, std::string &telemDir) {
    std::string &rootPath = pLinuxSysmanImp->getPciRootPath();

    std::map<uint32_t, std::string> telemPciPath;
    NEO::PmtUtil::getTelemNodesInPciPath(std::string_view(rootPath), telemPciPath);
    uint32_t subDeviceCount = pLinuxSysmanImp->getSubDeviceCount() + 1;
    if (telemPciPath.size() < subDeviceCount) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Number of telemetry nodes:%d is lessthan %d \n", __FUNCTION__, telemPciPath.size(), subDeviceCount);
        return false;
    }

    auto iterator = telemPciPath.begin();
    telemDir = iterator->second;

    if (!NEO::PmtUtil::readOffset(telemDir, telemOffset)) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read offset from %s\n", __FUNCTION__, telemDir.c_str());
        return false;
    }
    return true;
}

bool PlatformMonitoringTech::getTelemOffsetForContainer(SysmanProductHelper *pSysmanProductHelper, const std::string &telemDir, const std::string &key, uint64_t &telemOffset) {
    std::array<char, NEO::PmtUtil::guidStringSize> guidString = {};
    if (!NEO::PmtUtil::readGuid(telemDir, guidString)) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read GUID from %s \n", __FUNCTION__, telemDir.c_str());
        return false;
    }

    std::map<std::string, uint64_t> keyOffsetMap;
    if (!PlatformMonitoringTech::getKeyOffsetMap(pSysmanProductHelper, guidString.data(), keyOffsetMap)) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get KeyOffsetMap for Guid : %s\n", __FUNCTION__, guidString.data());
        return false;
    }

    auto keyOffset = keyOffsetMap.find(key.c_str());
    if (keyOffset == keyOffsetMap.end()) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to find keyOffset in keyOffsetMap \n", __FUNCTION__);
        return false;
    }

    telemOffset = keyOffset->second;
    return true;
}

bool PlatformMonitoringTech::readValue(const std::map<std::string, uint64_t> keyOffsetMap, const std::string &telemDir, const std::string &key, const uint64_t &telemOffset, uint32_t &value) {

    auto containerOffset = keyOffsetMap.find(key);
    if (containerOffset == keyOffsetMap.end()) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to find keyOffset in keyOffsetMap \n", __FUNCTION__);
        return false;
    }

    uint64_t offset = telemOffset + containerOffset->second;
    ssize_t bytesRead = NEO::PmtUtil::readTelem(telemDir.data(), sizeof(uint32_t), offset, &value);
    if (bytesRead != sizeof(uint32_t)) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read value for %s key \n", __FUNCTION__, key.c_str());
        return false;
    }
    return true;
}

bool PlatformMonitoringTech::readValue(const std::map<std::string, uint64_t> keyOffsetMap, const std::string &telemDir, const std::string &key, const uint64_t &telemOffset, uint64_t &value) {

    auto containerOffset = keyOffsetMap.find(key);
    if (containerOffset == keyOffsetMap.end()) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to find keyOffset in keyOffsetMap \n", __FUNCTION__);
        return false;
    }

    uint64_t offset = telemOffset + containerOffset->second;
    ssize_t bytesRead = NEO::PmtUtil::readTelem(telemDir.data(), sizeof(uint64_t), offset, &value);
    if (bytesRead != sizeof(uint64_t)) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read value for %s key \n", __FUNCTION__, key.c_str());
        return false;
    }
    return true;
}

bool PlatformMonitoringTech::getTelemDataForTileAggregator(const std::map<uint32_t, std::string> telemNodesInPciPath, uint32_t subdeviceId, std::string &telemDir, std::string &guid, uint64_t &telemOffset) {

    uint32_t rootDeviceTelemIndex = telemNodesInPciPath.begin()->first;
    const std::string telemNode = "telem" + std::to_string(rootDeviceTelemIndex + subdeviceId + 1);
    telemDir = baseTelemSysfs + "/" + telemNode;
    if (!NEO::PmtUtil::readOffset(telemDir, telemOffset)) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read offset from %s\n", __FUNCTION__, telemDir.c_str());
        return false;
    }

    std::array<char, NEO::PmtUtil::guidStringSize> guidString = {};
    if (!NEO::PmtUtil::readGuid(telemDir, guidString)) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read GUID from %s \n", __FUNCTION__, telemDir.c_str());
        return false;
    }

    guid = guidString.data();
    return true;
}

bool PlatformMonitoringTech::getTelemData(const std::map<uint32_t, std::string> telemNodesInPciPath, std::string &telemDir, std::string &guid, uint64_t &telemOffset) {
    auto iterator = telemNodesInPciPath.begin();
    telemDir = iterator->second;
    if (!NEO::PmtUtil::readOffset(telemDir, telemOffset)) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read offset from %s\n", __FUNCTION__, telemDir.c_str());
        return false;
    }

    std::array<char, NEO::PmtUtil::guidStringSize> guidString = {};
    if (!NEO::PmtUtil::readGuid(telemDir, guidString)) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read GUID from %s \n", __FUNCTION__, telemDir.c_str());
        return false;
    }

    guid = guidString.data();

    return true;
}

bool PlatformMonitoringTech::isTelemetrySupportAvailable(LinuxSysmanImp *pLinuxSysmanImp, uint32_t subdeviceId) {

    std::string telemDir = "";
    std::string guid = "";
    uint64_t offset = 0;
    if (!pLinuxSysmanImp->getTelemData(subdeviceId, telemDir, guid, offset)) {
        return false;
    }

    std::map<std::string, uint64_t> keyOffsetMap;
    auto pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
    if (!PlatformMonitoringTech::getKeyOffsetMap(pSysmanProductHelper, guid, keyOffsetMap)) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to get KeyOffsetMap for Guid : %s\n", __FUNCTION__, guid.c_str());
        return false;
    }

    std::string telemetryDeviceEntry = telemDir + "/telem";
    auto pFsAccess = &pLinuxSysmanImp->getFsAccess();
    if (!pFsAccess->fileExists(telemetryDeviceEntry)) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Telemetry support not available. No file %s\n", telemetryDeviceEntry.c_str());
        return false;
    }

    return true;
}

} // namespace Sysman
} // namespace L0

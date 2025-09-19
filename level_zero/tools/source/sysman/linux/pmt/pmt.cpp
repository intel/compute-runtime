/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/linux/pmt/pmt.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/linux/file_descriptor.h"

#include "level_zero/tools/source/sysman/sysman_imp.h"

#include <algorithm>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

namespace L0 {
const std::string PlatformMonitoringTech::baseTelemSysFS("/sys/class/intel_pmt");
const std::string PlatformMonitoringTech::telem("telem");
uint32_t PlatformMonitoringTech::rootDeviceTelemNodeIndex = 0;

std::string PlatformMonitoringTech::getGuid() {
    return guid;
}

ze_result_t PlatformMonitoringTech::readValue(const std::string key, uint32_t &value) {
    auto offset = keyOffsetMap.find(key);
    if (offset == keyOffsetMap.end()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    auto fd = NEO::FileDescriptor(telemetryDeviceEntry.c_str(), O_RDONLY);
    if (fd == -1) {
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }

    ze_result_t res = ZE_RESULT_SUCCESS;
    if (this->preadFunction(fd, &value, sizeof(uint32_t), baseOffset + offset->second) != sizeof(uint32_t)) {
        res = ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }
    return res;
}

ze_result_t PlatformMonitoringTech::readValue(const std::string key, uint64_t &value) {
    auto offset = keyOffsetMap.find(key);
    if (offset == keyOffsetMap.end()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    auto fd = NEO::FileDescriptor(telemetryDeviceEntry.c_str(), O_RDONLY);
    if (fd == -1) {
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }

    ze_result_t res = ZE_RESULT_SUCCESS;
    if (this->preadFunction(fd, &value, sizeof(uint64_t), baseOffset + offset->second) != sizeof(uint64_t)) {
        res = ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }

    return res;
}

bool compareTelemNodes(std::string &telemNode1, std::string &telemNode2) {
    std::string telem = "telem";
    auto indexString1 = telemNode1.substr(telem.size(), telemNode1.size());
    auto indexForTelemNode1 = stoi(indexString1);
    auto indexString2 = telemNode2.substr(telem.size(), telemNode2.size());
    auto indexForTelemNode2 = stoi(indexString2);
    return indexForTelemNode1 < indexForTelemNode2;
}

// Check if Telemetry node(say /sys/class/intel_pmt/telem1) and gpuUpstreamPortPath share same PCI Root port
static bool isValidTelemNode(FsAccess *pFsAccess, const std::string &gpuUpstreamPortPath, const std::string sysfsTelemNode) {
    std::string realPathOfTelemNode;
    auto result = pFsAccess->getRealPath(sysfsTelemNode, realPathOfTelemNode);
    if (result != ZE_RESULT_SUCCESS) {
        return false;
    }

    // Example: If
    // gpuUpstreamPortPath = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0";
    // realPathOfTelemNode = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem1";
    // As gpuUpstreamPortPath is a substring of realPathOfTelemNode , hence both sysfs telemNode and GPU device share same PCI Root.
    // the PMT is part of the OOBMSM sitting on a switch port 0000:8b:02.0 attached to the upstream port/ Also known as CardBus
    // Hence this telem node entry is valid for GPU device.
    return (realPathOfTelemNode.compare(0, gpuUpstreamPortPath.size(), gpuUpstreamPortPath) == 0);
}

ze_result_t PlatformMonitoringTech::enumerateRootTelemIndex(FsAccess *pFsAccess, std::string &gpuUpstreamPortPath) {
    std::vector<std::string> listOfTelemNodes;
    auto result = pFsAccess->listDirectory(baseTelemSysFS, listOfTelemNodes);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    // listOfTelemNodes vector could contain non "telem" entries which are not interested to us.
    // Lets refactor listOfTelemNodes vector as below
    for (auto iterator = listOfTelemNodes.begin(); iterator != listOfTelemNodes.end();) {
        if (iterator->compare(0, telem.size(), telem) != 0) {
            iterator = listOfTelemNodes.erase(iterator); // Remove entry if its suffix is not "telem"
        } else {
            iterator++;
        }
    }

    // Example: For below directory
    // # /sys/class/intel_pmt$ ls
    // telem1  telem2  telem3
    // Then listOfTelemNodes would contain telem1, telem2, telem3
    std::sort(listOfTelemNodes.begin(), listOfTelemNodes.end(), compareTelemNodes); // sort listOfTelemNodes, to arange telem nodes in ascending order
    for (const auto &telemNode : listOfTelemNodes) {
        if (isValidTelemNode(pFsAccess, gpuUpstreamPortPath, baseTelemSysFS + "/" + telemNode)) {
            auto indexString = telemNode.substr(telem.size(), telemNode.size());
            rootDeviceTelemNodeIndex = stoi(indexString); // if telemNode is telemN, then rootDeviceTelemNodeIndex = N
            return ZE_RESULT_SUCCESS;
        }
    }
    return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
}

ze_result_t PlatformMonitoringTech::init(FsAccess *pFsAccess, const std::string &gpuUpstreamPortPath, PRODUCT_FAMILY productFamily) {
    std::string telemNode = telem + std::to_string(rootDeviceTelemNodeIndex);
    // For XE_HP_SDV and PVC single tile devices, telemetry info is retrieved from
    // tile's telem node rather from root device telem node.
    if ((isSubdevice) || (productFamily == IGFX_PVC)) {
        uint32_t telemNodeIndex = 0;
        // If rootDeviceTelemNode is telem1, then rootDeviceTelemNodeIndex = 1
        // And thus for subdevice0 --> telem node will be telem2,
        // for subdevice1 --> telem node will be telem3 etc
        telemNodeIndex = rootDeviceTelemNodeIndex + subdeviceId + 1;
        telemNode = telem + std::to_string(telemNodeIndex);
    }
    std::string baseTelemSysFSNode = baseTelemSysFS + "/" + telemNode;
    if (!isValidTelemNode(pFsAccess, gpuUpstreamPortPath, baseTelemSysFSNode)) {
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }

    telemetryDeviceEntry = baseTelemSysFSNode + "/" + telem;
    if (!pFsAccess->fileExists(telemetryDeviceEntry)) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "Telemetry support not available. No file %s\n", telemetryDeviceEntry.c_str());
        return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
    }

    std::string guid;
    std::string guidPath = baseTelemSysFSNode + std::string("/guid");
    ze_result_t result = pFsAccess->read(guidPath, guid);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "Telemetry sysfs entry not available %s\n", guidPath.c_str());
        return result;
    }
    result = PlatformMonitoringTech::getKeyOffsetMap(guid, keyOffsetMap);
    if (ZE_RESULT_SUCCESS != result) {
        // We did not have any entry for this guid in guidToKeyOffsetMap
        return result;
    }

    std::string offsetPath = baseTelemSysFSNode + std::string("/offset");
    result = pFsAccess->read(offsetPath, baseOffset);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "Telemetry sysfs entry not available %s\n", offsetPath.c_str());
        return result;
    }

    return ZE_RESULT_SUCCESS;
}

PlatformMonitoringTech::PlatformMonitoringTech(FsAccess *pFsAccess, ze_bool_t onSubdevice,
                                               uint32_t subdeviceId) : subdeviceId(subdeviceId), isSubdevice(onSubdevice) {
}

void PlatformMonitoringTech::doInitPmtObject(FsAccess *pFsAccess, uint32_t subdeviceId, PlatformMonitoringTech *pPmt,
                                             const std::string &gpuUpstreamPortPath,
                                             std::map<uint32_t, L0::PlatformMonitoringTech *> &mapOfSubDeviceIdToPmtObject, PRODUCT_FAMILY productFamily) {
    if (pPmt->init(pFsAccess, gpuUpstreamPortPath, productFamily) == ZE_RESULT_SUCCESS) {
        mapOfSubDeviceIdToPmtObject.emplace(subdeviceId, pPmt);
        return;
    }
    delete pPmt; // We are here as pPmt->init failed and thus this pPmt object is not useful. Let's delete that.
}

void PlatformMonitoringTech::create(const std::vector<ze_device_handle_t> &deviceHandles,
                                    FsAccess *pFsAccess, std::string &gpuUpstreamPortPath,
                                    std::map<uint32_t, L0::PlatformMonitoringTech *> &mapOfSubDeviceIdToPmtObject) {
    if (ZE_RESULT_SUCCESS == PlatformMonitoringTech::enumerateRootTelemIndex(pFsAccess, gpuUpstreamPortPath)) {
        for (const auto &deviceHandle : deviceHandles) {
            uint32_t subdeviceId = 0;
            ze_bool_t onSubdevice = false;
            SysmanDeviceImp::getSysmanDeviceInfo(deviceHandle, subdeviceId, onSubdevice, true);
            auto productFamily = SysmanDeviceImp::getProductFamily(Device::fromHandle(deviceHandle));
            auto pPmt = new PlatformMonitoringTech(pFsAccess, onSubdevice, subdeviceId);
            UNRECOVERABLE_IF(nullptr == pPmt);
            PlatformMonitoringTech::doInitPmtObject(pFsAccess, subdeviceId, pPmt,
                                                    gpuUpstreamPortPath, mapOfSubDeviceIdToPmtObject, productFamily);
        }
    }
}

PlatformMonitoringTech::~PlatformMonitoringTech() {
}

} // namespace L0

/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/linux/pmt/pmt.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

#include <algorithm>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>

namespace L0 {
const std::string PlatformMonitoringTech::baseTelemSysFS("/sys/class/intel_pmt");
const std::string PlatformMonitoringTech::telem("telem");
uint32_t PlatformMonitoringTech::rootDeviceTelemNodeIndex = 0;

ze_result_t PlatformMonitoringTech::enumerateRootTelemIndex(FsAccess *pFsAccess, std::string &rootPciPathOfGpuDevice) {
    std::vector<std::string> listOfTelemNodes;
    auto result = pFsAccess->listDirectory(baseTelemSysFS, listOfTelemNodes);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    // Exmaple: For below directory
    // # /sys/class/intel_pmt$ ls
    // telem1  telem2  telem3
    // Then listOfTelemNodes would contain telem1, telem2, telem3
    std::sort(listOfTelemNodes.begin(), listOfTelemNodes.end()); // sort listOfTelemNodes, to arange telem nodes in ascending order
    for (const auto &telemNode : listOfTelemNodes) {
        std::string realPathOfTelemNode;
        result = pFsAccess->getRealPath(baseTelemSysFS + "/" + telemNode, realPathOfTelemNode);
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }
        // Check if Telemetry node(say telem1) and rootPciPathOfGpuDevice share same PCI Root port
        if (realPathOfTelemNode.compare(0, rootPciPathOfGpuDevice.size(), rootPciPathOfGpuDevice) == 0) {
            // Example: If
            // rootPciPathOfGpuDevice = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0";
            // realPathOfTelemNode = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem1";
            // Thus As realPathOfTelemNode consists of rootPciPathOfGpuDevice, hence both telemNode and GPU device share same PCI Root.
            auto indexString = telemNode.substr(telem.size(), telemNode.size());
            rootDeviceTelemNodeIndex = stoi(indexString); // if telemNode is telemN, then rootDeviceTelemNodeIndex = N
            return ZE_RESULT_SUCCESS;
        }
    }
    return result;
}

void PlatformMonitoringTech::init(FsAccess *pFsAccess) {
    auto getErrorVal = [](auto err) {
        if ((EPERM == err) || (EACCES == err)) {
            return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
        } else if (ENOENT == err) {
            return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
        } else {
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
    };

    std::string telemNode = telem + std::to_string(rootDeviceTelemNodeIndex);
    if (isSubdevice) {
        uint32_t telemNodeIndex = 0;
        // If rootDeviceTelemNode is telem1, then rootDeviceTelemNodeIndex = 1
        // And thus for subdevice0 --> telem node will be telem2,
        // for subdevice1 --> telem node will be telem3 etc
        telemNodeIndex = rootDeviceTelemNodeIndex + subdeviceId + 1;
        telemNode = telem + std::to_string(telemNodeIndex);
    }
    std::string baseTelemSysFSNode = baseTelemSysFS + "/" + telemNode;
    std::string telemetryDeviceEntry = baseTelemSysFSNode + "/" + telem;
    if (!pFsAccess->fileExists(telemetryDeviceEntry)) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr,
                              "Telemetry support not available. No file %s\n", telemetryDeviceEntry.c_str());
        retVal = ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
        return;
    }

    std::string guid;
    std::string guidPath = baseTelemSysFSNode + std::string("/guid");
    ze_result_t result = pFsAccess->read(guidPath, guid);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr,
                              "Telemetry sysfs entry not available %s\n", guidPath.c_str());
        retVal = result;
        return;
    }
    if (getKeyOffsetMap(guid, keyOffsetMap) != ZE_RESULT_SUCCESS) {
        // We didnt have any entry for this guid in guidToKeyOffsetMap
        retVal = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        return;
    }

    std::string sizePath = baseTelemSysFSNode + std::string("/size");
    result = pFsAccess->read(sizePath, size);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr,
                              "Telemetry sysfs entry not available %s\n", sizePath.c_str());
        retVal = result;
        return;
    }

    std::string offsetPath = baseTelemSysFSNode + std::string("/offset");
    result = pFsAccess->read(offsetPath, baseOffset);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr,
                              "Telemetry sysfs entry not available %s\n", offsetPath.c_str());
        retVal = result;
        return;
    }

    int fd = open(static_cast<const char *>(telemetryDeviceEntry.c_str()), O_RDONLY);
    if (fd == -1) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr,
                              "Failure opening telemetry file %s : %s \n", telemetryDeviceEntry.c_str(), strerror(errno));
        retVal = getErrorVal(errno);
        return;
    }

    mappedMemory = static_cast<char *>(mmap(nullptr, static_cast<size_t>(size), PROT_READ, MAP_SHARED, fd, 0));
    if (mappedMemory == MAP_FAILED) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr,
                              "Failure mapping telemetry file %s : %s \n", telemetryDeviceEntry.c_str(), strerror(errno));
        close(fd);
        retVal = getErrorVal(errno);
        return;
    }

    if (close(fd) == -1) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr,
                              "Failure closing telemetry file %s : %s \n", telemetryDeviceEntry.c_str(), strerror(errno));
        munmap(mappedMemory, size);
        mappedMemory = nullptr;
        retVal = getErrorVal(errno);
        return;
    }

    mappedMemory += baseOffset;
}

PlatformMonitoringTech::PlatformMonitoringTech(FsAccess *pFsAccess, ze_bool_t onSubdevice,
                                               uint32_t subdeviceId) : subdeviceId(subdeviceId), isSubdevice(onSubdevice) {
    init(pFsAccess);
}

void PlatformMonitoringTech::create(const std::vector<ze_device_handle_t> &deviceHandles,
                                    FsAccess *pFsAccess, std::string &rootPciPathOfGpuDevice,
                                    std::map<uint32_t, L0::PlatformMonitoringTech *> &mapOfSubDeviceIdToPmtObject) {
    if (ZE_RESULT_SUCCESS == PlatformMonitoringTech::enumerateRootTelemIndex(pFsAccess, rootPciPathOfGpuDevice)) {
        for (const auto &deviceHandle : deviceHandles) {
            ze_device_properties_t deviceProperties = {};
            Device::fromHandle(deviceHandle)->getProperties(&deviceProperties);
            auto pPmt = new PlatformMonitoringTech(pFsAccess, deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_SUBDEVICE,
                                                   deviceProperties.subdeviceId);
            UNRECOVERABLE_IF(nullptr == pPmt);
            mapOfSubDeviceIdToPmtObject.emplace(deviceProperties.subdeviceId, pPmt);
        }
    }
}

PlatformMonitoringTech::~PlatformMonitoringTech() {
    if (mappedMemory != nullptr) {
        munmap(mappedMemory - baseOffset, size);
    }
}

} // namespace L0

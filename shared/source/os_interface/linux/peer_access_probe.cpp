/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/peer_access_probe.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/arrayref.h"
#include "shared/source/utilities/directory.h"

#include <fcntl.h>
#include <limits>
#include <string>
#include <vector>

namespace NEO {

namespace {
const std::string iafDirectoryLegacy = "iaf.";
const std::string iafDirectory = "i915.iaf.";
const std::string fabricIdFile = "/iaf_fabric_id";

bool hasDrmDriverModel(Device &device) {
    auto &osInterface = device.getRootDeviceEnvironment().osInterface;
    if (osInterface == nullptr || osInterface->getDriverModel() == nullptr) {
        return false;
    }
    return osInterface->getDriverModel()->getDriverModelType() == DriverModelType::drm;
}
} // namespace

bool queryFabricStatsDrm(Device &sourceDevice, Device &peerDevice, uint32_t &latency, uint32_t &bandwidth) {
    if (!hasDrmDriverModel(sourceDevice) || !hasDrmDriverModel(peerDevice)) {
        return false;
    }

    auto &osPeerInterface = peerDevice.getRootDeviceEnvironment().osInterface;
    auto pPeerDrm = osPeerInterface->getDriverModel()->as<Drm>();
    auto peerDevicePath = pPeerDrm->getSysFsPciPath();

    std::string fabricPath;
    std::vector<std::string> list = Directory::getFiles(peerDevicePath + "/device");
    for (auto &entry : list) {
        if ((entry.find(iafDirectory) != std::string::npos) || (entry.find(iafDirectoryLegacy) != std::string::npos)) {
            fabricPath = entry + fabricIdFile;
            break;
        }
    }
    if (fabricPath.empty()) {
        return false;
    }

    std::string fabricIdStr(64, '\0');
    int fd = SysCalls::open(fabricPath.c_str(), O_RDONLY);
    if (fd < 0) {
        return false;
    }
    ssize_t bytesRead = SysCalls::pread(fd, fabricIdStr.data(), fabricIdStr.size() - 1, 0);
    SysCalls::close(fd);
    if (bytesRead <= 0) {
        return false;
    }

    size_t end = 0;
    uint32_t fabricId = 0;
    try {
        fabricId = static_cast<uint32_t>(std::stoul(fabricIdStr, &end, 16));
    } catch (const std::exception &) {
        return false;
    }

    auto &osInterface = sourceDevice.getRootDeviceEnvironment().osInterface;
    auto pDrm = osInterface->getDriverModel()->as<Drm>();
    if (pDrm->getIoctlHelper()->getFabricLatency(fabricId, latency, bandwidth) == false) {
        return false;
    }

    PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                 "Connection detected between device %d and peer device %d: latency %d hops, bandwidth %d GBPS\n",
                 sourceDevice.getRootDeviceIndex(), peerDevice.getRootDeviceIndex(), latency, bandwidth);

    return true;
}

bool queryPeerAccessDrm(Device &device, Device &peerDevice, GraphicsAllocation **probeAllocPtr, uint64_t *handle) {
    if (!hasDrmDriverModel(peerDevice)) {
        return false;
    }

    uint32_t latency = std::numeric_limits<uint32_t>::max();
    uint32_t bandwidth = 0;
    if (queryFabricStatsDrm(device, peerDevice, latency, bandwidth)) {
        return true;
    }

    auto memoryManager = device.getMemoryManager();

    if (*probeAllocPtr == nullptr) {
        AllocationProperties probeProperties{device.getRootDeviceIndex(),
                                             MemoryConstants::pageSize,
                                             AllocationType::buffer,
                                             device.getDeviceBitfield()};
        probeProperties.flags.shareable = true;
        *probeAllocPtr = memoryManager->allocateGraphicsMemoryWithProperties(probeProperties);
        if (*probeAllocPtr == nullptr) {
            return false;
        }
        if ((*probeAllocPtr)->peekInternalHandle(memoryManager, *handle, nullptr) < 0) {
            memoryManager->freeGraphicsMemory(*probeAllocPtr);
            *probeAllocPtr = nullptr;
            return false;
        }
    }

    MemoryManager::OsHandleData osHandleData{*handle};
    AllocationProperties importProperties{peerDevice.getRootDeviceIndex(),
                                          MemoryConstants::pageSize,
                                          AllocationType::buffer,
                                          peerDevice.getDeviceBitfield()};
    auto importedAlloc = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, importProperties, false, false, false, nullptr);
    if (importedAlloc == nullptr) {
        return false;
    }

    auto memoryOperationsInterface = peerDevice.getRootDeviceEnvironment().memoryOperationsInterface.get();
    if (memoryOperationsInterface != nullptr) {
        auto residencyStatus = memoryOperationsInterface->makeResident(&peerDevice, ArrayRef<GraphicsAllocation *>(&importedAlloc, 1), false, false);
        if (residencyStatus != MemoryOperationsStatus::success) {
            memoryManager->freeGraphicsMemory(importedAlloc, true);
            return false;
        }
    }

    memoryManager->freeGraphicsMemory(importedAlloc, true);
    return true;
}

} // namespace NEO

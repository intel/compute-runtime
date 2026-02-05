/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/device_imp_drm/device_imp_peer.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/utilities/directory.h"

#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/device/device.h"

#include <fcntl.h>

namespace L0 {

const std::string iafDirectoryLegacy = "iaf.";
const std::string iafDirectory = "i915.iaf.";
const std::string fabricIdFile = "/iaf_fabric_id";

ze_result_t queryFabricStatsDrm(Device *pSourceDevice, Device *pPeerDevice, uint32_t &latency, uint32_t &bandwidth) {
    auto &osPeerInterface = pPeerDevice->getNEODevice()->getRootDeviceEnvironment().osInterface;
    auto pPeerDrm = osPeerInterface->getDriverModel()->as<NEO::Drm>();
    auto peerDevicePath = pPeerDrm->getSysFsPciPath();

    std::string fabricPath;
    fabricPath.clear();
    std::vector<std::string> list = NEO::Directory::getFiles(peerDevicePath + "/device");
    for (auto &entry : list) {
        if ((entry.find(iafDirectory) != std::string::npos) || (entry.find(iafDirectoryLegacy) != std::string::npos)) {
            fabricPath = entry + fabricIdFile;
            break;
        }
    }
    if (fabricPath.empty()) {
        // This device does not have a fabric
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    std::string fabricIdStr(64, '\0');
    int fd = NEO::SysCalls::open(fabricPath.c_str(), O_RDONLY);
    if (fd < 0) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    NEO::SysCalls::pread(fd, fabricIdStr.data(), fabricIdStr.size() - 1, 0);
    NEO::SysCalls::close(fd);

    size_t end = 0;
    uint32_t fabricId = static_cast<uint32_t>(std::stoul(fabricIdStr, &end, 16));

    auto &osInterface = pSourceDevice->getNEODevice()->getRootDeviceEnvironment().osInterface;
    auto pDrm = osInterface->getDriverModel()->as<NEO::Drm>();
    bool success = pDrm->getIoctlHelper()->getFabricLatency(fabricId, latency, bandwidth);
    if (success == false) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                 "Connection detected between device %d and peer device %d: latency %d hops, bandwidth %d GBPS\n",
                 pSourceDevice->getRootDeviceIndex(), pPeerDevice->getRootDeviceIndex(), latency, bandwidth);

    return ZE_RESULT_SUCCESS;
}

bool queryPeerAccessDrm(NEO::Device &device, NEO::Device &peerDevice, void **handlePtr, uint64_t *handle) {
    auto l0Device = device.getSpecializedDevice<Device>();
    auto l0PeerDevice = peerDevice.getSpecializedDevice<Device>();

    uint32_t latency = std::numeric_limits<uint32_t>::max();
    uint32_t bandwidth = 0;
    ze_result_t fabricResult = queryFabricStatsDrm(l0Device, l0PeerDevice, latency, bandwidth);
    if (fabricResult == ZE_RESULT_SUCCESS) {
        return true;
    }

    auto driverHandle = l0Device->getDriverHandle();
    auto context = static_cast<ContextImp *>(driverHandle->getDefaultContext());

    if (*handlePtr == nullptr) {
        ze_external_memory_export_desc_t exportDesc = {};
        exportDesc.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC;
        exportDesc.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;

        ze_device_mem_alloc_desc_t deviceAllocDesc = {};
        deviceAllocDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
        deviceAllocDesc.pNext = &exportDesc;

        ze_result_t allocResult = context->allocDeviceMem(l0Device->toHandle(), &deviceAllocDesc, 1u, 1u, handlePtr);
        if (allocResult != ZE_RESULT_SUCCESS) {
            return false;
        }

        const auto alloc = driverHandle->svmAllocsManager->getSVMAlloc(*handlePtr);
        auto handleResult = alloc->gpuAllocations.getDefaultGraphicsAllocation()->peekInternalHandle(driverHandle->getMemoryManager(), *handle, nullptr);
        if (handleResult < 0) {
            return false;
        }
    }

    ze_ipc_memory_flags_t flags = {};
    NEO::SvmAllocationData allocDataInternal(peerDevice.getRootDeviceIndex());
    void *importedPtr = driverHandle->importFdHandle(&peerDevice, flags, *handle, NEO::AllocationType::buffer, nullptr, nullptr, allocDataInternal);

    bool canAccess = importedPtr != nullptr;
    if (canAccess) {
        context->freeMem(importedPtr);
    }

    return canAccess;
}

} // namespace L0

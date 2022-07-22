/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/kernel/debug_data.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_interface.h"
namespace NEO {
bool DebuggerL0::initDebuggingInOs(NEO::OSInterface *osInterface) {
    if (osInterface != nullptr) {
        auto drm = osInterface->getDriverModel()->as<NEO::Drm>();
        if (drm->isVmBindAvailable() && drm->isPerContextVMRequired()) {
            drm->registerResourceClasses();
            return true;
        }
    }
    return false;
}

void DebuggerL0::initSbaTrackingMode() {
    singleAddressSpaceSbaTracking = false;
}

void DebuggerL0::registerAllocationType(GraphicsAllocation *allocation) {}

void DebuggerL0::registerElf(NEO::DebugData *debugData, NEO::GraphicsAllocation *isaAllocation) {
    if (device->getRootDeviceEnvironment().osInterface.get() != nullptr) {
        auto drm = device->getRootDeviceEnvironment().osInterface->getDriverModel()->as<NEO::Drm>();
        auto handle = drm->registerResource(NEO::DrmResourceClass::Elf, debugData->vIsa, debugData->vIsaSize);
        static_cast<NEO::DrmAllocation *>(isaAllocation)->linkWithRegisteredHandle(handle);
    }
}

bool DebuggerL0::attachZebinModuleToSegmentAllocations(const StackVec<NEO::GraphicsAllocation *, 32> &allocs, uint32_t &moduleHandle) {
    if (device->getRootDeviceEnvironment().osInterface == nullptr) {
        return false;
    }
    auto drm = device->getRootDeviceEnvironment().osInterface->getDriverModel()->as<NEO::Drm>();
    uint32_t segmentCount = static_cast<uint32_t>(allocs.size());
    moduleHandle = drm->registerResource(NEO::DrmResourceClass::L0ZebinModule, &segmentCount, sizeof(uint32_t));

    for (auto &allocation : allocs) {
        auto drmAllocation = static_cast<NEO::DrmAllocation *>(allocation);
        drmAllocation->linkWithRegisteredHandle(moduleHandle);
    }

    return true;
}

bool DebuggerL0::removeZebinModule(uint32_t moduleHandle) {
    if (device->getRootDeviceEnvironment().osInterface == nullptr || moduleHandle == 0) {
        return false;
    }
    auto drm = device->getRootDeviceEnvironment().osInterface->getDriverModel()->as<NEO::Drm>();

    drm->unregisterResource(moduleHandle);
    return true;
}

void DebuggerL0::notifyCommandQueueCreated(NEO::Device *device) {
    if (this->device->getRootDeviceEnvironment().osInterface.get() != nullptr) {
        std::unique_lock<std::mutex> commandQueueCountLock(debuggerL0Mutex);

        auto index = device->isSubDevice() ? static_cast<NEO::SubDevice *>(device)->getSubDeviceIndex() : 0;

        if (++commandQueueCount[index] == 1) {
            auto drm = this->device->getRootDeviceEnvironment().osInterface->getDriverModel()->as<NEO::Drm>();

            CommandQueueNotification notification = {index, this->device->getNumSubDevices()};
            uuidL0CommandQueueHandle[index] = drm->notifyFirstCommandQueueCreated(&notification, sizeof(CommandQueueNotification));
        }
    }
}

void DebuggerL0::notifyCommandQueueDestroyed(NEO::Device *device) {
    if (this->device->getRootDeviceEnvironment().osInterface.get() != nullptr) {
        std::unique_lock<std::mutex> commandQueueCountLock(debuggerL0Mutex);

        auto index = device->isSubDevice() ? static_cast<NEO::SubDevice *>(device)->getSubDeviceIndex() : 0;

        if (--commandQueueCount[index] == 0) {
            auto drm = this->device->getRootDeviceEnvironment().osInterface->getDriverModel()->as<NEO::Drm>();
            drm->notifyLastCommandQueueDestroyed(uuidL0CommandQueueHandle[index]);
            uuidL0CommandQueueHandle[index] = 0;
        }
    }
}

void DebuggerL0::notifyModuleCreate(void *module, uint32_t moduleSize, uint64_t moduleLoadAddress) {}

} // namespace NEO

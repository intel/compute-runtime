/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/kernel/debug_data.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/core/source/debugger/debugger_l0.h"
namespace L0 {
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

void DebuggerL0::registerElf(NEO::DebugData *debugData, NEO::GraphicsAllocation *isaAllocation) {
    if (device->getRootDeviceEnvironment().osInterface.get() != nullptr) {
        auto drm = device->getRootDeviceEnvironment().osInterface->getDriverModel()->as<NEO::Drm>();
        auto handle = drm->registerResource(NEO::Drm::ResourceClass::Elf, debugData->vIsa, debugData->vIsaSize);
        static_cast<NEO::DrmAllocation *>(isaAllocation)->linkWithRegisteredHandle(handle);
    }
}

bool DebuggerL0::attachZebinModuleToSegmentAllocations(const StackVec<NEO::GraphicsAllocation *, 32> &allocs, uint32_t &moduleHandle) {
    if (device->getRootDeviceEnvironment().osInterface == nullptr) {
        return false;
    }
    auto drm = device->getRootDeviceEnvironment().osInterface->getDriverModel()->as<NEO::Drm>();
    uint32_t segmentCount = static_cast<uint32_t>(allocs.size());
    moduleHandle = drm->registerResource(NEO::Drm::ResourceClass::L0ZebinModule, &segmentCount, sizeof(uint32_t));

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

void DebuggerL0::notifyCommandQueueCreated() {
    if (device->getRootDeviceEnvironment().osInterface.get() != nullptr) {
        if (++commandQueueCount == 1) {
            auto drm = device->getRootDeviceEnvironment().osInterface->getDriverModel()->as<NEO::Drm>();
            uuidL0CommandQueueHandle = drm->notifyFirstCommandQueueCreated();
        }
    }
}

void DebuggerL0::notifyCommandQueueDestroyed() {
    if (device->getRootDeviceEnvironment().osInterface.get() != nullptr) {
        if (--commandQueueCount == 0) {
            auto drm = device->getRootDeviceEnvironment().osInterface->getDriverModel()->as<NEO::Drm>();
            drm->notifyLastCommandQueueDestroyed(uuidL0CommandQueueHandle);
        }
    }
}

} // namespace L0

/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/device/sub_device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/kernel/debug_data.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_interface.h"

namespace NEO {
bool DebuggerL0::initDebuggingInOs(NEO::OSInterface *osInterface) {
    if (osInterface != nullptr) {
        auto drm = osInterface->getDriverModel()->as<NEO::Drm>();

        const bool vmBindAvailable = drm->isVmBindAvailable();
        const bool perContextVms = drm->isPerContextVMRequired();
        auto &hwInfo = *drm->getHardwareInfo();
        bool allowDebug = false;

        if (drm->getRootDeviceEnvironment().executionEnvironment.getDebuggingMode() == DebuggingMode::online) {
            allowDebug = drm->getRootDeviceEnvironment().getHelper<CompilerProductHelper>().isHeaplessModeEnabled(hwInfo) ? true : perContextVms;
        } else if (drm->getRootDeviceEnvironment().executionEnvironment.getDebuggingMode() == DebuggingMode::offline) {
            allowDebug = true;
        }

        if (vmBindAvailable && allowDebug) {
            drm->registerResourceClasses();
            return true;
        } else {
            printDebugString(debugManager.flags.PrintDebugMessages.get(), stderr,
                             "Debugging not enabled. VmBind: %d, per-context VMs: %d\n", vmBindAvailable ? 1 : 0, perContextVms ? 1 : 0);
        }
    }
    return false;
}

void DebuggerL0::initSbaTrackingMode() {
    if (device->getExecutionEnvironment()->getDebuggingMode() == DebuggingMode::offline) {
        singleAddressSpaceSbaTracking = true;
    } else {
        singleAddressSpaceSbaTracking = false;
    }
}

void DebuggerL0::registerAllocationType(GraphicsAllocation *allocation) {}

void DebuggerL0::registerElfAndLinkWithAllocation(NEO::DebugData *debugData, NEO::GraphicsAllocation *isaAllocation) {
    auto handle = registerElf(debugData);
    if (handle != 0) {
        static_cast<NEO::DrmAllocation *>(isaAllocation)->linkWithRegisteredHandle(handle);
    }
}

uint32_t DebuggerL0::registerElf(NEO::DebugData *debugData) {
    uint32_t handle = 0;
    if (device->getRootDeviceEnvironment().osInterface.get() != nullptr) {
        auto drm = device->getRootDeviceEnvironment().osInterface->getDriverModel()->as<NEO::Drm>();
        handle = drm->registerResource(NEO::DrmResourceClass::elf, debugData->vIsa, debugData->vIsaSize);
    }
    return handle;
}

bool DebuggerL0::attachZebinModuleToSegmentAllocations(const StackVec<NEO::GraphicsAllocation *, 32> &allocs, uint32_t &moduleHandle, uint32_t elfHandle) {
    if (device->getRootDeviceEnvironment().osInterface == nullptr) {
        return false;
    }
    auto drm = device->getRootDeviceEnvironment().osInterface->getDriverModel()->as<NEO::Drm>();
    uint32_t segmentCount = static_cast<uint32_t>(allocs.size());
    moduleHandle = drm->registerResource(NEO::DrmResourceClass::l0ZebinModule, &segmentCount, sizeof(uint32_t));

    for (auto &allocation : allocs) {
        auto drmAllocation = static_cast<NEO::DrmAllocation *>(allocation);

        DEBUG_BREAK_IF(allocation->getAllocationType() == AllocationType::kernelIsaInternal);
        drmAllocation->linkWithRegisteredHandle(elfHandle);
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

void DebuggerL0::notifyModuleDestroy(uint64_t moduleLoadAddress) {}

void DebuggerL0::notifyCommandQueueCreated(NEO::Device *device) {
    if (this->device->getRootDeviceEnvironment().osInterface.get() != nullptr) {
        std::unique_lock<std::mutex> commandQueueCountLock(debuggerL0Mutex);

        if (!device->isSubDevice() && device->getDeviceBitfield().count() > 1) {
            UNRECOVERABLE_IF(this->device->getNumSubDevices() != device->getDeviceBitfield().count());

            for (size_t i = 0; i < device->getDeviceBitfield().size(); i++) {
                if (device->getDeviceBitfield().test(i)) {
                    if (++commandQueueCount[i] == 1) {
                        auto drm = this->device->getRootDeviceEnvironment().osInterface->getDriverModel()->as<NEO::Drm>();

                        CommandQueueNotification notification = {static_cast<uint32_t>(i), this->device->getNumSubDevices()};
                        uuidL0CommandQueueHandle[i] = drm->notifyFirstCommandQueueCreated(&notification, sizeof(CommandQueueNotification));
                    }
                }
            }
            return;
        }

        auto index = 0u;
        auto deviceIndex = 0u;

        if (device->isSubDevice()) {
            index = static_cast<NEO::SubDevice *>(device)->getSubDeviceIndex();
            deviceIndex = index;
        } else if (device->getDeviceBitfield().count() == 1) {
            deviceIndex = Math::log2(static_cast<uint32_t>(device->getDeviceBitfield().to_ulong()));
        }

        if (++commandQueueCount[index] == 1) {
            auto drm = this->device->getRootDeviceEnvironment().osInterface->getDriverModel()->as<NEO::Drm>();

            CommandQueueNotification notification = {deviceIndex, this->device->getNumSubDevices()};
            uuidL0CommandQueueHandle[index] = drm->notifyFirstCommandQueueCreated(&notification, sizeof(CommandQueueNotification));
        }
    }
}

void DebuggerL0::notifyCommandQueueDestroyed(NEO::Device *device) {
    if (this->device->getRootDeviceEnvironment().osInterface.get() != nullptr) {
        std::unique_lock<std::mutex> commandQueueCountLock(debuggerL0Mutex);

        if (!device->isSubDevice() && device->getDeviceBitfield().count() > 1) {
            UNRECOVERABLE_IF(this->device->getNumSubDevices() != device->getDeviceBitfield().count());

            for (size_t i = 0; i < device->getDeviceBitfield().size(); i++) {
                if (device->getDeviceBitfield().test(i)) {
                    if (--commandQueueCount[i] == 0) {
                        auto drm = this->device->getRootDeviceEnvironment().osInterface->getDriverModel()->as<NEO::Drm>();
                        drm->notifyLastCommandQueueDestroyed(uuidL0CommandQueueHandle[i]);
                        uuidL0CommandQueueHandle[i] = 0;
                    }
                }
            }
            return;
        }

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

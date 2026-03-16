/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/debugger/linux/debugger_xe.h"
#include "shared/source/device/sub_device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/kernel/debug_data.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/os_interface.h"

namespace NEO {
char DebuggerL0::debugProcDir[PATH_MAX];

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

            if (drm->getEuDebugInterfaceType() == EuDebugInterfaceType::upstream) {
                return createDebugDirectory();
            }
            return true;
        } else {
            PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr,
                         "Debugging not enabled. VmBind: %d, per-context VMs: %d\n", vmBindAvailable ? 1 : 0, perContextVms ? 1 : 0);
        }
    }
    return false;
}

bool DebuggerL0::createDebugDirectory() {
    auto debugDir = std::string(DebuggerL0::debugTmpDirPrefix);
    NEO::SysCalls::mkdir(debugDir);

    std::string procDir = debugDir + "/" + std::to_string(NEO::SysCalls::getpid());
    strcpy_s(DebuggerL0::debugProcDir, sizeof(DebuggerL0::debugProcDir), procDir.c_str());
    auto ret = NEO::SysCalls::mkdir(DebuggerL0::debugProcDir);

    if (ret != 0 && errno != EEXIST) {
        PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "Failed to create debug directory %s, error: %d\n", DebuggerL0::debugProcDir, errno);
        return false;
    }
    return true;
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
    if (!device->getRootDeviceEnvironment().osInterface.get()) {
        return handle;
    }
    auto drm = device->getRootDeviceEnvironment().osInterface->getDriverModel()->as<NEO::Drm>();
    if (drm->getEuDebugInterfaceType() == EuDebugInterfaceType::upstream) {
        handle = IsaDebugData::nextDebugDataMapHandle.fetch_add(1);

        auto fullName = std::string(DebuggerL0::debugProcDir) + "/elf-" + std::to_string(handle);
        auto &isaDebugData = drm->isaDebugDataMap[handle];
        strcpy_s(isaDebugData.elfPath, sizeof(isaDebugData.elfPath), fullName.c_str());
        dumpFileIncrement(debugData->vIsa, debugData->vIsaSize, fullName, "");
    } else {
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
    if (drm->getEuDebugInterfaceType() == EuDebugInterfaceType::upstream) {
        moduleHandle = elfHandle;
        auto &isaDebugData = drm->isaDebugDataMap[elfHandle];
        isaDebugData.totalSegments = segmentCount;
        for (auto &allocation : allocs) {
            auto drmAllocation = static_cast<NEO::DrmAllocation *>(allocation);
            drmAllocation->setIsaDebugDataHandle(elfHandle);
            drmAllocation->setDrmResourceClass(DrmResourceClass::isa);
        }

    } else {
        moduleHandle = drm->registerResource(NEO::DrmResourceClass::l0ZebinModule, &segmentCount, sizeof(uint32_t));

        for (auto &allocation : allocs) {
            auto drmAllocation = static_cast<NEO::DrmAllocation *>(allocation);

            DEBUG_BREAK_IF(allocation->getAllocationType() == AllocationType::kernelIsaInternal);
            drmAllocation->linkWithRegisteredHandle(elfHandle);
            drmAllocation->linkWithRegisteredHandle(moduleHandle);
        }
    }

    return true;
}

bool DebuggerL0::removeZebinModule(uint32_t moduleHandle) {
    if (device->getRootDeviceEnvironment().osInterface == nullptr || moduleHandle == 0) {
        return false;
    }
    auto drm = device->getRootDeviceEnvironment().osInterface->getDriverModel()->as<NEO::Drm>();
    if (drm->getEuDebugInterfaceType() == EuDebugInterfaceType::upstream) {
        drm->isaDebugDataMap.erase(moduleHandle);
    } else {
        drm->unregisterResource(moduleHandle);
    }

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

void DebuggerL0::removeTempFiles() {

    DIR *dir = NEO::SysCalls::opendir(DebuggerL0::debugProcDir);
    if (dir != nullptr) {
        struct dirent *entry;
        while ((entry = NEO::SysCalls::readdir(dir)) != nullptr) {
            std::string filePath = std::string(DebuggerL0::debugProcDir) + "/" + entry->d_name;
            NEO::SysCalls::unlink(filePath);
        }
        NEO::SysCalls::closedir(dir);
        NEO::SysCalls::rmdir(DebuggerL0::debugProcDir);
    }
}

} // namespace NEO

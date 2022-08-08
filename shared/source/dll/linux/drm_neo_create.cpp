/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/drm_null_device.h"

#include "hw_cmds.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <memory>

namespace NEO {
const DeviceDescriptor deviceDescriptorTable[] = {
#define NAMEDDEVICE(devId, gt, devName) {devId, &gt::hwInfo, &gt::setupHardwareInfo, devName},
#define DEVICE(devId, gt) {devId, &gt::hwInfo, &gt::setupHardwareInfo, ""},
#include "devices.inl"
#undef DEVICE
#undef NAMEDDEVICE
    {0, nullptr, nullptr}};

Drm *Drm::create(std::unique_ptr<HwDeviceIdDrm> &&hwDeviceId, RootDeviceEnvironment &rootDeviceEnvironment) {
    std::unique_ptr<Drm> drm;
    if (DebugManager.flags.EnableNullHardware.get() == true) {
        drm.reset(new DrmNullDevice(std::move(hwDeviceId), rootDeviceEnvironment));
    } else {
        drm.reset(new Drm(std::move(hwDeviceId), rootDeviceEnvironment));
    }

    if (!drm->queryDeviceIdAndRevision()) {
        return nullptr;
    }
    auto hwInfo = rootDeviceEnvironment.getMutableHardwareInfo();
    if (!DeviceFactory::isAllowedDeviceId(hwInfo->platform.usDeviceID, DebugManager.flags.FilterDeviceId.get())) {
        return nullptr;
    }

    const DeviceDescriptor *deviceDescriptor = nullptr;
    const char *deviceName = "";
    for (auto &deviceDescriptorEntry : deviceDescriptorTable) {
        if (hwInfo->platform.usDeviceID == deviceDescriptorEntry.deviceId) {
            deviceDescriptor = &deviceDescriptorEntry;
            deviceName = deviceDescriptorEntry.devName;
            break;
        }
    }
    int ret = 0;
    if (deviceDescriptor) {
        ret = drm->setupHardwareInfo(deviceDescriptor, true);
        if (ret != 0) {
            return nullptr;
        }
        hwInfo->capabilityTable.deviceName = deviceName;
    } else {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr,
                         "FATAL: Unknown device: deviceId: %04x, revisionId: %04x\n", hwInfo->platform.usDeviceID, hwInfo->platform.usRevId);
        return nullptr;
    }

    // Detect device parameters
    int hasExecSoftPin = 0;
    ret = drm->getExecSoftPin(hasExecSoftPin);
    if (ret != 0) {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Cannot query Soft Pin parameter!\n");
        return nullptr;
    }

    if (!hasExecSoftPin) {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s",
                         "FATAL: Device doesn't support Soft-Pin but this is required.\n");
        return nullptr;
    }

    // Activate the Turbo Boost Frequency feature
    ret = drm->enableTurboBoost();
    if (ret != 0) {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "WARNING: Failed to request OCL Turbo Boost\n");
    }

    if (!drm->queryMemoryInfo()) {
        drm->setPerContextVMRequired(true);
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "WARNING: Failed to query memory info\n");
    }

    if (!drm->queryEngineInfo()) {
        drm->setPerContextVMRequired(true);
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "WARNING: Failed to query engine info\n");
    }

    drm->checkContextDebugSupport();

    drm->queryPageFaultSupport();

    if (rootDeviceEnvironment.executionEnvironment.isDebuggingEnabled()) {
        if (drm->isVmBindAvailable()) {
            drm->setPerContextVMRequired(true);
        } else {
            printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "WARNING: Debugging not supported\n");
        }
    }

    if (!drm->isPerContextVMRequired()) {
        if (!drm->createVirtualMemoryAddressSpace(HwHelper::getSubDevicesCount(rootDeviceEnvironment.getHardwareInfo()))) {
            printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "INFO: Device doesn't support GEM Virtual Memory\n");
        }
    }

    drm->queryAdapterBDF();

    return drm.release();
}

void Drm::overrideBindSupport(bool &useVmBind) {
    if (DebugManager.flags.UseVmBind.get() != -1) {
        useVmBind = DebugManager.flags.UseVmBind.get();
    }
}

} // namespace NEO

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
    std::unique_ptr<Drm> drmObject;
    if (DebugManager.flags.EnableNullHardware.get() == true) {
        drmObject.reset(new DrmNullDevice(std::move(hwDeviceId), rootDeviceEnvironment));
    } else {
        drmObject.reset(new Drm(std::move(hwDeviceId), rootDeviceEnvironment));
    }

    if (!drmObject->queryDeviceIdAndRevision()) {
        return nullptr;
    }

    if (!DeviceFactory::isAllowedDeviceId(drmObject->deviceId, DebugManager.flags.FilterDeviceId.get())) {
        return nullptr;
    }

    const DeviceDescriptor *device = nullptr;
    const char *devName = "";
    for (auto &d : deviceDescriptorTable) {
        if (drmObject->deviceId == d.deviceId) {
            device = &d;
            devName = d.devName;
            break;
        }
    }
    int ret = 0;
    if (device) {
        ret = drmObject->setupHardwareInfo(device, true);
        if (ret != 0) {
            return nullptr;
        }
        rootDeviceEnvironment.getMutableHardwareInfo()->capabilityTable.deviceName = devName;
    } else {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr,
                         "FATAL: Unknown device: deviceId: %04x, revisionId: %04x\n", drmObject->deviceId, drmObject->revisionId);
        return nullptr;
    }

    // Detect device parameters
    int hasExecSoftPin = 0;
    ret = drmObject->getExecSoftPin(hasExecSoftPin);
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
    ret = drmObject->enableTurboBoost();
    if (ret != 0) {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "WARNING: Failed to request OCL Turbo Boost\n");
    }

    if (!drmObject->queryMemoryInfo()) {
        drmObject->setPerContextVMRequired(true);
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "WARNING: Failed to query memory info\n");
    }

    if (!drmObject->queryEngineInfo()) {
        drmObject->setPerContextVMRequired(true);
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "WARNING: Failed to query engine info\n");
    }

    drmObject->checkContextDebugSupport();

    drmObject->queryPageFaultSupport();

    if (rootDeviceEnvironment.executionEnvironment.isDebuggingEnabled()) {
        if (drmObject->isVmBindAvailable()) {
            drmObject->setPerContextVMRequired(true);
        } else {
            printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "WARNING: Debugging not supported\n");
        }
    }

    if (!drmObject->isPerContextVMRequired()) {
        if (!drmObject->createVirtualMemoryAddressSpace(HwHelper::getSubDevicesCount(rootDeviceEnvironment.getHardwareInfo()))) {
            printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "INFO: Device doesn't support GEM Virtual Memory\n");
        }
    }

    drmObject->queryAdapterBDF();

    return drmObject.release();
}

void Drm::overrideBindSupport(bool &useVmBind) {
    if (DebugManager.flags.UseVmBind.get() != -1) {
        useVmBind = DebugManager.flags.UseVmBind.get();
    }
}

} // namespace NEO

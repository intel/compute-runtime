/*
 * Copyright (C) 2018-2021 Intel Corporation
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
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/drm_null_device.h"

#include "drm/i915_drm.h"
#include "hw_cmds.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <memory>

namespace NEO {
const DeviceDescriptor deviceDescriptorTable[] = {
#define NAMEDDEVICE(devId, gt, gtType, devName) {devId, &gt::hwInfo, &gt::setupHardwareInfo, gtType, devName},
#define DEVICE(devId, gt, gtType) {devId, &gt::hwInfo, &gt::setupHardwareInfo, gtType, ""},
#include "devices.inl"
#undef DEVICE
#undef NAMEDDEVICE
    {0, nullptr, nullptr, GTTYPE_UNDEFINED}};

Drm *Drm::create(std::unique_ptr<HwDeviceIdDrm> &&hwDeviceId, RootDeviceEnvironment &rootDeviceEnvironment) {
    std::unique_ptr<Drm> drmObject;
    if (DebugManager.flags.EnableNullHardware.get() == true) {
        drmObject.reset(new DrmNullDevice(std::move(hwDeviceId), rootDeviceEnvironment));
    } else {
        drmObject.reset(new Drm(std::move(hwDeviceId), rootDeviceEnvironment));
    }

    // Get HW version (I915_drm.h)
    int ret = drmObject->getDeviceID(drmObject->deviceId);
    if (ret != 0) {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Cannot query device ID parameter!\n");
        return nullptr;
    }

    // Get HW Revision (I915_drm.h)
    ret = drmObject->getDeviceRevID(drmObject->revisionId);
    if (ret != 0) {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Cannot query device Rev ID parameter!\n");
        return nullptr;
    }

    const DeviceDescriptor *device = nullptr;
    const char *devName = "";
    GTTYPE eGtType = GTTYPE_UNDEFINED;
    for (auto &d : deviceDescriptorTable) {
        if (drmObject->deviceId == d.deviceId) {
            device = &d;
            eGtType = d.eGtType;
            devName = d.devName;
            break;
        }
    }
    if (device) {
        ret = drmObject->setupHardwareInfo(const_cast<DeviceDescriptor *>(device), true);
        if (ret != 0) {
            return nullptr;
        }
        drmObject->setGtType(eGtType);
        rootDeviceEnvironment.setHwInfo(device->pHwInfo);
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

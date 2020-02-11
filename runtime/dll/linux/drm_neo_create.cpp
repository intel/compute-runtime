/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/debug_settings/debug_settings_manager.h"
#include "core/execution_environment/execution_environment.h"
#include "core/execution_environment/root_device_environment.h"
#include "core/gmm_helper/gmm_helper.h"
#include "core/helpers/hw_cmds.h"
#include "core/helpers/hw_helper.h"
#include "core/helpers/hw_info.h"
#include "core/os_interface/linux/drm_neo.h"
#include "core/os_interface/linux/drm_null_device.h"

#include "drm/i915_drm.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <memory>

namespace NEO {
const DeviceDescriptor deviceDescriptorTable[] = {
#define DEVICE(devId, gt, gtType) {devId, &gt::hwInfo, &gt::setupHardwareInfo, gtType},
#include "devices.inl"
#undef DEVICE
    {0, nullptr, nullptr, GTTYPE_UNDEFINED}};

Drm::~Drm() = default;

Drm *Drm::create(std::unique_ptr<HwDeviceId> hwDeviceId, RootDeviceEnvironment &rootDeviceEnvironment) {
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
    GTTYPE eGtType = GTTYPE_UNDEFINED;
    for (auto &d : deviceDescriptorTable) {
        if (drmObject->deviceId == d.deviceId) {
            device = &d;
            eGtType = d.eGtType;
            break;
        }
    }
    if (device) {
        ret = drmObject->setupHardwareInfo(const_cast<DeviceDescriptor *>(device), true);
        if (ret != 0) {
            return nullptr;
        }
        drmObject->setGtType(eGtType);
        rootDeviceEnvironment.executionEnvironment.setHwInfo(device->pHwInfo);
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

    if (!drmObject->queryEngineInfo()) {
        printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "WARNING: Failed to query engine info\n");
    }

    if (HwHelper::get(device->pHwInfo->platform.eRenderCoreFamily).getEnableLocalMemory(*device->pHwInfo)) {
        if (!drmObject->queryMemoryInfo()) {
            printDebugString(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "WARNING: Failed to query memory info\n");
        }
    }

    return drmObject.release();
}
} // namespace NEO

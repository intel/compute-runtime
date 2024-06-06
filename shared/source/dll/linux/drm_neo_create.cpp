/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/linux/drm_neo.h"

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
    {0, nullptr, nullptr, ""}};

Drm *Drm::create(std::unique_ptr<HwDeviceIdDrm> &&hwDeviceId, RootDeviceEnvironment &rootDeviceEnvironment) {
    auto drm = std::unique_ptr<Drm>(new Drm(std::move(hwDeviceId), rootDeviceEnvironment));

    if (!drm->queryDeviceIdAndRevision()) {
        return nullptr;
    }
    auto hwInfo = rootDeviceEnvironment.getMutableHardwareInfo();
    if (!DeviceFactory::isAllowedDeviceId(hwInfo->platform.usDeviceID, debugManager.flags.FilterDeviceId.get())) {
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
        printDebugString(debugManager.flags.PrintDebugMessages.get(), stderr,
                         "FATAL: Unknown device: deviceId: %04x, revisionId: %04x\n", hwInfo->platform.usDeviceID, hwInfo->platform.usRevId);
        return nullptr;
    }

    // Activate the Turbo Boost Frequency feature
    ret = drm->enableTurboBoost();
    if (ret != 0) {
        printDebugString(debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "WARNING: Failed to request OCL Turbo Boost\n");
    }

    if (!drm->queryMemoryInfo()) {
        drm->setPerContextVMRequired(true);
        printDebugString(debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "WARNING: Failed to query memory info\n");
    }

    if (!drm->queryEngineInfo()) {
        drm->setPerContextVMRequired(true);
        printDebugString(debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "WARNING: Failed to query engine info\n");
    }

    drm->checkContextDebugSupport();

    drm->queryPageFaultSupport();

    if (rootDeviceEnvironment.executionEnvironment.isDebuggingEnabled()) {
        if (drm->getRootDeviceEnvironment().executionEnvironment.getDebuggingMode() == DebuggingMode::offline) {
            drm->setPerContextVMRequired(false);
        } else {
            if (drm->isVmBindAvailable()) {
                drm->setPerContextVMRequired(true);
            } else {
                printDebugString(debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "WARNING: Debugging not supported\n");
            }
        }
    }

    drm->isSetPairAvailable();
    drm->isChunkingAvailable();

    drm->configureScratchPagePolicy();
    drm->configureGpuFaultCheckThreshold();

    if (!drm->isPerContextVMRequired()) {
        if (!drm->createVirtualMemoryAddressSpace(GfxCoreHelper::getSubDevicesCount(rootDeviceEnvironment.getHardwareInfo()))) {
            printDebugString(debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "INFO: Device doesn't support GEM Virtual Memory\n");
        }
    }

    drm->queryAdapterBDF();

    return drm.release();
}

void Drm::overrideBindSupport(bool &useVmBind) {
    if (debugManager.flags.UseVmBind.get() != -1) {
        useVmBind = debugManager.flags.UseVmBind.get();
    }
}

} // namespace NEO

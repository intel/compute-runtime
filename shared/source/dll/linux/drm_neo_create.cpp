/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/unified_memory/usm_memory_support.h"

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
    std::unique_ptr<Drm> drm{new Drm(std::move(hwDeviceId), rootDeviceEnvironment)};

    if (!drm->queryDeviceIdAndRevision()) {
        return nullptr;
    }

    const auto usDeviceID = rootDeviceEnvironment.getHardwareInfo()->platform.usDeviceID;
    const auto usRevId = rootDeviceEnvironment.getHardwareInfo()->platform.usRevId;
    if (!DeviceFactory::isAllowedDeviceId(usDeviceID, debugManager.flags.FilterDeviceId.get())) {
        return nullptr;
    }

    const DeviceDescriptor *deviceDescriptor = nullptr;
    for (auto &deviceDescriptorEntry : deviceDescriptorTable) {
        if (usDeviceID == deviceDescriptorEntry.deviceId) {
            deviceDescriptor = &deviceDescriptorEntry;
            break;
        }
    }
    if (!deviceDescriptor) {
        printDebugString(debugManager.flags.PrintDebugMessages.get(), stderr,
                         "FATAL: Unknown device: deviceId: %04x, revisionId: %04x\n", usDeviceID, usRevId);
        return nullptr;
    }

    if (drm->setupHardwareInfo(deviceDescriptor, true)) {
        return nullptr;
    }

    if (drm->enableTurboBoost()) {
        printDebugString(debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "WARNING: Failed to request OCL Turbo Boost\n");
    }

    drm->checkContextDebugSupport();

    drm->queryPageFaultSupport();
    auto &compilerProductHelper = rootDeviceEnvironment.getHelper<CompilerProductHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    if (rootDeviceEnvironment.executionEnvironment.isDebuggingEnabled() && !compilerProductHelper.isHeaplessModeEnabled(hwInfo)) {
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
        if (!drm->createVirtualMemoryAddressSpace(GfxCoreHelper::getSubDevicesCount(&hwInfo))) {
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

bool DrmMemoryManager::isGemCloseWorkerSupported() {
    return true;
}

} // namespace NEO

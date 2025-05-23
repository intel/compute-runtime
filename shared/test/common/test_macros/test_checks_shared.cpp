/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test_checks_shared.h"

#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"

using namespace NEO;

bool TestChecks::supportsBlitter(const RootDeviceEnvironment &rootDeviceEnvironment) {
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    auto hwInfo = rootDeviceEnvironment.getMutableHardwareInfo();
    auto engines = gfxCoreHelper.getGpgpuEngineInstances(rootDeviceEnvironment);
    for (const auto &engine : engines) {
        if (engine.first == aub_stream::EngineType::ENGINE_BCS) {
            return hwInfo->capabilityTable.blitterOperationsSupported;
        }
    }
    return false;
}

bool TestChecks::fullySupportsBlitter(const RootDeviceEnvironment &rootDeviceEnvironment) {
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    auto hwInfo = rootDeviceEnvironment.getMutableHardwareInfo();
    auto engines = gfxCoreHelper.getGpgpuEngineInstances(rootDeviceEnvironment);
    for (const auto &engine : engines) {
        if (engine.first == aub_stream::EngineType::ENGINE_BCS) {
            auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
            return productHelper.isBlitterFullySupported(*hwInfo);
        }
    }
    return false;
}

bool TestChecks::allowsDcFlush(const Device *device) {
    return device->getProductHelper().isDcFlushAllowed();
}

bool TestChecks::supportsImages(const HardwareInfo &hardwareInfo) {
    return hardwareInfo.capabilityTable.supportsImages;
}

bool TestChecks::supportsImages(const std::unique_ptr<HardwareInfo> &pHardwareInfo) {
    return supportsImages(*pHardwareInfo);
}
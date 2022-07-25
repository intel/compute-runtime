/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test_checks_shared.h"

#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

bool TestChecks::supportsBlitter(const HardwareInfo *pHardwareInfo) {
    auto engines = HwHelper::get(::renderCoreFamily).getGpgpuEngineInstances(*pHardwareInfo);
    for (const auto &engine : engines) {
        if (engine.first == aub_stream::EngineType::ENGINE_BCS) {
            return pHardwareInfo->capabilityTable.blitterOperationsSupported;
        }
    }
    return false;
}

bool TestChecks::fullySupportsBlitter(const HardwareInfo *pHardwareInfo) {
    auto engines = HwHelper::get(::renderCoreFamily).getGpgpuEngineInstances(*pHardwareInfo);
    for (const auto &engine : engines) {
        if (engine.first == aub_stream::EngineType::ENGINE_BCS) {
            return HwInfoConfig::get(pHardwareInfo->platform.eProductFamily)->isBlitterFullySupported(*pHardwareInfo);
        }
    }
    return false;
}

bool TestChecks::supportsImages(const HardwareInfo &hardwareInfo) {
    return hardwareInfo.capabilityTable.supportsImages;
}

bool TestChecks::supportsImages(const std::unique_ptr<HardwareInfo> &pHardwareInfo) {
    return supportsImages(*pHardwareInfo);
}

bool TestChecks::supportsSvm(const HardwareInfo *pHardwareInfo) {
    return pHardwareInfo->capabilityTable.ftrSvm;
}
bool TestChecks::supportsSvm(const std::unique_ptr<HardwareInfo> &pHardwareInfo) {
    return supportsSvm(pHardwareInfo.get());
}
bool TestChecks::supportsSvm(const Device *pDevice) {
    return supportsSvm(&pDevice->getHardwareInfo());
}
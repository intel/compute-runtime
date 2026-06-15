/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/kernel/kernel_properties.h"

namespace NEO {

std::unique_ptr<ReleaseHelper> ReleaseHelper::create(HardwareIpVersion hardwareIpVersion) {

    auto architecture = hardwareIpVersion.architecture;
    auto release = hardwareIpVersion.release;
    if (releaseHelperFactory[architecture] == nullptr || releaseHelperFactory[architecture][release] == nullptr) {
        return {nullptr};
    }
    auto createFunction = releaseHelperFactory[architecture][release];
    return createFunction(hardwareIpVersion);
}

void ReleaseHelper::getKernelCapabilitiesExtra(uint32_t &extraCaps) const {
    extraCaps |= this->getAdditionalExtraCaps();
}

void ReleaseHelper::getKernelFp16AtomicCapabilities(uint32_t &fp16Caps) const {
    fp16Caps = (0u | FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps);
    fp16Caps |= this->getAdditionalFp16Caps();
}

bool ReleaseHelper::isAvailableSemaphore64(const HardwareInfo &hwInfo) const {
    if (debugManager.flags.Enable64BitSemaphore.get() != -1) {
        return debugManager.flags.Enable64BitSemaphore.get() == 1;
    }

    if (!hwInfo.featureTable.flags.ftrHwSemaphore64) {
        return false;
    }

    return this->isAvailableSemaphore64Base();
}

std::pair<bool, bool> ReleaseHelper::isPipeControlPriorToNonPipelinedStateCommandsWARequired(const HardwareInfo &hwInfo, bool isRcs) const {
    auto isExtendedWARequired = isPipeControlPriorToNonPipelinedStateCommandsExtendedWARequired(hwInfo, isRcs);
    if (debugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.get() != -1) {
        isExtendedWARequired = debugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.get();
    }
    return {isPipeControlPriorToNonPipelinedStateCommandsBaseWARequired(), isExtendedWARequired};
}

} // namespace NEO

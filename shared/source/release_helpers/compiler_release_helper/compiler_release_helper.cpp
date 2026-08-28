/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/kernel/kernel_properties.h"

namespace NEO {

std::unique_ptr<CompilerReleaseHelper> CompilerReleaseHelper::create(HardwareIpVersion hardwareIpVersion) {
    auto architecture = hardwareIpVersion.architecture;
    auto release = hardwareIpVersion.release;
    UNRECOVERABLE_IF(compilerReleaseHelperFactory[architecture] == nullptr || compilerReleaseHelperFactory[architecture][release] == nullptr);
    auto createFunction = compilerReleaseHelperFactory[architecture][release];
    return createFunction(hardwareIpVersion);
}

void CompilerReleaseHelper::getKernelFp16AtomicCapabilities(uint32_t &fp16Caps) const {
    fp16Caps = (0u | FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps);
    fp16Caps |= this->getAdditionalFp16Caps();
}

bool CompilerReleaseHelper::isAvailableSemaphore64(const HardwareInfo &hwInfo) const {
    if (debugManager.flags.Enable64BitSemaphore.get() != -1) {
        return debugManager.flags.Enable64BitSemaphore.get() == 1;
    }

    if (!hwInfo.featureTable.flags.ftrHwSemaphore64) {
        return false;
    }

    return hwInfo.caps.availableSemaphore64;
}

} // namespace NEO

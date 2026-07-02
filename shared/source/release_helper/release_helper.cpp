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
    UNRECOVERABLE_IF(releaseHelperFactory[architecture] == nullptr || releaseHelperFactory[architecture][release] == nullptr);
    auto createFunction = releaseHelperFactory[architecture][release];
    return createFunction(hardwareIpVersion);
}

void ReleaseHelper::getKernelFp16AtomicCapabilities(uint32_t &fp16Caps) const {
    fp16Caps = (0u | FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps);
    fp16Caps |= this->getAdditionalFp16Caps();
}

} // namespace NEO

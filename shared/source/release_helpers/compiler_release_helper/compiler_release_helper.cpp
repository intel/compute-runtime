/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper.h"

#include "shared/source/helpers/debug_helpers.h"
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

} // namespace NEO

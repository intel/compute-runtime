/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper.h"

namespace NEO {

std::unique_ptr<CompilerReleaseHelper> CompilerReleaseHelper::create(HardwareIpVersion hardwareIpVersion) {
    auto architecture = hardwareIpVersion.architecture;
    auto release = hardwareIpVersion.release;
    if (compilerReleaseHelperFactory[architecture] == nullptr || compilerReleaseHelperFactory[architecture][release] == nullptr) {
        return nullptr;
    }
    auto createFunction = compilerReleaseHelperFactory[architecture][release];
    return createFunction(hardwareIpVersion);
}

} // namespace NEO

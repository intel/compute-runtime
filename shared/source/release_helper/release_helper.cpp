/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"

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

} // namespace NEO
/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"

namespace NEO {

bool Device::isBlitSplitEnabled() {
    return true;
}

bool Device::isInitDeviceWithFirstSubmissionEnabled() {
    return true;
}

} // namespace NEO

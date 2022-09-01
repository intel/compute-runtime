/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"

namespace NEO {

bool Device::isBlitSplitEnabled() {
    return true;
}

} // namespace NEO

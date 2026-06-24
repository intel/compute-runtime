/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/gtpin/gtpin_notify.h"

#include "opencl/source/platform/platform.h"

namespace NEO {

void gtPinTryNotifyInit() {
    if (platformsImpl->empty()) {
        return;
    }
    auto &pPlatform = *(*platformsImpl)[0];
    pPlatform.tryNotifyGtpinInit();
}

} // namespace NEO

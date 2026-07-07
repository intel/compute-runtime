/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/gtpin/leo_gtpin_notify.h"

#include "level_zero/api/opencl/source/platform/leo_platform.h"

namespace NEO {
namespace LEO {

void gtPinTryNotifyInit() {
    if (platformsImpl == nullptr || platformsImpl->empty()) {
        return;
    }
    auto &platform = *(*platformsImpl)[0];
    platform.tryNotifyGtpinInit();
}

} // namespace LEO
} // namespace NEO

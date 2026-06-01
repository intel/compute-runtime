/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/api/leo_forwarding.h"
#include "opencl/source/platform/platform.h"

namespace NEO {
volatile bool wasPlatformTeardownCalled = false;

void globalPlatformSetup() {
    platformsImpl = new std::vector<std::unique_ptr<Platform>>;
    leoSetup();
}

void globalPlatformTeardown(bool processTermination) {
    wasPlatformTeardownCalled = true;
    if (processTermination) {
        for (auto &platform : *platformsImpl) {
            platform->devicesCleanup(processTermination);
        }
        return;
    }
    leoTeardown();
    delete platformsImpl;
    platformsImpl = nullptr;
}
} // namespace NEO

/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/api/leo_forwarding.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/sharings/sharing_factory.h"

namespace NEO {
volatile bool wasPlatformTeardownCalled = false;

void globalPlatformSetup() {
    platformsImpl = new std::vector<std::unique_ptr<Platform>>;
    leoPlatformEntries = new std::vector<LeoPlatformEntry>;
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
    delete leoPlatformEntries;
    leoPlatformEntries = nullptr;
    delete platformsImpl;
    platformsImpl = nullptr;
    leoTeardown();
    SharingFactory::clearSharingBuilders();
}
} // namespace NEO

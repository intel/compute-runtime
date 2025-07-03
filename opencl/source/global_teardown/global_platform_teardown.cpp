/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/platform/platform.h"

namespace NEO {
volatile bool wasPlatformTeardownCalled = false;

void globalPlatformSetup() {
    platformsImpl = new std::vector<std::unique_ptr<Platform>>;
}

void globalPlatformTeardown(bool processTermination) {
    wasPlatformTeardownCalled = true;
    if (processTermination) {
        /* When terminating process, clean up should be skipped according to the DllMain spec. */
        return;
    }
    delete platformsImpl;
    platformsImpl = nullptr;
}
} // namespace NEO

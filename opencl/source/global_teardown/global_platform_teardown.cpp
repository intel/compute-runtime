/*
 * Copyright (C) 2024 Intel Corporation
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

void globalPlatformTeardown() {
    delete platformsImpl;
    platformsImpl = nullptr;
    wasPlatformTeardownCalled = true;
}
} // namespace NEO

/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/global_teardown/global_platform_teardown.h"

namespace NEO {
void __attribute__((constructor)) platformsConstructor() {
    globalPlatformSetup();
}
void __attribute__((destructor)) platformsDestructor() {
    globalPlatformTeardown();
}
} // namespace NEO

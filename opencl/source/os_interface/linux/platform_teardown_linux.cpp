/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/platform/platform.h"

namespace NEO {
void __attribute__((destructor)) platformsDestructor() {
    delete platformsImpl;
    platformsImpl = nullptr;
}
} // namespace NEO

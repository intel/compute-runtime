/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/context/context.h"

namespace L0 {

void Context::initOpaqueHandleResourcesImpl() {
    // No-op for Windows - opaque handles use NT handles
}

} // namespace L0

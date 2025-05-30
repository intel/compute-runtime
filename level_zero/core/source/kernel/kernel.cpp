/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/kernel/kernel.h"

#include "neo_igfxfmid.h"

namespace L0 {

KernelAllocatorFn kernelFactory[IGFX_MAX_PRODUCT] = {};

} // namespace L0

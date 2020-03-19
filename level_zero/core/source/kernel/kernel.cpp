/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/kernel/kernel.h"

#include "igfxfmid.h"

namespace L0 {

KernelAllocatorFn kernelFactory[IGFX_MAX_PRODUCT] = {};

} // namespace L0

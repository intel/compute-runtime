/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/kernel/kernel.h"

#include "neo_igfxfmid.h"

namespace L0 {

KernelAllocatorFn kernelFactory[NEO::maxCoreEnumValue] = {};

} // namespace L0

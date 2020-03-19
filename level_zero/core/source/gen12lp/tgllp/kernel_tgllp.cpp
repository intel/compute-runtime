/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/kernel/kernel_hw.h"

namespace L0 {

static KernelPopulateFactory<IGFX_TIGERLAKE_LP, KernelHw<IGFX_GEN12LP_CORE>> populateTGLLP;

} // namespace L0

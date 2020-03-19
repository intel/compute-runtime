/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/kernel/kernel_hw.h"

namespace L0 {

static KernelPopulateFactory<IGFX_ICELAKE_LP, KernelHw<IGFX_GEN11_CORE>> populateICLLP;

} // namespace L0

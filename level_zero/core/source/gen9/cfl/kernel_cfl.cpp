/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/kernel_hw.h"

namespace L0 {

static KernelPopulateFactory<IGFX_COFFEELAKE, KernelHw<IGFX_GEN9_CORE>> populateCFL;

} // namespace L0

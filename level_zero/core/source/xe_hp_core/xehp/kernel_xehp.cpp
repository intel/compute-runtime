/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/kernel/kernel_hw.h"

namespace L0 {

static KernelPopulateFactory<IGFX_XE_HP_SDV, KernelHw<IGFX_XE_HP_CORE>> populateXEHP;

} // namespace L0

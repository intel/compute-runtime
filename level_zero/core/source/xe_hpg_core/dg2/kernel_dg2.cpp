/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/kernel/kernel_hw.h"

namespace L0 {

static KernelPopulateFactory<IGFX_DG2, KernelHw<IGFX_XE_HPG_CORE>> populateDG2;

} // namespace L0

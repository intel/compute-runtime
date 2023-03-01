/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/cmdlist/cmdlist_hw.h"

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
inline NEO::PreemptionMode CommandListCoreFamily<gfxCoreFamily>::obtainKernelPreemptionMode(Kernel *kernel) {
    return NEO::PreemptionMode::ThreadGroup;
}

} // namespace L0

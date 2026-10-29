/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
template <>
void GpgpuWalkerHelper<Family>::setSystolicModeEnable(Family::COMPUTE_WALKER *walkerCmd) {
    if (debugManager.flags.OverrideSystolicInComputeWalker.get() != -1) {
        walkerCmd->setSystolicModeEnable((debugManager.flags.OverrideSystolicInComputeWalker.get()));
    }
}

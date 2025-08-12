/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_base.h"

static auto gfxCore = IGFX_GEN12LP_CORE;

#include "shared/source/helpers/windows/gmm_callbacks.inl"

namespace NEO {
template struct DeviceCallbacks<Gen12LpFamily>;
template struct TTCallbacks<Gen12LpFamily>;
GmmCallbacksFactory<Gen12LpFamily> gmmCallbacksGen12lp;
} // namespace NEO

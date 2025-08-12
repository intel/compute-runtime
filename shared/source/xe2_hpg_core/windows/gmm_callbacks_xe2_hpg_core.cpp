/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe2_hpg_core/hw_cmds.h"

static auto gfxCore = IGFX_XE2_HPG_CORE;

#include "shared/source/helpers/windows/gmm_callbacks.inl"

namespace NEO {

template struct DeviceCallbacks<Xe2HpgCoreFamily>;
template struct TTCallbacks<Xe2HpgCoreFamily>;
GmmCallbacksFactory<Xe2HpgCoreFamily> gmmCallbacksXe2Hpg;

} // namespace NEO

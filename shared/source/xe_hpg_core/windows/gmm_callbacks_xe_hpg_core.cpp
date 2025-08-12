/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

static auto gfxCore = IGFX_XE_HPG_CORE;

#include "shared/source/helpers/windows/gmm_callbacks.inl"

namespace NEO {

template struct DeviceCallbacks<XeHpgCoreFamily>;
template struct TTCallbacks<XeHpgCoreFamily>;
GmmCallbacksFactory<XeHpgCoreFamily> gmmCallbacksXeHpg;

} // namespace NEO

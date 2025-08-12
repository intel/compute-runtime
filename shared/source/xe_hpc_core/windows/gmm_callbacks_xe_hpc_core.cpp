/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

static auto gfxCore = IGFX_XE_HPC_CORE;

#include "shared/source/helpers/windows/gmm_callbacks.inl"

namespace NEO {

template struct DeviceCallbacks<XeHpcCoreFamily>;
template struct TTCallbacks<XeHpcCoreFamily>;
GmmCallbacksFactory<XeHpcCoreFamily> gmmCallbacksXeHpc;

} // namespace NEO

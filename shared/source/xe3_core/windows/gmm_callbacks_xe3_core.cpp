/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3_core/hw_cmds_base.h"

static auto gfxCore = IGFX_XE3_CORE;

#include "shared/source/helpers/windows/gmm_callbacks.inl"

namespace NEO {

template struct DeviceCallbacks<Xe3CoreFamily>;
template struct TTCallbacks<Xe3CoreFamily>;
GmmCallbacksFactory<Xe3CoreFamily> gmmCallbacksXe3;

} // namespace NEO

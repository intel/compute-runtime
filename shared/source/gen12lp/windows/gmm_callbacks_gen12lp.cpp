/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_base.h"
#include "shared/source/helpers/windows/gmm_callbacks_tgllp_and_later.inl"

namespace NEO {
template struct DeviceCallbacks<TGLLPFamily>;
template struct TTCallbacks<TGLLPFamily>;
} // namespace NEO

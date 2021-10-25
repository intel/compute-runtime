/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen8/hw_cmds.h"
#include "shared/source/helpers/windows/gmm_callbacks.h"
#include "shared/source/helpers/windows/gmm_callbacks.inl"

namespace NEO {
template struct DeviceCallbacks<BDWFamily>;
template struct TTCallbacks<BDWFamily>;
} // namespace NEO

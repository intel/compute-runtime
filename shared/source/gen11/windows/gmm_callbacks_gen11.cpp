/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds_base.h"
#include "shared/source/helpers/windows/gmm_callbacks.h"
#include "shared/source/helpers/windows/gmm_callbacks.inl"

namespace NEO {
template struct DeviceCallbacks<Gen11Family>;
template struct TTCallbacks<Gen11Family>;
} // namespace NEO

/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hp_core/hw_cmds.h"

#include "opencl/source/helpers/windows/gmm_callbacks_tgllp_plus.inl"

namespace NEO {
template struct DeviceCallbacks<XeHpFamily>;
template struct TTCallbacks<XeHpFamily>;
} // namespace NEO

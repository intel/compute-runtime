/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/windows/gmm_callbacks_tgllp_and_later.inl"
#include "shared/source/xe2_hpg_core/hw_cmds.h"

namespace NEO {

template struct DeviceCallbacks<Xe2HpgCoreFamily>;
template struct TTCallbacks<Xe2HpgCoreFamily>;

} // namespace NEO

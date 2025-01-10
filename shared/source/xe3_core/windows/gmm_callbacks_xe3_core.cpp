/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/windows/gmm_callbacks_tgllp_and_later.inl"
#include "shared/source/xe3_core/hw_cmds_base.h"

namespace NEO {

template struct DeviceCallbacks<Xe3CoreFamily>;
template struct TTCallbacks<Xe3CoreFamily>;

} // namespace NEO

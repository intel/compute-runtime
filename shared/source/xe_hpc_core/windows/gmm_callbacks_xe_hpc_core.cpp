/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/windows/gmm_callbacks_tgllp_and_later.inl"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

namespace NEO {

template struct DeviceCallbacks<XE_HPC_COREFamily>;
template struct TTCallbacks<XE_HPC_COREFamily>;

} // namespace NEO

/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/state_base_address_xehp_and_later.inl"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

namespace NEO {
#include "shared/source/helpers/state_base_address_xe_hpg_core_and_later.inl"
template struct StateBaseAddressHelper<XE_HPC_COREFamily>;
} // namespace NEO

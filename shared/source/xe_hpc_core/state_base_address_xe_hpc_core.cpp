/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/state_base_address_xehp_and_later.inl"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

namespace NEO {
template struct StateBaseAddressHelper<XeHpcCoreFamily>;
} // namespace NEO

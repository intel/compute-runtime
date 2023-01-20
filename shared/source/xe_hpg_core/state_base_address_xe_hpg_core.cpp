/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/state_base_address_xehp_and_later.inl"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

namespace NEO {
#include "shared/source/helpers/state_base_address_xe_hpg_core_and_later.inl"
template struct StateBaseAddressHelper<XeHpgCoreFamily>;
} // namespace NEO

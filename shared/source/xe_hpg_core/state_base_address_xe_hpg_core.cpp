/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/state_base_address_xehp_and_later.inl"

namespace NEO {
#include "shared/source/helpers/state_base_address_xe_hpg_core_and_later.inl"
template struct StateBaseAddressHelper<XE_HPG_COREFamily>;
} // namespace NEO

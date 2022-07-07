/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds_base.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/helpers/state_base_address_bdw_and_later.inl"
#include "shared/source/helpers/state_base_address_skl.inl"

namespace NEO {
template struct StateBaseAddressHelper<SKLFamily>;
}

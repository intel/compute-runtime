/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_base.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/helpers/state_base_address_icllp_and_later.inl"

namespace NEO {
template struct StateBaseAddressHelper<Gen12LpFamily>;
}

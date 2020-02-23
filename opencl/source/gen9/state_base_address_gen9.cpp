/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "helpers/state_base_address.h"
#include "helpers/state_base_address_bdw_plus.inl"

namespace NEO {
template struct StateBaseAddressHelper<SKLFamily>;
}

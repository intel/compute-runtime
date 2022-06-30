/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/state_base_address.h"
#include "shared/source/helpers/state_base_address_bdw_and_later.inl"
#include "shared/source/helpers/state_base_address_icllp_and_later.inl"

namespace NEO {
template struct StateBaseAddressHelper<TGLLPFamily>;
}

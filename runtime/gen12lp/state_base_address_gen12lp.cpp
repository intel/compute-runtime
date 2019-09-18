/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/state_base_address.h"
#include "runtime/helpers/state_base_address_bdw_plus.inl"

namespace NEO {
template struct StateBaseAddressHelper<TGLLPFamily>;
}

/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/state_base_address_xehp_and_later.inl"

namespace NEO {

template <>
void StateBaseAddressHelper<XeHpFamily>::appendExtraCacheSettings(STATE_BASE_ADDRESS *stateBaseAddress, GmmHelper *gmmHelper) {
}

template struct StateBaseAddressHelper<XeHpFamily>;
} // namespace NEO

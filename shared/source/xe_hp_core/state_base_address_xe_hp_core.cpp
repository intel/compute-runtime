/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/state_base_address_xehp_plus.inl"

namespace NEO {

template <>
void StateBaseAddressHelper<XeHpFamily>::appendExtraCacheSettings(STATE_BASE_ADDRESS *stateBaseAddress, GmmHelper *gmmHelper) {
    if (DebugManager.flags.ForceStatelessL1CachingPolicy.get() != -1) {
        stateBaseAddress->setL1CachePolicyL1CacheControl(static_cast<typename STATE_BASE_ADDRESS::L1_CACHE_POLICY>(DebugManager.flags.ForceStatelessL1CachingPolicy.get()));
    }
}

template struct StateBaseAddressHelper<XeHpFamily>;
} // namespace NEO

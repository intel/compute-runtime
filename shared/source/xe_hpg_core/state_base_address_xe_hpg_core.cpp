/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/state_base_address_xehp_and_later.inl"

namespace NEO {

template <>
void StateBaseAddressHelper<XE_HPG_COREFamily>::appendExtraCacheSettings(STATE_BASE_ADDRESS *stateBaseAddress, GmmHelper *gmmHelper) {
    stateBaseAddress->setL1CachePolicyL1CacheControl(STATE_BASE_ADDRESS::L1_CACHE_POLICY_WBP);

    if (DebugManager.flags.ForceStatelessL1CachingPolicy.get() != -1) {
        stateBaseAddress->setL1CachePolicyL1CacheControl(static_cast<typename STATE_BASE_ADDRESS::L1_CACHE_POLICY>(DebugManager.flags.ForceStatelessL1CachingPolicy.get()));
    }
}

template struct StateBaseAddressHelper<XE_HPG_COREFamily>;
} // namespace NEO

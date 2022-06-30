/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::appendExtraCacheSettings(STATE_BASE_ADDRESS *stateBaseAddress, const HardwareInfo *hwInfo) {
    auto hwInfoConfig = HwInfoConfig::get(hwInfo->platform.eProductFamily);
    auto cachePolicy = hwInfoConfig->getL1CachePolicy();
    stateBaseAddress->setL1CachePolicyL1CacheControl(static_cast<typename STATE_BASE_ADDRESS::L1_CACHE_POLICY>(cachePolicy));

    if (DebugManager.flags.ForceStatelessL1CachingPolicy.get() != -1) {
        stateBaseAddress->setL1CachePolicyL1CacheControl(static_cast<typename STATE_BASE_ADDRESS::L1_CACHE_POLICY>(DebugManager.flags.ForceStatelessL1CachingPolicy.get()));
    }
}

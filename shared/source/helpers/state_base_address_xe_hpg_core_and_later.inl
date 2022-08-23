/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::appendExtraCacheSettings(StateBaseAddressHelperArgs<GfxFamily> &args) {
    auto hwInfoConfig = HwInfoConfig::get(args.gmmHelper->getHardwareInfo()->platform.eProductFamily);
    auto cachePolicy = hwInfoConfig->getL1CachePolicy(args.isDebuggerActive);
    args.stateBaseAddressCmd->setL1CachePolicyL1CacheControl(static_cast<typename STATE_BASE_ADDRESS::L1_CACHE_POLICY>(cachePolicy));

    if (DebugManager.flags.ForceStatelessL1CachingPolicy.get() != -1 &&
        DebugManager.flags.ForceAllResourcesUncached.get() == false) {
        args.stateBaseAddressCmd->setL1CachePolicyL1CacheControl(static_cast<typename STATE_BASE_ADDRESS::L1_CACHE_POLICY>(DebugManager.flags.ForceStatelessL1CachingPolicy.get()));
    }
}

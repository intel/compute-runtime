/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/helpers/state_base_address_tgllp_and_later.inl"

namespace NEO {

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::appendStateBaseAddressParameters(
    StateBaseAddressHelperArgs<GfxFamily> &args) {

    if (!args.useGlobalHeapsBaseAddress) {
        if (args.bindlessSurfaceStateBaseAddress != 0) {
            args.stateBaseAddressCmd->setBindlessSurfaceStateBaseAddress(args.bindlessSurfaceStateBaseAddress);
            args.stateBaseAddressCmd->setBindlessSurfaceStateBaseAddressModifyEnable(true);
            const auto surfaceStateCount = getMaxBindlessSurfaceStates();
            args.stateBaseAddressCmd->setBindlessSurfaceStateSize(surfaceStateCount);
        } else if (args.ssh) {
            args.stateBaseAddressCmd->setBindlessSurfaceStateBaseAddressModifyEnable(true);
            args.stateBaseAddressCmd->setBindlessSurfaceStateBaseAddress(args.ssh->getHeapGpuBase());
            uint32_t size = uint32_t(args.ssh->getMaxAvailableSpace() / 64) - 1;
            args.stateBaseAddressCmd->setBindlessSurfaceStateSize(size);
        }
    }

    args.stateBaseAddressCmd->setBindlessSamplerStateBaseAddressModifyEnable(true);

    auto l3CacheOnPolicy = GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER;

    if (args.gmmHelper != nullptr) {
        args.stateBaseAddressCmd->setBindlessSurfaceStateMemoryObjectControlState(args.gmmHelper->getMOCS(l3CacheOnPolicy));
        args.stateBaseAddressCmd->setBindlessSamplerStateMemoryObjectControlState(args.gmmHelper->getMOCS(l3CacheOnPolicy));
    }

    StateBaseAddressHelper<GfxFamily>::appendIohParameters(args);
}

template <typename GfxFamily>
uint32_t StateBaseAddressHelper<GfxFamily>::getMaxBindlessSurfaceStates() {
    return (1 << 20) - 1;
}

} // namespace NEO

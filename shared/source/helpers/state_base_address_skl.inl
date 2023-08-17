/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/state_base_address.h"

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

    StateBaseAddressHelper<GfxFamily>::appendIohParameters(args);
}

template <typename GfxFamily>
uint32_t StateBaseAddressHelper<GfxFamily>::getMaxBindlessSurfaceStates() {
    return (1 << 20) - 1;
}
} // namespace NEO

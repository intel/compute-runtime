/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/state_base_address.h"

namespace NEO {

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::appendStateBaseAddressParameters(
    StateBaseAddressHelperArgs<GfxFamily> &args,
    bool overrideBindlessSurfaceStateBase) {

    if (overrideBindlessSurfaceStateBase && args.ssh) {
        args.stateBaseAddressCmd->setBindlessSurfaceStateBaseAddressModifyEnable(true);
        args.stateBaseAddressCmd->setBindlessSurfaceStateBaseAddress(args.ssh->getHeapGpuBase());
        uint32_t size = uint32_t(args.ssh->getMaxAvailableSpace() / 64) - 1;
        args.stateBaseAddressCmd->setBindlessSurfaceStateSize(size);
    }
}

template <typename GfxFamily>
uint32_t StateBaseAddressHelper<GfxFamily>::getMaxBindlessSurfaceStates() {
    return (1 << 20) - 1;
}
} // namespace NEO

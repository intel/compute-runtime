/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/state_base_address.h"

namespace NEO {

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::appendStateBaseAddressParameters(
    STATE_BASE_ADDRESS *stateBaseAddress,
    const IndirectHeap *ssh,
    bool setGeneralStateBaseAddress,
    uint64_t internalHeapBase,
    GmmHelper *gmmHelper,
    bool isMultiOsContextCapable) {

    if (ssh) {
        stateBaseAddress->setBindlessSurfaceStateBaseAddressModifyEnable(true);
        stateBaseAddress->setBindlessSurfaceStateBaseAddress(ssh->getHeapGpuBase());
        uint32_t size = uint32_t(ssh->getMaxAvailableSpace() / 64) - 1;
        stateBaseAddress->setBindlessSurfaceStateSize(size);
    }
}

} // namespace NEO

/*
 * Copyright (C) 2020-2021 Intel Corporation
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
    uint64_t indirectObjectHeapBaseAddress,
    GmmHelper *gmmHelper,
    bool isMultiOsContextCapable,
    MemoryCompressionState memoryCompressionState,
    bool overrideBindlessSurfaceStateBase,
    bool useGlobalAtomics,
    bool areMultipleSubDevicesInContext) {

    if (overrideBindlessSurfaceStateBase && ssh) {
        stateBaseAddress->setBindlessSurfaceStateBaseAddressModifyEnable(true);
        stateBaseAddress->setBindlessSurfaceStateBaseAddress(ssh->getHeapGpuBase());
        uint32_t size = uint32_t(ssh->getMaxAvailableSpace() / 64) - 1;
        stateBaseAddress->setBindlessSurfaceStateSize(size);
    }

    stateBaseAddress->setBindlessSamplerStateBaseAddressModifyEnable(true);

    auto l3CacheOnPolicy = GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER;

    if (gmmHelper != nullptr) {
        stateBaseAddress->setBindlessSurfaceStateMemoryObjectControlState(gmmHelper->getMOCS(l3CacheOnPolicy));
        stateBaseAddress->setBindlessSamplerStateMemoryObjectControlState(gmmHelper->getMOCS(l3CacheOnPolicy));
    }

    appendExtraCacheSettings(stateBaseAddress, gmmHelper);
}

template <typename GfxFamily>
uint32_t StateBaseAddressHelper<GfxFamily>::getMaxBindlessSurfaceStates() {
    return (1 << 20) - 1;
}

} // namespace NEO

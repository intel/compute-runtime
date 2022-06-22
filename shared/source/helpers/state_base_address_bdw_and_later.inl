/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/state_base_address_base.inl"

namespace NEO {

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::programBindingTableBaseAddress(LinearStream &commandStream, const IndirectHeap &ssh, GmmHelper *gmmHelper) {
}

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::appendIohParameters(typename GfxFamily::STATE_BASE_ADDRESS *stateBaseAddress, const IndirectHeap *ioh, bool useGlobalHeapsBaseAddress, uint64_t indirectObjectHeapBaseAddress) {
    if (useGlobalHeapsBaseAddress) {
        stateBaseAddress->setIndirectObjectBaseAddressModifyEnable(true);
        stateBaseAddress->setIndirectObjectBufferSizeModifyEnable(true);
        stateBaseAddress->setIndirectObjectBaseAddress(indirectObjectHeapBaseAddress);
        stateBaseAddress->setIndirectObjectBufferSize(MemoryConstants::sizeOf4GBinPageEntities);
    } else if (ioh) {
        stateBaseAddress->setIndirectObjectBaseAddressModifyEnable(true);
        stateBaseAddress->setIndirectObjectBufferSizeModifyEnable(true);
        stateBaseAddress->setIndirectObjectBaseAddress(ioh->getHeapGpuBase());
        stateBaseAddress->setIndirectObjectBufferSize(ioh->getHeapSizeInPages());
    }
}

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::appendExtraCacheSettings(STATE_BASE_ADDRESS *stateBaseAddress, const HardwareInfo *hwInfo) {}

} // namespace NEO

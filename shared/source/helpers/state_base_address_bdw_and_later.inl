/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/state_base_address_base.inl"

namespace NEO {

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::programBindingTableBaseAddress(LinearStream &commandStream, uint64_t baseAddress, uint32_t sizeInPages, GmmHelper *gmmHelper) {
}

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::appendIohParameters(StateBaseAddressHelperArgs<GfxFamily> &args) {
    if (args.useGlobalHeapsBaseAddress) {
        args.stateBaseAddressCmd->setIndirectObjectBaseAddressModifyEnable(true);
        args.stateBaseAddressCmd->setIndirectObjectBufferSizeModifyEnable(true);
        args.stateBaseAddressCmd->setIndirectObjectBaseAddress(args.indirectObjectHeapBaseAddress);
        args.stateBaseAddressCmd->setIndirectObjectBufferSize(MemoryConstants::sizeOf4GBinPageEntities);
    } else if (args.ioh) {
        args.stateBaseAddressCmd->setIndirectObjectBaseAddressModifyEnable(true);
        args.stateBaseAddressCmd->setIndirectObjectBufferSizeModifyEnable(true);
        args.stateBaseAddressCmd->setIndirectObjectBaseAddress(args.ioh->getHeapGpuBase());
        args.stateBaseAddressCmd->setIndirectObjectBufferSize(args.ioh->getHeapSizeInPages());
    }
}

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::appendExtraCacheSettings(StateBaseAddressHelperArgs<GfxFamily> &args) {}

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::programStateBaseAddressIntoCommandStream(StateBaseAddressHelperArgs<GfxFamily> &args,
                                                                                 NEO::LinearStream &commandStream) {
    programStateBaseAddressIntoCommandStreamBase(args, commandStream);
}

} // namespace NEO

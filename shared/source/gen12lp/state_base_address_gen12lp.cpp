/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_base.h"
#include "shared/source/helpers/state_base_address_base.inl"

namespace NEO {
using Family = Gen12LpFamily;
template <>
uint32_t StateBaseAddressHelper<Family>::getMaxBindlessSurfaceStates() {
    return (1 << 20) - 1;
}

template <>
void StateBaseAddressHelper<Family>::programBindingTableBaseAddress(LinearStream &commandStream, uint64_t baseAddress, uint32_t sizeInPages, GmmHelper *gmmHelper) {
}

template <>
void StateBaseAddressHelper<Family>::appendIohParameters(StateBaseAddressHelperArgs<Family> &args) {
    if (args.sbaProperties) {
        if (args.sbaProperties->indirectObjectBaseAddress.value != StreamProperty64::initValue) {
            auto baseAddress = static_cast<uint64_t>(args.sbaProperties->indirectObjectBaseAddress.value);
            UNRECOVERABLE_IF(!args.gmmHelper);
            args.stateBaseAddressCmd->setIndirectObjectBaseAddress(args.gmmHelper->decanonize(baseAddress));
            args.stateBaseAddressCmd->setIndirectObjectBaseAddressModifyEnable(true);
            args.stateBaseAddressCmd->setIndirectObjectBufferSizeModifyEnable(true);
            args.stateBaseAddressCmd->setIndirectObjectBufferSize(static_cast<uint32_t>(args.sbaProperties->indirectObjectSize.value));
        }
    } else if (args.useGlobalHeapsBaseAddress) {
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

template <>
void StateBaseAddressHelper<Family>::appendExtraCacheSettings(StateBaseAddressHelperArgs<Family> &args) {}

template <>
void StateBaseAddressHelper<Family>::appendStateBaseAddressParameters(
    StateBaseAddressHelperArgs<Family> &args) {

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

    StateBaseAddressHelper<Family>::appendIohParameters(args);
}

template struct StateBaseAddressHelper<Gen12LpFamily>;
} // namespace NEO

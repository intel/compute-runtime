/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/state_base_address_base.inl"

namespace NEO {

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::appendStateBaseAddressParameters(
    STATE_BASE_ADDRESS *stateBaseAddress,
    const IndirectHeap *ssh,
    uint64_t internalHeapBase,
    GmmHelper *gmmHelper,
    bool isMultiOsContextCapable) {
}

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::programBindingTableBaseAddress(LinearStream &commandStream, const IndirectHeap &ssh,
                                                                       size_t stateBaseAddressCmdOffset, GmmHelper *gmmHelper) {

    auto sbaCommand = static_cast<STATE_BASE_ADDRESS *>(ptrOffset(commandStream.getCpuBase(), stateBaseAddressCmdOffset));
    UNRECOVERABLE_IF(sbaCommand->getSurfaceStateBaseAddress() != ssh.getGraphicsAllocation()->getGpuAddress());
}

} // namespace NEO

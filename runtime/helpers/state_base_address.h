/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstddef>
#include <cstdint>

namespace OCLRT {

class GmmHelper;
class IndirectHeap;
class LinearStream;

template <typename GfxFamily>
struct StateBaseAddressHelper {
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;

    static void programStateBaseAddress(
        LinearStream &commandStream,
        const IndirectHeap &dsh,
        const IndirectHeap &ioh,
        const IndirectHeap &ssh,
        uint64_t generalStateBase,
        uint32_t statelessMocsIndex,
        uint64_t internalHeapBase,
        GmmHelper *gmmHelper);

    static void appendStateBaseAddressParameters(
        STATE_BASE_ADDRESS *stateBaseAddress,
        const IndirectHeap &dsh,
        const IndirectHeap &ioh,
        const IndirectHeap &ssh,
        uint64_t generalStateBase,
        uint64_t internalHeapBase,
        GmmHelper *gmmHelper);

    static void programBindingTableBaseAddress(LinearStream &commandStream, const IndirectHeap &ssh, size_t stateBaseAddressCmdOffset,
                                               GmmHelper *gmmHelper);
};
} // namespace OCLRT

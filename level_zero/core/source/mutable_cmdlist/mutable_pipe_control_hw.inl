/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_pipe_control_hw.h"

namespace L0::MCL {

template <typename GfxFamily>
void MutablePipeControlHw<GfxFamily>::setPostSyncAddress(GpuAddress postSyncAddress) {
    using Address = typename PipeControl::ADDRESS;
    constexpr uint32_t addressLowIndex = 2;

    postSyncAddress = (postSyncAddress >> Address::ADDRESS_BIT_SHIFT << Address::ADDRESS_BIT_SHIFT);

    auto pipeControlCmd = reinterpret_cast<PipeControl *>(pipeControl);
    pipeControlCmd->getRawData(addressLowIndex) = getLowPart(postSyncAddress);
    pipeControlCmd->setAddressHigh(getHighPart(postSyncAddress));
}

} // namespace L0::MCL

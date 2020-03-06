/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/interlocked_max.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/os_interface/os_context.h"

#include "level_zero/core/source/cmdlist.h"
#include "level_zero/core/source/cmdqueue_hw.h"
#include "level_zero/core/source/device.h"
#include "level_zero/core/source/fence.h"
#include "level_zero/tools/source/metrics/metric.h"

#include <limits>

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programGeneralStateBaseAddress(uint64_t gsba, NEO::LinearStream &commandStream) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

    PIPE_CONTROL *pcCmd = commandStream.getSpaceForCmd<PIPE_CONTROL>();
    *pcCmd = GfxFamily::cmdInitPipeControl;

    pcCmd->setTextureCacheInvalidationEnable(true);
    pcCmd->setDcFlushEnable(true);
    pcCmd->setCommandStreamerStallEnable(true);

    auto gmmHelper = device->getNEODevice()->getGmmHelper();

    NEO::StateBaseAddressHelper<GfxFamily>::programStateBaseAddress(commandStream,
                                                                    nullptr,
                                                                    nullptr,
                                                                    nullptr,
                                                                    gsba,
                                                                    true,
                                                                    (device->getMOCS(true, false) >> 1),
                                                                    device->getDriverHandle()->getMemoryManager()->getInternalHeapBaseAddress(0),
                                                                    true,
                                                                    gmmHelper,
                                                                    false);

    gsbaInit = true;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateStateBaseAddressCmdSize() {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

    constexpr size_t size = sizeof(STATE_BASE_ADDRESS) + sizeof(PIPE_CONTROL);
    return size;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::handleScratchSpace(NEO::ResidencyContainer &residency,
                                                       NEO::ScratchSpaceController *scratchController,
                                                       bool &gsbaState, bool &frontEndState) {

    if (commandQueuePerThreadScratchSize > 0) {
        scratchController->setRequiredScratchSpace(nullptr, commandQueuePerThreadScratchSize, 0u, csr->peekTaskCount(),
                                                   csr->getOsContext(), gsbaState, frontEndState);
        auto scratchAllocation = scratchController->getScratchSpaceAllocation();
        residency.push_back(scratchAllocation);
    }
}

} // namespace L0

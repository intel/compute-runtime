/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/interlocked_max.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/os_interface/os_context.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_hw.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/fence/fence.h"
#include "level_zero/tools/source/metrics/metric.h"

#include <limits>

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programGeneralStateBaseAddress(uint64_t gsba, bool useLocalMemoryForIndirectHeap, NEO::LinearStream &commandStream) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

    PIPE_CONTROL *pcCmd = commandStream.getSpaceForCmd<PIPE_CONTROL>();
    PIPE_CONTROL cmd = GfxFamily::cmdInitPipeControl;

    cmd.setTextureCacheInvalidationEnable(true);
    cmd.setDcFlushEnable(true);
    cmd.setCommandStreamerStallEnable(true);

    *pcCmd = cmd;

    NEO::Device *neoDevice = device->getNEODevice();
    NEO::EncodeWA<GfxFamily>::encodeAdditionalPipelineSelect(*neoDevice, commandStream, true);

    auto pSbaCmd = static_cast<STATE_BASE_ADDRESS *>(commandStream.getSpace(sizeof(STATE_BASE_ADDRESS)));
    STATE_BASE_ADDRESS sbaCmd;

    NEO::StateBaseAddressHelper<GfxFamily>::programStateBaseAddress(&sbaCmd,
                                                                    nullptr,
                                                                    nullptr,
                                                                    nullptr,
                                                                    gsba,
                                                                    true,
                                                                    (device->getMOCS(true, false) >> 1),
                                                                    neoDevice->getMemoryManager()->getInternalHeapBaseAddress(device->getRootDeviceIndex(), useLocalMemoryForIndirectHeap),
                                                                    true,
                                                                    neoDevice->getGmmHelper(),
                                                                    false);
    *pSbaCmd = sbaCmd;
    gsbaInit = true;

    if (device->getL0Debugger()) {

        NEO::Debugger::SbaAddresses sbaAddresses = {};
        sbaAddresses.BindlessSurfaceStateBaseAddress = sbaCmd.getBindlessSurfaceStateBaseAddress();
        sbaAddresses.DynamicStateBaseAddress = sbaCmd.getDynamicStateBaseAddress();
        sbaAddresses.GeneralStateBaseAddress = sbaCmd.getGeneralStateBaseAddress();
        sbaAddresses.IndirectObjectBaseAddress = sbaCmd.getIndirectObjectBaseAddress();
        sbaAddresses.InstructionBaseAddress = sbaCmd.getInstructionBaseAddress();
        sbaAddresses.SurfaceStateBaseAddress = sbaCmd.getSurfaceStateBaseAddress();

        device->getL0Debugger()->programSbaTrackingCommands(commandStream, sbaAddresses);
    }

    NEO::EncodeWA<GfxFamily>::encodeAdditionalPipelineSelect(*device->getNEODevice(), commandStream, false);
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateStateBaseAddressCmdSize() {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

    size_t size = sizeof(STATE_BASE_ADDRESS) + sizeof(PIPE_CONTROL) + NEO::EncodeWA<GfxFamily>::getAdditionalPipelineSelectSize(*device->getNEODevice());

    if (device->getL0Debugger() != nullptr) {
        size += device->getL0Debugger()->getSbaTrackingCommandsSize(1);
    }
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

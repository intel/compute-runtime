/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/helpers/populate_factory.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/command_queue/command_queue_hw_base.inl"
#include "opencl/source/command_queue/enqueue_resource_barrier.h"

namespace NEO {

typedef Gen12LpFamily Family;
static auto gfxCore = IGFX_GEN12LP_CORE;

template <typename GfxFamily>
void CommandQueueHw<GfxFamily>::submitCacheFlush(Surface **surfaces,
                                                 size_t numSurfaces,
                                                 LinearStream *commandStream,
                                                 uint64_t postSyncAddress) {
}

template <typename GfxFamily>
bool CommandQueueHw<GfxFamily>::isCacheFlushCommand(uint32_t commandType) const {
    return false;
}

template <>
void populateFactoryTable<CommandQueueHw<Family>>() {
    extern CommandQueueCreateFunc commandQueueFactory[NEO::maxCoreEnumValue];
    commandQueueFactory[gfxCore] = CommandQueueHw<Family>::create;
}

template class CommandQueueHw<Family>;

} // namespace NEO

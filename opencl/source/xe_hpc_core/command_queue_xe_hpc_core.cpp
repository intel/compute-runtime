/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/populate_factory.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/command_queue/enqueue_resource_barrier.h"

namespace NEO {

using Family = XE_HPC_COREFamily;
static auto gfxCore = IGFX_XE_HPC_CORE;
} // namespace NEO

#include "opencl/source/command_queue/command_queue_hw_xehp_and_later.inl"

namespace NEO {
template <>
void populateFactoryTable<CommandQueueHw<Family>>() {
    extern CommandQueueCreateFunc commandQueueFactory[IGFX_MAX_CORE];
    commandQueueFactory[gfxCore] = CommandQueueHw<Family>::create;
}

template <>
bool CommandQueueHw<Family>::isCacheFlushForBcsRequired() const {
    if (DebugManager.flags.ForceCacheFlushForBcs.get() != -1) {
        return !!DebugManager.flags.ForceCacheFlushForBcs.get();
    }
    return false;
}
} // namespace NEO

template class NEO::CommandQueueHw<NEO::Family>;

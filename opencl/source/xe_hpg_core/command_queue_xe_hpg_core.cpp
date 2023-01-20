/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/populate_factory.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/command_queue/enqueue_resource_barrier.h"

namespace NEO {

using Family = XeHpgCoreFamily;
static auto gfxCore = IGFX_XE_HPG_CORE;
} // namespace NEO

#include "opencl/source/command_queue/command_queue_hw_xehp_and_later.inl"

namespace NEO {
template <>
void populateFactoryTable<CommandQueueHw<Family>>() {
    extern CommandQueueCreateFunc commandQueueFactory[IGFX_MAX_CORE];
    commandQueueFactory[gfxCore] = CommandQueueHw<Family>::create;
}

} // namespace NEO

template class NEO::CommandQueueHw<NEO::Family>;

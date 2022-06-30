/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/populate_factory.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/xe_hp_core/hw_cmds.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/command_queue/enqueue_resource_barrier.h"

namespace NEO {

using Family = XeHpFamily;
static auto gfxCore = IGFX_XE_HP_CORE;
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

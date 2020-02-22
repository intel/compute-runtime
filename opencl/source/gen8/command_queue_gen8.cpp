/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/memory_manager/unified_memory_manager.h"

#include "command_queue/command_queue_hw.h"
#include "command_queue/command_queue_hw_bdw_plus.inl"
#include "command_queue/enqueue_resource_barrier.h"
#include "enqueue_init_dispatch_globals.h"

namespace NEO {

typedef BDWFamily Family;
static auto gfxCore = IGFX_GEN8_CORE;

template class CommandQueueHw<Family>;

template <>
void populateFactoryTable<CommandQueueHw<Family>>() {
    extern CommandQueueCreateFunc commandQueueFactory[IGFX_MAX_CORE];
    commandQueueFactory[gfxCore] = CommandQueueHw<Family>::create;
}

} // namespace NEO

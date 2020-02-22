/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/command_queue/command_queue_hw_bdw_plus.inl"

#include "command_queue_helpers_gen12lp.inl"
#include "enqueue_init_dispatch_globals.h"

namespace NEO {

typedef TGLLPFamily Family;
static auto gfxCore = IGFX_GEN12LP_CORE;

template <>
void populateFactoryTable<CommandQueueHw<Family>>() {
    extern CommandQueueCreateFunc commandQueueFactory[IGFX_MAX_CORE];
    commandQueueFactory[gfxCore] = CommandQueueHw<Family>::create;
}

template class CommandQueueHw<Family>;

} // namespace NEO

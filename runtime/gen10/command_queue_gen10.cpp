/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/svm_memory_manager.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_queue/command_queue_hw.inl"

namespace OCLRT {

typedef CNLFamily Family;
static auto gfxCore = IGFX_GEN10_CORE;

template <>
void populateFactoryTable<CommandQueueHw<Family>>() {
    extern CommandQueueCreateFunc commandQueueFactory[IGFX_MAX_CORE];
    commandQueueFactory[gfxCore] = CommandQueueHw<Family>::create;
}
} // namespace OCLRT

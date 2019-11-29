/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gen12lp/hw_cmds.h"
#include "runtime/device_queue/device_queue_hw.h"
#include "runtime/device_queue/device_queue_hw_bdw_plus.inl"
#include "runtime/device_queue/device_queue_hw_profiling.inl"

namespace NEO {
typedef TGLLPFamily Family;
static auto gfxCore = IGFX_GEN12LP_CORE;

template <>
void populateFactoryTable<DeviceQueueHw<Family>>() {
    extern DeviceQueueCreateFunc deviceQueueFactory[IGFX_MAX_CORE];
    deviceQueueFactory[gfxCore] = DeviceQueueHw<Family>::create;
}

template <>
size_t DeviceQueueHw<Family>::getWaCommandsSize() { return 0; }

template <>
void DeviceQueueHw<Family>::addArbCheckCmdWa() {}

template <>
void DeviceQueueHw<Family>::addMiAtomicCmdWa(uint64_t atomicOpPlaceholder) {}

template <>
void DeviceQueueHw<Family>::addLriCmdWa(bool setArbCheck) {}

template <>
void DeviceQueueHw<Family>::addPipeControlCmdWa(bool isNoopCmd) {}

template class DeviceQueueHw<Family>;
} // namespace NEO

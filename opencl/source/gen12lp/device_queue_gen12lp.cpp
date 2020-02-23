/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "opencl/source/device_queue/device_queue_hw.h"
#include "opencl/source/device_queue/device_queue_hw_bdw_plus.inl"
#include "opencl/source/device_queue/device_queue_hw_profiling.inl"

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

/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/device_queue/device_queue_hw.h"
#include "runtime/device_queue/device_queue_hw_bdw_plus.inl"
#include "runtime/device_queue/device_queue_hw_profiling.inl"
#include "runtime/gen10/hw_cmds.h"

namespace NEO {
typedef CNLFamily Family;
static auto gfxCore = IGFX_GEN10_CORE;

static const size_t csPrefetchSizeWA = 100;

template <>
size_t DeviceQueueHw<Family>::getCSPrefetchSize() {
    return 512 + csPrefetchSizeWA;
}

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

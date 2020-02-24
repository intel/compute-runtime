/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds.h"

#include "opencl/source/device_queue/device_queue_hw.h"
#include "opencl/source/device_queue/device_queue_hw_bdw_plus.inl"
#include "opencl/source/device_queue/device_queue_hw_profiling.inl"
#include "opencl/source/gen11/device_enqueue.h"

namespace NEO {
typedef ICLFamily Family;
static auto gfxCore = IGFX_GEN11_CORE;

template <>
void populateFactoryTable<DeviceQueueHw<Family>>() {
    extern DeviceQueueCreateFunc deviceQueueFactory[IGFX_MAX_CORE];
    deviceQueueFactory[gfxCore] = DeviceQueueHw<Family>::create;
}

static const size_t csPrefetchSizeAlignementWa = SLB_SIZE_ALIGNEMENT_WA_GEN11;

template <>
size_t DeviceQueueHw<Family>::getCSPrefetchSize() {
    return 8 * MemoryConstants::cacheLineSize + csPrefetchSizeAlignementWa;
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

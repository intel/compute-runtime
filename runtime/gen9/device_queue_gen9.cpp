/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/device_queue/device_queue_hw.h"
#include "runtime/device_queue/device_queue_hw_bdw_plus.inl"
#include "runtime/device_queue/device_queue_hw_profiling.inl"
#include "runtime/gen9/hw_cmds.h"

namespace NEO {
typedef SKLFamily Family;
static auto gfxCore = IGFX_GEN9_CORE;

template <>
void populateFactoryTable<DeviceQueueHw<Family>>() {
    extern DeviceQueueCreateFunc deviceQueueFactory[IGFX_MAX_CORE];
    deviceQueueFactory[gfxCore] = DeviceQueueHw<Family>::create;
}

template <>
size_t DeviceQueueHw<Family>::getWaCommandsSize() {
    return sizeof(Family::MI_ARB_CHECK) +
           sizeof(Family::MI_ATOMIC) +
           sizeof(Family::MI_ARB_CHECK) +
           sizeof(Family::PIPE_CONTROL) +
           sizeof(Family::PIPE_CONTROL);
}

template <>
void DeviceQueueHw<Family>::addArbCheckCmdWa() {
    auto arbCheck = slbCS.getSpaceForCmd<Family::MI_ARB_CHECK>();
    *arbCheck = Family::cmdInitArbCheck;
}

template <>
void DeviceQueueHw<Family>::addMiAtomicCmdWa(uint64_t atomicOpPlaceholder) {
    auto miAtomic = slbCS.getSpaceForCmd<Family::MI_ATOMIC>();
    *miAtomic = Family::cmdInitAtomic;
    miAtomic->setAtomicOpcode(Family::MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_8B_INCREMENT);
    miAtomic->setReturnDataControl(0x1);
    miAtomic->setCsStall(0x1);
    miAtomic->setDataSize(Family::MI_ATOMIC::DATA_SIZE::DATA_SIZE_QWORD);
    miAtomic->setMemoryAddress(static_cast<uint32_t>(atomicOpPlaceholder & 0x0000FFFFFFFFULL));
    miAtomic->setMemoryAddressHigh(static_cast<uint32_t>((atomicOpPlaceholder >> 32) & 0x0000FFFFFFFFULL));
}

template <>
void DeviceQueueHw<Family>::addLriCmdWa(bool setArbCheck) {}

template <>
void DeviceQueueHw<Family>::addPipeControlCmdWa(bool isNoopCmd) {
    auto pipeControl = slbCS.getSpaceForCmd<Family::PIPE_CONTROL>();
    if (isNoopCmd)
        memset(pipeControl, 0x0, sizeof(Family::PIPE_CONTROL));
    else
        initPipeControl(pipeControl);
}

template class DeviceQueueHw<Family>;
} // namespace NEO

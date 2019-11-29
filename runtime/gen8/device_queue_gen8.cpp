/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gen8/hw_cmds.h"
#include "runtime/device_queue/device_queue_hw.h"
#include "runtime/device_queue/device_queue_hw_bdw_plus.inl"

namespace NEO {
typedef BDWFamily Family;
static auto gfxCore = IGFX_GEN8_CORE;

template <>
void populateFactoryTable<DeviceQueueHw<Family>>() {
    extern DeviceQueueCreateFunc deviceQueueFactory[IGFX_MAX_CORE];
    deviceQueueFactory[gfxCore] = DeviceQueueHw<Family>::create;
}

template <>
size_t DeviceQueueHw<Family>::getWaCommandsSize() {
    return sizeof(Family::MI_ATOMIC) +
           sizeof(Family::MI_LOAD_REGISTER_IMM) +
           sizeof(Family::MI_LOAD_REGISTER_IMM);
}

template <>
void DeviceQueueHw<Family>::addArbCheckCmdWa() {}

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
void DeviceQueueHw<Family>::addLriCmdWa(bool setArbCheck) {
    auto lri = slbCS.getSpaceForCmd<Family::MI_LOAD_REGISTER_IMM>();
    *lri = Family::cmdInitLoadRegisterImm;
    lri->setRegisterOffset(0x2248); // CTXT_PREMP_DBG offset
    if (setArbCheck)
        lri->setDataDword(0x00000100); // set only bit 8 (Preempt On MI_ARB_CHK Only)
    else
        lri->setDataDword(0x0);
}

template <>
void DeviceQueueHw<Family>::addPipeControlCmdWa(bool isNoopCmd) {}

template <>
void DeviceQueueHw<Family>::addProfilingEndCmds(uint64_t timestampAddress) {
    auto pPipeControlCmd = (PIPE_CONTROL *)slbCS.getSpace(sizeof(PIPE_CONTROL));
    *pPipeControlCmd = Family::cmdInitPipeControl;
    pPipeControlCmd->setCommandStreamerStallEnable(true);
    pPipeControlCmd->setPostSyncOperation(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP);
    pPipeControlCmd->setAddressHigh(timestampAddress >> 32);
    pPipeControlCmd->setAddress(timestampAddress & (0xffffffff));
}

template <>
void DeviceQueueHw<Family>::addDcFlushToPipeControlWa(PIPE_CONTROL *pc) {
    pc->setDcFlushEnable(true);
}

template class DeviceQueueHw<Family>;
} // namespace NEO

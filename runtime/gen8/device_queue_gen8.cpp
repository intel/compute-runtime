/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/device_queue/device_queue_hw.h"
#include "runtime/device_queue/device_queue_hw.inl"

#include "runtime/gen8/hw_cmds.h"

namespace OCLRT {
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
    *miAtomic = Family::MI_ATOMIC::sInit();
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
    *lri = Family::MI_LOAD_REGISTER_IMM::sInit();
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
    *pPipeControlCmd = PIPE_CONTROL::sInit();
    pPipeControlCmd->setCommandStreamerStallEnable(true);
    pPipeControlCmd->setPostSyncOperation(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP);
    pPipeControlCmd->setAddressHigh(timestampAddress >> 32);
    pPipeControlCmd->setAddress(timestampAddress & (0xffffffff));
}

template class DeviceQueueHw<Family>;
} // namespace OCLRT

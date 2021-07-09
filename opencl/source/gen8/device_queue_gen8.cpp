/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/gen8/hw_cmds.h"
#include "shared/source/helpers/populate_factory.h"

#include "opencl/source/device_queue/device_queue_hw.h"
#include "opencl/source/device_queue/device_queue_hw_bdw_plus.inl"

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
    EncodeAtomic<Family>::programMiAtomic(slbCS,
                                          atomicOpPlaceholder,
                                          Family::MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_8B_INCREMENT,
                                          Family::MI_ATOMIC::DATA_SIZE::DATA_SIZE_QWORD,
                                          0x1u, 0x1u, 0x0u, 0x0u);
}

template <>
void DeviceQueueHw<Family>::addLriCmdWa(bool setArbCheck) {
    // CTXT_PREMP_DBG offset
    constexpr uint32_t registerAddress = 0x2248u;
    uint32_t value = 0u;
    if (setArbCheck) {
        // set only bit 8 (Preempt On MI_ARB_CHK Only)
        value = 0x00000100;
    }

    LriHelper<Family>::program(&slbCS,
                               registerAddress,
                               value,
                               false);
}

template <>
void DeviceQueueHw<Family>::addPipeControlCmdWa(bool isNoopCmd) {}

template <>
void DeviceQueueHw<Family>::addProfilingEndCmds(uint64_t timestampAddress) {
    auto pipeControlSpace = (PIPE_CONTROL *)slbCS.getSpace(sizeof(PIPE_CONTROL));
    auto pipeControlCmd = Family::cmdInitPipeControl;
    pipeControlCmd.setCommandStreamerStallEnable(true);
    pipeControlCmd.setPostSyncOperation(PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP);
    pipeControlCmd.setAddressHigh(timestampAddress >> 32);
    pipeControlCmd.setAddress(timestampAddress & (0xffffffff));
    *pipeControlSpace = pipeControlCmd;
}

template <>
void DeviceQueueHw<Family>::addDcFlushToPipeControlWa(PIPE_CONTROL *pc) {
    pc->setDcFlushEnable(true);
}

template class DeviceQueueHw<Family>;
} // namespace NEO

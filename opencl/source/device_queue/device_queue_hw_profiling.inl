/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/source/device_queue/device_queue_hw.h"

namespace NEO {

template <typename GfxFamily>
void DeviceQueueHw<GfxFamily>::addProfilingEndCmds(uint64_t timestampAddress) {

    auto pipeControlSpace = (PIPE_CONTROL *)slbCS.getSpace(sizeof(PIPE_CONTROL));
    auto pipeControlCmd = GfxFamily::cmdInitPipeControl;
    pipeControlCmd.setCommandStreamerStallEnable(true);
    *pipeControlSpace = pipeControlCmd;

    //low part
    auto mICmdLowSpace = (MI_STORE_REGISTER_MEM *)slbCS.getSpace(sizeof(MI_STORE_REGISTER_MEM));
    auto mICmdLow = GfxFamily::cmdInitStoreRegisterMem;
    GpgpuWalkerHelper<GfxFamily>::adjustMiStoreRegMemMode(&mICmdLow);
    mICmdLow.setRegisterAddress(GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
    mICmdLow.setMemoryAddress(timestampAddress);
    *mICmdLowSpace = mICmdLow;
}
} // namespace NEO

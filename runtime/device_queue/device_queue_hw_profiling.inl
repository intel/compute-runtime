/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace OCLRT {

template <typename GfxFamily>
void DeviceQueueHw<GfxFamily>::addProfilingEndCmds(uint64_t timestampAddress) {

    auto pPipeControlCmd = (PIPE_CONTROL *)slbCS.getSpace(sizeof(PIPE_CONTROL));
    *pPipeControlCmd = GfxFamily::cmdInitPipeControl;
    pPipeControlCmd->setCommandStreamerStallEnable(true);

    //low part
    auto pMICmdLow = (MI_STORE_REGISTER_MEM *)slbCS.getSpace(sizeof(MI_STORE_REGISTER_MEM));
    *pMICmdLow = GfxFamily::cmdInitStoreRegisterMem;
    GpgpuWalkerHelper<GfxFamily>::adjustMiStoreRegMemMode(pMICmdLow);
    pMICmdLow->setRegisterAddress(GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
    pMICmdLow->setMemoryAddress(timestampAddress);
}
} // namespace OCLRT

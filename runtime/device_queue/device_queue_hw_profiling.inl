/*
 * Copyright (c) 2017, Intel Corporation
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

namespace OCLRT {

template <typename GfxFamily>
void DeviceQueueHw<GfxFamily>::addProfilingEndCmds(uint64_t timestampAddress) {

    auto pPipeControlCmd = (PIPE_CONTROL *)slbCS.getSpace(sizeof(PIPE_CONTROL));
    *pPipeControlCmd = PIPE_CONTROL::sInit();
    pPipeControlCmd->setCommandStreamerStallEnable(true);

    //low part
    auto pMICmdLow = (MI_STORE_REGISTER_MEM *)slbCS.getSpace(sizeof(MI_STORE_REGISTER_MEM));
    *pMICmdLow = MI_STORE_REGISTER_MEM::sInit();
    pMICmdLow->setRegisterAddress(GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
    pMICmdLow->setMemoryAddress(timestampAddress);

    //hi part
    timestampAddress += sizeof(uint32_t);
    auto pMICmdHigh = (MI_STORE_REGISTER_MEM *)slbCS.getSpace(sizeof(MI_STORE_REGISTER_MEM));
    *pMICmdHigh = MI_STORE_REGISTER_MEM::sInit();
    pMICmdHigh->setRegisterAddress(GP_THREAD_TIME_REG_ADDRESS_OFFSET_HIGH);
    pMICmdHigh->setMemoryAddress(timestampAddress);
}
} // namespace OCLRT

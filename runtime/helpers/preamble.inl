/*
 * Copyright (c) 2018, Intel Corporation
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

#include "runtime/helpers/preamble.h"
#include "runtime/command_stream/linear_stream.h"
#include "runtime/command_stream/preemption.h"
#include "hw_cmds.h"
#include "reg_configs.h"
#include "runtime/device/device.h"
#include "runtime/kernel/kernel.h"
#include "runtime/helpers/aligned_memory.h"
#include <cstddef>

namespace OCLRT {

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programThreadArbitration(LinearStream *pCommandStream, uint32_t requiredThreadArbitrationPolicy) {
    typedef typename GfxFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef typename GfxFamily::PIPE_CONTROL PIPE_CONTROL;

    // Add a PIPE_CONTROL w/ CS_stall
    auto pPipeControl = (PIPE_CONTROL *)pCommandStream->getSpace(sizeof(PIPE_CONTROL));
    *pPipeControl = PIPE_CONTROL::sInit();
    pPipeControl->setCommandStreamerStallEnable(true);
    setupPipeControlInFrontOfCommand(pPipeControl, nullptr, false);

    auto pCmd = (MI_LOAD_REGISTER_IMM *)pCommandStream->getSpace(sizeof(MI_LOAD_REGISTER_IMM));
    *pCmd = MI_LOAD_REGISTER_IMM::sInit();

    pCmd->setRegisterOffset(0xE404);
    auto data = requiredThreadArbitrationPolicy;
    pCmd->setDataDword(data);
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programGenSpecificPreambleWorkArounds(LinearStream *pCommandStream, const HardwareInfo &hwInfo) {
}

template <typename GfxFamily>
uint32_t PreambleHelper<GfxFamily>::getAdditionalCommandsSize(const Device &device) {
    typedef typename GfxFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef typename GfxFamily::PIPE_CONTROL PIPE_CONTROL;
    size_t requiredSize = sizeof(MI_LOAD_REGISTER_IMM) + sizeof(PIPE_CONTROL);
    requiredSize += PreemptionHelper::getRequiredPreambleSize<GfxFamily>(device);
    return static_cast<uint32_t>(requiredSize);
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programVFEState(LinearStream *pCommandStream, const HardwareInfo &hwInfo, int scratchSize, uint64_t scratchAddress) {
    typedef typename GfxFamily::PIPE_CONTROL PIPE_CONTROL;
    typedef typename GfxFamily::MEDIA_VFE_STATE MEDIA_VFE_STATE;

    // Add a PIPE_CONTROL w/ CS_stall
    auto pPipeControl = (PIPE_CONTROL *)pCommandStream->getSpace(sizeof(PIPE_CONTROL));
    *pPipeControl = PIPE_CONTROL::sInit();
    pPipeControl->setCommandStreamerStallEnable(true);
    setupPipeControlInFrontOfCommand(pPipeControl, &hwInfo, true);

    auto pMediaVfeState = (MEDIA_VFE_STATE *)pCommandStream->getSpace(sizeof(MEDIA_VFE_STATE));
    *pMediaVfeState = MEDIA_VFE_STATE::sInit();
    pMediaVfeState->setMaximumNumberOfThreads(hwInfo.pSysInfo->ThreadCount);
    pMediaVfeState->setNumberOfUrbEntries(1);
    pMediaVfeState->setUrbEntryAllocationSize(PreambleHelper<GfxFamily>::getUrbEntryAllocationSize());
    pMediaVfeState->setPerThreadScratchSpace(Kernel::getScratchSizeValueToProgramMediaVfeState(scratchSize));
    pMediaVfeState->setStackSize(Kernel::getScratchSizeValueToProgramMediaVfeState(scratchSize));
    uint32_t lowAddress = uint32_t(0xFFFFFFFF & scratchAddress);
    uint32_t highAddress = uint32_t(0xFFFFFFFF & (scratchAddress >> 32));
    pMediaVfeState->setScratchSpaceBasePointer(lowAddress);
    pMediaVfeState->setScratchSpaceBasePointerHigh(highAddress);
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programL3(LinearStream *pCommandStream, uint32_t l3Config) {
    typedef typename GfxFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    auto pCmd = (MI_LOAD_REGISTER_IMM *)pCommandStream->getSpace(sizeof(MI_LOAD_REGISTER_IMM));
    *pCmd = MI_LOAD_REGISTER_IMM::sInit();

    pCmd->setRegisterOffset(L3CNTLRegisterOffset<GfxFamily>::registerOffset);
    pCmd->setDataDword(l3Config);
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programPreamble(LinearStream *pCommandStream, const Device &device, uint32_t l3Config,
                                                uint32_t requiredThreadArbitrationPolicy, GraphicsAllocation *preemptionCsr) {
    programL3(pCommandStream, l3Config);
    programThreadArbitration(pCommandStream, requiredThreadArbitrationPolicy);
    programPreemption(pCommandStream, device, preemptionCsr);
    programGenSpecificPreambleWorkArounds(pCommandStream, device.getHardwareInfo());
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programPreemption(LinearStream *pCommandStream, const Device &device, GraphicsAllocation *preemptionCsr) {
    PreemptionHelper::programPreamble<GfxFamily>(*pCommandStream, device, preemptionCsr);
}

template <typename GfxFamily>
uint32_t PreambleHelper<GfxFamily>::getUrbEntryAllocationSize() {
    return 0x782;
}

} // namespace OCLRT

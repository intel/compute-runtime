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

#include "runtime/helpers/preamble.inl"
#include "runtime/command_queue/gpgpu_walker.h"

namespace OCLRT {

template <>
uint32_t PreambleHelper<CNLFamily>::getL3Config(const HardwareInfo &hwInfo, bool useSLM) {
    uint32_t l3Config = 0;

    switch (hwInfo.pPlatform->eProductFamily) {
    case IGFX_CANNONLAKE:
        l3Config = getL3ConfigHelper<IGFX_CANNONLAKE>(useSLM);
        break;
    default:
        l3Config = getL3ConfigHelper<IGFX_CANNONLAKE>(true);
    }
    return l3Config;
}

template <>
uint32_t PreambleHelper<CNLFamily>::getDefaultThreadArbitrationPolicy() {
    return ThreadArbitrationPolicy::RoundRobinAfterDependency;
}

template <>
void PreambleHelper<CNLFamily>::programThreadArbitration(LinearStream *pCommandStream, uint32_t requiredThreadArbitrationPolicy) {
    UNRECOVERABLE_IF(requiredThreadArbitrationPolicy == ThreadArbitrationPolicy::NotPresent);

    auto pipeControl = pCommandStream->getSpaceForCmd<PIPE_CONTROL>();
    *pipeControl = PIPE_CONTROL::sInit();
    pipeControl->setCommandStreamerStallEnable(true);

    auto pCmd = pCommandStream->getSpaceForCmd<MI_LOAD_REGISTER_IMM>();
    *pCmd = MI_LOAD_REGISTER_IMM::sInit();

    pCmd->setRegisterOffset(RowChickenReg4::address);
    pCmd->setDataDword(RowChickenReg4::regDataForArbitrationPolicy[requiredThreadArbitrationPolicy]);
}

template <>
size_t PreambleHelper<CNLFamily>::getThreadArbitrationCommandsSize() {
    return sizeof(MI_LOAD_REGISTER_IMM) + sizeof(PIPE_CONTROL);
}

template <>
size_t PreambleHelper<CNLFamily>::getAdditionalCommandsSize(const Device &device) {
    typedef typename CNLFamily::MI_LOAD_REGISTER_REG MI_LOAD_REGISTER_REG;
    typedef typename CNLFamily::PIPE_CONTROL PIPE_CONTROL;
    typedef typename CNLFamily::MI_MATH MI_MATH;
    typedef typename CNLFamily::MI_MATH_ALU_INST_INLINE MI_MATH_ALU_INST_INLINE;

    size_t size = PreemptionHelper::getRequiredPreambleSize<CNLFamily>(device) + sizeof(MI_LOAD_REGISTER_IMM);
    size += getKernelDebuggingCommandsSize(device.isSourceLevelDebuggerActive());
    return size;
}

template <>
void PreambleHelper<CNLFamily>::programGenSpecificPreambleWorkArounds(LinearStream *pCommandStream, const HardwareInfo &hwInfo) {
    typedef typename CNLFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;

    auto pCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(pCommandStream->getSpace(sizeof(MI_LOAD_REGISTER_IMM)));
    *pCmd = MI_LOAD_REGISTER_IMM::sInit();
    pCmd->setRegisterOffset(FfSliceCsChknReg2::address);
    pCmd->setDataDword(FfSliceCsChknReg2::regVal);
}

template <>
uint32_t PreambleHelper<CNLFamily>::getUrbEntryAllocationSize() {
    return 1024;
}

template <>
void PreambleHelper<CNLFamily>::addPipeControlBeforeVfeCmd(LinearStream *pCommandStream, const HardwareInfo *hwInfo) {
    auto pipeControl = pCommandStream->getSpaceForCmd<PIPE_CONTROL>();
    *pipeControl = PIPE_CONTROL::sInit();
    pipeControl->setCommandStreamerStallEnable(true);
    if (hwInfo->pWaTable->waSendMIFLUSHBeforeVFE) {
        pipeControl->setRenderTargetCacheFlushEnable(true);
        pipeControl->setDepthCacheFlushEnable(true);
        pipeControl->setDcFlushEnable(true);
    }
}

template <>
void PreambleHelper<CNLFamily>::programPipelineSelect(LinearStream *pCommandStream, bool mediaSamplerRequired) {
    typedef typename CNLFamily::PIPELINE_SELECT PIPELINE_SELECT;

    auto pCmd = (PIPELINE_SELECT *)pCommandStream->getSpace(sizeof(PIPELINE_SELECT));
    *pCmd = PIPELINE_SELECT::sInit();

    auto mask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits;
    pCmd->setMaskBits(mask);
    pCmd->setPipelineSelection(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU);
    pCmd->setMediaSamplerDopClockGateEnable(!mediaSamplerRequired);
}

template struct PreambleHelper<CNLFamily>;
} // namespace OCLRT

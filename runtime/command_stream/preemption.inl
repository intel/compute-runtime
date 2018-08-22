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

#include "runtime/built_ins/built_ins.h"
#include "runtime/built_ins/sip.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/device/device.h"
#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/memory_manager/graphics_allocation.h"

namespace OCLRT {

template <typename GfxFamily>
size_t PreemptionHelper::getPreemptionWaCsSize(const Device &device) {
    typedef typename GfxFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    size_t size = 0;
    PreemptionMode preemptionMode = device.getPreemptionMode();
    if (preemptionMode == PreemptionMode::ThreadGroup ||
        preemptionMode == PreemptionMode::MidThread) {
        if (device.getWaTable()->waModifyVFEStateAfterGPGPUPreemption) {
            size += 2 * sizeof(MI_LOAD_REGISTER_IMM);
        }
    }
    return size;
}

template <typename GfxFamily>
void PreemptionHelper::applyPreemptionWaCmdsBegin(LinearStream *pCommandStream, const Device &device) {
    typedef typename GfxFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    PreemptionMode preemptionMode = device.getPreemptionMode();
    if (preemptionMode == PreemptionMode::ThreadGroup ||
        preemptionMode == PreemptionMode::MidThread) {
        if (device.getWaTable()->waModifyVFEStateAfterGPGPUPreemption) {
            auto pCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(pCommandStream->getSpace(sizeof(MI_LOAD_REGISTER_IMM)));
            *pCmd = MI_LOAD_REGISTER_IMM::sInit();
            pCmd->setRegisterOffset(CS_GPR_R0);
            pCmd->setDataDword(GPGPU_WALKER_COOKIE_VALUE_BEFORE_WALKER);
        }
    }
}

template <typename GfxFamily>
void PreemptionHelper::applyPreemptionWaCmdsEnd(LinearStream *pCommandStream, const Device &device) {
    typedef typename GfxFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    PreemptionMode preemptionMode = device.getPreemptionMode();
    if (preemptionMode == PreemptionMode::ThreadGroup ||
        preemptionMode == PreemptionMode::MidThread) {
        if (device.getWaTable()->waModifyVFEStateAfterGPGPUPreemption) {
            auto pCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(pCommandStream->getSpace(sizeof(MI_LOAD_REGISTER_IMM)));
            *pCmd = MI_LOAD_REGISTER_IMM::sInit();
            pCmd->setRegisterOffset(CS_GPR_R0);
            pCmd->setDataDword(GPGPU_WALKER_COOKIE_VALUE_AFTER_WALKER);
        }
    }
}

template <typename GfxFamily>
void PreemptionHelper::programPreamble(LinearStream &preambleCmdStream, Device &device,
                                       const GraphicsAllocation *preemptionCsr) {
    using GPGPU_CSR_BASE_ADDRESS = typename GfxFamily::GPGPU_CSR_BASE_ADDRESS;
    using STATE_SIP = typename GfxFamily::STATE_SIP;
    bool sourceLevelDebuggerActive = device.isSourceLevelDebuggerActive();
    if (device.getPreemptionMode() == PreemptionMode::MidThread) {
        UNRECOVERABLE_IF(nullptr == preemptionCsr);

        auto csr = reinterpret_cast<GPGPU_CSR_BASE_ADDRESS *>(preambleCmdStream.getSpace(sizeof(GPGPU_CSR_BASE_ADDRESS)));
        csr->init();
        csr->setGpgpuCsrBaseAddress(preemptionCsr->getGpuAddressToPatch());
    }
    if (device.getPreemptionMode() == PreemptionMode::MidThread || sourceLevelDebuggerActive) {
        auto sip = reinterpret_cast<STATE_SIP *>(preambleCmdStream.getSpace(sizeof(STATE_SIP)));
        sip->init();
        auto sipType = SipKernel::getSipKernelType(device.getHardwareInfo().pPlatform->eRenderCoreFamily, sourceLevelDebuggerActive);
        sip->setSystemInstructionPointer(device.getExecutionEnvironment()->getBuiltIns()->getSipKernel(sipType, device).getSipAllocation()->getGpuAddressToPatch());
    }
}

template <typename GfxFamily>
void PreemptionHelper::programCmdStream(LinearStream &cmdStream,
                                        PreemptionMode newPreemptionMode, PreemptionMode oldPreemptionMode,
                                        GraphicsAllocation *preemptionCsr, Device &device) {
    if (oldPreemptionMode == newPreemptionMode) {
        return;
    }

    uint32_t regVal = 0;
    if (newPreemptionMode == PreemptionMode::MidThread) {
        regVal = PreemptionConfig<GfxFamily>::midThreadVal | PreemptionConfig<GfxFamily>::mask;
    } else if (newPreemptionMode == PreemptionMode::ThreadGroup) {
        regVal = PreemptionConfig<GfxFamily>::threadGroupVal | PreemptionConfig<GfxFamily>::mask;
    } else {
        regVal = PreemptionConfig<GfxFamily>::cmdLevelVal | PreemptionConfig<GfxFamily>::mask;
    }

    LriHelper<GfxFamily>::program(&cmdStream, PreemptionConfig<GfxFamily>::mmioAddress, regVal);
}

template <typename GfxFamily>
size_t PreemptionHelper::getRequiredCmdStreamSize(PreemptionMode newPreemptionMode, PreemptionMode oldPreemptionMode) {
    if (newPreemptionMode == oldPreemptionMode) {
        return 0;
    }
    return sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM);
}

template <typename GfxFamily>
size_t PreemptionHelper::getRequiredPreambleSize(const Device &device) {
    size_t size = 0;
    if (device.getPreemptionMode() == PreemptionMode::MidThread) {
        size += sizeof(typename GfxFamily::GPGPU_CSR_BASE_ADDRESS);
    }
    if (device.getPreemptionMode() == PreemptionMode::MidThread || device.isSourceLevelDebuggerActive()) {
        size += sizeof(typename GfxFamily::STATE_SIP);
    }

    return size;
}

template <typename GfxFamily>
void PreemptionHelper::programInterfaceDescriptorDataPreemption(INTERFACE_DESCRIPTOR_DATA<GfxFamily> *idd, PreemptionMode preemptionMode) {
}

} // namespace OCLRT

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

#include "runtime/command_stream/preemption.h"
#include "runtime/device/device.h"
#include "runtime/command_queue/dispatch_walker_helper.h"

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

} // namespace OCLRT

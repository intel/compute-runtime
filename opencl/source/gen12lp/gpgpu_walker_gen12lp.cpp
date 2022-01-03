/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_info.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/memory_manager/graphics_allocation.h"

#include "opencl/source/command_queue/gpgpu_walker_bdw_and_later.inl"
#include "opencl/source/command_queue/hardware_interface_bdw_and_later.inl"

namespace NEO {

template <>
void GpgpuWalkerHelper<TGLLPFamily>::adjustMiStoreRegMemMode(MI_STORE_REG_MEM<TGLLPFamily> *storeCmd) {
    storeCmd->setMmioRemapEnable(true);
}

template <>
void HardwareInterface<TGLLPFamily>::dispatchWorkarounds(
    LinearStream *commandStream,
    CommandQueue &commandQueue,
    Kernel &kernel,
    const bool &enable) {

    using MI_LOAD_REGISTER_IMM = typename TGLLPFamily::MI_LOAD_REGISTER_IMM;
    using PIPE_CONTROL = typename TGLLPFamily::PIPE_CONTROL;

    if (kernel.requiresWaDisableRccRhwoOptimization()) {

        PIPE_CONTROL cmdPipeControl = TGLLPFamily::cmdInitPipeControl;
        cmdPipeControl.setCommandStreamerStallEnable(true);
        auto pCmdPipeControl = commandStream->getSpaceForCmd<PIPE_CONTROL>();
        *pCmdPipeControl = cmdPipeControl;

        uint32_t value = enable ? 0x40004000 : 0x40000000;
        NEO::LriHelper<TGLLPFamily>::program(commandStream,
                                             0x7010,
                                             value,
                                             false);
    }
}

template <>
size_t GpgpuWalkerHelper<TGLLPFamily>::getSizeForWaDisableRccRhwoOptimization(const Kernel *pKernel) {
    if (pKernel->requiresWaDisableRccRhwoOptimization()) {
        return (2 * (sizeof(TGLLPFamily::PIPE_CONTROL) + sizeof(TGLLPFamily::MI_LOAD_REGISTER_IMM)));
    }
    return 0u;
}

template class HardwareInterface<TGLLPFamily>;

template class GpgpuWalkerHelper<TGLLPFamily>;

template struct EnqueueOperation<TGLLPFamily>;

} // namespace NEO

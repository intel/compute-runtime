/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"

#include "opencl/source/command_queue/gpgpu_walker_bdw_and_later.inl"
#include "opencl/source/command_queue/hardware_interface_bdw_and_later.inl"

namespace NEO {

template <>
void GpgpuWalkerHelper<Gen12LpFamily>::adjustMiStoreRegMemMode(MI_STORE_REG_MEM<Gen12LpFamily> *storeCmd) {
    storeCmd->setMmioRemapEnable(true);
}

template <>
void HardwareInterface<Gen12LpFamily>::dispatchWorkarounds(
    LinearStream *commandStream,
    CommandQueue &commandQueue,
    Kernel &kernel,
    const bool &enable) {

    using MI_LOAD_REGISTER_IMM = typename Gen12LpFamily::MI_LOAD_REGISTER_IMM;
    using PIPE_CONTROL = typename Gen12LpFamily::PIPE_CONTROL;

    if (kernel.requiresWaDisableRccRhwoOptimization()) {

        PIPE_CONTROL cmdPipeControl = Gen12LpFamily::cmdInitPipeControl;
        cmdPipeControl.setCommandStreamerStallEnable(true);
        auto pCmdPipeControl = commandStream->getSpaceForCmd<PIPE_CONTROL>();
        *pCmdPipeControl = cmdPipeControl;

        uint32_t value = enable ? 0x40004000 : 0x40000000;
        NEO::LriHelper<Gen12LpFamily>::program(commandStream,
                                               0x7010,
                                               value,
                                               false);
    }
}

template <>
size_t GpgpuWalkerHelper<Gen12LpFamily>::getSizeForWaDisableRccRhwoOptimization(const Kernel *pKernel) {
    if (pKernel->requiresWaDisableRccRhwoOptimization()) {
        return (2 * (sizeof(Gen12LpFamily::PIPE_CONTROL) + sizeof(Gen12LpFamily::MI_LOAD_REGISTER_IMM)));
    }
    return 0u;
}

template class HardwareInterface<Gen12LpFamily>;

template class GpgpuWalkerHelper<Gen12LpFamily>;

template struct EnqueueOperation<Gen12LpFamily>;

} // namespace NEO

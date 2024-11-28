/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"

#include "opencl/source/command_queue/gpgpu_walker_bdw_and_later.inl"
#include "opencl/source/command_queue/hardware_interface_bdw_and_later.inl"

namespace NEO {

using Family = Gen12LpFamily;

template <>
void GpgpuWalkerHelper<Family>::adjustMiStoreRegMemMode(MI_STORE_REG_MEM<Family> *storeCmd) {
    storeCmd->setMmioRemapEnable(true);
}

template <>
void HardwareInterface<Family>::dispatchWorkarounds(
    LinearStream *commandStream,
    CommandQueue &commandQueue,
    Kernel &kernel,
    const bool &enable) {

    using PIPE_CONTROL = typename Family::PIPE_CONTROL;

    if (kernel.requiresWaDisableRccRhwoOptimization()) {

        PIPE_CONTROL cmdPipeControl = Family::cmdInitPipeControl;
        cmdPipeControl.setCommandStreamerStallEnable(true);
        auto pCmdPipeControl = commandStream->getSpaceForCmd<PIPE_CONTROL>();
        *pCmdPipeControl = cmdPipeControl;

        uint32_t value = enable ? 0x40004000 : 0x40000000;
        NEO::LriHelper<Family>::program(commandStream,
                                        0x7010,
                                        value,
                                        false,
                                        commandQueue.isBcs());
    }
}

template <>
size_t GpgpuWalkerHelper<Family>::getSizeForWaDisableRccRhwoOptimization(const Kernel *pKernel) {
    if (pKernel->requiresWaDisableRccRhwoOptimization()) {
        return (2 * (sizeof(Gen12LpFamily::PIPE_CONTROL) + sizeof(Family::MI_LOAD_REGISTER_IMM)));
    }
    return 0u;
}

template class HardwareInterface<Family>;

template void HardwareInterface<Family>::dispatchWalker<Family::DefaultWalkerType>(CommandQueue &commandQueue, const MultiDispatchInfo &multiDispatchInfo, const CsrDependencies &csrDependencies, HardwareInterfaceWalkerArgs &walkerArgs);
template void HardwareInterface<Family>::programWalker<Family::DefaultWalkerType>(LinearStream &commandStream, Kernel &kernel, CommandQueue &commandQueue, IndirectHeap &dsh, IndirectHeap &ioh, IndirectHeap &ssh, const DispatchInfo &dispatchInfo, HardwareInterfaceWalkerArgs &walkerArgs);
template void HardwareInterface<Family>::dispatchKernelCommands<Family::DefaultWalkerType>(CommandQueue &commandQueue, const DispatchInfo &dispatchInfo, LinearStream &commandStream, IndirectHeap &dsh, IndirectHeap &ioh, IndirectHeap &ssh, HardwareInterfaceWalkerArgs &walkerArgs);
template Family::DefaultWalkerType *HardwareInterface<Family>::allocateWalkerSpace<Family::DefaultWalkerType>(LinearStream &commandStream, const Kernel &kernel);

template class GpgpuWalkerHelper<Family>;
template void GpgpuWalkerHelper<Family>::setupTimestampPacket<Family::DefaultWalkerType>(LinearStream *cmdStream, Family::DefaultWalkerType *walkerCmd, TagNodeBase *timestampPacketNode, const RootDeviceEnvironment &rootDeviceEnvironment);
template size_t GpgpuWalkerHelper<Family>::setGpgpuWalkerThreadData<Family::DefaultWalkerType>(Family::DefaultWalkerType *walkerCmd, const KernelDescriptor &kernelDescriptor, const size_t startWorkGroups[3],
                                                                                               const size_t numWorkGroups[3], const size_t localWorkSizesIn[3], uint32_t simd, uint32_t workDim, bool localIdsGenerationByRuntime, bool inlineDataProgrammingRequired, uint32_t requiredWorkGroupOrder);

template struct EnqueueOperation<Family>;

} // namespace NEO

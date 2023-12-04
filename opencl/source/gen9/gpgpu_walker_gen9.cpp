/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds_base.h"

#include "opencl/source/command_queue/gpgpu_walker_bdw_and_later.inl"
#include "opencl/source/command_queue/hardware_interface_bdw_and_later.inl"

namespace NEO {

using Family = Gen9Family;

template <>
void GpgpuWalkerHelper<Family>::applyWADisableLSQCROPERFforOCL(NEO::LinearStream *pCommandStream, const Kernel &kernel, bool disablePerfMode) {
    if (disablePerfMode) {
        if (kernel.getKernelInfo().kernelDescriptor.kernelAttributes.flags.usesFencesForReadWriteImages) {
            // Set bit L3SQC_BIT_LQSC_RO_PERF_DIS in L3SQC_REG4
            GpgpuWalkerHelper<Family>::addAluReadModifyWriteRegister(pCommandStream, L3SQC_REG4, AluRegisters::OPCODE_OR, L3SQC_BIT_LQSC_RO_PERF_DIS);
        }
    } else {
        if (kernel.getKernelInfo().kernelDescriptor.kernelAttributes.flags.usesFencesForReadWriteImages) {
            // Add PIPE_CONTROL with CS_Stall to wait till GPU finishes its work
            typedef typename Family::PIPE_CONTROL PIPE_CONTROL;
            auto pipeControlSpace = reinterpret_cast<PIPE_CONTROL *>(pCommandStream->getSpace(sizeof(PIPE_CONTROL)));
            auto pipeControl = Family::cmdInitPipeControl;
            pipeControl.setCommandStreamerStallEnable(true);
            *pipeControlSpace = pipeControl;

            // Clear bit L3SQC_BIT_LQSC_RO_PERF_DIS in L3SQC_REG4
            GpgpuWalkerHelper<Family>::addAluReadModifyWriteRegister(pCommandStream, L3SQC_REG4, AluRegisters::OPCODE_AND, ~L3SQC_BIT_LQSC_RO_PERF_DIS);
        }
    }
}

template <>
size_t GpgpuWalkerHelper<Family>::getSizeForWADisableLSQCROPERFforOCL(const Kernel *pKernel) {
    typedef typename Family::MI_LOAD_REGISTER_REG MI_LOAD_REGISTER_REG;
    typedef typename Family::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef typename Family::PIPE_CONTROL PIPE_CONTROL;
    typedef typename Family::MI_MATH MI_MATH;
    typedef typename Family::MI_MATH_ALU_INST_INLINE MI_MATH_ALU_INST_INLINE;
    size_t n = 0;
    if (pKernel->getKernelInfo().kernelDescriptor.kernelAttributes.flags.usesFencesForReadWriteImages) {
        n += sizeof(PIPE_CONTROL) +
             (2 * sizeof(MI_LOAD_REGISTER_REG) +
              sizeof(MI_LOAD_REGISTER_IMM) +
              sizeof(PIPE_CONTROL) +
              sizeof(MI_MATH) +
              NUM_ALU_INST_FOR_READ_MODIFY_WRITE * sizeof(MI_MATH_ALU_INST_INLINE)) *
                 2; // For 2 WADisableLSQCROPERFforOCL WAs
    }
    return n;
}

template class HardwareInterface<Family>;

template void HardwareInterface<Family>::dispatchWalker<Family::DefaultWalkerType>(CommandQueue &commandQueue, const MultiDispatchInfo &multiDispatchInfo, const CsrDependencies &csrDependencies, HardwareInterfaceWalkerArgs &walkerArgs);
template void HardwareInterface<Family>::programWalker<Family::DefaultWalkerType>(LinearStream &commandStream, Kernel &kernel, CommandQueue &commandQueue, IndirectHeap &dsh, IndirectHeap &ioh, IndirectHeap &ssh, const DispatchInfo &dispatchInfo, HardwareInterfaceWalkerArgs &walkerArgs);
template void HardwareInterface<Family>::dispatchKernelCommands<Family::DefaultWalkerType>(CommandQueue &commandQueue, const DispatchInfo &dispatchInfo, LinearStream &commandStream, IndirectHeap &dsh, IndirectHeap &ioh, IndirectHeap &ssh, HardwareInterfaceWalkerArgs &walkerArgs);
template Family::DefaultWalkerType *HardwareInterface<Family>::allocateWalkerSpace<Family::DefaultWalkerType>(LinearStream &commandStream, const Kernel &kernel);

template class GpgpuWalkerHelper<Family>;
template void GpgpuWalkerHelper<Family>::setupTimestampPacket<Family::DefaultWalkerType>(LinearStream *cmdStream, Family::DefaultWalkerType *walkerCmd, TagNodeBase *timestampPacketNode, const RootDeviceEnvironment &rootDeviceEnvironment);
template size_t GpgpuWalkerHelper<Family>::setGpgpuWalkerThreadData<Family::DefaultWalkerType>(Family::DefaultWalkerType *walkerCmd, const KernelDescriptor &kernelDescriptor, const size_t globalOffsets[3], const size_t startWorkGroups[3],
                                                                                               const size_t numWorkGroups[3], const size_t localWorkSizesIn[3], uint32_t simd, uint32_t workDim, bool localIdsGenerationByRuntime, bool inlineDataProgrammingRequired, uint32_t requiredWorkGroupOrder);

template struct EnqueueOperation<Family>;

} // namespace NEO

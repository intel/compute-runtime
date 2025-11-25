/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/product_helper.h"

#include "opencl/source/command_queue/gpgpu_walker_xehp_and_later.inl"
#include "opencl/source/command_queue/hardware_interface_xehp_and_later.inl"

#include "hw_cmds_xe3p_core.h"

namespace NEO {
using Family = Xe3pCoreFamily;
}

#include "opencl/source/command_queue/gpgpu_walker_xe3p_and_later.inl"

namespace NEO {

template class GpgpuWalkerHelper<Family>;

template void GpgpuWalkerHelper<Family>::setupTimestampPacketFlushL3<Family::COMPUTE_WALKER>(Family::COMPUTE_WALKER &walkerCmd, CommandQueue &commandQueue, const FlushL3Args &args);
template void GpgpuWalkerHelper<Family>::setupTimestampPacketFlushL3<Family::COMPUTE_WALKER_2>(Family::COMPUTE_WALKER_2 &walkerCmd, CommandQueue &commandQueue, const FlushL3Args &args);
template void GpgpuWalkerHelper<Family>::setupTimestampPacket<Family::COMPUTE_WALKER>(LinearStream *cmdStream, Family::COMPUTE_WALKER *walkerCmd, TagNodeBase *timestampPacketNode, const RootDeviceEnvironment &rootDeviceEnvironment);
template void GpgpuWalkerHelper<Family>::setupTimestampPacket<Family::COMPUTE_WALKER_2>(LinearStream *cmdStream, Family::COMPUTE_WALKER_2 *walkerCmd, TagNodeBase *timestampPacketNode, const RootDeviceEnvironment &rootDeviceEnvironment);
template size_t GpgpuWalkerHelper<Family>::setGpgpuWalkerThreadData<Family::COMPUTE_WALKER>(Family::COMPUTE_WALKER *walkerCmd, const KernelDescriptor &kernelDescriptor, const size_t startWorkGroups[3],
                                                                                            const size_t numWorkGroups[3], const size_t localWorkSizesIn[3], uint32_t simd, uint32_t workDim, bool localIdsGenerationByRuntime, bool inlineDataProgrammingRequired, uint32_t requiredWorkGroupOrder);
template size_t GpgpuWalkerHelper<Family>::setGpgpuWalkerThreadData<Family::COMPUTE_WALKER_2>(Family::COMPUTE_WALKER_2 *walkerCmd, const KernelDescriptor &kernelDescriptor, const size_t startWorkGroups[3],
                                                                                              const size_t numWorkGroups[3], const size_t localWorkSizesIn[3], uint32_t simd, uint32_t workDim, bool localIdsGenerationByRuntime, bool inlineDataProgrammingRequired, uint32_t requiredWorkGroupOrder);
template <>
void HardwareInterface<Family>::dispatchWalkerCommon(CommandQueue &commandQueue, const MultiDispatchInfo &multiDispatchInfo, const CsrDependencies &csrDependencies, HardwareInterfaceWalkerArgs &walkerArgs) {

    auto heaplessModeEnabled = commandQueue.getHeaplessModeEnabled();

    if (heaplessModeEnabled) {
        dispatchWalker<Family::COMPUTE_WALKER_2>(commandQueue, multiDispatchInfo, csrDependencies, walkerArgs);
    } else {
        dispatchWalker<Family::COMPUTE_WALKER>(commandQueue, multiDispatchInfo, csrDependencies, walkerArgs);
    }
}

template class HardwareInterface<Family>;

template void HardwareInterface<Family>::dispatchWalker<Family::COMPUTE_WALKER>(CommandQueue &commandQueue, const MultiDispatchInfo &multiDispatchInfo, const CsrDependencies &csrDependencies, HardwareInterfaceWalkerArgs &walkerArgs);
template void HardwareInterface<Family>::dispatchWalker<Family::COMPUTE_WALKER_2>(CommandQueue &commandQueue, const MultiDispatchInfo &multiDispatchInfo, const CsrDependencies &csrDependencies, HardwareInterfaceWalkerArgs &walkerArgs);
template void HardwareInterface<Family>::programWalker<Family::COMPUTE_WALKER>(LinearStream &commandStream, Kernel &kernel, CommandQueue &commandQueue, IndirectHeap &dsh, IndirectHeap &ioh, IndirectHeap &ssh, const DispatchInfo &dispatchInfo, HardwareInterfaceWalkerArgs &walkerArgs);
template void HardwareInterface<Family>::programWalker<Family::COMPUTE_WALKER_2>(LinearStream &commandStream, Kernel &kernel, CommandQueue &commandQueue, IndirectHeap &dsh, IndirectHeap &ioh, IndirectHeap &ssh, const DispatchInfo &dispatchInfo, HardwareInterfaceWalkerArgs &walkerArgs);
template void HardwareInterface<Family>::dispatchKernelCommands<Family::COMPUTE_WALKER>(CommandQueue &commandQueue, const DispatchInfo &dispatchInfo, LinearStream &commandStream, IndirectHeap &dsh, IndirectHeap &ioh, IndirectHeap &ssh, HardwareInterfaceWalkerArgs &walkerArgs);
template void HardwareInterface<Family>::dispatchKernelCommands<Family::COMPUTE_WALKER_2>(CommandQueue &commandQueue, const DispatchInfo &dispatchInfo, LinearStream &commandStream, IndirectHeap &dsh, IndirectHeap &ioh, IndirectHeap &ssh, HardwareInterfaceWalkerArgs &walkerArgs);
template Family::COMPUTE_WALKER *HardwareInterface<Family>::allocateWalkerSpace<Family::COMPUTE_WALKER>(LinearStream &commandStream, const Kernel &kernel);
template Family::COMPUTE_WALKER_2 *HardwareInterface<Family>::allocateWalkerSpace<Family::COMPUTE_WALKER_2>(LinearStream &commandStream, const Kernel &kernel);

template <>
size_t EnqueueOperation<Family>::getSizeRequiredCS(uint32_t cmdType, bool reserveProfilingCmdsSpace, bool reservePerfCounters, CommandQueue &commandQueue, const Kernel *pKernel, const DispatchInfo &dispatchInfo) {
    if (isCommandWithoutKernel(cmdType)) {
        return EnqueueOperation<Family>::getSizeRequiredCSNonKernel(reserveProfilingCmdsSpace, reservePerfCounters, commandQueue);
    } else {

        auto heaplessModeEnabled = commandQueue.getHeaplessModeEnabled();
        if (heaplessModeEnabled) {
            return EnqueueOperation<Family>::getSizeRequiredCSKernel<Family::COMPUTE_WALKER_2>(reserveProfilingCmdsSpace, reservePerfCounters, commandQueue, pKernel, dispatchInfo);

        } else {
            return EnqueueOperation<Family>::getSizeRequiredCSKernel<Family::COMPUTE_WALKER>(reserveProfilingCmdsSpace, reservePerfCounters, commandQueue, pKernel, dispatchInfo);
        }
    }
}

template struct EnqueueOperation<Family>;

} // namespace NEO

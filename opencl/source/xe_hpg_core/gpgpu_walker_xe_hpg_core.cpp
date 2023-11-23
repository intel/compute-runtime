/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

#include "opencl/source/command_queue/gpgpu_walker_xehp_and_later.inl"
#include "opencl/source/command_queue/hardware_interface_xehp_and_later.inl"

namespace NEO {

using Family = XeHpgCoreFamily;

template class GpgpuWalkerHelper<Family>;
template void GpgpuWalkerHelper<Family>::setupTimestampPacket<Family::WALKER_TYPE>(LinearStream *cmdStream, Family::WALKER_TYPE *walkerCmd, TagNodeBase *timestampPacketNode, const RootDeviceEnvironment &rootDeviceEnvironment);
template size_t GpgpuWalkerHelper<Family>::setGpgpuWalkerThreadData<Family::WALKER_TYPE>(Family::WALKER_TYPE *walkerCmd, const KernelDescriptor &kernelDescriptor, const size_t globalOffsets[3], const size_t startWorkGroups[3],
                                                                                         const size_t numWorkGroups[3], const size_t localWorkSizesIn[3], uint32_t simd, uint32_t workDim, bool localIdsGenerationByRuntime, bool inlineDataProgrammingRequired, uint32_t requiredWorkGroupOrder);

template class HardwareInterface<Family>;

template void HardwareInterface<Family>::dispatchWalker<Family::WALKER_TYPE>(CommandQueue &commandQueue, const MultiDispatchInfo &multiDispatchInfo, const CsrDependencies &csrDependencies, HardwareInterfaceWalkerArgs &walkerArgs);
template void HardwareInterface<Family>::programWalker<Family::WALKER_TYPE>(LinearStream &commandStream, Kernel &kernel, CommandQueue &commandQueue, IndirectHeap &dsh, IndirectHeap &ioh, IndirectHeap &ssh, const DispatchInfo &dispatchInfo, HardwareInterfaceWalkerArgs &walkerArgs);
template void HardwareInterface<Family>::dispatchKernelCommands<Family::WALKER_TYPE>(CommandQueue &commandQueue, const DispatchInfo &dispatchInfo, LinearStream &commandStream, IndirectHeap &dsh, IndirectHeap &ioh, IndirectHeap &ssh, HardwareInterfaceWalkerArgs &walkerArgs);
template Family::WALKER_TYPE *HardwareInterface<Family>::allocateWalkerSpace<Family::WALKER_TYPE>(LinearStream &commandStream, const Kernel &kernel);

template struct EnqueueOperation<Family>;

} // namespace NEO

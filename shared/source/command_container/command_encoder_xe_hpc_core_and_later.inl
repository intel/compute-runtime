/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/xe_hpc_core/hw_cmds.h"

namespace NEO {

template <>
size_t EncodeMemoryFence<Family>::getSystemMemoryFenceSize() {
    return sizeof(typename Family::STATE_SYSTEM_MEM_FENCE_ADDRESS);
}

template <>
void EncodeMemoryFence<Family>::encodeSystemMemoryFence(LinearStream &commandStream, const GraphicsAllocation *globalFenceAllocation) {
    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename Family::STATE_SYSTEM_MEM_FENCE_ADDRESS;

    auto stateSystemFenceAddressSpace = commandStream.getSpaceForCmd<STATE_SYSTEM_MEM_FENCE_ADDRESS>();
    STATE_SYSTEM_MEM_FENCE_ADDRESS stateSystemFenceAddress = Family::cmdInitStateSystemMemFenceAddress;
    stateSystemFenceAddress.setSystemMemoryFenceAddress(globalFenceAllocation->getGpuAddress());
    *stateSystemFenceAddressSpace = stateSystemFenceAddress;
}

template <>
void EncodeBatchBufferStartOrEnd<Family>::appendBatchBufferStart(MI_BATCH_BUFFER_START &cmd, bool indirect, bool predicate) {
    cmd.setIndirectAddressEnable(indirect);
    cmd.setPredicationEnable(predicate);
}

template <>
template <typename WalkerType, typename InterfaceDescriptorType>
void EncodeDispatchKernel<Family>::adjustInterfaceDescriptorData(InterfaceDescriptorType &interfaceDescriptor, const Device &device, const HardwareInfo &hwInfo, const uint32_t threadGroupCount, const uint32_t grfCount, WalkerType &walkerCmd) {
    EncodeDispatchKernel<Family>::adjustInterfaceDescriptorDataForOverdispatch(interfaceDescriptor, device, hwInfo, threadGroupCount, grfCount, walkerCmd);
}

} // namespace NEO
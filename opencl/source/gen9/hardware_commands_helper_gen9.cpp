/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds.h"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/helpers/hardware_commands_helper_base.inl"
#include "opencl/source/helpers/hardware_commands_helper_bdw_and_later.inl"

#include <cstdint>

namespace NEO {
using FamilyType = Gen9Family;

template struct HardwareCommandsHelper<FamilyType>;
template size_t HardwareCommandsHelper<FamilyType>::sendIndirectState<FamilyType::DefaultWalkerType, FamilyType::INTERFACE_DESCRIPTOR_DATA>(
    LinearStream &commandStream,
    IndirectHeap &dsh,
    IndirectHeap &ioh,
    IndirectHeap &ssh,
    Kernel &kernel,
    uint64_t kernelStartOffset,
    uint32_t simd,
    const size_t localWorkSize[3],
    const uint32_t threadGroupCount,
    const uint64_t offsetInterfaceDescriptorTable,
    uint32_t &interfaceDescriptorIndex,
    PreemptionMode preemptionMode,
    FamilyType::DefaultWalkerType *walkerCmd,
    FamilyType::INTERFACE_DESCRIPTOR_DATA *inlineInterfaceDescriptor,
    bool localIdsGenerationByRuntime,
    uint64_t scratchAddress,
    const Device &device);

template size_t HardwareCommandsHelper<FamilyType>::sendCrossThreadData<FamilyType::DefaultWalkerType>(
    IndirectHeap &indirectHeap,
    Kernel &kernel,
    bool inlineDataProgrammingRequired,
    FamilyType::DefaultWalkerType *walkerCmd,
    uint32_t &sizeCrossThreadData,
    uint64_t scratchAddress);

template size_t HardwareCommandsHelper<FamilyType>::sendInterfaceDescriptorData<FamilyType::DefaultWalkerType, FamilyType::INTERFACE_DESCRIPTOR_DATA>(
    const IndirectHeap &indirectHeap,
    uint64_t offsetInterfaceDescriptor,
    uint64_t kernelStartOffset,
    size_t sizeCrossThreadData,
    size_t sizePerThreadData,
    size_t bindingTablePointer,
    [[maybe_unused]] size_t offsetSamplerState,
    uint32_t numSamplers,
    const uint32_t threadGroupCount,
    uint32_t numThreadsPerThreadGroup,
    const Kernel &kernel,
    uint32_t bindingTablePrefetchSize,
    PreemptionMode preemptionMode,
    const Device &device,
    FamilyType::DefaultWalkerType *walkerCmd,
    FamilyType::INTERFACE_DESCRIPTOR_DATA *inlineInterfaceDescriptor);

template void HardwareCommandsHelper<FamilyType>::programInlineData<FamilyType::DefaultWalkerType>(
    Kernel &kernel,
    FamilyType::DefaultWalkerType *walkerCmd, uint64_t indirectDataAddress, uint64_t scratchAddress);

} // namespace NEO

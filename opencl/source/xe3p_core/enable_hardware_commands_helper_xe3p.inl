/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/hardware_commands_helper.h"

template size_t NEO::HardwareCommandsHelper<NEO::FamilyType>::sendIndirectState<NEO::FamilyType::COMPUTE_WALKER_2, NEO::FamilyType::INTERFACE_DESCRIPTOR_DATA_2>(
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
    FamilyType::COMPUTE_WALKER_2 *walkerCmd,
    FamilyType::INTERFACE_DESCRIPTOR_DATA_2 *inlineInterfaceDescriptor,
    bool localIdsGenerationByRuntime,
    uint64_t scratchAddress,
    const Device &device);

template size_t NEO::HardwareCommandsHelper<NEO::FamilyType>::sendCrossThreadData<NEO::FamilyType::COMPUTE_WALKER_2>(
    IndirectHeap &indirectHeap,
    Kernel &kernel,
    bool inlineDataProgrammingRequired,
    FamilyType::COMPUTE_WALKER_2 *walkerCmd,
    uint32_t &sizeCrossThreadData,
    uint64_t scratchAddress,
    const RootDeviceEnvironment &rootDeviceEnvironment);

template size_t NEO::HardwareCommandsHelper<NEO::FamilyType>::sendInterfaceDescriptorData<NEO::FamilyType::COMPUTE_WALKER, NEO::FamilyType::INTERFACE_DESCRIPTOR_DATA>(
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
    FamilyType::COMPUTE_WALKER *walkerCmd,
    FamilyType::INTERFACE_DESCRIPTOR_DATA *inlineInterfaceDescriptor);

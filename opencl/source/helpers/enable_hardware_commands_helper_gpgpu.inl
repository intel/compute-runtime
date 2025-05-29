/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hardware_commands_helper.h"

template struct NEO::HardwareCommandsHelper<NEO::FamilyType>;
template size_t NEO::HardwareCommandsHelper<NEO::FamilyType>::sendIndirectState<NEO::FamilyType::GPGPU_WALKER, NEO::FamilyType::INTERFACE_DESCRIPTOR_DATA>(
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
    FamilyType::GPGPU_WALKER *walkerCmd,
    FamilyType::INTERFACE_DESCRIPTOR_DATA *inlineInterfaceDescriptor,
    bool localIdsGenerationByRuntime,
    uint64_t scratchAddress,
    const Device &device);

template size_t NEO::HardwareCommandsHelper<NEO::FamilyType>::sendCrossThreadData<NEO::FamilyType::GPGPU_WALKER>(
    IndirectHeap &indirectHeap,
    Kernel &kernel,
    bool inlineDataProgrammingRequired,
    FamilyType::GPGPU_WALKER *walkerCmd,
    uint32_t &sizeCrossThreadData,
    uint64_t scratchAddress,
    const RootDeviceEnvironment &rootDeviceEnvironment);

template size_t NEO::HardwareCommandsHelper<NEO::FamilyType>::sendInterfaceDescriptorData<NEO::FamilyType::GPGPU_WALKER, NEO::FamilyType::INTERFACE_DESCRIPTOR_DATA>(
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
    FamilyType::GPGPU_WALKER *walkerCmd,
    FamilyType::INTERFACE_DESCRIPTOR_DATA *inlineInterfaceDescriptor);

template void NEO::HardwareCommandsHelper<NEO::FamilyType>::programInlineData<NEO::FamilyType::GPGPU_WALKER>(
    Kernel &kernel,
    FamilyType::GPGPU_WALKER *walkerCmd, uint64_t indirectDataAddress, uint64_t scratchAddress);

template void NEO::HardwareCommandsHelper<NEO::FamilyType>::setInterfaceDescriptorOffset<NEO::FamilyType::GPGPU_WALKER>(
    FamilyType::GPGPU_WALKER *walkerCmd,
    uint32_t &interfaceDescriptorIndex);

template NEO::FamilyType::DefaultWalkerType::InterfaceDescriptorType *
NEO::HardwareCommandsHelper<NEO::FamilyType>::getInterfaceDescriptor<NEO::FamilyType::DefaultWalkerType::InterfaceDescriptorType>(
    const NEO::IndirectHeap &,
    uint64_t,
    NEO::FamilyType::DefaultWalkerType::InterfaceDescriptorType *);

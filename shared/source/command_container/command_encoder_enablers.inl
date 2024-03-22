/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/command_encoder.h"

template struct NEO::EncodeDispatchKernel<Family>;
template void NEO::EncodeDispatchKernel<Family>::encodeAdditionalWalkerFields<Family::DefaultWalkerType>(const RootDeviceEnvironment &rootDeviceEnvironment, Family::DefaultWalkerType &walkerCmd, const EncodeWalkerArgs &walkerArgs);
template void NEO::EncodeDispatchKernel<Family>::adjustTimestampPacket<Family::DefaultWalkerType>(Family::DefaultWalkerType &walkerCmd, const EncodeDispatchKernelArgs &args);
template void NEO::EncodeDispatchKernel<Family>::setupPostSyncForRegularEvent<Family::DefaultWalkerType>(Family::DefaultWalkerType &walkerCmd, const EncodeDispatchKernelArgs &args);
template void NEO::EncodeDispatchKernel<Family>::setupPostSyncForInOrderExec<Family::DefaultWalkerType>(Family::DefaultWalkerType &walkerCmd, const EncodeDispatchKernelArgs &args);
template void NEO::EncodeDispatchKernel<Family>::setGrfInfo<Family::INTERFACE_DESCRIPTOR_DATA>(Family::INTERFACE_DESCRIPTOR_DATA *pInterfaceDescriptor, uint32_t grfCount, const size_t &sizeCrossThreadData, const size_t &sizePerThreadData, const RootDeviceEnvironment &rootDeviceEnvironment);
template void NEO::EncodeDispatchKernel<Family>::appendAdditionalIDDFields<Family::INTERFACE_DESCRIPTOR_DATA>(Family::INTERFACE_DESCRIPTOR_DATA *pInterfaceDescriptor, const RootDeviceEnvironment &rootDeviceEnvironment, const uint32_t threadsPerThreadGroup, uint32_t slmTotalSize, SlmPolicy slmPolicy);
template void NEO::EncodeDispatchKernel<Family>::adjustInterfaceDescriptorData<Family::DefaultWalkerType, Family::INTERFACE_DESCRIPTOR_DATA>(Family::INTERFACE_DESCRIPTOR_DATA &interfaceDescriptor, const Device &device, const HardwareInfo &hwInfo, const uint32_t threadGroupCount, const uint32_t grfCount, Family::DefaultWalkerType &walkerCmd);
template void NEO::EncodeDispatchKernel<Family>::setupPostSyncMocs<Family::DefaultWalkerType>(Family::DefaultWalkerType &walkerCmd, const RootDeviceEnvironment &rootDeviceEnvironment, bool dcFlush);
template void NEO::EncodeDispatchKernel<Family>::encode<Family::DefaultWalkerType>(CommandContainer &container, EncodeDispatchKernelArgs &args);
template void NEO::EncodeDispatchKernel<Family>::encodeThreadData<Family::DefaultWalkerType>(Family::DefaultWalkerType &walkerCmd, const uint32_t *startWorkGroup, const uint32_t *numWorkGroups, const uint32_t *workGroupSizes, uint32_t simd, uint32_t localIdDimensions, uint32_t threadsPerThreadGroup, uint32_t threadExecutionMask, bool localIdsGenerationByRuntime, bool inlineDataProgrammingRequired, bool isIndirect, uint32_t requiredWorkGroupOrder, const RootDeviceEnvironment &rootDeviceEnvironment);
template void NEO::EncodeDispatchKernel<Family>::adjustWalkOrder<Family::DefaultWalkerType>(Family::DefaultWalkerType &walkerCmd, uint32_t requiredWorkGroupOrder, const RootDeviceEnvironment &rootDeviceEnvironment);
template void NEO::EncodeDispatchKernel<Family>::programBarrierEnable<Family::INTERFACE_DESCRIPTOR_DATA>(Family::INTERFACE_DESCRIPTOR_DATA &interfaceDescriptor, uint32_t value, const HardwareInfo &hwInfo);
template void NEO::EncodeDispatchKernel<Family>::setScratchAddress<false>(uint64_t &scratchAddress, uint32_t requiredScratchSlot0Size, uint32_t requiredScratchSlot1Size, IndirectHeap *ssh, CommandStreamReceiver &csr);
template void NEO::EncodeDispatchKernel<Family>::setScratchAddress<true>(uint64_t &scratchAddress, uint32_t requiredScratchSlot0Size, uint32_t requiredScratchSlot1Size, IndirectHeap *ssh, CommandStreamReceiver &csr);

template struct NEO::EncodeStates<Family>;
template struct NEO::EncodeMath<Family>;
template struct NEO::EncodeMathMMIO<Family>;
template struct NEO::EncodeIndirectParams<Family>;
template struct NEO::EncodeSetMMIO<Family>;
template struct NEO::EncodeMediaInterfaceDescriptorLoad<Family>;
template struct NEO::EncodeStateBaseAddress<Family>;
template struct NEO::EncodeStoreMMIO<Family>;
template struct NEO::EncodeSurfaceState<Family>;
template struct NEO::EncodeComputeMode<Family>;
template struct NEO::EncodeAtomic<Family>;
template struct NEO::EncodeSemaphore<Family>;
template struct NEO::EncodeBatchBufferStartOrEnd<Family>;
template struct NEO::EncodeMiFlushDW<Family>;
template struct NEO::EncodeMiPredicate<Family>;
template struct NEO::EncodeMemoryPrefetch<Family>;
template struct NEO::EncodeMiArbCheck<Family>;
template struct NEO::EncodeWA<Family>;
template struct NEO::EncodeEnableRayTracing<Family>;
template struct NEO::EncodeNoop<Family>;
template struct NEO::EncodeStoreMemory<Family>;
template struct NEO::EncodeMemoryFence<Family>;
template struct NEO::EnodeUserInterrupt<Family>;

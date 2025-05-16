/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"

template struct NEO::EncodeDispatchKernel<Family>;
template void NEO::EncodeDispatchKernel<Family>::encodeAdditionalWalkerFields<Family::DefaultWalkerType>(const RootDeviceEnvironment &rootDeviceEnvironment, Family::DefaultWalkerType &walkerCmd, const EncodeWalkerArgs &walkerArgs);
template void NEO::EncodeDispatchKernel<Family>::setGrfInfo<Family::DefaultWalkerType::InterfaceDescriptorType>(Family::DefaultWalkerType::InterfaceDescriptorType *pInterfaceDescriptor, uint32_t grfCount, const size_t &sizeCrossThreadData, const size_t &sizePerThreadData, const RootDeviceEnvironment &rootDeviceEnvironment);
template void NEO::EncodeDispatchKernel<Family>::setupPreferredSlmSize<Family::DefaultWalkerType::InterfaceDescriptorType>(Family::DefaultWalkerType::InterfaceDescriptorType *pInterfaceDescriptor, const RootDeviceEnvironment &rootDeviceEnvironment, const uint32_t threadsPerThreadGroup, uint32_t slmTotalSize, SlmPolicy slmPolicy);
template void NEO::EncodeDispatchKernel<Family>::encodeThreadGroupDispatch<Family::DefaultWalkerType, Family::DefaultWalkerType::InterfaceDescriptorType>(Family::DefaultWalkerType::InterfaceDescriptorType &interfaceDescriptor, const Device &device, const HardwareInfo &hwInfo, const uint32_t *threadGroupDimensions, const uint32_t threadGroupCount, const uint32_t requiredThreadGroupDispatchSize, const uint32_t grfCount, const uint32_t threadsPerThreadGroup, Family::DefaultWalkerType &walkerCmd);
template void NEO::EncodeDispatchKernel<Family>::encode<Family::DefaultWalkerType>(CommandContainer &container, EncodeDispatchKernelArgs &args);
template void NEO::EncodeDispatchKernel<Family>::encodeThreadData<Family::DefaultWalkerType>(Family::DefaultWalkerType &walkerCmd, const uint32_t *startWorkGroup, const uint32_t *numWorkGroups, const uint32_t *workGroupSizes, uint32_t simd, uint32_t localIdDimensions, uint32_t threadsPerThreadGroup, uint32_t threadExecutionMask, bool localIdsGenerationByRuntime, bool inlineDataProgrammingRequired, bool isIndirect, uint32_t requiredWorkGroupOrder, const RootDeviceEnvironment &rootDeviceEnvironment);
template void NEO::EncodeDispatchKernel<Family>::adjustWalkOrder<Family::DefaultWalkerType>(Family::DefaultWalkerType &walkerCmd, uint32_t requiredWorkGroupOrder, const RootDeviceEnvironment &rootDeviceEnvironment);
template void NEO::EncodeDispatchKernel<Family>::programBarrierEnable<Family::DefaultWalkerType::InterfaceDescriptorType>(Family::DefaultWalkerType::InterfaceDescriptorType &interfaceDescriptor, const KernelDescriptor &kernelDescriptor, const HardwareInfo &hwInfo);
template void NEO::EncodeDispatchKernel<Family>::setScratchAddress<false>(uint64_t &scratchAddress, uint32_t requiredScratchSlot0Size, uint32_t requiredScratchSlot1Size, IndirectHeap *ssh, CommandStreamReceiver &submissionCsr);
template void NEO::EncodeDispatchKernel<Family>::setScratchAddress<true>(uint64_t &scratchAddress, uint32_t requiredScratchSlot0Size, uint32_t requiredScratchSlot1Size, IndirectHeap *ssh, CommandStreamReceiver &submissionCsr);
template void NEO::EncodeDispatchKernel<Family>::programInlineDataHeapless<false>(uint8_t *inlineDataPtr, EncodeDispatchKernelArgs &args, CommandContainer &container, uint64_t offsetThreadData, uint64_t scratchPtr);
template void NEO::EncodeDispatchKernel<Family>::encodeEuSchedulingPolicy<Family::DefaultWalkerType::InterfaceDescriptorType>(Family::DefaultWalkerType::InterfaceDescriptorType *pInterfaceDescriptor, const KernelDescriptor &kernelDesc, int32_t defaultPipelinedThreadArbitrationPolicy);
template uint64_t NEO::EncodeDispatchKernel<Family>::getScratchAddressForImmediatePatching<false>(CommandContainer &container, EncodeDispatchKernelArgs &args);
template void NEO::EncodeDispatchKernel<Family>::patchScratchAddressInImplicitArgs<false>(ImplicitArgs &implicitArgs, uint64_t scratchAddress, bool scratchPtrPatchingRequired);
template void NEO::EncodeDispatchKernel<Family>::forceComputeWalkerPostSyncFlushWithWrite<Family::DefaultWalkerType>(Family::DefaultWalkerType &walkerCmd);
template void NEO::EncodeDispatchKernel<Family>::setWalkerRegionSettings<Family::DefaultWalkerType>(Family::DefaultWalkerType &walkerCmd, const NEO::Device &device, uint32_t partitionCount,
                                                                                                    uint32_t workgroupSize, uint32_t threadGroupCount, uint32_t maxWgCountPerTile, bool requiredDispatchWalkOrder);
template void NEO::EncodeDispatchKernel<Family>::overrideDefaultValues<Family::DefaultWalkerType, Family::DefaultWalkerType::InterfaceDescriptorType>(Family::DefaultWalkerType &walkerCmd, Family::DefaultWalkerType::InterfaceDescriptorType &interfaceDescriptor);
template void NEO::EncodeDispatchKernel<Family>::encodeWalkerPostSyncFields<Family::DefaultWalkerType>(Family::DefaultWalkerType &walkerCmd, const RootDeviceEnvironment &rootDeviceEnvironment, const EncodeWalkerArgs &walkerArgs);
template void NEO::EncodeDispatchKernel<Family>::encodeComputeDispatchAllWalker<Family::DefaultWalkerType, Family::DefaultWalkerType::InterfaceDescriptorType>(Family::DefaultWalkerType &walkerCmd, const Family::DefaultWalkerType::InterfaceDescriptorType *idd, const RootDeviceEnvironment &rootDeviceEnvironment, const EncodeWalkerArgs &walkerArgs);

template struct NEO::EncodePostSync<Family>;

template void NEO::EncodePostSync<Family>::adjustTimestampPacket<Family::DefaultWalkerType>(Family::DefaultWalkerType &walkerCmd, const EncodePostSyncArgs &args);
template void NEO::EncodePostSync<Family>::setupPostSyncForRegularEvent<Family::DefaultWalkerType>(Family::DefaultWalkerType &walkerCmd, const EncodePostSyncArgs &args);
template void NEO::EncodePostSync<Family>::encodeL3Flush<Family::DefaultWalkerType>(Family::DefaultWalkerType &walkerCmd, const EncodePostSyncArgs &args);
template void NEO::EncodePostSync<Family>::setupPostSyncForInOrderExec<Family::DefaultWalkerType>(Family::DefaultWalkerType &walkerCmd, const EncodePostSyncArgs &args);

template struct NEO::EncodeStates<Family>;
template struct NEO::EncodeMediaInterfaceDescriptorLoad<Family>;

template struct NEO::EncodeMath<Family>;
template struct NEO::EncodeMathMMIO<Family>;
template struct NEO::EncodeIndirectParams<Family>;
template struct NEO::EncodeSetMMIO<Family>;
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

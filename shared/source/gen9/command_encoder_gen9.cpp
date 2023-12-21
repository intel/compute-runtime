/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/gen9/hw_cmds_base.h"
#include "shared/source/gen9/reg_configs.h"

using Family = NEO::Gen9Family;

#include "shared/source/command_container/command_encoder.inl"
#include "shared/source/command_container/command_encoder_bdw_and_later.inl"
#include "shared/source/command_container/encode_compute_mode_bdw_and_later.inl"
#include "shared/source/command_container/image_surface_state/compression_params_bdw_and_later.inl"

#include "stream_properties.inl"

namespace NEO {
template <>
void EncodeSurfaceState<Family>::setAuxParamsForMCSCCS(R_SURFACE_STATE *surfaceState, const ReleaseHelper *releaseHelper) {
}

template <>
void EncodeSurfaceState<Family>::setClearColorParams(R_SURFACE_STATE *surfaceState, Gmm *gmm) {
}

template <>
void EncodeSurfaceState<Family>::setFlagsForMediaCompression(R_SURFACE_STATE *surfaceState, Gmm *gmm) {
    if (gmm->gmmResourceInfo->getResourceFlags()->Info.MediaCompressed) {
        surfaceState->setAuxiliarySurfaceMode(Family::RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
    }
}
template <typename Family>
size_t EncodeComputeMode<Family>::getCmdSizeForComputeMode(const RootDeviceEnvironment &rootDeviceEnvironment, bool hasSharedHandles, bool isRcs) {
    return sizeof(typename Family::PIPE_CONTROL) + sizeof(typename Family::MI_LOAD_REGISTER_IMM);
}

template <typename Family>
void EncodeComputeMode<Family>::programComputeModeCommand(LinearStream &csr, StateComputeModeProperties &properties,
                                                          const RootDeviceEnvironment &rootDeviceEnvironment) {
    using PIPE_CONTROL = typename Family::PIPE_CONTROL;
    UNRECOVERABLE_IF(properties.threadArbitrationPolicy.value == ThreadArbitrationPolicy::NotPresent);

    if (properties.threadArbitrationPolicy.isDirty) {
        PipeControlArgs args;
        args.csStallOnly = true;
        MemorySynchronizationCommands<Family>::addSingleBarrier(csr, args);

        LriHelper<Gen9Family>::program(&csr,
                                       DebugControlReg2::address,
                                       DebugControlReg2::getRegData(properties.threadArbitrationPolicy.value),
                                       false);
    }
}

template struct EncodeDispatchKernel<Family>;
template void EncodeDispatchKernel<Family>::encodeAdditionalWalkerFields<Family::DefaultWalkerType>(const RootDeviceEnvironment &rootDeviceEnvironment, Family::DefaultWalkerType &walkerCmd, const EncodeWalkerArgs &walkerArgs);
template void EncodeDispatchKernel<Family>::adjustTimestampPacket<Family::DefaultWalkerType>(Family::DefaultWalkerType &walkerCmd, const EncodeDispatchKernelArgs &args);
template void EncodeDispatchKernel<Family>::setupPostSyncForRegularEvent<Family::DefaultWalkerType>(Family::DefaultWalkerType &walkerCmd, const EncodeDispatchKernelArgs &args);
template void EncodeDispatchKernel<Family>::setupPostSyncForInOrderExec<Family::DefaultWalkerType>(Family::DefaultWalkerType &walkerCmd, const EncodeDispatchKernelArgs &args);
template void EncodeDispatchKernel<Family>::setGrfInfo<Family::INTERFACE_DESCRIPTOR_DATA>(Family::INTERFACE_DESCRIPTOR_DATA *pInterfaceDescriptor, uint32_t numGrf, const size_t &sizeCrossThreadData, const size_t &sizePerThreadData, const RootDeviceEnvironment &rootDeviceEnvironment);
template void EncodeDispatchKernel<Family>::appendAdditionalIDDFields<Family::INTERFACE_DESCRIPTOR_DATA>(Family::INTERFACE_DESCRIPTOR_DATA *pInterfaceDescriptor, const RootDeviceEnvironment &rootDeviceEnvironment, const uint32_t threadsPerThreadGroup, uint32_t slmTotalSize, SlmPolicy slmPolicy);
template void EncodeDispatchKernel<Family>::programBarrierEnable<Family::INTERFACE_DESCRIPTOR_DATA>(Family::INTERFACE_DESCRIPTOR_DATA &interfaceDescriptor, uint32_t value, const HardwareInfo &hwInfo);
template void EncodeDispatchKernel<Family>::adjustInterfaceDescriptorData<Family::DefaultWalkerType, Family::INTERFACE_DESCRIPTOR_DATA>(Family::INTERFACE_DESCRIPTOR_DATA &interfaceDescriptor, const Device &device, const HardwareInfo &hwInfo, const uint32_t threadGroupCount, const uint32_t numGrf, Family::DefaultWalkerType &walkerCmd);
template void EncodeDispatchKernel<Family>::setupPostSyncMocs<Family::DefaultWalkerType>(Family::DefaultWalkerType &walkerCmd, const RootDeviceEnvironment &rootDeviceEnvironment, bool dcFlush);
template void EncodeDispatchKernel<Family>::encode<Family::DefaultWalkerType>(CommandContainer &container, EncodeDispatchKernelArgs &args);
template void EncodeDispatchKernel<Family>::encodeThreadData<Family::DefaultWalkerType>(Family::DefaultWalkerType &walkerCmd, const uint32_t *startWorkGroup, const uint32_t *numWorkGroups, const uint32_t *workGroupSizes, uint32_t simd, uint32_t localIdDimensions, uint32_t threadsPerThreadGroup, uint32_t threadExecutionMask, bool localIdsGenerationByRuntime, bool inlineDataProgrammingRequired, bool isIndirect, uint32_t requiredWorkGroupOrder, const RootDeviceEnvironment &rootDeviceEnvironment);
template void EncodeDispatchKernel<Family>::adjustWalkOrder<Family::DefaultWalkerType>(Family::DefaultWalkerType &walkerCmd, uint32_t requiredWorkGroupOrder, const RootDeviceEnvironment &rootDeviceEnvironment);

template void InOrderPatchCommandHelpers::PatchCmd<Family>::patchComputeWalker(uint64_t appendCounterValue);

template struct EncodeStates<Family>;
template struct EncodeMath<Family>;
template struct EncodeMathMMIO<Family>;
template struct EncodeIndirectParams<Family>;
template struct EncodeSetMMIO<Family>;
template struct EncodeL3State<Family>;
template struct EncodeMediaInterfaceDescriptorLoad<Family>;
template struct EncodeStateBaseAddress<Family>;
template struct EncodeStoreMMIO<Family>;
template struct EncodeSurfaceState<Family>;
template struct EncodeAtomic<Family>;
template struct EncodeSemaphore<Family>;
template struct EncodeBatchBufferStartOrEnd<Family>;
template struct EncodeMiFlushDW<Family>;
template struct EncodeMiPredicate<Family>;
template struct EncodeMemoryPrefetch<Family>;
template struct EncodeWA<Family>;
template struct EncodeMiArbCheck<Family>;
template struct EncodeComputeMode<Family>;
template struct EncodeEnableRayTracing<Family>;
template struct EncodeNoop<Family>;
template struct EncodeStoreMemory<Family>;
template struct EncodeMemoryFence<Family>;
template struct EnodeUserInterrupt<Family>;
} // namespace NEO

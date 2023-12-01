/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/gen8/hw_cmds_base.h"
#include "shared/source/gen8/reg_configs.h"

using Family = NEO::Gen8Family;

#include "shared/source/command_container/command_encoder.inl"
#include "shared/source/command_container/command_encoder_bdw_and_later.inl"
#include "shared/source/command_container/encode_compute_mode_bdw_and_later.inl"
#include "shared/source/command_container/image_surface_state/compression_params_bdw_and_later.inl"

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
    return 0u;
}

template <typename Family>
void EncodeComputeMode<Family>::programComputeModeCommand(LinearStream &csr, StateComputeModeProperties &properties, const RootDeviceEnvironment &rootDeviceEnvironment) {
}

template <>
void EncodeStateBaseAddress<Family>::setSbaAddressesForDebugger(NEO::Debugger::SbaAddresses &sbaAddress, const STATE_BASE_ADDRESS &sbaCmd) {
    sbaAddress.indirectObjectBaseAddress = sbaCmd.getIndirectObjectBaseAddress();
    sbaAddress.dynamicStateBaseAddress = sbaCmd.getDynamicStateBaseAddress();
    sbaAddress.generalStateBaseAddress = sbaCmd.getGeneralStateBaseAddress();
    sbaAddress.instructionBaseAddress = sbaCmd.getInstructionBaseAddress();
    sbaAddress.surfaceStateBaseAddress = sbaCmd.getSurfaceStateBaseAddress();
}

template struct EncodeDispatchKernel<Family>;
template void EncodeDispatchKernel<Family>::encodeAdditionalWalkerFields<Family::WALKER_TYPE>(const RootDeviceEnvironment &rootDeviceEnvironment, Family::WALKER_TYPE &walkerCmd, const EncodeWalkerArgs &walkerArgs);
template void EncodeDispatchKernel<Family>::adjustTimestampPacket<Family::WALKER_TYPE>(Family::WALKER_TYPE &walkerCmd, const HardwareInfo &hwInfo);
template void EncodeDispatchKernel<Family>::setGrfInfo<Family::INTERFACE_DESCRIPTOR_DATA>(Family::INTERFACE_DESCRIPTOR_DATA *pInterfaceDescriptor, uint32_t numGrf, const size_t &sizeCrossThreadData, const size_t &sizePerThreadData, const RootDeviceEnvironment &rootDeviceEnvironment);
template void EncodeDispatchKernel<Family>::appendAdditionalIDDFields<Family::INTERFACE_DESCRIPTOR_DATA>(Family::INTERFACE_DESCRIPTOR_DATA *pInterfaceDescriptor, const RootDeviceEnvironment &rootDeviceEnvironment, const uint32_t threadsPerThreadGroup, uint32_t slmTotalSize, SlmPolicy slmPolicy);
template void EncodeDispatchKernel<Family>::programBarrierEnable<Family::INTERFACE_DESCRIPTOR_DATA>(Family::INTERFACE_DESCRIPTOR_DATA &interfaceDescriptor, uint32_t value, const HardwareInfo &hwInfo);
template void EncodeDispatchKernel<Family>::adjustInterfaceDescriptorData<Family::WALKER_TYPE, Family::INTERFACE_DESCRIPTOR_DATA>(Family::INTERFACE_DESCRIPTOR_DATA &interfaceDescriptor, const Device &device, const HardwareInfo &hwInfo, const uint32_t threadGroupCount, const uint32_t numGrf, Family::WALKER_TYPE &walkerCmd);
template void EncodeDispatchKernel<Family>::setupPostSyncMocs<Family::WALKER_TYPE>(Family::WALKER_TYPE &walkerCmd, const RootDeviceEnvironment &rootDeviceEnvironment, bool dcFlush);
template void EncodeDispatchKernel<Family>::encode<Family::WALKER_TYPE>(CommandContainer &container, EncodeDispatchKernelArgs &args);
template void EncodeDispatchKernel<Family>::encodeThreadData<Family::WALKER_TYPE>(Family::WALKER_TYPE &walkerCmd, const uint32_t *startWorkGroup, const uint32_t *numWorkGroups, const uint32_t *workGroupSizes, uint32_t simd, uint32_t localIdDimensions, uint32_t threadsPerThreadGroup, uint32_t threadExecutionMask, bool localIdsGenerationByRuntime, bool inlineDataProgrammingRequired, bool isIndirect, uint32_t requiredWorkGroupOrder, const RootDeviceEnvironment &rootDeviceEnvironment);
template void EncodeDispatchKernel<Family>::adjustWalkOrder<Family::WALKER_TYPE>(Family::WALKER_TYPE &walkerCmd, uint32_t requiredWorkGroupOrder, const RootDeviceEnvironment &rootDeviceEnvironment);

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

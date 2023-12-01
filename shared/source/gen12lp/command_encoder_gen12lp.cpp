/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/gen12lp/hw_cmds_base.h"
#include "shared/source/gen12lp/reg_configs.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/release_helper/release_helper.h"

#include "encode_surface_state_args.h"

using Family = NEO::Gen12LpFamily;

#include "shared/source/command_container/command_encoder.inl"
#include "shared/source/command_container/command_encoder_bdw_and_later.inl"
#include "shared/source/command_container/command_encoder_tgllp_and_later.inl"
#include "shared/source/command_container/encode_compute_mode_tgllp_and_later.inl"
#include "shared/source/command_container/image_surface_state/compression_params_bdw_and_later.inl"
#include "shared/source/command_container/image_surface_state/compression_params_tgllp_and_later.inl"
#include "shared/source/command_stream/command_stream_receiver.h"

namespace NEO {

template <>
size_t EncodeWA<Family>::getAdditionalPipelineSelectSize(Device &device, bool isRcs) {
    size_t size = 0;
    const auto &productHelper = device.getProductHelper();
    if (isRcs && productHelper.is3DPipelineSelectWARequired()) {
        size += 2 * PreambleHelper<Family>::getCmdSizeForPipelineSelect(device.getRootDeviceEnvironment());
    }
    return size;
}

template <>
void EncodeComputeMode<Family>::programComputeModeCommand(LinearStream &csr, StateComputeModeProperties &properties, const RootDeviceEnvironment &rootDeviceEnvironment) {
    using STATE_COMPUTE_MODE = typename Family::STATE_COMPUTE_MODE;
    using FORCE_NON_COHERENT = typename STATE_COMPUTE_MODE::FORCE_NON_COHERENT;

    STATE_COMPUTE_MODE stateComputeMode = Family::cmdInitStateComputeMode;
    auto maskBits = stateComputeMode.getMaskBits();

    FORCE_NON_COHERENT coherencyValue = (properties.isCoherencyRequired.value == 1) ? FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_DISABLED
                                                                                    : FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT;
    stateComputeMode.setForceNonCoherent(coherencyValue);
    maskBits |= Family::stateComputeModeForceNonCoherentMask;

    stateComputeMode.setMaskBits(maskBits);

    auto buffer = csr.getSpace(sizeof(STATE_COMPUTE_MODE));
    *reinterpret_cast<STATE_COMPUTE_MODE *>(buffer) = stateComputeMode;
}

template <>
void EncodeWA<Family>::encodeAdditionalPipelineSelect(LinearStream &stream, const PipelineSelectArgs &args,
                                                      bool is3DPipeline, const RootDeviceEnvironment &rootDeviceEnvironment, bool isRcs) {
    const auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    if (productHelper.is3DPipelineSelectWARequired() && isRcs) {
        PipelineSelectArgs pipelineSelectArgs = args;
        pipelineSelectArgs.is3DPipelineRequired = is3DPipeline;
        PreambleHelper<Family>::programPipelineSelect(&stream, pipelineSelectArgs, rootDeviceEnvironment);
    }
}

template <>
void EncodeSurfaceState<Family>::encodeExtraBufferParams(EncodeSurfaceStateArgs &args) {
    auto surfaceState = reinterpret_cast<R_SURFACE_STATE *>(args.outMemory);
    const bool isL3Allowed = surfaceState->getMemoryObjectControlState() == args.gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
    if (isL3Allowed) {
        const bool isConstantSurface = args.allocation && args.allocation->getAllocationType() == AllocationType::CONSTANT_SURFACE;
        bool useL1 = args.isReadOnly || isConstantSurface;

        if (debugManager.flags.ForceL1Caching.get() != 1) {
            useL1 = false;
        }

        if (useL1) {
            surfaceState->setMemoryObjectControlState(args.gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST));
        }
    }
}

template <>
bool EncodeSurfaceState<Family>::isBindingTablePrefetchPreferred() {
    return false;
}

template <>
void EncodeL3State<Family>::encode(CommandContainer &container, bool enableSLM) {
}

template <>
void EncodeStoreMMIO<Family>::appendFlags(MI_STORE_REGISTER_MEM *storeRegMem, bool workloadPartition) {
    storeRegMem->setMmioRemapEnable(true);
}

template <>
void EncodeComputeMode<Family>::adjustPipelineSelect(CommandContainer &container, const NEO::KernelDescriptor &kernelDescriptor) {

    PipelineSelectArgs pipelineSelectArgs;
    pipelineSelectArgs.systolicPipelineSelectMode = kernelDescriptor.kernelAttributes.flags.usesSystolicPipelineSelectMode;
    pipelineSelectArgs.systolicPipelineSelectSupport = container.systolicModeSupportRef();

    PreambleHelper<Family>::programPipelineSelect(container.getCommandStream(),
                                                  pipelineSelectArgs,
                                                  container.getDevice()->getRootDeviceEnvironment());
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
template struct EncodeWA<Family>;
template struct EncodeMemoryPrefetch<Family>;
template struct EncodeMiArbCheck<Family>;
template struct EncodeComputeMode<Family>;
template struct EncodeEnableRayTracing<Family>;
template struct EncodeNoop<Family>;
template struct EncodeStoreMemory<Family>;
template struct EncodeMemoryFence<Family>;
template struct EnodeUserInterrupt<Family>;
} // namespace NEO

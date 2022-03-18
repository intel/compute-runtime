/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/command_encoder.inl"
#include "shared/source/command_container/command_encoder_raytracing_xehp_and_later.inl"
#include "shared/source/command_container/command_encoder_xehp_and_later.inl"
#include "shared/source/command_container/encode_compute_mode_tgllp_and_later.inl"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/xe_hp_core/hw_cmds_base.h"

using Family = NEO::XeHpFamily;

#include "shared/source/command_container/command_encoder_tgllp_and_later.inl"
#include "shared/source/command_container/image_surface_state/compression_params_tgllp_and_later.inl"
#include "shared/source/command_container/image_surface_state/compression_params_xehp_and_later.inl"

namespace NEO {

template <>
void EncodeDispatchKernel<Family>::adjustTimestampPacket(WALKER_TYPE &walkerCmd, const HardwareInfo &hwInfo) {
}

template <>
inline void EncodeSurfaceState<Family>::encodeExtraCacheSettings(R_SURFACE_STATE *surfaceState, const HardwareInfo &hwInfo) {
}

template <>
void EncodeSurfaceState<Family>::encodeImplicitScalingParams(const EncodeSurfaceStateArgs &args) {
    auto surfaceState = reinterpret_cast<R_SURFACE_STATE *>(args.outMemory);
    bool enablePartialWrites = args.implicitScaling;
    bool enableMultiGpuAtomics = enablePartialWrites;

    if (DebugManager.flags.EnableMultiGpuAtomicsOptimization.get()) {
        enableMultiGpuAtomics = args.useGlobalAtomics && (enablePartialWrites || args.areMultipleSubDevicesInContext);
    }

    surfaceState->setDisableSupportForMultiGpuAtomics(!enableMultiGpuAtomics);
    surfaceState->setDisableSupportForMultiGpuPartialWrites(!enablePartialWrites);

    if (DebugManager.flags.ForceMultiGpuAtomics.get() != -1) {
        surfaceState->setDisableSupportForMultiGpuAtomics(!!DebugManager.flags.ForceMultiGpuAtomics.get());
    }

    if (DebugManager.flags.ForceMultiGpuPartialWrites.get() != -1) {
        surfaceState->setDisableSupportForMultiGpuPartialWrites(!!DebugManager.flags.ForceMultiGpuPartialWrites.get());
    }
}

template <>
void EncodeDispatchKernel<Family>::appendAdditionalIDDFields(INTERFACE_DESCRIPTOR_DATA *pInterfaceDescriptor, const HardwareInfo &hwInfo, const uint32_t threadsPerThreadGroup, uint32_t slmTotalSize, SlmPolicy slmPolicy) {
}

template <>
void EncodeDispatchKernel<Family>::programBarrierEnable(INTERFACE_DESCRIPTOR_DATA &interfaceDescriptor, uint32_t value, const HardwareInfo &hwInfo) {
    interfaceDescriptor.setBarrierEnable(value);
}

template <>
void EncodeDispatchKernel<Family>::adjustInterfaceDescriptorData(INTERFACE_DESCRIPTOR_DATA &interfaceDescriptor, const HardwareInfo &hwInfo) {
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    if (hwInfoConfig.isDisableOverdispatchAvailable(hwInfo)) {
        interfaceDescriptor.setThreadGroupDispatchSize(INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1);
    }

    if (DebugManager.flags.ForceThreadGroupDispatchSize.get() != -1) {
        interfaceDescriptor.setThreadGroupDispatchSize(static_cast<INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE>(
            DebugManager.flags.ForceThreadGroupDispatchSize.get()));
    }
}

template struct EncodeDispatchKernel<Family>;
template struct EncodeStates<Family>;
template struct EncodeMath<Family>;
template struct EncodeMathMMIO<Family>;
template struct EncodeIndirectParams<Family>;
template struct EncodeSetMMIO<Family>;
template struct EncodeMediaInterfaceDescriptorLoad<Family>;
template struct EncodeStateBaseAddress<Family>;
template struct EncodeStoreMMIO<Family>;
template struct EncodeSurfaceState<Family>;
template struct EncodeComputeMode<Family>;
template struct EncodeAtomic<Family>;
template struct EncodeSempahore<Family>;
template struct EncodeBatchBufferStartOrEnd<Family>;
template struct EncodeMiFlushDW<Family>;
template struct EncodeMemoryPrefetch<Family>;
template struct EncodeMiArbCheck<Family>;
template struct EncodeWA<Family>;
template struct EncodeEnableRayTracing<Family>;
template struct EncodeNoop<Family>;
template struct EncodeStoreMemory<Family>;
} // namespace NEO

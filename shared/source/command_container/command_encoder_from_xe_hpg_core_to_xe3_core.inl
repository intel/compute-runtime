/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/helpers/in_order_cmd_helpers.h"

namespace NEO {

template <typename Family>
size_t EncodeDispatchKernel<Family>::getScratchPtrOffsetOfImplicitArgs() {
    return 0;
}

template <typename Family>
void EncodeSurfaceState<Family>::setPitchForScratch(R_SURFACE_STATE *surfaceState, uint32_t pitch, const ProductHelper &productHelper) {
    surfaceState->setSurfacePitch(pitch);
}

template <typename Family>
uint32_t EncodeSurfaceState<Family>::getPitchForScratchInBytes(R_SURFACE_STATE *surfaceState, const ProductHelper &productHelper) {
    return surfaceState->getSurfacePitch();
}

template <typename Family>
void EncodeSemaphore<Family>::appendSemaphoreCommand(MI_SEMAPHORE_WAIT &cmd, uint64_t compareData, bool indirect, bool useQwordData, bool switchOnUnsuccessful) {
    constexpr uint64_t upper32b = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) << 32;
    UNRECOVERABLE_IF(useQwordData || (compareData & upper32b));
}

template <typename Family>
template <bool isHeapless>
void EncodeDispatchKernel<Family>::setScratchAddress(uint64_t &scratchAddress, uint32_t requiredScratchSlot0Size, uint32_t requiredScratchSlot1Size, IndirectHeap *ssh, CommandStreamReceiver &submissionCsr) {
}

template <typename Family>
template <typename InterfaceDescriptorType>
void EncodeDispatchKernel<Family>::encodeEuSchedulingPolicy(InterfaceDescriptorType *pInterfaceDescriptor, const KernelDescriptor &kernelDesc, int32_t defaultPipelinedThreadArbitrationPolicy) {
}

template <typename Family>
bool EncodeDispatchKernel<Family>::singleTileExecImplicitScalingRequired(bool cooperativeKernel) {
    return cooperativeKernel;
}

template <typename Family>
template <typename WalkerType>
void EncodeDispatchKernel<Family>::setupPostSyncForInOrderExec(WalkerType &walkerCmd, const EncodeDispatchKernelArgs &args) {
    using POSTSYNC_DATA = decltype(Family::template getPostSyncType<WalkerType>());

    auto &postSync = walkerCmd.getPostSync();

    postSync.setDataportPipelineFlush(true);
    postSync.setDataportSubsliceCacheFlush(true);
    if (NEO::debugManager.flags.ForcePostSyncL1Flush.get() != -1) {
        postSync.setDataportPipelineFlush(!!NEO::debugManager.flags.ForcePostSyncL1Flush.get());
        postSync.setDataportSubsliceCacheFlush(!!NEO::debugManager.flags.ForcePostSyncL1Flush.get());
    }

    uint64_t gpuVa = args.inOrderExecInfo->getBaseDeviceAddress() + args.inOrderExecInfo->getAllocationOffset();

    UNRECOVERABLE_IF(!(isAligned<immWriteDestinationAddressAlignment>(gpuVa)));

    postSync.setOperation(POSTSYNC_DATA::OPERATION_WRITE_IMMEDIATE_DATA);
    postSync.setImmediateData(args.inOrderCounterValue);
    postSync.setDestinationAddress(gpuVa);

    EncodeDispatchKernel<Family>::setupPostSyncMocs(walkerCmd, args.device->getRootDeviceEnvironment(), args.dcFlushEnable);
    EncodeDispatchKernel<Family>::adjustTimestampPacket(walkerCmd, args);
}

template <typename Family>
void InOrderPatchCommandHelpers::PatchCmd<Family>::patchComputeWalker(uint64_t appendCounterValue) {
    auto walkerCmd = reinterpret_cast<typename Family::DefaultWalkerType *>(cmd1);
    auto &postSync = walkerCmd->getPostSync();
    postSync.setImmediateData(baseCounterValue + appendCounterValue);
}

template <typename Family>
template <typename WalkerType>
void EncodeDispatchKernel<Family>::encodeAdditionalWalkerFields(const RootDeviceEnvironment &rootDeviceEnvironment, WalkerType &walkerCmd, const EncodeWalkerArgs &walkerArgs) {}

template <typename Family>
template <typename WalkerType>
void EncodeDispatchKernel<Family>::adjustTimestampPacket(WalkerType &walkerCmd, const EncodeDispatchKernelArgs &args) {}

template <typename Family>
template <typename WalkerType>
void EncodeDispatchKernel<Family>::setWalkerRegionSettings(WalkerType &walkerCmd, const NEO::Device &device, uint32_t partitionCount, uint32_t workgroupSize, uint32_t maxWgCountPerTile, bool requiredDispatchWalkOrder) {}

} // namespace NEO

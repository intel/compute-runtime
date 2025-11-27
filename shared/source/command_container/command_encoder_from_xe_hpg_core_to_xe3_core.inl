/*
 * Copyright (C) 2024-2025 Intel Corporation
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
void EncodeSurfaceState<Family>::convertSurfaceStateToPacked(R_SURFACE_STATE *surfaceState, ImageInfo &imgInfo) {
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
bool EncodeDispatchKernel<Family>::singleTileExecImplicitScalingRequired(bool cooperativeKernel) {
    return cooperativeKernel;
}

template <typename Family>
template <typename CommandType>
inline auto &EncodePostSync<Family>::getPostSync(CommandType &cmd, size_t index) {
    UNRECOVERABLE_IF(index != 0);
    return cmd.getPostSync();
}

template <typename Family>
template <typename CommandType>
void EncodePostSync<Family>::setupPostSyncForInOrderExec(CommandType &cmd, const EncodePostSyncArgs &args) {
    using POSTSYNC_DATA = decltype(Family::template getPostSyncType<CommandType>());

    auto &postSync = getPostSync(cmd, 0);

    uint64_t gpuVa = args.inOrderExecInfo->getBaseDeviceAddress() + args.inOrderExecInfo->getAllocationOffset();
    UNRECOVERABLE_IF(!(isAligned<immWriteDestinationAddressAlignment>(gpuVa)));

    uint32_t mocs = getPostSyncMocs(args.device->getRootDeviceEnvironment(), args.dcFlushEnable);

    setPostSyncData(postSync, POSTSYNC_DATA::OPERATION_WRITE_IMMEDIATE_DATA, gpuVa, args.inOrderCounterValue, 0, mocs, false, false);
    adjustTimestampPacket(cmd, args);
}

template <typename Family>
template <typename PostSyncT>
void EncodePostSync<Family>::setPostSyncData(PostSyncT &postSyncData, typename PostSyncT::OPERATION operation, uint64_t gpuVa, uint64_t immData,
                                             [[maybe_unused]] uint32_t atomicOpcode, uint32_t mocs, [[maybe_unused]] bool interrupt, bool requiresSystemMemoryFence) {
    setPostSyncDataCommon(postSyncData, operation, gpuVa, immData);

    postSyncData.setDataportPipelineFlush(true);
    postSyncData.setDataportSubsliceCacheFlush(true);

    if (NEO::debugManager.flags.ForcePostSyncL1Flush.get() != -1) {
        postSyncData.setDataportPipelineFlush(!!NEO::debugManager.flags.ForcePostSyncL1Flush.get());
        postSyncData.setDataportSubsliceCacheFlush(!!NEO::debugManager.flags.ForcePostSyncL1Flush.get());
    }

    postSyncData.setMocs(mocs);
    postSyncData.setSystemMemoryFenceRequest(requiresSystemMemoryFence);
}

template <typename Family>
void InOrderPatchCommandHelpers::PatchCmd<Family>::patchComputeWalker(uint64_t appendCounterValue) {
    auto walkerCmd = reinterpret_cast<typename Family::DefaultWalkerType *>(cmd1);
    auto &postSync = walkerCmd->getPostSync();
    postSync.setImmediateData(baseCounterValue + appendCounterValue);
}

template <typename Family>
void InOrderPatchCommandHelpers::PatchCmd<Family>::patchBlitterCommand(uint64_t appendCounterValue, InOrderPatchCommandHelpers::PatchCmdType patchCmdType) {
}
template <typename Family>
template <typename CommandType>
void EncodeDispatchKernel<Family>::encodeAdditionalWalkerFields(const RootDeviceEnvironment &rootDeviceEnvironment, CommandType &cmd, const EncodeWalkerArgs &walkerArgs) {}

template <typename Family>
template <typename CommandType>
void EncodePostSync<Family>::adjustTimestampPacket(CommandType &cmd, const EncodePostSyncArgs &args) {}

template <typename Family>
template <typename CommandType>
void EncodePostSync<Family>::encodeL3Flush(CommandType &cmd, const EncodePostSyncArgs &args) {}

template <typename Family>
template <typename WalkerType>
void EncodeDispatchKernel<Family>::setWalkerRegionSettings(WalkerType &walkerCmd, const NEO::Device &device, uint32_t partitionCount, uint32_t workgroupSize, uint32_t threadGroupCount, uint32_t maxWgCountPerTile, bool requiredDispatchWalkOrder) {}

template <typename Family>
void EncodeSurfaceState<Family>::setAdditionalCacheSettings(R_SURFACE_STATE *surfaceState) {
}
} // namespace NEO

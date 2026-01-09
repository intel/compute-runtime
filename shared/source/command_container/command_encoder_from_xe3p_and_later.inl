/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/scratch_space_controller.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/utilities/tag_allocator.h"

#include "implicit_args.h"

#include <algorithm>

namespace NEO {

template <typename Family>
void EncodeSurfaceState<Family>::setAuxParamsForMCSCCS(R_SURFACE_STATE *surfaceState, const ReleaseHelper *releaseHelper) {
}

template <typename Family>
void EncodeSurfaceState<Family>::setPitchForScratch(R_SURFACE_STATE *surfaceState, uint32_t pitchInBytes, const ProductHelper &productHelper) {
    if (!productHelper.isAvailableExtendedScratch()) {
        surfaceState->setSurfacePitch(pitchInBytes);
        return;
    }
    auto pitchInUnitsOf64Bytes = pitchInBytes / 64;
    surfaceState->setSurfacePitch(pitchInUnitsOf64Bytes);
    surfaceState->setExtendScratchSurfaceSize(true);
}

template <typename Family>
uint32_t EncodeSurfaceState<Family>::getPitchForScratchInBytes(R_SURFACE_STATE *surfaceState, const ProductHelper &productHelper) {
    if (!productHelper.isAvailableExtendedScratch()) {
        return surfaceState->getSurfacePitch();
    }
    auto pitchInUnitsOf64Bytes = surfaceState->getSurfacePitch();
    return 64 * pitchInUnitsOf64Bytes;
}

template <typename Family>
template <bool isHeapless>
void EncodeDispatchKernel<Family>::setScratchAddress(uint64_t &scratchAddress, uint32_t requiredScratchSlot0Size, uint32_t requiredScratchSlot1Size, IndirectHeap *ssh, CommandStreamReceiver &submissionCsr) {

    if constexpr (isHeapless) {
        if (requiredScratchSlot0Size > 0u || requiredScratchSlot1Size > 0u) {
            std::unique_lock<NEO::CommandStreamReceiver::MutexType> primaryCsrLock;
            auto primaryCsr = submissionCsr.getPrimaryCsr();

            if (primaryCsr && primaryCsr != &submissionCsr) {
                primaryCsrLock = primaryCsr->obtainUniqueOwnership();
            }

            auto scratchController = submissionCsr.getPrimaryScratchSpaceController();
            UNRECOVERABLE_IF(scratchController == nullptr);
            bool sbaStateDirty = false;
            bool frontEndStateDirty = false;
            scratchController->setRequiredScratchSpace(ssh->getCpuBase(), 0, requiredScratchSlot0Size, requiredScratchSlot1Size,
                                                       submissionCsr.getOsContext(), sbaStateDirty, frontEndStateDirty);
            if (scratchController->getScratchSpaceSlot0Allocation()) {
                submissionCsr.makeResident(*scratchController->getScratchSpaceSlot0Allocation());
            }
            if (scratchController->getScratchSpaceSlot1Allocation()) {
                submissionCsr.makeResident(*scratchController->getScratchSpaceSlot1Allocation());
            }

            scratchAddress = ssh->getGpuBase() + scratchController->getScratchPatchAddress();
        }
    }
}

template <typename Family>
template <typename InterfaceDescriptorType>
void EncodeDispatchKernel<Family>::encodeEuSchedulingPolicy(InterfaceDescriptorType *pInterfaceDescriptor, const KernelDescriptor &kernelDesc, int32_t defaultPipelinedThreadArbitrationPolicy) {
    auto pipelinedThreadArbitrationPolicy = kernelDesc.kernelAttributes.threadArbitrationPolicy;

    if (pipelinedThreadArbitrationPolicy == ThreadArbitrationPolicy::NotPresent) {
        pipelinedThreadArbitrationPolicy = static_cast<ThreadArbitrationPolicy>(defaultPipelinedThreadArbitrationPolicy);
    }

    using EU_THREAD_SCHEDULING_MODE_OVERRIDE = typename InterfaceDescriptorType::EU_THREAD_SCHEDULING_MODE_OVERRIDE;

    switch (pipelinedThreadArbitrationPolicy) {
    case ThreadArbitrationPolicy::RoundRobin:
        pInterfaceDescriptor->setEuThreadSchedulingModeOverride(EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_ROUND_ROBIN);
        break;
    case ThreadArbitrationPolicy::AgeBased:
        pInterfaceDescriptor->setEuThreadSchedulingModeOverride(EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_OLDEST_FIRST);
        break;
    case ThreadArbitrationPolicy::RoundRobinAfterDependency:
        pInterfaceDescriptor->setEuThreadSchedulingModeOverride(EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_STALL_BASED_ROUND_ROBIN);
        break;
    default:
        pInterfaceDescriptor->setEuThreadSchedulingModeOverride(EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_STALL_BASED_ROUND_ROBIN);
    }
}

template <typename Family>
bool EncodeDispatchKernel<Family>::singleTileExecImplicitScalingRequired(bool cooperativeKernel) {
    if (debugManager.flags.SingleTileExecutionForCooperativeKernels.get() == 1) {
        return cooperativeKernel;
    }

    return false;
}

template <typename Family>
template <typename CommandType>
void EncodePostSync<Family>::encodeL3Flush(CommandType &cmd, const EncodePostSyncArgs &args) {

    using POSTSYNC_DATA_TYPE = decltype(Family::template getPostSyncType<CommandType>());
    if constexpr (std::is_same_v<POSTSYNC_DATA_TYPE, typename Family::POSTSYNC_DATA_2>) {
        bool l2Flush = args.dcFlushEnable && args.isFlushL3ForExternalAllocationRequired;
        bool l2TransientFlush = args.dcFlushEnable && args.isFlushL3ForHostUsmRequired;

        cmd.getPostSync().setL2Flush(l2Flush);
        cmd.getPostSync().setL2TransientFlush(l2TransientFlush);
    }
}

template <typename Family>
template <typename CommandType>
void EncodePostSync<Family>::setupPostSyncForInOrderExec(CommandType &cmd, const EncodePostSyncArgs &args) {
    using POSTSYNC_DATA_TYPE = decltype(Family::template getPostSyncType<CommandType>());
    auto gmmHelper = args.device->getGmmHelper();
    uint32_t mocs = args.dcFlushEnable ? gmmHelper->getUncachedMOCS() : gmmHelper->getL3EnabledMOCS();
    if (debugManager.flags.OverridePostSyncMocs.get() != -1) {
        mocs = debugManager.flags.OverridePostSyncMocs.get();
    }

    auto requiresSystemMemoryFence = args.requiresSystemMemoryFence();
    int32_t overrideProgramSystemMemoryFence = debugManager.flags.ProgramGlobalFenceAsPostSyncOperationInComputeWalker.get();
    if (overrideProgramSystemMemoryFence != -1) {
        requiresSystemMemoryFence = !!overrideProgramSystemMemoryFence;
    }

    const uint64_t deviceGpuVa = args.inOrderExecInfo->getBaseDeviceAddress() + args.inOrderExecInfo->getAllocationOffset();
    const uint64_t data = args.inOrderCounterValue;

    if constexpr (Family::template isPostSyncData1<POSTSYNC_DATA_TYPE>()) {
        setPostSyncData(cmd.getPostSync(), POSTSYNC_DATA_TYPE::OPERATION_WRITE_IMMEDIATE_DATA, deviceGpuVa, data, 0, mocs, false, requiresSystemMemoryFence);
    } else {
        uint32_t postSyncId = 0;
        const bool deviceInterrupt = (args.interruptEvent && !args.inOrderExecInfo->isHostStorageDuplicated());

        if (args.inOrderExecInfo->isAtomicDeviceSignalling()) {
            setPostSyncData(getPostSync(cmd, postSyncId++), POSTSYNC_DATA_TYPE::OPERATION_ATOMIC_OPN, deviceGpuVa, 0, static_cast<uint32_t>(POSTSYNC_DATA_TYPE::ATOMIC_OPCODE::ATOMIC_OPCODE_ATOMIC_INC8B),
                            mocs, deviceInterrupt, requiresSystemMemoryFence);
        } else {
            setPostSyncData(getPostSync(cmd, postSyncId++), POSTSYNC_DATA_TYPE::OPERATION_WRITE_IMMEDIATE_DATA, deviceGpuVa, data, 0, mocs, deviceInterrupt, requiresSystemMemoryFence);
        }

        if (args.inOrderExecInfo->isHostStorageDuplicated()) {
            setPostSyncData(getPostSync(cmd, postSyncId++), POSTSYNC_DATA_TYPE::OPERATION_WRITE_IMMEDIATE_DATA, args.inOrderExecInfo->getBaseHostGpuAddress(), data, 0, mocs, args.interruptEvent, requiresSystemMemoryFence);
        }

        if (args.eventAddress) {
            setPostSyncData(getPostSync(cmd, postSyncId++), (args.isTimestampEvent ? POSTSYNC_DATA_TYPE::OPERATION_WRITE_TIMESTAMP : POSTSYNC_DATA_TYPE::OPERATION_WRITE_IMMEDIATE_DATA), args.eventAddress, args.postSyncImmValue, 0, mocs, false, requiresSystemMemoryFence);
        }

        if (args.inOrderIncrementValue > 0) {
            setPostSyncData(getPostSync(cmd, postSyncId++), POSTSYNC_DATA_TYPE::OPERATION_ATOMIC_OPN, args.inOrderIncrementGpuAddress, args.inOrderIncrementValue,
                            static_cast<uint32_t>(POSTSYNC_DATA_TYPE::ATOMIC_OPCODE::ATOMIC_OPCODE_ATOMIC_ADD8B), mocs, false, requiresSystemMemoryFence);
        }
        setCommandLevelInterrupt(cmd, args.interruptEvent);
        encodeL3Flush(cmd, args);
    }
}

template <typename Family>
template <typename WalkerType>
void EncodeDispatchKernel<Family>::setWalkerRegionSettings(WalkerType &walkerCmd, const NEO::Device &device, uint32_t partitionCount, uint32_t workgroupSize, uint32_t threadGroupCount, uint32_t maxWgCountPerTile, bool requiredDispatchWalkOrder) {
    if constexpr (std::is_same_v<WalkerType, typename Family::COMPUTE_WALKER_2>) {
        using QUANTUMSIZE = typename Family::COMPUTE_WALKER_2::QUANTUMSIZE;
        auto quantumSize = static_cast<QUANTUMSIZE>(getQuantumSizeHw<Family, WalkerType>(0));
        walkerCmd.setQuantumsize(quantumSize);
    }
}

template <typename Family>
template <typename WalkerType>
void EncodeDispatchKernel<Family>::encodeAdditionalWalkerFields(const RootDeviceEnvironment &rootDeviceEnvironment, WalkerType &walkerCmd, const EncodeWalkerArgs &walkerArgs) {
    constexpr bool heaplessModeEnabled = std::is_same_v<WalkerType, typename Family::COMPUTE_WALKER_2>;
    if constexpr (heaplessModeEnabled) {
        auto maxNumberOfThreads = walkerArgs.maxFrontEndThreads;
        maxNumberOfThreads = std::max(64U, maxNumberOfThreads);
        if (debugManager.flags.MaximumNumberOfThreads.get() != -1) {
            maxNumberOfThreads = static_cast<uint32_t>(debugManager.flags.MaximumNumberOfThreads.get());
        }
        walkerCmd.setMaximumNumberOfThreads(maxNumberOfThreads);
    }

    if (walkerArgs.hasSample) {
        walkerCmd.setDispatchWalkOrder(WalkerType::DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_MORTON_WALK);
        walkerCmd.setThreadGroupBatchSize(WalkerType::THREAD_GROUP_BATCH_SIZE::THREAD_GROUP_BATCH_SIZE_TG_BATCH_4);
    }

    if (walkerArgs.requiredDispatchWalkOrder == NEO::RequiredDispatchWalkOrder::x) {
        walkerCmd.setDispatchWalkOrder(WalkerType::DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_LINEAR_WALK);
    } else if (walkerArgs.requiredDispatchWalkOrder == NEO::RequiredDispatchWalkOrder::y) {
        walkerCmd.setDispatchWalkOrder(WalkerType::DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_Y_ORDER_WALK);
    } else {
        UNRECOVERABLE_IF(walkerArgs.requiredDispatchWalkOrder != NEO::RequiredDispatchWalkOrder::none);
    }
}

template <typename Family>
void InOrderPatchCommandHelpers::PatchCmd<Family>::patchComputeWalker(uint64_t appendCounterValue) {
    if (!deviceAtomicSignaling) {
        auto walkerCmd = reinterpret_cast<typename Family::DefaultWalkerType *>(cmd1);
        auto &postSync0 = walkerCmd->getPostSync();
        postSync0.setImmediateData(baseCounterValue + appendCounterValue);
    }

    if (duplicatedHostStorage) {
        auto walkerCmd = reinterpret_cast<typename Family::COMPUTE_WALKER_2 *>(cmd1);
        auto &postSync1 = walkerCmd->getPostSyncOpn1();

        postSync1.setImmediateData(baseCounterValue + appendCounterValue);
    }
}

template <typename Family>
bool EncodeEnableRayTracing<Family>::is48bResourceNeededForRayTracing() {
    return false;
}

template <typename Family>
void EncodeSurfaceState<Family>::convertSurfaceStateToPacked(R_SURFACE_STATE *surfaceState, ImageInfo &imgInfo) {
    using SURFACE_FORMAT = typename R_SURFACE_STATE::SURFACE_FORMAT;
    using SHADER_CHANNEL_SELECT = typename R_SURFACE_STATE::SHADER_CHANNEL_SELECT;

    constexpr uint32_t r32g32b32a32SurfaceFormatImageElementSizeInBytes = 16u;

    constexpr auto convertSurfaceFormatToPackedLambda = [](uint32_t bitsPerPixel) -> typename R_SURFACE_STATE::OVERRIDETILEBPP {
        using OVERRIDETILEBPP = typename R_SURFACE_STATE::OVERRIDETILEBPP;

        switch (bitsPerPixel) {
        case 64:
            return OVERRIDETILEBPP::OVERRIDETILEBPP_TILE64_64BPP;
        case 32:
            return OVERRIDETILEBPP::OVERRIDETILEBPP_TILE64_32BPP;
        case 16:
            return OVERRIDETILEBPP::OVERRIDETILEBPP_TILE64_16BPP;
        case 8:
            return OVERRIDETILEBPP::OVERRIDETILEBPP_TILE64_8BPP;
        default:
            return OVERRIDETILEBPP::OVERRIDETILEBPP_TILE64_LEGACY;
        }
    };

    auto scale = r32g32b32a32SurfaceFormatImageElementSizeInBytes / imgInfo.surfaceFormat->imageElementSizeInBytes;
    auto newWidth = static_cast<uint32_t>(Math::divideAndRoundUp(imgInfo.imgDesc.imageWidth, scale));

    surfaceState->setOverrideTileBPP(convertSurfaceFormatToPackedLambda(static_cast<uint32_t>(imgInfo.surfaceFormat->imageElementSizeInBytes * 8)));
    surfaceState->setWidth(newWidth);
    surfaceState->setSurfaceFormat(SURFACE_FORMAT::SURFACE_FORMAT_R32G32B32A32_UINT);

    surfaceState->setShaderChannelSelectRed(SHADER_CHANNEL_SELECT::SHADER_CHANNEL_SELECT_RED);
    surfaceState->setShaderChannelSelectGreen(SHADER_CHANNEL_SELECT::SHADER_CHANNEL_SELECT_GREEN);
    surfaceState->setShaderChannelSelectBlue(SHADER_CHANNEL_SELECT::SHADER_CHANNEL_SELECT_BLUE);
    surfaceState->setShaderChannelSelectAlpha(SHADER_CHANNEL_SELECT::SHADER_CHANNEL_SELECT_ALPHA);
}

template <typename Family>
void EncodeSurfaceState<Family>::setAdditionalCacheSettings(R_SURFACE_STATE *surfaceState) {
    int32_t forceL1P5CacheForRenderSurface = debugManager.flags.ForceL1P5CacheForRenderSurface.get();
    if (forceL1P5CacheForRenderSurface != -1) {
        surfaceState->setDisableL1P5(!forceL1P5CacheForRenderSurface);
    }
}

} // namespace NEO

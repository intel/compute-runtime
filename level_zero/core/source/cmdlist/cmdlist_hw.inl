/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/device/device.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/heap_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/logical_state_helper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memadvise_flags.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"
#include "shared/source/program/sync_buffer_handler.h"
#include "shared/source/program/sync_buffer_handler.inl"
#include "shared/source/utilities/software_tags_manager.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_imp.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/core/source/image/image.h"
#include "level_zero/core/source/kernel/kernel.h"
#include "level_zero/core/source/module/module.h"

#include "CL/cl.h"

#include <algorithm>

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
struct EncodeStateBaseAddress;

inline ze_result_t parseErrorCode(NEO::CommandContainer::ErrorCode returnValue) {
    switch (returnValue) {
    case NEO::CommandContainer::ErrorCode::OUT_OF_DEVICE_MEMORY:
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    default:
        return ZE_RESULT_SUCCESS;
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
CommandListCoreFamily<gfxCoreFamily>::~CommandListCoreFamily() {
    clearCommandsToPatch();
    for (auto alloc : this->ownedPrivateAllocations) {
        device->getNEODevice()->getMemoryManager()->freeGraphicsMemory(alloc);
    }
    this->ownedPrivateAllocations.clear();
    for (auto &patternAlloc : this->patternAllocations) {
        device->storeReusableAllocation(*patternAlloc);
    }
    this->patternAllocations.clear();
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::reset() {
    printfKernelContainer.clear();
    removeDeallocationContainerData();
    removeHostPtrAllocations();
    commandContainer.reset();
    containsStatelessUncachedResource = false;
    performMemoryPrefetch = false;
    indirectAllocationsAllowed = false;
    unifiedMemoryControls.indirectHostAllocationsAllowed = false;
    unifiedMemoryControls.indirectSharedAllocationsAllowed = false;
    unifiedMemoryControls.indirectDeviceAllocationsAllowed = false;
    commandListPreemptionMode = device->getDevicePreemptionMode();
    commandListPerThreadScratchSize = 0u;
    requiredStreamState = {};
    finalStreamState = requiredStreamState;
    containsAnyKernel = false;
    containsCooperativeKernelsFlag = false;
    clearCommandsToPatch();
    commandListSLMEnabled = false;

    if (!isCopyOnly()) {
        if (!NEO::ApiSpecificConfig::getBindlessConfiguration()) {
            programStateBaseAddress(commandContainer, false);
        }
        commandContainer.setDirtyStateForAllHeaps(false);
    }

    for (auto alloc : this->ownedPrivateAllocations) {
        device->getNEODevice()->getMemoryManager()->freeGraphicsMemory(alloc);
    }
    this->ownedPrivateAllocations.clear();
    cmdListCurrentStartOffset = 0;
    this->returnPoints.clear();
    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::handlePostSubmissionState() {
    this->commandContainer.getResidencyContainer().clear();
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::initialize(Device *device, NEO::EngineGroupType engineGroupType,
                                                             ze_command_list_flags_t flags) {
    this->device = device;
    this->commandListPreemptionMode = device->getDevicePreemptionMode();
    this->engineGroupType = engineGroupType;
    this->flags = flags;

    auto &hwInfo = device->getHwInfo();
    this->dcFlushSupport = NEO::MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(true, hwInfo);
    this->systolicModeSupport = NEO::PreambleHelper<GfxFamily>::isSystolicModeConfigurable(hwInfo);
    this->stateComputeModeTracking = L0HwHelper::enableStateComputeModeTracking(hwInfo);
    this->frontEndStateTracking = L0HwHelper::enableFrontEndStateTracking(hwInfo);
    this->pipelineSelectStateTracking = L0HwHelper::enablePipelineSelectStateTracking(hwInfo);

    if (device->isImplicitScalingCapable() && !this->internalUsage && !isCopyOnly()) {
        this->partitionCount = static_cast<uint32_t>(this->device->getNEODevice()->getDeviceBitfield().count());
    }

    if (this->isFlushTaskSubmissionEnabled) {
        commandContainer.setFlushTaskUsedForImmediate(this->isFlushTaskSubmissionEnabled);
    }

    if (this->immediateCmdListHeapSharing) {
        commandContainer.enableHeapSharing();
        commandContainer.setNumIddPerBlock(1);
    }

    commandContainer.setReservedSshSize(getReserveSshSize());
    DeviceImp *deviceImp = static_cast<DeviceImp *>(device);
    auto returnValue = commandContainer.initialize(deviceImp->getActiveDevice(), deviceImp->allocationsForReuse.get(), !isCopyOnly());
    if (!this->pipelineSelectStateTracking) {
        // allow systolic support set in container when tracking disabled
        // setting systolic support allows dispatching untracked command in legacy mode
        commandContainer.systolicModeSupport = this->systolicModeSupport;
    }

    ze_result_t returnType = parseErrorCode(returnValue);
    if (returnType == ZE_RESULT_SUCCESS) {
        if (!isCopyOnly()) {
            if (!NEO::ApiSpecificConfig::getBindlessConfiguration()) {
                if (!this->isFlushTaskSubmissionEnabled) {
                    programStateBaseAddress(commandContainer, false);
                }
            }
            commandContainer.setDirtyStateForAllHeaps(false);
        }
    }

    createLogicalStateHelper();

    return returnType;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::createLogicalStateHelper() {
    this->nonImmediateLogicalStateHelper.reset(NEO::LogicalStateHelper::create<GfxFamily>());
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::executeCommandListImmediate(bool performMigration) {
    return executeCommandListImmediateImpl(performMigration, this->cmdQImmediate);
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline ze_result_t CommandListCoreFamily<gfxCoreFamily>::executeCommandListImmediateImpl(bool performMigration, L0::CommandQueue *cmdQImmediate) {
    this->close();
    ze_command_list_handle_t immediateHandle = this->toHandle();

    this->commandContainer.removeDuplicatesFromResidencyContainer();
    const auto commandListExecutionResult = cmdQImmediate->executeCommandLists(1, &immediateHandle, nullptr, performMigration);
    if (commandListExecutionResult == ZE_RESULT_ERROR_DEVICE_LOST) {
        return commandListExecutionResult;
    }

    if (this->isCopyOnly() && !this->isSyncModeQueue && !this->isTbxMode) {
        this->commandContainer.currentLinearStreamStartOffset = this->commandContainer.getCommandStream()->getUsed();
        this->handlePostSubmissionState();
    } else {
        const auto synchronizationResult = cmdQImmediate->synchronize(std::numeric_limits<uint64_t>::max());
        if (synchronizationResult == ZE_RESULT_ERROR_DEVICE_LOST) {
            return synchronizationResult;
        }

        this->reset();
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::close() {
    commandContainer.removeDuplicatesFromResidencyContainer();
    NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferEnd(commandContainer);

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::programL3(bool isSLMused) {}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernel(ze_kernel_handle_t kernelHandle,
                                                                     const ze_group_count_t *threadGroupDimensions,
                                                                     ze_event_handle_t hEvent,
                                                                     uint32_t numWaitEvents,
                                                                     ze_event_handle_t *phWaitEvents,
                                                                     const CmdListKernelLaunchParams &launchParams) {

    NEO::Device *neoDevice = device->getNEODevice();
    uint32_t callId = 0;
    if (NEO::DebugManager.flags.EnableSWTags.get()) {
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameBeginTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            "zeCommandListAppendLaunchKernel",
            ++neoDevice->getRootDeviceEnvironment().tagsManager->currentCallCount);
        callId = neoDevice->getRootDeviceEnvironment().tagsManager->currentCallCount;
    }

    ze_result_t ret = addEventsToCmdList(numWaitEvents, phWaitEvents);
    if (ret) {
        return ret;
    }

    Event *event = nullptr;
    if (hEvent) {
        event = Event::fromHandle(hEvent);
    }

    auto res = appendLaunchKernelWithParams(Kernel::fromHandle(kernelHandle), threadGroupDimensions,
                                            event, launchParams);

    if (NEO::DebugManager.flags.EnableSWTags.get()) {
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameEndTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            "zeCommandListAppendLaunchKernel",
            callId);
    }

    return res;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendLaunchCooperativeKernel(ze_kernel_handle_t kernelHandle,
                                                                                const ze_group_count_t *launchKernelArgs,
                                                                                ze_event_handle_t signalEvent,
                                                                                uint32_t numWaitEvents,
                                                                                ze_event_handle_t *waitEventHandles) {

    ze_result_t ret = addEventsToCmdList(numWaitEvents, waitEventHandles);
    if (ret) {
        return ret;
    }

    Event *event = nullptr;
    if (signalEvent) {
        event = Event::fromHandle(signalEvent);
    }

    CmdListKernelLaunchParams launchParams = {};
    launchParams.isCooperative = true;
    return appendLaunchKernelWithParams(Kernel::fromHandle(kernelHandle), launchKernelArgs,
                                        event, launchParams);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernelIndirect(ze_kernel_handle_t kernelHandle,
                                                                             const ze_group_count_t *pDispatchArgumentsBuffer,
                                                                             ze_event_handle_t hEvent,
                                                                             uint32_t numWaitEvents,
                                                                             ze_event_handle_t *phWaitEvents) {

    ze_result_t ret = addEventsToCmdList(numWaitEvents, phWaitEvents);
    if (ret) {
        return ret;
    }

    Event *event = nullptr;
    if (hEvent) {
        event = Event::fromHandle(hEvent);
    }

    appendEventForProfiling(event, true, false);
    CmdListKernelLaunchParams launchParams = {};
    launchParams.isIndirect = true;
    ret = appendLaunchKernelWithParams(Kernel::fromHandle(kernelHandle), pDispatchArgumentsBuffer,
                                       nullptr, launchParams);
    appendSignalEventPostWalker(event, false);

    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendLaunchMultipleKernelsIndirect(uint32_t numKernels,
                                                                                      const ze_kernel_handle_t *kernelHandles,
                                                                                      const uint32_t *pNumLaunchArguments,
                                                                                      const ze_group_count_t *pLaunchArgumentsBuffer,
                                                                                      ze_event_handle_t hEvent,
                                                                                      uint32_t numWaitEvents,
                                                                                      ze_event_handle_t *phWaitEvents) {

    ze_result_t ret = addEventsToCmdList(numWaitEvents, phWaitEvents);
    if (ret) {
        return ret;
    }

    Event *event = nullptr;
    if (hEvent) {
        event = Event::fromHandle(hEvent);
    }

    appendEventForProfiling(event, true, false);
    const bool haveLaunchArguments = pLaunchArgumentsBuffer != nullptr;
    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(pNumLaunchArguments);
    auto alloc = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    commandContainer.addToResidencyContainer(alloc);

    for (uint32_t i = 0; i < numKernels; i++) {
        NEO::EncodeMathMMIO<GfxFamily>::encodeGreaterThanPredicate(commandContainer, alloc->getGpuAddress(), i);

        CmdListKernelLaunchParams launchParams = {};
        launchParams.isIndirect = true;
        launchParams.isPredicate = true;
        ret = appendLaunchKernelWithParams(Kernel::fromHandle(kernelHandles[i]),
                                           haveLaunchArguments ? &pLaunchArgumentsBuffer[i] : nullptr,
                                           nullptr, launchParams);
        if (ret) {
            return ret;
        }
    }

    appendSignalEventPostWalker(event, false);

    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendEventReset(ze_event_handle_t hEvent) {
    auto event = Event::fromHandle(hEvent);

    uint64_t baseAddr = event->getGpuAddress(this->device);
    uint32_t packetsToReset = event->getPacketsInUse();
    bool appendPipeControlWithPostSync = false;

    NEO::Device *neoDevice = device->getNEODevice();
    uint32_t callId = 0;
    if (NEO::DebugManager.flags.EnableSWTags.get()) {
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameBeginTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            "zeCommandListAppendEventReset",
            ++neoDevice->getRootDeviceEnvironment().tagsManager->currentCallCount);
        callId = neoDevice->getRootDeviceEnvironment().tagsManager->currentCallCount;
    }

    if (event->isUsingContextEndOffset()) {
        baseAddr += event->getContextEndOffset();
    }

    if (event->isEventTimestampFlagSet()) {
        packetsToReset = EventPacketsCount::eventPackets;
    }
    event->resetPackets();
    event->resetCompletion();
    commandContainer.addToResidencyContainer(&event->getAllocation(this->device));
    const auto &hwInfo = this->device->getHwInfo();
    if (isCopyOnly()) {
        NEO::MiFlushArgs args;
        args.commandWithPostSync = true;
        NEO::EncodeMiFlushDW<GfxFamily>::programMiFlushDw(*commandContainer.getCommandStream(),
                                                          baseAddr,
                                                          Event::STATE_CLEARED, args, hwInfo);
    } else {
        bool applyScope = event->signalScope;
        uint32_t packetsToResetUsingSdi = packetsToReset;
        if (applyScope || event->isEventTimestampFlagSet()) {
            UNRECOVERABLE_IF(packetsToReset == 0);
            packetsToResetUsingSdi = packetsToReset - 1;
            appendPipeControlWithPostSync = true;
        }

        for (uint32_t i = 0u; i < packetsToResetUsingSdi; i++) {
            NEO::EncodeStoreMemory<GfxFamily>::programStoreDataImm(
                *commandContainer.getCommandStream(),
                baseAddr,
                Event::STATE_CLEARED,
                0u,
                false,
                false);
            baseAddr += event->getSinglePacketSize();
        }

        if (appendPipeControlWithPostSync) {
            NEO::PipeControlArgs args;
            args.dcFlushEnable = getDcFlushRequired(!!event->signalScope);
            NEO::MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
                *commandContainer.getCommandStream(),
                NEO::PostSyncMode::ImmediateData,
                baseAddr,
                Event::STATE_CLEARED,
                hwInfo,
                args);
        }

        if (this->partitionCount > 1) {
            appendMultiTileBarrier(*neoDevice);
        }
    }

    if (NEO::DebugManager.flags.EnableSWTags.get()) {
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameEndTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            "zeCommandListAppendEventReset",
            callId);
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemoryRangesBarrier(uint32_t numRanges,
                                                                            const size_t *pRangeSizes,
                                                                            const void **pRanges,
                                                                            ze_event_handle_t hSignalEvent,
                                                                            uint32_t numWaitEvents,
                                                                            ze_event_handle_t *phWaitEvents) {

    ze_result_t ret = addEventsToCmdList(numWaitEvents, phWaitEvents);
    if (ret) {
        return ret;
    }

    Event *signalEvent = nullptr;
    if (hSignalEvent) {
        signalEvent = Event::fromHandle(hSignalEvent);
    }

    bool workloadPartition = setupTimestampEventForMultiTile(signalEvent);

    appendEventForProfiling(signalEvent, true, workloadPartition);
    applyMemoryRangesBarrier(numRanges, pRangeSizes, pRanges);
    appendSignalEventPostWalker(signalEvent, workloadPartition);

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendImageCopyFromMemory(ze_image_handle_t hDstImage,
                                                                            const void *srcPtr,
                                                                            const ze_image_region_t *pDstRegion,
                                                                            ze_event_handle_t hEvent,
                                                                            uint32_t numWaitEvents,
                                                                            ze_event_handle_t *phWaitEvents) {

    auto image = Image::fromHandle(hDstImage);
    auto bytesPerPixel = static_cast<uint32_t>(image->getImageInfo().surfaceFormat->ImageElementSizeInBytes);

    Vec3<size_t> imgSize = {image->getImageDesc().width,
                            image->getImageDesc().height,
                            image->getImageDesc().depth};

    Event *event = nullptr;
    if (hEvent) {
        event = Event::fromHandle(hEvent);
    }

    ze_image_region_t tmpRegion;
    if (pDstRegion == nullptr) {
        // If this is a 1D or 2D image, then the height or depth is ignored and must be set to 1.
        // Internally, all dimensions must be >= 1.
        if (image->getImageDesc().type == ZE_IMAGE_TYPE_1D || image->getImageDesc().type == ZE_IMAGE_TYPE_1DARRAY) {
            imgSize.y = 1;
        }
        if (image->getImageDesc().type != ZE_IMAGE_TYPE_3D) {
            imgSize.z = 1;
        }
        tmpRegion = {0,
                     0,
                     0,
                     static_cast<uint32_t>(imgSize.x),
                     static_cast<uint32_t>(imgSize.y),
                     static_cast<uint32_t>(imgSize.z)};
        pDstRegion = &tmpRegion;
    }

    uint64_t bufferSize = getInputBufferSize(image->getImageInfo().imgDesc.imageType, bytesPerPixel, pDstRegion);

    auto allocationStruct = getAlignedAllocation(this->device, srcPtr, bufferSize, true);

    auto rowPitch = pDstRegion->width * bytesPerPixel;
    auto slicePitch =
        image->getImageInfo().imgDesc.imageType == NEO::ImageType::Image1DArray ? 1 : pDstRegion->height * rowPitch;

    if (isCopyOnly()) {
        return appendCopyImageBlit(allocationStruct.alloc, image->getAllocation(),
                                   {0, 0, 0}, {pDstRegion->originX, pDstRegion->originY, pDstRegion->originZ}, rowPitch, slicePitch,
                                   rowPitch, slicePitch, bytesPerPixel, {pDstRegion->width, pDstRegion->height, pDstRegion->depth}, {pDstRegion->width, pDstRegion->height, pDstRegion->depth}, imgSize, event);
    }

    auto lock = device->getBuiltinFunctionsLib()->obtainUniqueOwnership();

    Kernel *builtinKernel = nullptr;

    switch (bytesPerPixel) {
    default:
        UNRECOVERABLE_IF(true);
        break;
    case 1u:
        builtinKernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::CopyBufferToImage3dBytes);
        break;
    case 2u:
        builtinKernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::CopyBufferToImage3d2Bytes);
        break;
    case 4u:
        builtinKernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::CopyBufferToImage3d4Bytes);
        break;
    case 8u:
        builtinKernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::CopyBufferToImage3d8Bytes);
        break;
    case 16u:
        builtinKernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::CopyBufferToImage3d16Bytes);
        break;
    }

    builtinKernel->setArgBufferWithAlloc(0u, allocationStruct.alignedAllocationPtr,
                                         allocationStruct.alloc);
    builtinKernel->setArgRedescribedImage(1u, hDstImage);
    builtinKernel->setArgumentValue(2u, sizeof(size_t), &allocationStruct.offset);

    uint32_t origin[] = {
        static_cast<uint32_t>(pDstRegion->originX),
        static_cast<uint32_t>(pDstRegion->originY),
        static_cast<uint32_t>(pDstRegion->originZ),
        0};
    builtinKernel->setArgumentValue(3u, sizeof(origin), &origin);

    uint32_t pitch[] = {
        rowPitch,
        slicePitch};
    builtinKernel->setArgumentValue(4u, sizeof(pitch), &pitch);

    uint32_t groupSizeX = pDstRegion->width;
    uint32_t groupSizeY = pDstRegion->height;
    uint32_t groupSizeZ = pDstRegion->depth;

    if (builtinKernel->suggestGroupSize(groupSizeX, groupSizeY, groupSizeZ,
                                        &groupSizeX, &groupSizeY, &groupSizeZ) != ZE_RESULT_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (builtinKernel->setGroupSize(groupSizeX, groupSizeY, groupSizeZ) != ZE_RESULT_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (pDstRegion->width % groupSizeX || pDstRegion->height % groupSizeY || pDstRegion->depth % groupSizeZ) {
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ze_group_count_t kernelArgs{pDstRegion->width / groupSizeX, pDstRegion->height / groupSizeY,
                                pDstRegion->depth / groupSizeZ};

    CmdListKernelLaunchParams launchParams = {};
    launchParams.isBuiltInKernel = true;
    return CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernel(builtinKernel->toHandle(), &kernelArgs,
                                                                    event, numWaitEvents, phWaitEvents,
                                                                    launchParams);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendImageCopyToMemory(void *dstPtr,
                                                                          ze_image_handle_t hSrcImage,
                                                                          const ze_image_region_t *pSrcRegion,
                                                                          ze_event_handle_t hEvent,
                                                                          uint32_t numWaitEvents,
                                                                          ze_event_handle_t *phWaitEvents) {

    auto image = Image::fromHandle(hSrcImage);
    auto bytesPerPixel = static_cast<uint32_t>(image->getImageInfo().surfaceFormat->ImageElementSizeInBytes);

    Vec3<size_t> imgSize = {image->getImageDesc().width,
                            image->getImageDesc().height,
                            image->getImageDesc().depth};

    Event *event = nullptr;
    if (hEvent) {
        event = Event::fromHandle(hEvent);
    }

    ze_image_region_t tmpRegion;
    if (pSrcRegion == nullptr) {
        // If this is a 1D or 2D image, then the height or depth is ignored and must be set to 1.
        // Internally, all dimensions must be >= 1.
        if (image->getImageDesc().type == ZE_IMAGE_TYPE_1D || image->getImageDesc().type == ZE_IMAGE_TYPE_1DARRAY) {
            imgSize.y = 1;
        }
        if (image->getImageDesc().type != ZE_IMAGE_TYPE_3D) {
            imgSize.z = 1;
        }
        tmpRegion = {0,
                     0,
                     0,
                     static_cast<uint32_t>(imgSize.x),
                     static_cast<uint32_t>(imgSize.y),
                     static_cast<uint32_t>(imgSize.z)};
        pSrcRegion = &tmpRegion;
    }

    uint64_t bufferSize = getInputBufferSize(image->getImageInfo().imgDesc.imageType, bytesPerPixel, pSrcRegion);

    auto allocationStruct = getAlignedAllocation(this->device, dstPtr, bufferSize, false);

    auto rowPitch = pSrcRegion->width * bytesPerPixel;
    auto slicePitch =
        (image->getImageInfo().imgDesc.imageType == NEO::ImageType::Image1DArray ? 1 : pSrcRegion->height) * rowPitch;

    if (isCopyOnly()) {
        return appendCopyImageBlit(image->getAllocation(), allocationStruct.alloc,
                                   {pSrcRegion->originX, pSrcRegion->originY, pSrcRegion->originZ}, {0, 0, 0}, rowPitch, slicePitch,
                                   rowPitch, slicePitch, bytesPerPixel, {pSrcRegion->width, pSrcRegion->height, pSrcRegion->depth}, imgSize, {pSrcRegion->width, pSrcRegion->height, pSrcRegion->depth}, event);
    }

    auto lock = device->getBuiltinFunctionsLib()->obtainUniqueOwnership();

    Kernel *builtinKernel = nullptr;

    switch (bytesPerPixel) {
    default:
        UNRECOVERABLE_IF(true);
        break;
    case 1u:
        builtinKernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::CopyImage3dToBufferBytes);
        break;
    case 2u:
        builtinKernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::CopyImage3dToBuffer2Bytes);
        break;
    case 4u:
        builtinKernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::CopyImage3dToBuffer4Bytes);
        break;
    case 8u:
        builtinKernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::CopyImage3dToBuffer8Bytes);
        break;
    case 16u:
        builtinKernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::CopyImage3dToBuffer16Bytes);
        break;
    }

    builtinKernel->setArgRedescribedImage(0u, hSrcImage);
    builtinKernel->setArgBufferWithAlloc(1u, allocationStruct.alignedAllocationPtr,
                                         allocationStruct.alloc);

    uint32_t origin[] = {
        static_cast<uint32_t>(pSrcRegion->originX),
        static_cast<uint32_t>(pSrcRegion->originY),
        static_cast<uint32_t>(pSrcRegion->originZ),
        0};
    builtinKernel->setArgumentValue(2u, sizeof(origin), &origin);

    builtinKernel->setArgumentValue(3u, sizeof(size_t), &allocationStruct.offset);

    uint32_t pitch[] = {
        rowPitch,
        slicePitch};
    builtinKernel->setArgumentValue(4u, sizeof(pitch), &pitch);

    uint32_t groupSizeX = pSrcRegion->width;
    uint32_t groupSizeY = pSrcRegion->height;
    uint32_t groupSizeZ = pSrcRegion->depth;

    if (builtinKernel->suggestGroupSize(groupSizeX, groupSizeY, groupSizeZ,
                                        &groupSizeX, &groupSizeY, &groupSizeZ) != ZE_RESULT_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (builtinKernel->setGroupSize(groupSizeX, groupSizeY, groupSizeZ) != ZE_RESULT_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (pSrcRegion->width % groupSizeX || pSrcRegion->height % groupSizeY || pSrcRegion->depth % groupSizeZ) {
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ze_group_count_t kernelArgs{pSrcRegion->width / groupSizeX, pSrcRegion->height / groupSizeY,
                                pSrcRegion->depth / groupSizeZ};

    auto dstAllocationType = allocationStruct.alloc->getAllocationType();
    CmdListKernelLaunchParams launchParams = {};
    launchParams.isBuiltInKernel = true;
    launchParams.isDestinationAllocationInSystemMemory =
        (dstAllocationType == NEO::AllocationType::BUFFER_HOST_MEMORY) ||
        (dstAllocationType == NEO::AllocationType::EXTERNAL_HOST_PTR);
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernel(builtinKernel->toHandle(), &kernelArgs,
                                                                        event, numWaitEvents, phWaitEvents, launchParams);

    addFlushRequiredCommand(allocationStruct.needsFlush, event);

    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendImageCopyRegion(ze_image_handle_t hDstImage,
                                                                        ze_image_handle_t hSrcImage,
                                                                        const ze_image_region_t *pDstRegion,
                                                                        const ze_image_region_t *pSrcRegion,
                                                                        ze_event_handle_t hEvent,
                                                                        uint32_t numWaitEvents,
                                                                        ze_event_handle_t *phWaitEvents) {
    auto dstImage = L0::Image::fromHandle(hDstImage);
    auto srcImage = L0::Image::fromHandle(hSrcImage);
    cl_int4 srcOffset, dstOffset;

    ze_image_region_t srcRegion, dstRegion;

    if (pSrcRegion != nullptr) {
        srcRegion = *pSrcRegion;
    } else {
        ze_image_desc_t srcDesc = srcImage->getImageDesc();
        srcRegion = {0, 0, 0, static_cast<uint32_t>(srcDesc.width), srcDesc.height, srcDesc.depth};
    }

    srcOffset.x = static_cast<cl_int>(srcRegion.originX);
    srcOffset.y = static_cast<cl_int>(srcRegion.originY);
    srcOffset.z = static_cast<cl_int>(srcRegion.originZ);
    srcOffset.w = 0;

    if (pDstRegion != nullptr) {
        dstRegion = *pDstRegion;
    } else {
        ze_image_desc_t dstDesc = dstImage->getImageDesc();
        dstRegion = {0, 0, 0, static_cast<uint32_t>(dstDesc.width), dstDesc.height, dstDesc.depth};
    }

    dstOffset.x = static_cast<cl_int>(dstRegion.originX);
    dstOffset.y = static_cast<cl_int>(dstRegion.originY);
    dstOffset.z = static_cast<cl_int>(dstRegion.originZ);
    dstOffset.w = 0;

    if (srcRegion.width != dstRegion.width ||
        srcRegion.height != dstRegion.height ||
        srcRegion.depth != dstRegion.depth) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    uint32_t groupSizeX = srcRegion.width;
    uint32_t groupSizeY = srcRegion.height;
    uint32_t groupSizeZ = srcRegion.depth;

    Event *event = nullptr;
    if (hEvent) {
        event = Event::fromHandle(hEvent);
    }

    if (isCopyOnly()) {
        auto bytesPerPixel = static_cast<uint32_t>(srcImage->getImageInfo().surfaceFormat->ImageElementSizeInBytes);

        Vec3<size_t> srcImgSize = {srcImage->getImageInfo().imgDesc.imageWidth,
                                   srcImage->getImageInfo().imgDesc.imageHeight,
                                   srcImage->getImageInfo().imgDesc.imageDepth};

        Vec3<size_t> dstImgSize = {dstImage->getImageInfo().imgDesc.imageWidth,
                                   dstImage->getImageInfo().imgDesc.imageHeight,
                                   dstImage->getImageInfo().imgDesc.imageDepth};

        auto srcRowPitch = srcRegion.width * bytesPerPixel;
        auto srcSlicePitch =
            (srcImage->getImageInfo().imgDesc.imageType == NEO::ImageType::Image1DArray ? 1 : srcRegion.height) * srcRowPitch;

        auto dstRowPitch = dstRegion.width * bytesPerPixel;
        auto dstSlicePitch =
            (dstImage->getImageInfo().imgDesc.imageType == NEO::ImageType::Image1DArray ? 1 : dstRegion.height) * dstRowPitch;

        return appendCopyImageBlit(srcImage->getAllocation(), dstImage->getAllocation(),
                                   {srcRegion.originX, srcRegion.originY, srcRegion.originZ}, {dstRegion.originX, dstRegion.originY, dstRegion.originZ}, srcRowPitch, srcSlicePitch,
                                   dstRowPitch, dstSlicePitch, bytesPerPixel, {srcRegion.width, srcRegion.height, srcRegion.depth}, srcImgSize, dstImgSize, event);
    }

    auto lock = device->getBuiltinFunctionsLib()->obtainUniqueOwnership();

    auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::CopyImageRegion);

    if (kernel->suggestGroupSize(groupSizeX, groupSizeY, groupSizeZ, &groupSizeX,
                                 &groupSizeY, &groupSizeZ) != ZE_RESULT_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (kernel->setGroupSize(groupSizeX, groupSizeY, groupSizeZ) != ZE_RESULT_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (srcRegion.width % groupSizeX || srcRegion.height % groupSizeY || srcRegion.depth % groupSizeZ) {
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ze_group_count_t kernelArgs{srcRegion.width / groupSizeX, srcRegion.height / groupSizeY,
                                srcRegion.depth / groupSizeZ};

    kernel->setArgRedescribedImage(0, hSrcImage);
    kernel->setArgRedescribedImage(1, hDstImage);
    kernel->setArgumentValue(2, sizeof(srcOffset), &srcOffset);
    kernel->setArgumentValue(3, sizeof(dstOffset), &dstOffset);

    CmdListKernelLaunchParams launchParams = {};
    launchParams.isBuiltInKernel = true;
    return CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernel(kernel->toHandle(), &kernelArgs,
                                                                    event, numWaitEvents, phWaitEvents,
                                                                    launchParams);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendImageCopy(ze_image_handle_t hDstImage,
                                                                  ze_image_handle_t hSrcImage,
                                                                  ze_event_handle_t hEvent,
                                                                  uint32_t numWaitEvents,
                                                                  ze_event_handle_t *phWaitEvents) {

    return this->appendImageCopyRegion(hDstImage, hSrcImage, nullptr, nullptr, hEvent,
                                       numWaitEvents, phWaitEvents);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemAdvise(ze_device_handle_t hDevice,
                                                                  const void *ptr, size_t size,
                                                                  ze_memory_advice_t advice) {
    NEO::MemAdviseFlags flags;
    flags.memadvise_flags = 0;

    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    if (allocData) {
        DeviceImp *deviceImp = static_cast<DeviceImp *>((L0::Device::fromHandle(hDevice)));

        if (deviceImp->memAdviseSharedAllocations.find(allocData) != deviceImp->memAdviseSharedAllocations.end()) {
            flags = deviceImp->memAdviseSharedAllocations[allocData];
        }

        switch (advice) {
        case ZE_MEMORY_ADVICE_SET_READ_MOSTLY:
            flags.read_only = 1;
            break;
        case ZE_MEMORY_ADVICE_CLEAR_READ_MOSTLY:
            flags.read_only = 0;
            break;
        case ZE_MEMORY_ADVICE_SET_PREFERRED_LOCATION:
            flags.device_preferred_location = 1;
            break;
        case ZE_MEMORY_ADVICE_CLEAR_PREFERRED_LOCATION:
            flags.device_preferred_location = 0;
            break;
        case ZE_MEMORY_ADVICE_BIAS_CACHED:
            flags.cached_memory = 1;
            break;
        case ZE_MEMORY_ADVICE_BIAS_UNCACHED:
            flags.cached_memory = 0;
            break;
        case ZE_MEMORY_ADVICE_SET_NON_ATOMIC_MOSTLY:
        case ZE_MEMORY_ADVICE_CLEAR_NON_ATOMIC_MOSTLY:
        default:
            break;
        }

        auto memoryManager = device->getDriverHandle()->getMemoryManager();
        auto pageFaultManager = memoryManager->getPageFaultManager();
        if (pageFaultManager) {
            /* If Read Only and Device Preferred Hints have been cleared, then cpu_migration of Shared memory can be re-enabled*/
            if (flags.cpu_migration_blocked) {
                if (flags.read_only == 0 && flags.device_preferred_location == 0) {
                    pageFaultManager->protectCPUMemoryAccess(const_cast<void *>(ptr), size);
                    flags.cpu_migration_blocked = 0;
                }
            }
            /* Given MemAdvise hints, use different gpu Domain Handler for the Page Fault Handling */
            pageFaultManager->setGpuDomainHandler(L0::handleGpuDomainTransferForHwWithHints);
        }

        auto alloc = allocData->gpuAllocations.getGraphicsAllocation(deviceImp->getRootDeviceIndex());
        memoryManager->setMemAdvise(alloc, flags, deviceImp->getRootDeviceIndex());

        deviceImp->memAdviseSharedAllocations[allocData] = flags;
        return ZE_RESULT_SUCCESS;
    }
    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyKernelWithGA(void *dstPtr,
                                                                               NEO::GraphicsAllocation *dstPtrAlloc,
                                                                               uint64_t dstOffset,
                                                                               void *srcPtr,
                                                                               NEO::GraphicsAllocation *srcPtrAlloc,
                                                                               uint64_t srcOffset,
                                                                               uint64_t size,
                                                                               uint64_t elementSize,
                                                                               Builtin builtin,
                                                                               Event *signalEvent,
                                                                               bool isStateless) {

    auto lock = device->getBuiltinFunctionsLib()->obtainUniqueOwnership();

    Kernel *builtinKernel = nullptr;

    builtinKernel = device->getBuiltinFunctionsLib()->getFunction(builtin);

    uint32_t groupSizeX = builtinKernel->getImmutableData()
                              ->getDescriptor()
                              .kernelAttributes.simdSize;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;

    if (builtinKernel->setGroupSize(groupSizeX, groupSizeY, groupSizeZ)) {
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    builtinKernel->setArgBufferWithAlloc(0u, *reinterpret_cast<uintptr_t *>(dstPtr), dstPtrAlloc);
    builtinKernel->setArgBufferWithAlloc(1u, *reinterpret_cast<uintptr_t *>(srcPtr), srcPtrAlloc);

    uint64_t elems = size / elementSize;
    builtinKernel->setArgumentValue(2, sizeof(elems), &elems);
    builtinKernel->setArgumentValue(3, sizeof(dstOffset), &dstOffset);
    builtinKernel->setArgumentValue(4, sizeof(srcOffset), &srcOffset);

    uint32_t groups = static_cast<uint32_t>((size + ((static_cast<uint64_t>(groupSizeX) * elementSize) - 1)) / (static_cast<uint64_t>(groupSizeX) * elementSize));
    ze_group_count_t dispatchKernelArgs{groups, 1u, 1u};

    auto dstAllocationType = dstPtrAlloc->getAllocationType();
    CmdListKernelLaunchParams launchParams = {};
    launchParams.isKernelSplitOperation = true;
    launchParams.isBuiltInKernel = true;
    launchParams.isDestinationAllocationInSystemMemory =
        (dstAllocationType == NEO::AllocationType::BUFFER_HOST_MEMORY) ||
        (dstAllocationType == NEO::AllocationType::SVM_CPU) ||
        (dstAllocationType == NEO::AllocationType::EXTERNAL_HOST_PTR);

    return CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernelSplit(builtinKernel, &dispatchKernelArgs, signalEvent, launchParams);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyBlit(uintptr_t dstPtr,
                                                                       NEO::GraphicsAllocation *dstPtrAlloc,
                                                                       uint64_t dstOffset, uintptr_t srcPtr,
                                                                       NEO::GraphicsAllocation *srcPtrAlloc,
                                                                       uint64_t srcOffset,
                                                                       uint64_t size) {
    dstOffset += ptrDiff<uintptr_t>(dstPtr, dstPtrAlloc->getGpuAddress());
    srcOffset += ptrDiff<uintptr_t>(srcPtr, srcPtrAlloc->getGpuAddress());

    auto clearColorAllocation = device->getNEODevice()->getDefaultEngine().commandStreamReceiver->getClearColorAllocation();

    auto blitProperties = NEO::BlitProperties::constructPropertiesForCopy(dstPtrAlloc, srcPtrAlloc, {dstOffset, 0, 0}, {srcOffset, 0, 0}, {size, 0, 0}, 0, 0, 0, 0, clearColorAllocation);
    commandContainer.addToResidencyContainer(dstPtrAlloc);
    commandContainer.addToResidencyContainer(srcPtrAlloc);
    commandContainer.addToResidencyContainer(clearColorAllocation);

    NEO::BlitPropertiesContainer blitPropertiesContainer{blitProperties};

    NEO::BlitCommandsHelper<GfxFamily>::dispatchBlitCommandsForBufferPerRow(blitProperties, *commandContainer.getCommandStream(), *device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]);

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyBlitRegion(NEO::GraphicsAllocation *srcAlloc,
                                                                             NEO::GraphicsAllocation *dstAlloc,
                                                                             size_t srcOffset,
                                                                             size_t dstOffset,
                                                                             ze_copy_region_t srcRegion,
                                                                             ze_copy_region_t dstRegion, const Vec3<size_t> &copySize,
                                                                             size_t srcRowPitch, size_t srcSlicePitch,
                                                                             size_t dstRowPitch, size_t dstSlicePitch,
                                                                             const Vec3<size_t> &srcSize, const Vec3<size_t> &dstSize,
                                                                             Event *signalEvent,
                                                                             uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {
    dstRegion.originX += static_cast<uint32_t>(dstOffset);
    srcRegion.originX += static_cast<uint32_t>(srcOffset);
    uint32_t bytesPerPixel = NEO::BlitCommandsHelper<GfxFamily>::getAvailableBytesPerPixel(copySize.x, srcRegion.originX, dstRegion.originX, srcSize.x, dstSize.x);
    Vec3<size_t> srcPtrOffset = {srcRegion.originX / bytesPerPixel, srcRegion.originY, srcRegion.originZ};
    Vec3<size_t> dstPtrOffset = {dstRegion.originX / bytesPerPixel, dstRegion.originY, dstRegion.originZ};
    auto clearColorAllocation = device->getNEODevice()->getDefaultEngine().commandStreamReceiver->getClearColorAllocation();

    Vec3<size_t> copySizeModified = {copySize.x / bytesPerPixel, copySize.y, copySize.z};
    auto blitProperties = NEO::BlitProperties::constructPropertiesForCopy(dstAlloc, srcAlloc,
                                                                          dstPtrOffset, srcPtrOffset, copySizeModified, srcRowPitch, srcSlicePitch,
                                                                          dstRowPitch, dstSlicePitch, clearColorAllocation);
    commandContainer.addToResidencyContainer(dstAlloc);
    commandContainer.addToResidencyContainer(srcAlloc);
    commandContainer.addToResidencyContainer(clearColorAllocation);
    blitProperties.bytesPerPixel = bytesPerPixel;
    blitProperties.srcSize = srcSize;
    blitProperties.dstSize = dstSize;

    ze_result_t ret = addEventsToCmdList(numWaitEvents, phWaitEvents);
    if (ret) {
        return ret;
    }

    NEO::BlitPropertiesContainer blitPropertiesContainer{blitProperties};

    appendEventForProfiling(signalEvent, true, false);
    bool copyRegionPreferred = NEO::BlitCommandsHelper<GfxFamily>::isCopyRegionPreferred(copySizeModified, *device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]);
    if (copyRegionPreferred) {
        NEO::BlitCommandsHelper<GfxFamily>::dispatchBlitCommandsForBufferRegion(blitProperties, *commandContainer.getCommandStream(), *device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]);
    } else {
        NEO::BlitCommandsHelper<GfxFamily>::dispatchBlitCommandsForBufferPerRow(blitProperties, *commandContainer.getCommandStream(), *device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]);
    }
    appendSignalEventPostWalker(signalEvent, false);
    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendCopyImageBlit(NEO::GraphicsAllocation *src,
                                                                      NEO::GraphicsAllocation *dst,
                                                                      const Vec3<size_t> &srcOffsets, const Vec3<size_t> &dstOffsets,
                                                                      size_t srcRowPitch, size_t srcSlicePitch,
                                                                      size_t dstRowPitch, size_t dstSlicePitch,
                                                                      size_t bytesPerPixel, const Vec3<size_t> &copySize,
                                                                      const Vec3<size_t> &srcSize, const Vec3<size_t> &dstSize,
                                                                      Event *signalEvent) {
    auto clearColorAllocation = device->getNEODevice()->getDefaultEngine().commandStreamReceiver->getClearColorAllocation();

    auto blitProperties = NEO::BlitProperties::constructPropertiesForCopy(dst, src,
                                                                          dstOffsets, srcOffsets, copySize, srcRowPitch, srcSlicePitch,
                                                                          dstRowPitch, dstSlicePitch, clearColorAllocation);
    blitProperties.bytesPerPixel = bytesPerPixel;
    blitProperties.srcSize = srcSize;
    blitProperties.dstSize = dstSize;
    commandContainer.addToResidencyContainer(dst);
    commandContainer.addToResidencyContainer(src);
    commandContainer.addToResidencyContainer(clearColorAllocation);

    appendEventForProfiling(signalEvent, true, false);
    NEO::BlitCommandsHelper<GfxFamily>::dispatchBlitCommandsForImageRegion(blitProperties, *commandContainer.getCommandStream(), *device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]);
    appendSignalEventPostWalker(signalEvent, false);
    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendPageFaultCopy(NEO::GraphicsAllocation *dstAllocation,
                                                                      NEO::GraphicsAllocation *srcAllocation,
                                                                      size_t size, bool flushHost) {

    size_t middleElSize = sizeof(uint32_t) * 4;
    uintptr_t rightSize = size % middleElSize;
    bool isStateless = false;

    if (size >= 4ull * MemoryConstants::gigaByte) {
        isStateless = true;
    }

    uintptr_t dstAddress = static_cast<uintptr_t>(dstAllocation->getGpuAddress());
    uintptr_t srcAddress = static_cast<uintptr_t>(srcAllocation->getGpuAddress());
    ze_result_t ret = ZE_RESULT_ERROR_UNKNOWN;
    if (isCopyOnly()) {
        return appendMemoryCopyBlit(dstAddress, dstAllocation, 0u,
                                    srcAddress, srcAllocation, 0u,
                                    size);
    } else {
        ret = appendMemoryCopyKernelWithGA(reinterpret_cast<void *>(&dstAddress),
                                           dstAllocation, 0,
                                           reinterpret_cast<void *>(&srcAddress),
                                           srcAllocation, 0,
                                           size - rightSize,
                                           middleElSize,
                                           Builtin::CopyBufferToBufferMiddle,
                                           nullptr,
                                           isStateless);
        if (ret == ZE_RESULT_SUCCESS && rightSize) {
            ret = appendMemoryCopyKernelWithGA(reinterpret_cast<void *>(&dstAddress),
                                               dstAllocation, size - rightSize,
                                               reinterpret_cast<void *>(&srcAddress),
                                               srcAllocation, size - rightSize,
                                               rightSize, 1UL,
                                               Builtin::CopyBufferToBufferSide,
                                               nullptr,
                                               isStateless);
        }

        if (this->dcFlushSupport) {
            if (flushHost) {
                NEO::PipeControlArgs args;
                args.dcFlushEnable = true;
                NEO::MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(*commandContainer.getCommandStream(), args);
            }
        }
    }

    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopy(void *dstptr,
                                                                   const void *srcptr,
                                                                   size_t size,
                                                                   ze_event_handle_t hSignalEvent,
                                                                   uint32_t numWaitEvents,
                                                                   ze_event_handle_t *phWaitEvents) {

    uintptr_t start = reinterpret_cast<uintptr_t>(dstptr);
    bool isStateless = false;

    NEO::Device *neoDevice = device->getNEODevice();
    uint32_t callId = 0;
    if (NEO::DebugManager.flags.EnableSWTags.get()) {
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameBeginTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            "zeCommandListAppendMemoryCopy",
            ++neoDevice->getRootDeviceEnvironment().tagsManager->currentCallCount);
        callId = neoDevice->getRootDeviceEnvironment().tagsManager->currentCallCount;
    }

    size_t middleAlignment = MemoryConstants::cacheLineSize;
    size_t middleElSize = sizeof(uint32_t) * 4;

    uintptr_t leftSize = start % middleAlignment;
    leftSize = (leftSize > 0) ? (middleAlignment - leftSize) : 0;
    leftSize = std::min(leftSize, size);

    uintptr_t rightSize = (start + size) % middleAlignment;
    rightSize = std::min(rightSize, size - leftSize);

    uintptr_t middleSizeBytes = size - leftSize - rightSize;

    if (!isAligned<4>(reinterpret_cast<uintptr_t>(srcptr) + leftSize)) {
        leftSize += middleSizeBytes;
        middleSizeBytes = 0;
    }

    DEBUG_BREAK_IF(size != leftSize + middleSizeBytes + rightSize);

    auto dstAllocationStruct = getAlignedAllocation(this->device, dstptr, size, false);
    auto srcAllocationStruct = getAlignedAllocation(this->device, srcptr, size, true);

    if (dstAllocationStruct.alloc == nullptr || srcAllocationStruct.alloc == nullptr) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (size >= 4ull * MemoryConstants::gigaByte) {
        isStateless = true;
    }

    ze_result_t ret = addEventsToCmdList(numWaitEvents, phWaitEvents);

    if (ret) {
        return ret;
    }

    Event *signalEvent = nullptr;
    if (hSignalEvent) {
        signalEvent = Event::fromHandle(hSignalEvent);
    }

    appendEventForProfilingAllWalkers(signalEvent, true);

    if (ret == ZE_RESULT_SUCCESS && leftSize) {
        Builtin copyKernel = Builtin::CopyBufferToBufferSide;
        if (isStateless) {
            copyKernel = Builtin::CopyBufferToBufferSideStateless;
        }
        if (isCopyOnly()) {
            ret = appendMemoryCopyBlit(dstAllocationStruct.alignedAllocationPtr,
                                       dstAllocationStruct.alloc, dstAllocationStruct.offset,
                                       srcAllocationStruct.alignedAllocationPtr,
                                       srcAllocationStruct.alloc, srcAllocationStruct.offset, leftSize);
        } else {
            ret = appendMemoryCopyKernelWithGA(reinterpret_cast<void *>(&dstAllocationStruct.alignedAllocationPtr),
                                               dstAllocationStruct.alloc, dstAllocationStruct.offset,
                                               reinterpret_cast<void *>(&srcAllocationStruct.alignedAllocationPtr),
                                               srcAllocationStruct.alloc, srcAllocationStruct.offset,
                                               leftSize, 1UL,
                                               copyKernel,
                                               signalEvent,
                                               isStateless);
        }
    }

    if (ret == ZE_RESULT_SUCCESS && middleSizeBytes) {
        Builtin copyKernel = Builtin::CopyBufferToBufferMiddle;
        if (isStateless) {
            copyKernel = Builtin::CopyBufferToBufferMiddleStateless;
        }
        if (isCopyOnly()) {
            ret = appendMemoryCopyBlit(dstAllocationStruct.alignedAllocationPtr,
                                       dstAllocationStruct.alloc, leftSize + dstAllocationStruct.offset,
                                       srcAllocationStruct.alignedAllocationPtr,
                                       srcAllocationStruct.alloc, leftSize + srcAllocationStruct.offset, middleSizeBytes);
        } else {
            ret = appendMemoryCopyKernelWithGA(reinterpret_cast<void *>(&dstAllocationStruct.alignedAllocationPtr),
                                               dstAllocationStruct.alloc, leftSize + dstAllocationStruct.offset,
                                               reinterpret_cast<void *>(&srcAllocationStruct.alignedAllocationPtr),
                                               srcAllocationStruct.alloc, leftSize + srcAllocationStruct.offset,
                                               middleSizeBytes,
                                               middleElSize,
                                               copyKernel,
                                               signalEvent,
                                               isStateless);
        }
    }

    if (ret == ZE_RESULT_SUCCESS && rightSize) {
        Builtin copyKernel = Builtin::CopyBufferToBufferSide;
        if (isStateless) {
            copyKernel = Builtin::CopyBufferToBufferSideStateless;
        }
        if (isCopyOnly()) {
            ret = appendMemoryCopyBlit(dstAllocationStruct.alignedAllocationPtr,
                                       dstAllocationStruct.alloc, leftSize + middleSizeBytes + dstAllocationStruct.offset,
                                       srcAllocationStruct.alignedAllocationPtr,
                                       srcAllocationStruct.alloc, leftSize + middleSizeBytes + srcAllocationStruct.offset, rightSize);
        } else {
            ret = appendMemoryCopyKernelWithGA(reinterpret_cast<void *>(&dstAllocationStruct.alignedAllocationPtr),
                                               dstAllocationStruct.alloc, leftSize + middleSizeBytes + dstAllocationStruct.offset,
                                               reinterpret_cast<void *>(&srcAllocationStruct.alignedAllocationPtr),
                                               srcAllocationStruct.alloc, leftSize + middleSizeBytes + srcAllocationStruct.offset,
                                               rightSize, 1UL,
                                               copyKernel,
                                               signalEvent,
                                               isStateless);
        }
    }

    appendEventForProfilingAllWalkers(signalEvent, false);
    addFlushRequiredCommand(dstAllocationStruct.needsFlush, signalEvent);

    if (NEO::DebugManager.flags.EnableSWTags.get()) {
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameEndTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            "zeCommandListAppendMemoryCopy",
            callId);
    }

    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyRegion(void *dstPtr,
                                                                         const ze_copy_region_t *dstRegion,
                                                                         uint32_t dstPitch,
                                                                         uint32_t dstSlicePitch,
                                                                         const void *srcPtr,
                                                                         const ze_copy_region_t *srcRegion,
                                                                         uint32_t srcPitch,
                                                                         uint32_t srcSlicePitch,
                                                                         ze_event_handle_t hSignalEvent,
                                                                         uint32_t numWaitEvents,
                                                                         ze_event_handle_t *phWaitEvents) {

    NEO::Device *neoDevice = device->getNEODevice();
    uint32_t callId = 0;
    if (NEO::DebugManager.flags.EnableSWTags.get()) {
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameBeginTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            "zeCommandListAppendMemoryCopyRegion",
            ++neoDevice->getRootDeviceEnvironment().tagsManager->currentCallCount);
        callId = neoDevice->getRootDeviceEnvironment().tagsManager->currentCallCount;
    }

    size_t dstSize = this->getTotalSizeForCopyRegion(dstRegion, dstPitch, dstSlicePitch);
    size_t srcSize = this->getTotalSizeForCopyRegion(srcRegion, srcPitch, srcSlicePitch);

    auto dstAllocationStruct = getAlignedAllocation(this->device, dstPtr, dstSize, false);
    auto srcAllocationStruct = getAlignedAllocation(this->device, srcPtr, srcSize, true);

    dstSize += dstAllocationStruct.offset;
    srcSize += srcAllocationStruct.offset;

    Vec3<size_t> srcSize3 = {srcPitch ? srcPitch : srcRegion->width + srcRegion->originX,
                             srcSlicePitch ? srcSlicePitch / srcPitch : srcRegion->height + srcRegion->originY,
                             srcRegion->depth + srcRegion->originZ};
    Vec3<size_t> dstSize3 = {dstPitch ? dstPitch : dstRegion->width + dstRegion->originX,
                             dstSlicePitch ? dstSlicePitch / dstPitch : dstRegion->height + dstRegion->originY,
                             dstRegion->depth + dstRegion->originZ};

    Event *signalEvent = nullptr;
    if (hSignalEvent) {
        signalEvent = Event::fromHandle(hSignalEvent);
    }

    ze_result_t result = ZE_RESULT_SUCCESS;
    if (srcRegion->depth > 1) {
        result = isCopyOnly() ? appendMemoryCopyBlitRegion(srcAllocationStruct.alloc, dstAllocationStruct.alloc, srcAllocationStruct.offset, dstAllocationStruct.offset, *srcRegion, *dstRegion, {srcRegion->width, srcRegion->height, srcRegion->depth},
                                                           srcPitch, srcSlicePitch, dstPitch, dstSlicePitch, srcSize3, dstSize3, signalEvent, numWaitEvents, phWaitEvents)
                              : this->appendMemoryCopyKernel3d(&dstAllocationStruct, &srcAllocationStruct,
                                                               Builtin::CopyBufferRectBytes3d, dstRegion, dstPitch, dstSlicePitch, dstAllocationStruct.offset,
                                                               srcRegion, srcPitch, srcSlicePitch, srcAllocationStruct.offset, signalEvent, numWaitEvents, phWaitEvents);
    } else {
        result = isCopyOnly() ? appendMemoryCopyBlitRegion(srcAllocationStruct.alloc, dstAllocationStruct.alloc, srcAllocationStruct.offset, dstAllocationStruct.offset, *srcRegion, *dstRegion, {srcRegion->width, srcRegion->height, srcRegion->depth},
                                                           srcPitch, srcSlicePitch, dstPitch, dstSlicePitch, srcSize3, dstSize3, signalEvent, numWaitEvents, phWaitEvents)
                              : this->appendMemoryCopyKernel2d(&dstAllocationStruct, &srcAllocationStruct,
                                                               Builtin::CopyBufferRectBytes2d, dstRegion, dstPitch, dstAllocationStruct.offset,
                                                               srcRegion, srcPitch, srcAllocationStruct.offset, signalEvent, numWaitEvents, phWaitEvents);
    }

    if (result) {
        return result;
    }

    addFlushRequiredCommand(dstAllocationStruct.needsFlush, signalEvent);

    if (NEO::DebugManager.flags.EnableSWTags.get()) {
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameEndTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            "zeCommandListAppendMemoryCopyRegion",
            callId);
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyKernel3d(AlignedAllocationData *dstAlignedAllocation,
                                                                           AlignedAllocationData *srcAlignedAllocation,
                                                                           Builtin builtin,
                                                                           const ze_copy_region_t *dstRegion,
                                                                           uint32_t dstPitch,
                                                                           uint32_t dstSlicePitch,
                                                                           size_t dstOffset,
                                                                           const ze_copy_region_t *srcRegion,
                                                                           uint32_t srcPitch,
                                                                           uint32_t srcSlicePitch,
                                                                           size_t srcOffset,
                                                                           Event *signalEvent,
                                                                           uint32_t numWaitEvents,
                                                                           ze_event_handle_t *phWaitEvents) {

    auto lock = device->getBuiltinFunctionsLib()->obtainUniqueOwnership();

    auto builtinKernel = device->getBuiltinFunctionsLib()->getFunction(builtin);

    uint32_t groupSizeX = srcRegion->width;
    uint32_t groupSizeY = srcRegion->height;
    uint32_t groupSizeZ = srcRegion->depth;

    if (builtinKernel->suggestGroupSize(groupSizeX, groupSizeY, groupSizeZ,
                                        &groupSizeX, &groupSizeY, &groupSizeZ) != ZE_RESULT_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (builtinKernel->setGroupSize(groupSizeX, groupSizeY, groupSizeZ) != ZE_RESULT_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (srcRegion->width % groupSizeX || srcRegion->height % groupSizeY || srcRegion->depth % groupSizeZ) {
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ze_group_count_t dispatchKernelArgs{srcRegion->width / groupSizeX, srcRegion->height / groupSizeY,
                                        srcRegion->depth / groupSizeZ};

    uint32_t srcOrigin[3] = {(srcRegion->originX + static_cast<uint32_t>(srcOffset)), (srcRegion->originY), (srcRegion->originZ)};
    uint32_t dstOrigin[3] = {(dstRegion->originX + static_cast<uint32_t>(dstOffset)), (dstRegion->originY), (dstRegion->originZ)};
    uint32_t srcPitches[2] = {(srcPitch), (srcSlicePitch)};
    uint32_t dstPitches[2] = {(dstPitch), (dstSlicePitch)};

    builtinKernel->setArgBufferWithAlloc(0, srcAlignedAllocation->alignedAllocationPtr, srcAlignedAllocation->alloc);
    builtinKernel->setArgBufferWithAlloc(1, dstAlignedAllocation->alignedAllocationPtr, dstAlignedAllocation->alloc);
    builtinKernel->setArgumentValue(2, sizeof(srcOrigin), &srcOrigin);
    builtinKernel->setArgumentValue(3, sizeof(dstOrigin), &dstOrigin);
    builtinKernel->setArgumentValue(4, sizeof(srcPitches), &srcPitches);
    builtinKernel->setArgumentValue(5, sizeof(dstPitches), &dstPitches);

    auto dstAllocationType = dstAlignedAllocation->alloc->getAllocationType();
    CmdListKernelLaunchParams launchParams = {};
    launchParams.isBuiltInKernel = true;
    launchParams.isDestinationAllocationInSystemMemory =
        (dstAllocationType == NEO::AllocationType::BUFFER_HOST_MEMORY) ||
        (dstAllocationType == NEO::AllocationType::EXTERNAL_HOST_PTR);
    return CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernel(builtinKernel->toHandle(), &dispatchKernelArgs, signalEvent, numWaitEvents,
                                                                    phWaitEvents, launchParams);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyKernel2d(AlignedAllocationData *dstAlignedAllocation,
                                                                           AlignedAllocationData *srcAlignedAllocation,
                                                                           Builtin builtin,
                                                                           const ze_copy_region_t *dstRegion,
                                                                           uint32_t dstPitch,
                                                                           size_t dstOffset,
                                                                           const ze_copy_region_t *srcRegion,
                                                                           uint32_t srcPitch,
                                                                           size_t srcOffset,
                                                                           Event *signalEvent,
                                                                           uint32_t numWaitEvents,
                                                                           ze_event_handle_t *phWaitEvents) {

    auto lock = device->getBuiltinFunctionsLib()->obtainUniqueOwnership();

    auto builtinKernel = device->getBuiltinFunctionsLib()->getFunction(builtin);

    uint32_t groupSizeX = srcRegion->width;
    uint32_t groupSizeY = srcRegion->height;
    uint32_t groupSizeZ = 1u;

    if (builtinKernel->suggestGroupSize(groupSizeX, groupSizeY, groupSizeZ, &groupSizeX,
                                        &groupSizeY, &groupSizeZ) != ZE_RESULT_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (builtinKernel->setGroupSize(groupSizeX, groupSizeY, groupSizeZ) != ZE_RESULT_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (srcRegion->width % groupSizeX || srcRegion->height % groupSizeY) {
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ze_group_count_t dispatchKernelArgs{srcRegion->width / groupSizeX, srcRegion->height / groupSizeY, 1u};

    uint32_t srcOrigin[2] = {(srcRegion->originX + static_cast<uint32_t>(srcOffset)), (srcRegion->originY)};
    uint32_t dstOrigin[2] = {(dstRegion->originX + static_cast<uint32_t>(dstOffset)), (dstRegion->originY)};

    builtinKernel->setArgBufferWithAlloc(0, srcAlignedAllocation->alignedAllocationPtr, srcAlignedAllocation->alloc);
    builtinKernel->setArgBufferWithAlloc(1, dstAlignedAllocation->alignedAllocationPtr, dstAlignedAllocation->alloc);
    builtinKernel->setArgumentValue(2, sizeof(srcOrigin), &srcOrigin);
    builtinKernel->setArgumentValue(3, sizeof(dstOrigin), &dstOrigin);
    builtinKernel->setArgumentValue(4, sizeof(srcPitch), &srcPitch);
    builtinKernel->setArgumentValue(5, sizeof(dstPitch), &dstPitch);

    auto dstAllocationType = dstAlignedAllocation->alloc->getAllocationType();
    CmdListKernelLaunchParams launchParams = {};
    launchParams.isBuiltInKernel = true;
    launchParams.isDestinationAllocationInSystemMemory =
        (dstAllocationType == NEO::AllocationType::BUFFER_HOST_MEMORY) ||
        (dstAllocationType == NEO::AllocationType::EXTERNAL_HOST_PTR);
    return CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernel(builtinKernel->toHandle(),
                                                                    &dispatchKernelArgs, signalEvent,
                                                                    numWaitEvents,
                                                                    phWaitEvents,
                                                                    launchParams);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemoryPrefetch(const void *ptr,
                                                                       size_t count) {
    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    if (allocData) {
        return ZE_RESULT_SUCCESS;
    }
    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendUnalignedFillKernel(bool isStateless, uint32_t unalignedSize, const AlignedAllocationData &dstAllocation, const void *pattern, Event *signalEvent, const CmdListKernelLaunchParams &launchParams) {
    Kernel *builtinKernel = nullptr;
    if (isStateless) {
        builtinKernel = device->getBuiltinFunctionsLib()->getFunction(Builtin::FillBufferImmediateLeftOverStateless);
    } else {
        builtinKernel = device->getBuiltinFunctionsLib()->getFunction(Builtin::FillBufferImmediateLeftOver);
    }
    uint32_t groupSizeY = 1, groupSizeZ = 1;
    uint32_t groupSizeX = static_cast<uint32_t>(unalignedSize);
    builtinKernel->suggestGroupSize(groupSizeX, groupSizeY, groupSizeZ, &groupSizeX, &groupSizeY, &groupSizeZ);
    builtinKernel->setGroupSize(groupSizeX, groupSizeY, groupSizeZ);
    ze_group_count_t dispatchKernelRemainderArgs{static_cast<uint32_t>(unalignedSize / groupSizeX), 1u, 1u};
    uint32_t value = *(reinterpret_cast<const unsigned char *>(pattern));
    builtinKernel->setArgBufferWithAlloc(0, dstAllocation.alignedAllocationPtr, dstAllocation.alloc);
    builtinKernel->setArgumentValue(1, sizeof(dstAllocation.offset), &dstAllocation.offset);
    builtinKernel->setArgumentValue(2, sizeof(value), &value);

    auto res = appendLaunchKernelSplit(builtinKernel, &dispatchKernelRemainderArgs, signalEvent, launchParams);
    if (res) {
        return res;
    }
    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemoryFill(void *ptr,
                                                                   const void *pattern,
                                                                   size_t patternSize,
                                                                   size_t size,
                                                                   ze_event_handle_t hSignalEvent,
                                                                   uint32_t numWaitEvents,
                                                                   ze_event_handle_t *phWaitEvents) {
    bool isStateless = false;

    NEO::Device *neoDevice = device->getNEODevice();
    uint32_t callId = 0;
    if (NEO::DebugManager.flags.EnableSWTags.get()) {
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameBeginTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            "zeCommandListAppendMemoryFill",
            ++neoDevice->getRootDeviceEnvironment().tagsManager->currentCallCount);
        callId = neoDevice->getRootDeviceEnvironment().tagsManager->currentCallCount;
    }

    Event *signalEvent = nullptr;
    if (hSignalEvent) {
        signalEvent = Event::fromHandle(hSignalEvent);
    }

    if (isCopyOnly()) {
        return appendBlitFill(ptr, pattern, patternSize, size, signalEvent, numWaitEvents, phWaitEvents);
    }

    ze_result_t res = addEventsToCmdList(numWaitEvents, phWaitEvents);
    if (res) {
        return res;
    }

    bool hostPointerNeedsFlush = false;

    NEO::SvmAllocationData *allocData = nullptr;
    bool dstAllocFound = device->getDriverHandle()->findAllocationDataForRange(ptr, size, &allocData);
    if (dstAllocFound) {
        if (allocData->memoryType == InternalMemoryType::HOST_UNIFIED_MEMORY ||
            allocData->memoryType == InternalMemoryType::SHARED_UNIFIED_MEMORY) {
            hostPointerNeedsFlush = true;
        }
    } else {
        if (device->getDriverHandle()->getHostPointerBaseAddress(ptr, nullptr) != ZE_RESULT_SUCCESS) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        } else {
            hostPointerNeedsFlush = true;
        }
    }

    auto dstAllocation = this->getAlignedAllocation(this->device, ptr, size, false);
    if (size >= 4ull * MemoryConstants::gigaByte) {
        isStateless = true;
    }
    auto lock = device->getBuiltinFunctionsLib()->obtainUniqueOwnership();

    CmdListKernelLaunchParams launchParams = {};
    launchParams.isKernelSplitOperation = true;
    launchParams.isBuiltInKernel = true;
    launchParams.isDestinationAllocationInSystemMemory = hostPointerNeedsFlush;
    if (patternSize == 1) {
        size_t middleSize = size;
        uint32_t leftRemainder = sizeof(uint32_t) - (dstAllocation.offset % sizeof(uint32_t));
        if (dstAllocation.offset % sizeof(uint32_t) != 0 && leftRemainder <= size) {
            res = appendUnalignedFillKernel(isStateless, leftRemainder, dstAllocation, pattern, signalEvent, launchParams);
            if (res) {
                return res;
            }
            middleSize -= leftRemainder;
            dstAllocation.offset += leftRemainder;
        }
        Kernel *builtinKernel = nullptr;

        if (isStateless) {
            builtinKernel = device->getBuiltinFunctionsLib()->getFunction(Builtin::FillBufferImmediateStateless);
        } else {
            builtinKernel = device->getBuiltinFunctionsLib()->getFunction(Builtin::FillBufferImmediate);
        }
        const auto dataTypeSize = sizeof(uint32_t) * 4;
        size_t adjustedSize = middleSize / dataTypeSize;
        size_t groupSizeX = device->getDeviceInfo().maxWorkGroupSize;
        if (groupSizeX > adjustedSize && adjustedSize > 0) {
            groupSizeX = adjustedSize;
        }
        if (builtinKernel->setGroupSize(static_cast<uint32_t>(groupSizeX), 1u, 1u)) {
            DEBUG_BREAK_IF(true);
            return ZE_RESULT_ERROR_UNKNOWN;
        }

        size_t groups = adjustedSize / groupSizeX;
        uint32_t remainingBytes = static_cast<uint32_t>((adjustedSize % groupSizeX) * dataTypeSize +
                                                        middleSize % dataTypeSize);
        ze_group_count_t dispatchKernelArgs{static_cast<uint32_t>(groups), 1u, 1u};

        uint32_t value = 0;
        memset(&value, *reinterpret_cast<const unsigned char *>(pattern), 4);
        builtinKernel->setArgBufferWithAlloc(0, dstAllocation.alignedAllocationPtr, dstAllocation.alloc);
        builtinKernel->setArgumentValue(1, sizeof(dstAllocation.offset), &dstAllocation.offset);
        builtinKernel->setArgumentValue(2, sizeof(value), &value);

        appendEventForProfilingAllWalkers(signalEvent, true);

        res = appendLaunchKernelSplit(builtinKernel, &dispatchKernelArgs, signalEvent, launchParams);
        if (res) {
            return res;
        }

        if (remainingBytes) {
            dstAllocation.offset += (middleSize - remainingBytes);
            res = appendUnalignedFillKernel(isStateless, remainingBytes, dstAllocation, pattern, signalEvent, launchParams);
            if (res) {
                return res;
            }
        }
    } else {

        Kernel *builtinKernel = nullptr;
        if (isStateless) {
            builtinKernel = device->getBuiltinFunctionsLib()->getFunction(Builtin::FillBufferMiddleStateless);
        } else {
            builtinKernel = device->getBuiltinFunctionsLib()->getFunction(Builtin::FillBufferMiddle);
        }
        size_t middleElSize = sizeof(uint32_t);
        size_t adjustedSize = size / middleElSize;
        uint32_t groupSizeX = static_cast<uint32_t>(adjustedSize);
        uint32_t groupSizeY = 1, groupSizeZ = 1;
        builtinKernel->suggestGroupSize(groupSizeX, groupSizeY, groupSizeZ, &groupSizeX, &groupSizeY, &groupSizeZ);
        builtinKernel->setGroupSize(groupSizeX, groupSizeY, groupSizeZ);

        uint32_t groups = static_cast<uint32_t>(adjustedSize) / groupSizeX;
        uint32_t remainingBytes = static_cast<uint32_t>((adjustedSize % groupSizeX) * middleElSize +
                                                        size % middleElSize);

        size_t patternAllocationSize = alignUp(patternSize, MemoryConstants::cacheLineSize);
        uint32_t patternSizeInEls = static_cast<uint32_t>(patternAllocationSize / middleElSize);

        auto patternGfxAlloc = device->obtainReusableAllocation(patternAllocationSize, NEO::AllocationType::FILL_PATTERN);
        if (patternGfxAlloc == nullptr) {
            patternGfxAlloc = device->getDriverHandle()->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getNEODevice()->getRootDeviceIndex(),
                                                                                                                   patternAllocationSize,
                                                                                                                   NEO::AllocationType::FILL_PATTERN,
                                                                                                                   device->getNEODevice()->getDeviceBitfield()});
        }
        void *patternGfxAllocPtr = patternGfxAlloc->getUnderlyingBuffer();
        patternAllocations.push_back(patternGfxAlloc);
        uint64_t patternAllocPtr = reinterpret_cast<uintptr_t>(patternGfxAllocPtr);
        uint64_t patternAllocOffset = 0;
        uint64_t patternSizeToCopy = patternSize;
        do {
            memcpy_s(reinterpret_cast<void *>(patternAllocPtr + patternAllocOffset),
                     patternSizeToCopy, pattern, patternSizeToCopy);

            if ((patternAllocOffset + patternSizeToCopy) > patternAllocationSize) {
                patternSizeToCopy = patternAllocationSize - patternAllocOffset;
            }

            patternAllocOffset += patternSizeToCopy;
        } while (patternAllocOffset < patternAllocationSize);
        builtinKernel->setArgBufferWithAlloc(0, dstAllocation.alignedAllocationPtr, dstAllocation.alloc);
        builtinKernel->setArgumentValue(1, sizeof(dstAllocation.offset), &dstAllocation.offset);
        builtinKernel->setArgBufferWithAlloc(2, reinterpret_cast<uintptr_t>(patternGfxAllocPtr), patternGfxAlloc);
        builtinKernel->setArgumentValue(3, sizeof(patternSizeInEls), &patternSizeInEls);

        appendEventForProfilingAllWalkers(signalEvent, true);

        ze_group_count_t dispatchKernelArgs{groups, 1u, 1u};
        res = appendLaunchKernelSplit(builtinKernel, &dispatchKernelArgs, signalEvent, launchParams);
        if (res) {
            return res;
        }

        if (remainingBytes) {
            uint32_t dstOffsetRemainder = groups * groupSizeX * static_cast<uint32_t>(middleElSize);
            uint64_t patternOffsetRemainder = (groupSizeX * groups & (patternSizeInEls - 1)) * middleElSize;

            Kernel *builtinKernelRemainder;
            if (isStateless) {
                builtinKernelRemainder = device->getBuiltinFunctionsLib()->getFunction(Builtin::FillBufferRightLeftoverStateless);
            } else {
                builtinKernelRemainder = device->getBuiltinFunctionsLib()->getFunction(Builtin::FillBufferRightLeftover);
            }

            builtinKernelRemainder->setGroupSize(remainingBytes, 1u, 1u);
            ze_group_count_t dispatchKernelArgs{1u, 1u, 1u};

            builtinKernelRemainder->setArgBufferWithAlloc(0,
                                                          dstAllocation.alignedAllocationPtr,
                                                          dstAllocation.alloc);
            builtinKernelRemainder->setArgumentValue(1,
                                                     sizeof(dstOffsetRemainder),
                                                     &dstOffsetRemainder);
            builtinKernelRemainder->setArgBufferWithAlloc(2,
                                                          reinterpret_cast<uintptr_t>(patternGfxAllocPtr) + patternOffsetRemainder,
                                                          patternGfxAlloc);
            builtinKernelRemainder->setArgumentValue(3, sizeof(patternAllocationSize), &patternAllocationSize);

            res = appendLaunchKernelSplit(builtinKernelRemainder, &dispatchKernelArgs, signalEvent, launchParams);
            if (res) {
                return res;
            }
        }
    }

    appendEventForProfilingAllWalkers(signalEvent, false);
    addFlushRequiredCommand(hostPointerNeedsFlush, signalEvent);

    if (NEO::DebugManager.flags.EnableSWTags.get()) {
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameEndTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            "zeCommandListAppendMemoryFill",
            callId);
    }

    return res;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendBlitFill(void *ptr,
                                                                 const void *pattern,
                                                                 size_t patternSize,
                                                                 size_t size,
                                                                 Event *signalEvent,
                                                                 uint32_t numWaitEvents,
                                                                 ze_event_handle_t *phWaitEvents) {
    auto neoDevice = device->getNEODevice();
    if (NEO::HwHelper::get(device->getHwInfo().platform.eRenderCoreFamily).getMaxFillPaternSizeForCopyEngine() < patternSize) {
        return ZE_RESULT_ERROR_INVALID_SIZE;
    } else {
        ze_result_t ret = addEventsToCmdList(numWaitEvents, phWaitEvents);
        if (ret) {
            return ret;
        }

        appendEventForProfiling(signalEvent, true, false);
        NEO::GraphicsAllocation *gpuAllocation = device->getDriverHandle()->getDriverSystemMemoryAllocation(ptr,
                                                                                                            size,
                                                                                                            neoDevice->getRootDeviceIndex(),
                                                                                                            nullptr);
        DriverHandleImp *driverHandle = static_cast<DriverHandleImp *>(device->getDriverHandle());
        auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
        if (driverHandle->isRemoteResourceNeeded(ptr, gpuAllocation, allocData, device)) {
            if (allocData) {
                uint64_t pbase = allocData->gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress();
                gpuAllocation = driverHandle->getPeerAllocation(device, allocData, reinterpret_cast<void *>(pbase), nullptr);
            }
            if (gpuAllocation == nullptr) {
                return ZE_RESULT_ERROR_INVALID_ARGUMENT;
            }
        }

        uint64_t offset = getAllocationOffsetForAppendBlitFill(ptr, *gpuAllocation);

        commandContainer.addToResidencyContainer(gpuAllocation);
        uint32_t patternToCommand[4] = {};
        memcpy_s(&patternToCommand, sizeof(patternToCommand), pattern, patternSize);
        NEO::BlitCommandsHelper<GfxFamily>::dispatchBlitMemoryColorFill(gpuAllocation, offset, patternToCommand, patternSize,
                                                                        *commandContainer.getCommandStream(),
                                                                        size,
                                                                        *neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]);
        appendSignalEventPostWalker(signalEvent, false);
    }
    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendSignalEventPostWalker(Event *event, bool workloadPartition) {
    if (event == nullptr) {
        return;
    }
    if (event->isEventTimestampFlagSet()) {
        appendEventForProfiling(event, false, workloadPartition);
    } else {
        commandContainer.addToResidencyContainer(&event->getAllocation(this->device));
        uint64_t baseAddr = event->getGpuAddress(this->device);
        if (event->isUsingContextEndOffset()) {
            baseAddr += event->getContextEndOffset();
        }

        const auto &hwInfo = this->device->getHwInfo();
        if (isCopyOnly()) {
            NEO::MiFlushArgs args;
            args.commandWithPostSync = true;
            NEO::EncodeMiFlushDW<GfxFamily>::programMiFlushDw(*commandContainer.getCommandStream(), baseAddr, Event::STATE_SIGNALED,
                                                              args, hwInfo);
        } else {
            NEO::PipeControlArgs args;
            args.dcFlushEnable = getDcFlushRequired(!!event->signalScope);
            if (this->partitionCount > 1) {
                args.workloadPartitionOffset = true;
                event->setPacketsInUse(this->partitionCount);
            }
            NEO::MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
                *commandContainer.getCommandStream(),
                NEO::PostSyncMode::ImmediateData,
                baseAddr,
                Event::STATE_SIGNALED,
                hwInfo,
                args);
        }
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendEventForProfilingCopyCommand(Event *event, bool beforeWalker) {
    if (!event->isEventTimestampFlagSet()) {
        return;
    }
    commandContainer.addToResidencyContainer(&event->getAllocation(this->device));

    const auto &hwInfo = this->device->getHwInfo();
    if (!beforeWalker) {
        NEO::MiFlushArgs args;
        NEO::EncodeMiFlushDW<GfxFamily>::programMiFlushDw(*commandContainer.getCommandStream(), 0, 0, args, hwInfo);
    }
    appendWriteKernelTimestamp(event, beforeWalker, false, false);
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline uint64_t CommandListCoreFamily<gfxCoreFamily>::getInputBufferSize(NEO::ImageType imageType,
                                                                         uint64_t bytesPerPixel,
                                                                         const ze_image_region_t *region) {

    switch (imageType) {
    default:
        UNRECOVERABLE_IF(true);
        break;
    case NEO::ImageType::Image1D:
    case NEO::ImageType::Image1DArray:
        return bytesPerPixel * region->width;
    case NEO::ImageType::Image2D:
    case NEO::ImageType::Image2DArray:
        return bytesPerPixel * region->width * region->height;
    case NEO::ImageType::Image3D:
        return bytesPerPixel * region->width * region->height * region->depth;
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline AlignedAllocationData CommandListCoreFamily<gfxCoreFamily>::getAlignedAllocation(Device *device,
                                                                                        const void *buffer,
                                                                                        uint64_t bufferSize,
                                                                                        bool hostCopyAllowed) {
    NEO::SvmAllocationData *allocData = nullptr;
    void *ptr = const_cast<void *>(buffer);
    bool srcAllocFound = device->getDriverHandle()->findAllocationDataForRange(ptr,
                                                                               bufferSize, &allocData);
    NEO::GraphicsAllocation *alloc = nullptr;

    uintptr_t sourcePtr = reinterpret_cast<uintptr_t>(ptr);
    size_t offset = 0;
    NEO::EncodeSurfaceState<GfxFamily>::getSshAlignedPointer(sourcePtr, offset);
    uintptr_t alignedPtr = 0u;
    bool hostPointerNeedsFlush = false;

    if (srcAllocFound == false) {
        alloc = device->getDriverHandle()->findHostPointerAllocation(ptr, static_cast<size_t>(bufferSize), device->getRootDeviceIndex());
        if (alloc != nullptr) {
            alignedPtr = static_cast<size_t>(alignDown(alloc->getGpuAddress(), NEO::EncodeSurfaceState<GfxFamily>::getSurfaceBaseAddressAlignment()));
            // get offset from GPUVA of allocation to align down GPU address
            offset = static_cast<size_t>(alloc->getGpuAddress()) - alignedPtr;
            // get offset from base of allocation to arg address
            offset += reinterpret_cast<size_t>(ptr) - reinterpret_cast<size_t>(alloc->getUnderlyingBuffer());
        } else {
            alloc = getHostPtrAlloc(buffer, bufferSize, hostCopyAllowed);
            alignedPtr = static_cast<uintptr_t>(alignDown(alloc->getGpuAddress(), NEO::EncodeSurfaceState<GfxFamily>::getSurfaceBaseAddressAlignment()));
            if (alloc->getAllocationType() == NEO::AllocationType::EXTERNAL_HOST_PTR) {
                auto hostAllocCpuPtr = reinterpret_cast<uintptr_t>(alloc->getUnderlyingBuffer());
                hostAllocCpuPtr = alignDown(hostAllocCpuPtr, NEO::EncodeSurfaceState<GfxFamily>::getSurfaceBaseAddressAlignment());
                auto allignedPtrOffset = sourcePtr - hostAllocCpuPtr;
                alignedPtr = ptrOffset(alignedPtr, allignedPtrOffset);
            }
        }

        hostPointerNeedsFlush = true;
    } else {
        alloc = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
        DeviceImp *deviceImp = static_cast<DeviceImp *>(device);
        DriverHandleImp *driverHandle = static_cast<DriverHandleImp *>(deviceImp->getDriverHandle());
        if (driverHandle->isRemoteResourceNeeded(const_cast<void *>(buffer), alloc, allocData, device)) {
            uint64_t pbase = allocData->gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress();
            uint64_t offset = sourcePtr - pbase;

            alloc = driverHandle->getPeerAllocation(device, allocData, reinterpret_cast<void *>(pbase), &alignedPtr);
            alignedPtr += offset;
        } else {
            alignedPtr = sourcePtr;
        }

        if (allocData->memoryType == InternalMemoryType::HOST_UNIFIED_MEMORY ||
            allocData->memoryType == InternalMemoryType::SHARED_UNIFIED_MEMORY) {
            hostPointerNeedsFlush = true;
        }
    }

    return {alignedPtr, offset, alloc, hostPointerNeedsFlush};
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline size_t CommandListCoreFamily<gfxCoreFamily>::getAllocationOffsetForAppendBlitFill(void *ptr, NEO::GraphicsAllocation &gpuAllocation) {
    uint64_t offset;
    if (gpuAllocation.getAllocationType() == NEO::AllocationType::EXTERNAL_HOST_PTR) {
        offset = castToUint64(ptr) - castToUint64(gpuAllocation.getUnderlyingBuffer()) + gpuAllocation.getAllocationOffset();
    } else {
        offset = castToUint64(ptr) - gpuAllocation.getGpuAddress();
    }
    return offset;
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline ze_result_t CommandListCoreFamily<gfxCoreFamily>::addEventsToCmdList(uint32_t numWaitEvents,
                                                                            ze_event_handle_t *phWaitEvents) {

    if (numWaitEvents > 0) {
        if (phWaitEvents) {
            CommandListCoreFamily<gfxCoreFamily>::appendWaitOnEvents(numWaitEvents, phWaitEvents);
        } else {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendSignalEvent(ze_event_handle_t hEvent) {
    auto event = Event::fromHandle(hEvent);

    commandContainer.addToResidencyContainer(&event->getAllocation(this->device));
    uint64_t baseAddr = event->getGpuAddress(this->device);
    NEO::Device *neoDevice = device->getNEODevice();
    uint32_t callId = 0;
    if (NEO::DebugManager.flags.EnableSWTags.get()) {
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameBeginTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            "zeCommandListAppendSignalEvent",
            ++neoDevice->getRootDeviceEnvironment().tagsManager->currentCallCount);
        callId = neoDevice->getRootDeviceEnvironment().tagsManager->currentCallCount;
    }
    size_t eventSignalOffset = 0;

    if (event->isUsingContextEndOffset()) {
        eventSignalOffset = event->getContextEndOffset();
    }

    const auto &hwInfo = this->device->getHwInfo();
    if (isCopyOnly()) {
        NEO::MiFlushArgs args;
        args.commandWithPostSync = true;
        NEO::EncodeMiFlushDW<GfxFamily>::programMiFlushDw(*commandContainer.getCommandStream(), ptrOffset(baseAddr, eventSignalOffset),
                                                          Event::STATE_SIGNALED, args, hwInfo);
    } else {
        NEO::PipeControlArgs args;
        bool applyScope = !!event->signalScope;
        args.dcFlushEnable = getDcFlushRequired(applyScope);
        if (this->partitionCount > 1) {
            event->setPacketsInUse(this->partitionCount);
            args.workloadPartitionOffset = true;
        }
        if (applyScope || event->isEventTimestampFlagSet()) {
            NEO::MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
                *commandContainer.getCommandStream(),
                NEO::PostSyncMode::ImmediateData,
                ptrOffset(baseAddr, eventSignalOffset),
                Event::STATE_SIGNALED,
                hwInfo,
                args);
        } else {
            NEO::EncodeStoreMemory<GfxFamily>::programStoreDataImm(
                *commandContainer.getCommandStream(),
                ptrOffset(baseAddr, eventSignalOffset),
                Event::STATE_SIGNALED,
                0u,
                false,
                args.workloadPartitionOffset);
        }
    }

    if (NEO::DebugManager.flags.EnableSWTags.get()) {
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameEndTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            "zeCommandListAppendSignalEvent",
            callId);
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendWaitOnEvents(uint32_t numEvents,
                                                                     ze_event_handle_t *phEvent) {

    using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    NEO::Device *neoDevice = device->getNEODevice();
    uint32_t callId = 0;
    if (NEO::DebugManager.flags.EnableSWTags.get()) {
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameBeginTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            "zeCommandListAppendWaitOnEvents",
            ++neoDevice->getRootDeviceEnvironment().tagsManager->currentCallCount);
        callId = neoDevice->getRootDeviceEnvironment().tagsManager->currentCallCount;
    }

    uint64_t gpuAddr = 0;
    constexpr uint32_t eventStateClear = Event::State::STATE_CLEARED;
    bool dcFlushRequired = false;

    const auto &hwInfo = this->device->getHwInfo();

    if (this->dcFlushSupport) {
        for (uint32_t i = 0; i < numEvents; i++) {
            auto event = Event::fromHandle(phEvent[i]);
            dcFlushRequired |= !!event->waitScope;
        }
    }
    if (dcFlushRequired) {
        if (isCopyOnly()) {
            NEO::MiFlushArgs args;
            NEO::EncodeMiFlushDW<GfxFamily>::programMiFlushDw(*commandContainer.getCommandStream(), 0, 0, args, hwInfo);
        } else {
            NEO::PipeControlArgs args;
            args.dcFlushEnable = true;
            NEO::MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(*commandContainer.getCommandStream(), args);
        }
    }

    for (uint32_t i = 0; i < numEvents; i++) {
        auto event = Event::fromHandle(phEvent[i]);
        commandContainer.addToResidencyContainer(&event->getAllocation(this->device));
        gpuAddr = event->getGpuAddress(this->device);
        uint32_t packetsToWait = event->getPacketsInUse();

        if (event->isUsingContextEndOffset()) {
            gpuAddr += event->getContextEndOffset();
        }
        for (uint32_t i = 0u; i < packetsToWait; i++) {
            NEO::EncodeSempahore<GfxFamily>::addMiSemaphoreWaitCommand(*commandContainer.getCommandStream(),
                                                                       gpuAddr,
                                                                       eventStateClear,
                                                                       COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);

            gpuAddr += event->getSinglePacketSize();
        }
    }

    if (NEO::DebugManager.flags.EnableSWTags.get()) {
        neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::CallNameEndTag>(
            *commandContainer.getCommandStream(),
            *neoDevice,
            "zeCommandListAppendWaitOnEvents",
            callId);
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::programSyncBuffer(Kernel &kernel, NEO::Device &device,
                                                                    const ze_group_count_t *threadGroupDimensions) {
    uint32_t maximalNumberOfWorkgroupsAllowed;
    auto ret = kernel.suggestMaxCooperativeGroupCount(&maximalNumberOfWorkgroupsAllowed, this->engineGroupType,
                                                      device.isEngineInstanced());
    UNRECOVERABLE_IF(ret != ZE_RESULT_SUCCESS);
    size_t requestedNumberOfWorkgroups = (threadGroupDimensions->groupCountX * threadGroupDimensions->groupCountY *
                                          threadGroupDimensions->groupCountZ);
    if (requestedNumberOfWorkgroups > maximalNumberOfWorkgroupsAllowed) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    device.allocateSyncBufferHandler();
    device.syncBufferHandler->prepareForEnqueue(requestedNumberOfWorkgroups, kernel);

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendWriteKernelTimestamp(Event *event, bool beforeWalker, bool maskLsb, bool workloadPartition) {
    constexpr uint32_t mask = 0xfffffffe;

    auto baseAddr = event->getPacketAddress(this->device);
    auto contextOffset = beforeWalker ? event->getContextStartOffset() : event->getContextEndOffset();
    auto globalOffset = beforeWalker ? event->getGlobalStartOffset() : event->getGlobalEndOffset();

    uint64_t globalAddress = ptrOffset(baseAddr, globalOffset);
    uint64_t contextAddress = ptrOffset(baseAddr, contextOffset);

    if (maskLsb) {
        NEO::EncodeMathMMIO<GfxFamily>::encodeBitwiseAndVal(commandContainer, REG_GLOBAL_TIMESTAMP_LDW, mask, globalAddress, workloadPartition);
        NEO::EncodeMathMMIO<GfxFamily>::encodeBitwiseAndVal(commandContainer, GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW, mask, contextAddress, workloadPartition);
    } else {
        NEO::EncodeStoreMMIO<GfxFamily>::encode(*commandContainer.getCommandStream(), REG_GLOBAL_TIMESTAMP_LDW, globalAddress, workloadPartition);
        NEO::EncodeStoreMMIO<GfxFamily>::encode(*commandContainer.getCommandStream(), GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW, contextAddress, workloadPartition);
    }

    adjustWriteKernelTimestamp(globalAddress, contextAddress, maskLsb, mask, workloadPartition);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendEventForProfiling(Event *event, bool beforeWalker, bool workloadPartition) {
    if (!event) {
        return;
    }
    if (isCopyOnly()) {
        appendEventForProfilingCopyCommand(event, beforeWalker);
    } else {
        if (!event->isEventTimestampFlagSet()) {
            return;
        }

        commandContainer.addToResidencyContainer(&event->getAllocation(this->device));

        if (beforeWalker) {
            appendWriteKernelTimestamp(event, beforeWalker, true, workloadPartition);
        } else {
            const auto &hwInfo = this->device->getHwInfo();
            NEO::PipeControlArgs args;
            args.dcFlushEnable = getDcFlushRequired(!!event->signalScope);
            NEO::MemorySynchronizationCommands<GfxFamily>::setPostSyncExtraProperties(args,
                                                                                      hwInfo);

            NEO::MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(*commandContainer.getCommandStream(), args);

            uint64_t baseAddr = event->getGpuAddress(this->device);
            NEO::MemorySynchronizationCommands<GfxFamily>::addAdditionalSynchronization(*commandContainer.getCommandStream(), baseAddr, false, hwInfo);
            appendWriteKernelTimestamp(event, beforeWalker, true, workloadPartition);
        }
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendWriteGlobalTimestamp(
    uint64_t *dstptr, ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {

    if (numWaitEvents > 0) {
        if (phWaitEvents) {
            CommandListCoreFamily<gfxCoreFamily>::appendWaitOnEvents(numWaitEvents, phWaitEvents);
        } else {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
    }

    Event *signalEvent = nullptr;
    if (hSignalEvent) {
        signalEvent = Event::fromHandle(hSignalEvent);
    }

    bool workloadPartition = setupTimestampEventForMultiTile(signalEvent);
    appendEventForProfiling(signalEvent, true, workloadPartition);

    const auto &hwInfo = this->device->getHwInfo();
    if (isCopyOnly()) {
        NEO::MiFlushArgs args;
        args.timeStampOperation = true;
        args.commandWithPostSync = true;
        NEO::EncodeMiFlushDW<GfxFamily>::programMiFlushDw(*commandContainer.getCommandStream(),
                                                          reinterpret_cast<uint64_t>(dstptr),
                                                          0,
                                                          args,
                                                          hwInfo);
    } else {
        NEO::PipeControlArgs args;

        NEO::MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
            *commandContainer.getCommandStream(),
            NEO::PostSyncMode::Timestamp,
            reinterpret_cast<uint64_t>(dstptr),
            0,
            hwInfo,
            args);
    }

    appendSignalEventPostWalker(signalEvent, workloadPartition);

    auto allocationStruct = getAlignedAllocation(this->device, dstptr, sizeof(uint64_t), false);
    commandContainer.addToResidencyContainer(allocationStruct.alloc);

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyFromContext(
    void *dstptr, ze_context_handle_t hContextSrc, const void *srcptr,
    size_t size, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {

    return CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopy(dstptr, srcptr, size, hSignalEvent, numWaitEvents, phWaitEvents);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendQueryKernelTimestamps(
    uint32_t numEvents, ze_event_handle_t *phEvents, void *dstptr,
    const size_t *pOffsets, ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {

    auto dstPtrAllocationStruct = getAlignedAllocation(this->device, dstptr, sizeof(ze_kernel_timestamp_result_t) * numEvents, false);
    commandContainer.addToResidencyContainer(dstPtrAllocationStruct.alloc);

    std::unique_ptr<EventData[]> timestampsData = std::make_unique<EventData[]>(numEvents);

    for (uint32_t i = 0u; i < numEvents; ++i) {
        auto event = Event::fromHandle(phEvents[i]);
        commandContainer.addToResidencyContainer(&event->getAllocation(this->device));
        timestampsData[i].address = event->getGpuAddress(this->device);
        timestampsData[i].packetsInUse = event->getPacketsInUse();
        timestampsData[i].timestampSizeInDw = event->getTimestampSizeInDw();
    }

    size_t alignedSize = alignUp<size_t>(sizeof(EventData) * numEvents, MemoryConstants::pageSize64k);
    NEO::AllocationType allocationType = NEO::AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER;
    auto devices = device->getNEODevice()->getDeviceBitfield();
    NEO::AllocationProperties allocationProperties{device->getRootDeviceIndex(),
                                                   true,
                                                   alignedSize,
                                                   allocationType,
                                                   devices.count() > 1,
                                                   false,
                                                   devices};

    NEO::GraphicsAllocation *timestampsGPUData = device->getDriverHandle()->getMemoryManager()->allocateGraphicsMemoryWithProperties(allocationProperties);

    UNRECOVERABLE_IF(timestampsGPUData == nullptr);

    commandContainer.addToResidencyContainer(timestampsGPUData);
    commandContainer.getDeallocationContainer().push_back(timestampsGPUData);

    bool result = device->getDriverHandle()->getMemoryManager()->copyMemoryToAllocation(timestampsGPUData, 0, timestampsData.get(), sizeof(EventData) * numEvents);

    UNRECOVERABLE_IF(!result);

    Kernel *builtinKernel = nullptr;
    auto useOnlyGlobalTimestamps = NEO::HwHelper::get(device->getHwInfo().platform.eRenderCoreFamily).useOnlyGlobalTimestamps() ? 1u : 0u;
    auto lock = device->getBuiltinFunctionsLib()->obtainUniqueOwnership();

    if (pOffsets == nullptr) {
        builtinKernel = device->getBuiltinFunctionsLib()->getFunction(Builtin::QueryKernelTimestamps);
        builtinKernel->setArgumentValue(2u, sizeof(uint32_t), &useOnlyGlobalTimestamps);
    } else {
        auto pOffsetAllocationStruct = getAlignedAllocation(this->device, pOffsets, sizeof(size_t) * numEvents, false);
        auto offsetValPtr = static_cast<uintptr_t>(pOffsetAllocationStruct.alloc->getGpuAddress());
        commandContainer.addToResidencyContainer(pOffsetAllocationStruct.alloc);
        builtinKernel = device->getBuiltinFunctionsLib()->getFunction(Builtin::QueryKernelTimestampsWithOffsets);
        builtinKernel->setArgBufferWithAlloc(2, offsetValPtr, pOffsetAllocationStruct.alloc);
        builtinKernel->setArgumentValue(3u, sizeof(uint32_t), &useOnlyGlobalTimestamps);
        offsetValPtr += sizeof(size_t);
    }

    uint32_t groupSizeX = 1u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;

    if (builtinKernel->suggestGroupSize(numEvents, 1u, 1u,
                                        &groupSizeX, &groupSizeY, &groupSizeZ) != ZE_RESULT_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (builtinKernel->setGroupSize(groupSizeX, groupSizeY, groupSizeZ) != ZE_RESULT_SUCCESS) {
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ze_group_count_t dispatchKernelArgs{numEvents / groupSizeX, 1u, 1u};

    auto dstValPtr = static_cast<uintptr_t>(dstPtrAllocationStruct.alloc->getGpuAddress());

    builtinKernel->setArgBufferWithAlloc(0u, static_cast<uintptr_t>(timestampsGPUData->getGpuAddress()), timestampsGPUData);
    builtinKernel->setArgBufferWithAlloc(1, dstValPtr, dstPtrAllocationStruct.alloc);

    auto dstAllocationType = dstPtrAllocationStruct.alloc->getAllocationType();
    CmdListKernelLaunchParams launchParams = {};
    launchParams.isBuiltInKernel = true;
    launchParams.isDestinationAllocationInSystemMemory =
        (dstAllocationType == NEO::AllocationType::BUFFER_HOST_MEMORY) ||
        (dstAllocationType == NEO::AllocationType::EXTERNAL_HOST_PTR);
    auto appendResult = appendLaunchKernel(builtinKernel->toHandle(), &dispatchKernelArgs, hSignalEvent, numWaitEvents,
                                           phWaitEvents, launchParams);
    if (appendResult != ZE_RESULT_SUCCESS) {
        return appendResult;
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::reserveSpace(size_t size, void **ptr) {
    auto availableSpace = commandContainer.getCommandStream()->getAvailableSpace();
    if (availableSpace < size) {
        *ptr = nullptr;
    } else {
        *ptr = commandContainer.getCommandStream()->getSpace(size);
    }
    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::prepareIndirectParams(const ze_group_count_t *threadGroupDimensions) {
    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(threadGroupDimensions);
    if (allocData) {
        auto alloc = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
        commandContainer.addToResidencyContainer(alloc);

        size_t groupCountOffset = 0;
        if (allocData->cpuAllocation != nullptr) {
            commandContainer.addToResidencyContainer(allocData->cpuAllocation);
            groupCountOffset = ptrDiff(threadGroupDimensions, allocData->cpuAllocation->getUnderlyingBuffer());
        } else {
            groupCountOffset = ptrDiff(threadGroupDimensions, alloc->getGpuAddress());
        }

        auto groupCount = ptrOffset(alloc->getGpuAddress(), groupCountOffset);

        NEO::EncodeSetMMIO<GfxFamily>::encodeMEM(commandContainer, GPUGPU_DISPATCHDIMX,
                                                 ptrOffset(groupCount, offsetof(ze_group_count_t, groupCountX)));
        NEO::EncodeSetMMIO<GfxFamily>::encodeMEM(commandContainer, GPUGPU_DISPATCHDIMY,
                                                 ptrOffset(groupCount, offsetof(ze_group_count_t, groupCountY)));
        NEO::EncodeSetMMIO<GfxFamily>::encodeMEM(commandContainer, GPUGPU_DISPATCHDIMZ,
                                                 ptrOffset(groupCount, offsetof(ze_group_count_t, groupCountZ)));
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::updateStreamProperties(Kernel &kernel, bool isCooperative) {
    using VFE_STATE_TYPE = typename GfxFamily::VFE_STATE_TYPE;

    auto &hwInfo = device->getHwInfo();

    auto &kernelAttributes = kernel.getKernelDescriptor().kernelAttributes;
    if (!containsAnyKernel) {
        requiredStreamState.frontEndState.setProperties(isCooperative, kernelAttributes.flags.requiresDisabledEUFusion, true, -1, hwInfo);
        requiredStreamState.pipelineSelect.setProperties(true, false, kernelAttributes.flags.usesSystolicPipelineSelectMode, hwInfo);
        if (this->stateComputeModeTracking) {
            requiredStreamState.stateComputeMode.setProperties(false, kernelAttributes.numGrfRequired, kernelAttributes.threadArbitrationPolicy, device->getDevicePreemptionMode(), hwInfo);
            finalStreamState = requiredStreamState;
        } else {
            finalStreamState = requiredStreamState;
            requiredStreamState.stateComputeMode.setProperties(false, kernelAttributes.numGrfRequired, kernelAttributes.threadArbitrationPolicy, device->getDevicePreemptionMode(), hwInfo);
        }
        containsAnyKernel = true;
    }

    auto logicalStateHelperBlock = !getLogicalStateHelper();

    finalStreamState.pipelineSelect.setProperties(true, false, kernelAttributes.flags.usesSystolicPipelineSelectMode, hwInfo);
    if (this->pipelineSelectStateTracking && finalStreamState.pipelineSelect.isDirty() && logicalStateHelperBlock) {
        NEO::PipelineSelectArgs pipelineSelectArgs;
        pipelineSelectArgs.systolicPipelineSelectMode = kernelAttributes.flags.usesSystolicPipelineSelectMode;
        pipelineSelectArgs.systolicPipelineSelectSupport = this->systolicModeSupport;

        NEO::PreambleHelper<GfxFamily>::programPipelineSelect(commandContainer.getCommandStream(),
                                                              pipelineSelectArgs,
                                                              hwInfo);
    }

    finalStreamState.frontEndState.setProperties(isCooperative, kernelAttributes.flags.requiresDisabledEUFusion, true, -1, hwInfo);
    bool isPatchingVfeStateAllowed = NEO::DebugManager.flags.AllowPatchingVfeStateInCommandLists.get();
    if (finalStreamState.frontEndState.isDirty() && logicalStateHelperBlock) {
        if (isPatchingVfeStateAllowed) {
            auto pVfeStateAddress = NEO::PreambleHelper<GfxFamily>::getSpaceForVfeState(commandContainer.getCommandStream(), hwInfo, engineGroupType);
            auto pVfeState = new VFE_STATE_TYPE;
            NEO::PreambleHelper<GfxFamily>::programVfeState(pVfeState, hwInfo, 0, 0, device->getMaxNumHwThreads(), finalStreamState, nullptr);
            commandsToPatch.push_back({pVfeStateAddress, pVfeState, CommandToPatch::FrontEndState});
        }
        if (this->frontEndStateTracking) {
            auto &stream = *commandContainer.getCommandStream();
            NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferEnd(stream);

            CmdListReturnPoint returnPoint = {
                {},
                stream.getGpuBase() + stream.getUsed(),
                stream.getGraphicsAllocation()};
            returnPoint.configSnapshot.frontEndState.setProperties(finalStreamState.frontEndState);
            returnPoints.push_back(returnPoint);
        }
    }

    finalStreamState.stateComputeMode.setProperties(false, kernelAttributes.numGrfRequired, kernelAttributes.threadArbitrationPolicy, device->getDevicePreemptionMode(), hwInfo);
    if (finalStreamState.stateComputeMode.isDirty() && logicalStateHelperBlock) {
        bool isRcs = (this->engineGroupType == NEO::EngineGroupType::RenderCompute);
        NEO::PipelineSelectArgs pipelineSelectArgs;
        pipelineSelectArgs.systolicPipelineSelectMode = kernelAttributes.flags.usesSystolicPipelineSelectMode;
        pipelineSelectArgs.systolicPipelineSelectSupport = this->systolicModeSupport;

        NEO::EncodeComputeMode<GfxFamily>::programComputeModeCommandWithSynchronization(
            *commandContainer.getCommandStream(), finalStreamState.stateComputeMode, pipelineSelectArgs, false, hwInfo, isRcs, this->dcFlushSupport, nullptr);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::clearCommandsToPatch() {
    using VFE_STATE_TYPE = typename GfxFamily::VFE_STATE_TYPE;

    for (auto &commandToPatch : commandsToPatch) {
        switch (commandToPatch.type) {
        case CommandList::CommandToPatch::FrontEndState:
            UNRECOVERABLE_IF(commandToPatch.pCommand == nullptr);
            delete reinterpret_cast<VFE_STATE_TYPE *>(commandToPatch.pCommand);
            break;
        case CommandList::CommandToPatch::PauseOnEnqueueSemaphoreStart:
        case CommandList::CommandToPatch::PauseOnEnqueueSemaphoreEnd:
        case CommandList::CommandToPatch::PauseOnEnqueuePipeControlStart:
        case CommandList::CommandToPatch::PauseOnEnqueuePipeControlEnd:
            UNRECOVERABLE_IF(commandToPatch.pCommand == nullptr);
            break;
        default:
            UNRECOVERABLE_IF(true);
        }
    }
    commandsToPatch.clear();
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline size_t CommandListCoreFamily<gfxCoreFamily>::getTotalSizeForCopyRegion(const ze_copy_region_t *region, uint32_t pitch, uint32_t slicePitch) {
    if (region->depth > 1) {
        uint32_t offset = region->originX + ((region->originY) * pitch) + ((region->originZ) * slicePitch);
        return (region->width * region->height * region->depth) + offset;
    } else {
        uint32_t offset = region->originX + ((region->originY) * pitch);
        return (region->width * region->height) + offset;
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamily<gfxCoreFamily>::isAppendSplitNeeded(void *dstPtr, const void *srcPtr, size_t size) {
    NEO::SvmAllocationData *srcAllocData = nullptr;
    NEO::SvmAllocationData *dstAllocData = nullptr;
    bool srcAllocFound = this->device->getDriverHandle()->findAllocationDataForRange(const_cast<void *>(srcPtr), size, &srcAllocData);
    bool dstAllocFound = this->device->getDriverHandle()->findAllocationDataForRange(dstPtr, size, &dstAllocData);

    if (srcAllocFound && dstAllocFound) {
        return this->isAppendSplitNeeded(dstAllocData->gpuAllocations.getDefaultGraphicsAllocation()->getMemoryPool(), srcAllocData->gpuAllocations.getDefaultGraphicsAllocation()->getMemoryPool(), size);
    }

    return false;
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline bool CommandListCoreFamily<gfxCoreFamily>::isAppendSplitNeeded(NEO::MemoryPool dstPool, NEO::MemoryPool srcPool, size_t size) {
    constexpr size_t minimalSizeForBcsSplit = 4 * MemoryConstants::megaByte;

    return this->isBcsSplitNeeded &&
           size >= minimalSizeForBcsSplit &&
           ((!NEO::MemoryPoolHelper::isSystemMemoryPool(dstPool) && NEO::MemoryPoolHelper::isSystemMemoryPool(srcPool)) ||
            (!NEO::MemoryPoolHelper::isSystemMemoryPool(srcPool) && NEO::MemoryPoolHelper::isSystemMemoryPool(dstPool)));
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::setGlobalWorkSizeIndirect(NEO::CrossThreadDataOffset offsets[3], uint64_t crossThreadAddress, uint32_t lws[3]) {
    NEO::EncodeIndirectParams<GfxFamily>::setGlobalWorkSizeIndirect(commandContainer, offsets, crossThreadAddress, lws);

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::programStateBaseAddress(NEO::CommandContainer &container, bool genericMediaStateClearRequired) {
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;

    const auto &hwInfo = this->device->getHwInfo();
    bool isRcs = (this->engineGroupType == NEO::EngineGroupType::RenderCompute);

    NEO::EncodeWA<GfxFamily>::addPipeControlBeforeStateBaseAddress(*commandContainer.getCommandStream(), hwInfo, isRcs, this->dcFlushSupport);

    auto gmmHelper = container.getDevice()->getRootDeviceEnvironment().getGmmHelper();
    uint32_t statelessMocsIndex = (gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER) >> 1);

    STATE_BASE_ADDRESS sba;

    NEO::EncodeStateBaseAddressArgs<GfxFamily> encodeStateBaseAddressArgs = {
        &commandContainer,
        sba,
        statelessMocsIndex,
        false,
        this->partitionCount > 1,
        isRcs};
    NEO::EncodeStateBaseAddress<GfxFamily>::encode(encodeStateBaseAddressArgs);

    bool sbaTrackingEnabled = NEO::Debugger::isDebugEnabled(this->internalUsage) && device->getL0Debugger();
    NEO::EncodeStateBaseAddress<GfxFamily>::setSbaTrackingForL0DebuggerIfEnabled(sbaTrackingEnabled,
                                                                                 *this->device->getNEODevice(),
                                                                                 *container.getCommandStream(),
                                                                                 sba);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::adjustWriteKernelTimestamp(uint64_t globalAddress, uint64_t contextAddress, bool maskLsb, uint32_t mask, bool workloadPartition) {}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendBarrier(ze_event_handle_t hSignalEvent,
                                                                uint32_t numWaitEvents,
                                                                ze_event_handle_t *phWaitEvents) {

    ze_result_t ret = addEventsToCmdList(numWaitEvents, phWaitEvents);
    if (ret) {
        return ret;
    }

    Event *signalEvent = nullptr;
    if (hSignalEvent) {
        signalEvent = Event::fromHandle(hSignalEvent);
    }

    bool workloadPartition = setupTimestampEventForMultiTile(signalEvent);
    appendEventForProfiling(signalEvent, true, workloadPartition);

    if (isCopyOnly()) {
        NEO::MiFlushArgs args;
        NEO::EncodeMiFlushDW<GfxFamily>::programMiFlushDw(*commandContainer.getCommandStream(), 0, 0, args, this->device->getHwInfo());
    } else {
        appendComputeBarrierCommand();
    }

    appendSignalEventPostWalker(signalEvent, workloadPartition);
    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::addFlushRequiredCommand(bool flushOperationRequired, Event *signalEvent) {
    if (isCopyOnly()) {
        return;
    }
    if (signalEvent) {
        flushOperationRequired &= !signalEvent->signalScope;
    }

    if (getDcFlushRequired(flushOperationRequired)) {
        NEO::PipeControlArgs args;
        args.dcFlushEnable = true;
        NEO::MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(*commandContainer.getCommandStream(), args);
    }
}

} // namespace L0

/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/heap_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/graphics_allocation.h"

#include "opencl/source/helpers/hardware_commands_helper.h"

#include "level_zero/core/source/cmdlist_hw.h"
#include "level_zero/core/source/device_imp.h"
#include "level_zero/core/source/event.h"
#include "level_zero/core/source/image.h"
#include "level_zero/core/source/module.h"

#include <algorithm>

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
struct EncodeStateBaseAddress;

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamily<gfxCoreFamily>::initialize(Device *device) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    if (!commandContainer.initialize(static_cast<DeviceImp *>(device)->neoDevice)) {
        return false;
    }
    NEO::EncodeStateBaseAddress<GfxFamily>::encode(commandContainer);
    commandContainer.setDirtyStateForAllHeaps(false);

    this->device = device;
    this->commandListPreemptionMode = device->getDevicePreemptionMode();

    return true;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::executeCommandListImmediate(bool performMigration) {
    this->close();
    ze_command_list_handle_t immediateHandle = this->toHandle();
    this->cmdQImmediate->executeCommandLists(1, &immediateHandle, nullptr, performMigration);
    this->cmdQImmediate->synchronize(std::numeric_limits<uint32_t>::max());
    this->reset();

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::close() {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferEnd(commandContainer);

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::programL3(bool isSLMused) {}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendLaunchFunction(
    ze_kernel_handle_t hFunction, const ze_group_count_t *pThreadGroupDimensions,
    ze_event_handle_t hEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {
    if (addEventsToCmdList(hEvent, numWaitEvents, phWaitEvents) == ZE_RESULT_ERROR_INVALID_ARGUMENT) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    ze_result_t ret = appendLaunchFunctionWithParams(hFunction, pThreadGroupDimensions, hEvent, numWaitEvents, phWaitEvents, false, false);
    if (ret != ZE_RESULT_SUCCESS) {
        return ret;
    }

    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendLaunchCooperativeKernel(ze_kernel_handle_t hKernel,
                                                                                const ze_group_count_t *pLaunchFuncArgs,
                                                                                ze_event_handle_t hSignalEvent,
                                                                                uint32_t numWaitEvents,
                                                                                ze_event_handle_t *phWaitEvents) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendLaunchFunctionIndirect(
    ze_kernel_handle_t hFunction, const ze_group_count_t *pDispatchArgumentsBuffer,
    ze_event_handle_t hEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {
    if (addEventsToCmdList(hEvent, numWaitEvents, phWaitEvents) == ZE_RESULT_ERROR_INVALID_ARGUMENT) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    ze_result_t ret = appendLaunchFunctionWithParams(hFunction, pDispatchArgumentsBuffer, nullptr, 0, nullptr, true, false);

    if (hEvent) {
        appendSignalEventPostWalker(hEvent);
    }

    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendLaunchMultipleFunctionsIndirect(uint32_t numFunctions,
                                                                                        const ze_kernel_handle_t *phFunctions,
                                                                                        const uint32_t *pNumLaunchArguments,
                                                                                        const ze_group_count_t *pLaunchArgumentsBuffer,
                                                                                        ze_event_handle_t hEvent,
                                                                                        uint32_t numWaitEvents,
                                                                                        ze_event_handle_t *phWaitEvents) {
    if (addEventsToCmdList(hEvent, numWaitEvents, phWaitEvents) == ZE_RESULT_ERROR_INVALID_ARGUMENT) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    for (uint32_t i = 0; i < numFunctions; i++) {
        NEO::EncodeMathMMIO<GfxFamily>::encodeGreaterThanPredicate(commandContainer,
                                                                   reinterpret_cast<uint64_t>(pNumLaunchArguments), i);

        auto ret = appendLaunchFunctionWithParams(phFunctions[i],
                                                  &pLaunchArgumentsBuffer[i],
                                                  nullptr, 0, nullptr, true, true);
        if (ret != ZE_RESULT_SUCCESS) {
            return ret;
        }
    }

    if (hEvent) {
        appendSignalEventPostWalker(hEvent);
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendEventReset(ze_event_handle_t hEvent) {
    using POST_SYNC_OPERATION = typename GfxFamily::PIPE_CONTROL::POST_SYNC_OPERATION;
    auto event = Event::fromHandle(hEvent);
    commandContainer.addToResidencyContainer(&event->getAllocation());

    NEO::MemorySynchronizationCommands<GfxFamily>::obtainPipeControlAndProgramPostSyncOperation(
        *commandContainer.getCommandStream(), POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA,
        event->getGpuAddress(), Event::STATE_CLEARED, true, commandContainer.getDevice()->getHardwareInfo());

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendBarrier(ze_event_handle_t hSignalEvent,
                                                                uint32_t numWaitEvents,
                                                                ze_event_handle_t *phWaitEvents) {
    if (addEventsToCmdList(hSignalEvent, numWaitEvents, phWaitEvents) == ZE_RESULT_ERROR_INVALID_ARGUMENT) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    NEO::MemorySynchronizationCommands<GfxFamily>::addPipeControl(*commandContainer.getCommandStream(), false);

    if (hSignalEvent) {
        this->appendSignalEventPostWalker(hSignalEvent);
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemoryRangesBarrier(
    uint32_t numRanges, const size_t *pRangeSizes, const void **pRanges,
    ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {
    if (addEventsToCmdList(hSignalEvent, numWaitEvents, phWaitEvents) == ZE_RESULT_ERROR_INVALID_ARGUMENT) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    applyMemoryRangesBarrier(numRanges, pRangeSizes, pRanges);

    if (hSignalEvent) {
        this->appendSignalEventPostWalker(hSignalEvent);
    }

    if (this->cmdListType == CommandListType::TYPE_IMMEDIATE) {
        executeCommandListImmediate(true);
    }

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
    auto bytesPerPixel = image->getImageInfo().surfaceFormat->NumChannels * image->getImageInfo().surfaceFormat->PerChannelSizeInBytes;

    ze_image_region_t tmpRegion;

    if (pDstRegion == nullptr) {
        tmpRegion = {0,
                     0,
                     0,
                     static_cast<uint32_t>(image->getImageInfo().imgDesc.imageWidth),
                     static_cast<uint32_t>(image->getImageInfo().imgDesc.imageHeight),
                     static_cast<uint32_t>(image->getImageInfo().imgDesc.imageDepth)};
        pDstRegion = &tmpRegion;
    }

    uint64_t bufferSize = getInputBufferSize(image->getImageInfo().imgDesc.imageType, bytesPerPixel, pDstRegion);

    auto allocationStruct = getAlignedAllocation(this->device, srcPtr, bufferSize);

    Kernel *builtinKernel = nullptr;

    switch (bytesPerPixel) {
    default:
        UNRECOVERABLE_IF(true);
    case 1u:
        builtinKernel = device->getBuiltinFunctionsLib()->getFunction(Builtin::CopyBufferToImage3dBytes);
        break;
    case 2u:
        builtinKernel = device->getBuiltinFunctionsLib()->getFunction(Builtin::CopyBufferToImage3d2Bytes);
        break;
    case 4u:
        builtinKernel = device->getBuiltinFunctionsLib()->getFunction(Builtin::CopyBufferToImage3d4Bytes);
        break;
    case 8u:
        builtinKernel = device->getBuiltinFunctionsLib()->getFunction(Builtin::CopyBufferToImage3d8Bytes);
        break;
    case 16u:
        builtinKernel = device->getBuiltinFunctionsLib()->getFunction(Builtin::CopyBufferToImage3d16Bytes);
        break;
    }

    builtinKernel->setArgBufferWithAlloc(0u, reinterpret_cast<void *>(&allocationStruct.alignedAllocationPtr), allocationStruct.alloc);
    builtinKernel->setArgRedescribedImage(1u, hDstImage);
    builtinKernel->setArgumentValue(2u, sizeof(size_t), &allocationStruct.offset);

    uint32_t origin[] = {
        static_cast<uint32_t>(pDstRegion->originX),
        static_cast<uint32_t>(pDstRegion->originY),
        static_cast<uint32_t>(pDstRegion->originZ),
        0};
    builtinKernel->setArgumentValue(3u, sizeof(origin), &origin);

    auto srcRowPitch = pDstRegion->width * bytesPerPixel;
    auto srcSlicePitch = (image->getImageInfo().imgDesc.imageType == NEO::ImageType::Image1DArray ? 1 : pDstRegion->height) * srcRowPitch;

    uint32_t pitch[] = {
        srcRowPitch,
        srcSlicePitch};
    builtinKernel->setArgumentValue(4u, sizeof(pitch), &pitch);

    uint32_t groupSizeX = pDstRegion->width;
    uint32_t groupSizeY = pDstRegion->height;
    uint32_t groupSizeZ = pDstRegion->depth;

    if (builtinKernel->suggestGroupSize(groupSizeX, groupSizeY, groupSizeZ, &groupSizeX, &groupSizeY, &groupSizeZ) != ZE_RESULT_SUCCESS) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (builtinKernel->setGroupSize(groupSizeX, groupSizeY, groupSizeZ) != ZE_RESULT_SUCCESS) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (pDstRegion->width % groupSizeX || pDstRegion->height % groupSizeY || pDstRegion->depth % groupSizeZ) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ze_group_count_t functionArgs{pDstRegion->width / groupSizeX, pDstRegion->height / groupSizeY, pDstRegion->depth / groupSizeZ};

    return this->appendLaunchFunction(builtinKernel->toHandle(), &functionArgs,
                                      hEvent, numWaitEvents, phWaitEvents);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendImageCopyToMemory(void *dstPtr,
                                                                          ze_image_handle_t hSrcImage,
                                                                          const ze_image_region_t *pSrcRegion,
                                                                          ze_event_handle_t hEvent,
                                                                          uint32_t numWaitEvents,
                                                                          ze_event_handle_t *phWaitEvents) {

    auto image = Image::fromHandle(hSrcImage);
    auto bytesPerPixel = image->getImageInfo().surfaceFormat->NumChannels * image->getImageInfo().surfaceFormat->PerChannelSizeInBytes;
    ze_image_region_t tmpRegion;

    if (pSrcRegion == nullptr) {
        tmpRegion = {0,
                     0,
                     0,
                     static_cast<uint32_t>(image->getImageInfo().imgDesc.imageWidth),
                     static_cast<uint32_t>(image->getImageInfo().imgDesc.imageHeight),
                     static_cast<uint32_t>(image->getImageInfo().imgDesc.imageDepth)};
        pSrcRegion = &tmpRegion;
    }

    uint64_t bufferSize = getInputBufferSize(image->getImageInfo().imgDesc.imageType, bytesPerPixel, pSrcRegion);

    auto allocationStruct = getAlignedAllocation(this->device, dstPtr, bufferSize);

    Kernel *builtinKernel = nullptr;

    switch (bytesPerPixel) {
    default:
        UNRECOVERABLE_IF(true);
    case 1u:
        builtinKernel = device->getBuiltinFunctionsLib()->getFunction(Builtin::CopyImage3dToBufferBytes);
        break;
    case 2u:
        builtinKernel = device->getBuiltinFunctionsLib()->getFunction(Builtin::CopyImage3dToBuffer2Bytes);
        break;
    case 4u:
        builtinKernel = device->getBuiltinFunctionsLib()->getFunction(Builtin::CopyImage3dToBuffer4Bytes);
        break;
    case 8u:
        builtinKernel = device->getBuiltinFunctionsLib()->getFunction(Builtin::CopyImage3dToBuffer8Bytes);
        break;
    case 16u:
        builtinKernel = device->getBuiltinFunctionsLib()->getFunction(Builtin::CopyImage3dToBuffer16Bytes);
        break;
    }

    builtinKernel->setArgRedescribedImage(0u, hSrcImage);
    builtinKernel->setArgBufferWithAlloc(1u, reinterpret_cast<void *>(&allocationStruct.alignedAllocationPtr), allocationStruct.alloc);

    uint32_t origin[] = {
        static_cast<uint32_t>(pSrcRegion->originX),
        static_cast<uint32_t>(pSrcRegion->originY),
        static_cast<uint32_t>(pSrcRegion->originZ),
        0};
    builtinKernel->setArgumentValue(2u, sizeof(origin), &origin);

    builtinKernel->setArgumentValue(3u, sizeof(size_t), &allocationStruct.offset);

    auto srcRowPitch = pSrcRegion->width * bytesPerPixel;
    auto srcSlicePitch = (image->getImageInfo().imgDesc.imageType == NEO::ImageType::Image1DArray ? 1 : pSrcRegion->height) * srcRowPitch;

    uint32_t pitch[] = {
        srcRowPitch,
        srcSlicePitch};
    builtinKernel->setArgumentValue(4u, sizeof(pitch), &pitch);

    uint32_t groupSizeX = pSrcRegion->width;
    uint32_t groupSizeY = pSrcRegion->height;
    uint32_t groupSizeZ = pSrcRegion->depth;

    if (builtinKernel->suggestGroupSize(groupSizeX, groupSizeY, groupSizeZ, &groupSizeX, &groupSizeY, &groupSizeZ) != ZE_RESULT_SUCCESS) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (builtinKernel->setGroupSize(groupSizeX, groupSizeY, groupSizeZ) != ZE_RESULT_SUCCESS) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (pSrcRegion->width % groupSizeX || pSrcRegion->height % groupSizeY || pSrcRegion->depth % groupSizeZ) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ze_group_count_t functionArgs{pSrcRegion->width / groupSizeX, pSrcRegion->height / groupSizeY, pSrcRegion->depth / groupSizeZ};

    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendLaunchFunction(builtinKernel->toHandle(), &functionArgs,
                                                                          hEvent, numWaitEvents, phWaitEvents);

    if (allocationStruct.needsFlush) {
        NEO::MemorySynchronizationCommands<GfxFamily>::addPipeControl(*commandContainer.getCommandStream(), true);
    }

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

    auto function = device->getBuiltinFunctionsLib()->getFunction(Builtin::CopyImageRegion);
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

    if (function->suggestGroupSize(groupSizeX, groupSizeY, groupSizeZ, &groupSizeX, &groupSizeY, &groupSizeZ) != ZE_RESULT_SUCCESS) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (function->setGroupSize(groupSizeX, groupSizeY, groupSizeZ) != ZE_RESULT_SUCCESS) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (srcRegion.width % groupSizeX || srcRegion.height % groupSizeY || srcRegion.depth % groupSizeZ) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ze_group_count_t functionArgs{srcRegion.width / groupSizeX, srcRegion.height / groupSizeY, srcRegion.depth / groupSizeZ};

    function->setArgRedescribedImage(0, hSrcImage);
    function->setArgRedescribedImage(1, hDstImage);
    function->setArgumentValue(2, sizeof(srcOffset), &srcOffset);
    function->setArgumentValue(3, sizeof(dstOffset), &dstOffset);

    appendEventForProfiling(hEvent, true);

    return this->CommandListCoreFamily<gfxCoreFamily>::appendLaunchFunction(function->toHandle(), &functionArgs,
                                                                            hEvent, numWaitEvents, phWaitEvents);
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
    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(ptr);
    if (allocData) {
        if (allocData->memoryType == InternalMemoryType::SHARED_UNIFIED_MEMORY) {
            return ZE_RESULT_SUCCESS;
        } else {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
    }
    return ZE_RESULT_ERROR_UNKNOWN;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyKernelWithGA(
    void *dstPtr, NEO::GraphicsAllocation *dstPtrAlloc, uint64_t dstOffset, void *srcPtr, NEO::GraphicsAllocation *srcPtrAlloc, uint64_t srcOffset, uint32_t size, uint32_t elementSize, Builtin builtin) {
    auto builtinFunction = device->getBuiltinFunctionsLib()->getFunction(builtin);

    uint32_t groupSizeX = builtinFunction->getImmutableData()
                              ->getDescriptor()
                              .kernelAttributes.simdSize;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;

    if (builtinFunction->setGroupSize(groupSizeX, groupSizeY, groupSizeZ)) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    builtinFunction->setArgBufferWithAlloc(0u, dstPtr, dstPtrAlloc);

    builtinFunction->setArgBufferWithAlloc(1u, srcPtr, srcPtrAlloc);

    uint32_t elems = size / elementSize;
    builtinFunction->setArgumentValue(2, sizeof(elems), &elems);
    builtinFunction->setArgumentValue(3, sizeof(dstOffset), &dstOffset);
    builtinFunction->setArgumentValue(4, sizeof(srcOffset), &srcOffset);

    uint32_t groups = (size + ((groupSizeX * elementSize) - 1)) / (groupSizeX * elementSize);
    ze_group_count_t dispatchFuncArgs{groups, 1u, 1u};

    return CommandListCoreFamily<gfxCoreFamily>::appendLaunchFunction(builtinFunction->toHandle(), &dispatchFuncArgs, nullptr, 0,
                                                                      nullptr);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendPageFaultCopy(
    NEO::GraphicsAllocation *dstptr, NEO::GraphicsAllocation *srcptr, size_t size, bool flushHost) {

    auto builtinFunction = device->getBuiltinFunctionsLib()->getPageFaultFunction();

    uint32_t groupSizeX = builtinFunction->getImmutableData()
                              ->getDescriptor()
                              .kernelAttributes.simdSize;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;

    if (builtinFunction->setGroupSize(groupSizeX, groupSizeY, groupSizeZ)) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    auto dstValPtr = static_cast<uintptr_t>(dstptr->getGpuAddress());
    auto srcValPtr = static_cast<uintptr_t>(srcptr->getGpuAddress());

    builtinFunction->setArgBufferWithAlloc(0, reinterpret_cast<void *>(&dstValPtr), dstptr);
    builtinFunction->setArgBufferWithAlloc(1, reinterpret_cast<void *>(&srcValPtr), srcptr);
    builtinFunction->setArgumentValue(2, sizeof(size), &size);

    uint32_t groups = (static_cast<uint32_t>(size) + ((groupSizeX)-1)) / (groupSizeX);
    ze_group_count_t dispatchFuncArgs{groups, 1u, 1u};

    ze_result_t ret = appendLaunchFunctionWithParams(builtinFunction->toHandle(), &dispatchFuncArgs,
                                                     nullptr, 0, nullptr, false, false);
    if (ret != ZE_RESULT_SUCCESS) {
        return ret;
    }

    if (flushHost) {
        NEO::MemorySynchronizationCommands<GfxFamily>::addPipeControl(*commandContainer.getCommandStream(), true);
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

    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    uintptr_t start = reinterpret_cast<uintptr_t>(dstptr);

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

    auto dstAllocationStruct = getAlignedAllocation(this->device, dstptr, size);
    auto srcAllocationStruct = getAlignedAllocation(this->device, srcptr, size);

    ze_result_t ret = ZE_RESULT_SUCCESS;

    appendEventForProfiling(hSignalEvent, true);

    if (ret == ZE_RESULT_SUCCESS && leftSize) {
        ret = appendMemoryCopyKernelWithGA(reinterpret_cast<void *>(&dstAllocationStruct.alignedAllocationPtr),
                                           dstAllocationStruct.alloc, dstAllocationStruct.offset, reinterpret_cast<void *>(&srcAllocationStruct.alignedAllocationPtr),
                                           srcAllocationStruct.alloc, srcAllocationStruct.offset, static_cast<uint32_t>(leftSize), 1,
                                           Builtin::CopyBufferToBufferSide);
    }

    if (ret == ZE_RESULT_SUCCESS && middleSizeBytes) {
        ret = appendMemoryCopyKernelWithGA(reinterpret_cast<void *>(&dstAllocationStruct.alignedAllocationPtr),
                                           dstAllocationStruct.alloc, leftSize + dstAllocationStruct.offset,
                                           reinterpret_cast<void *>(&srcAllocationStruct.alignedAllocationPtr),
                                           srcAllocationStruct.alloc, leftSize + srcAllocationStruct.offset,
                                           static_cast<uint32_t>(middleSizeBytes),
                                           static_cast<uint32_t>(middleElSize),
                                           Builtin::CopyBufferToBufferMiddle);
    }

    if (ret == ZE_RESULT_SUCCESS && rightSize) {
        ret = appendMemoryCopyKernelWithGA(reinterpret_cast<void *>(&dstAllocationStruct.alignedAllocationPtr),
                                           dstAllocationStruct.alloc, leftSize + middleSizeBytes + dstAllocationStruct.offset,
                                           reinterpret_cast<void *>(&srcAllocationStruct.alignedAllocationPtr),
                                           srcAllocationStruct.alloc, leftSize + middleSizeBytes + srcAllocationStruct.offset,
                                           static_cast<uint32_t>(rightSize), 1u,
                                           Builtin::CopyBufferToBufferSide);
    }

    if (hSignalEvent) {
        this->appendSignalEventPostWalker(hSignalEvent);
    }

    if (dstAllocationStruct.needsFlush) {
        NEO::MemorySynchronizationCommands<GfxFamily>::addPipeControl(*commandContainer.getCommandStream(), true);
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
                                                                         ze_event_handle_t hSignalEvent) {
    uintptr_t destinationPtr = reinterpret_cast<uintptr_t>(dstPtr);
    size_t dstOffset = 0;
    NEO::EncodeSurfaceState<GfxFamily>::getSshAlignedPointer(destinationPtr, dstOffset);
    void *alignedDstPtr = reinterpret_cast<void *>(destinationPtr);

    uintptr_t sourcePtr = reinterpret_cast<uintptr_t>(const_cast<void *>(srcPtr));
    size_t srcOffset = 0;
    NEO::EncodeSurfaceState<GfxFamily>::getSshAlignedPointer(sourcePtr, srcOffset);
    void *alignedSrcPtr = reinterpret_cast<void *>(sourcePtr);

    size_t dstSize = 0;
    size_t srcSize = 0;

    if (srcRegion->depth > 1) {
        uint hostPtrDstOffset = dstRegion->originX + ((dstRegion->originY) * dstPitch) + ((dstRegion->originZ) * dstSlicePitch);
        uint hostPtrSrcOffset = srcRegion->originX + ((srcRegion->originY) * srcPitch) + ((srcRegion->originZ) * srcSlicePitch);
        dstSize = (dstRegion->width * dstRegion->height * dstRegion->depth) + dstOffset + hostPtrDstOffset;
        srcSize = (srcRegion->width * srcRegion->height * srcRegion->depth) + srcOffset + hostPtrSrcOffset;
    } else {
        uint hostPtrDstOffset = dstRegion->originX + ((dstRegion->originY) * dstPitch);
        uint hostPtrSrcOffset = srcRegion->originX + ((srcRegion->originY) * srcPitch);
        dstSize = (dstRegion->width * dstRegion->height) + dstOffset + hostPtrDstOffset;
        srcSize = (srcRegion->width * srcRegion->height) + srcOffset + hostPtrSrcOffset;
    }

    NEO::SvmAllocationData *allocData = nullptr;
    bool hostPointerNeedsFlush = false;
    bool dstAllocFound = device->getDriverHandle()->findAllocationDataForRange(alignedDstPtr, dstSize, &allocData);
    if (dstAllocFound == false) {
        auto dstAlloc = device->getDriverHandle()->allocateManagedMemoryFromHostPtr(device, alignedDstPtr, dstSize, this);
        commandContainer.getDeallocationContainer().push_back(dstAlloc);
        hostPointerNeedsFlush = true;
    } else {
        if (allocData->memoryType == InternalMemoryType::HOST_UNIFIED_MEMORY ||
            allocData->memoryType == InternalMemoryType::SHARED_UNIFIED_MEMORY) {
            hostPointerNeedsFlush = true;
        }
    }

    bool srcAllocFound = device->getDriverHandle()->findAllocationDataForRange(alignedSrcPtr,
                                                                               srcSize, nullptr);
    if (srcAllocFound == false) {
        auto srcAlloc = device->getDriverHandle()->allocateManagedMemoryFromHostPtr(device, alignedSrcPtr, dstSize, this);
        commandContainer.getDeallocationContainer().push_back(srcAlloc);
    }

    appendEventForProfiling(hSignalEvent, true);

    ze_result_t result = ZE_RESULT_SUCCESS;
    if (srcRegion->depth > 1) {
        result = this->appendMemoryCopyKernel3d(alignedDstPtr, alignedSrcPtr,
                                                Builtin::CopyBufferRectBytes3d, dstRegion, dstPitch, dstSlicePitch, dstOffset,
                                                srcRegion, srcPitch, srcSlicePitch, srcOffset, hSignalEvent, 0, nullptr);
    } else {
        result = this->appendMemoryCopyKernel2d(alignedDstPtr, alignedSrcPtr,
                                                Builtin::CopyBufferRectBytes2d, dstRegion, dstPitch, dstOffset,
                                                srcRegion, srcPitch, srcOffset, hSignalEvent, 0, nullptr);
    }

    if (result) {
        return result;
    }

    if (hostPointerNeedsFlush) {
        NEO::MemorySynchronizationCommands<GfxFamily>::addPipeControl(*commandContainer.getCommandStream(), true);
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyKernel3d(const void *dstptr,
                                                                           const void *srcptr,
                                                                           Builtin builtin,
                                                                           const ze_copy_region_t *dstRegion,
                                                                           uint32_t dstPitch,
                                                                           uint32_t dstSlicePitch,
                                                                           size_t dstOffset,
                                                                           const ze_copy_region_t *srcRegion,
                                                                           uint32_t srcPitch,
                                                                           uint32_t srcSlicePitch,
                                                                           size_t srcOffset,
                                                                           ze_event_handle_t hSignalEvent,
                                                                           uint32_t numWaitEvents,
                                                                           ze_event_handle_t *phWaitEvents) {

    auto builtinFunction = device->getBuiltinFunctionsLib()->getFunction(builtin);

    uint32_t groupSizeX = srcRegion->width;
    uint32_t groupSizeY = srcRegion->height;
    uint32_t groupSizeZ = srcRegion->depth;

    if (builtinFunction->suggestGroupSize(groupSizeX, groupSizeY, groupSizeZ, &groupSizeX, &groupSizeY, &groupSizeZ) != ZE_RESULT_SUCCESS) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (builtinFunction->setGroupSize(groupSizeX, groupSizeY, groupSizeZ) != ZE_RESULT_SUCCESS) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (srcRegion->width % groupSizeX || srcRegion->height % groupSizeY || srcRegion->depth % groupSizeZ) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ze_group_count_t dispatchFuncArgs{srcRegion->width / groupSizeX, srcRegion->height / groupSizeY, srcRegion->depth / groupSizeZ};

    uint srcOrigin[3] = {(srcRegion->originX + static_cast<uint32_t>(srcOffset)), (srcRegion->originY), (srcRegion->originZ)};
    uint dstOrigin[3] = {(dstRegion->originX + static_cast<uint32_t>(dstOffset)), (dstRegion->originY), (srcRegion->originZ)};
    uint srcPitches[2] = {(srcPitch), (srcSlicePitch)};
    uint dstPitches[2] = {(dstPitch), (dstSlicePitch)};

    builtinFunction->setArgumentValue(0, sizeof(dstptr), &srcptr);
    builtinFunction->setArgumentValue(1, sizeof(srcptr), &dstptr);
    builtinFunction->setArgumentValue(2, sizeof(srcOrigin), &srcOrigin);
    builtinFunction->setArgumentValue(3, sizeof(dstOrigin), &dstOrigin);
    builtinFunction->setArgumentValue(4, sizeof(srcPitches), &srcPitches);
    builtinFunction->setArgumentValue(5, sizeof(dstPitches), &dstPitches);

    return CommandListCoreFamily<gfxCoreFamily>::appendLaunchFunction(builtinFunction->toHandle(), &dispatchFuncArgs, hSignalEvent, numWaitEvents,
                                                                      phWaitEvents);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyKernel2d(const void *dstptr,
                                                                           const void *srcptr,
                                                                           Builtin builtin,
                                                                           const ze_copy_region_t *dstRegion,
                                                                           uint32_t dstPitch,
                                                                           size_t dstOffset,
                                                                           const ze_copy_region_t *srcRegion,
                                                                           uint32_t srcPitch,
                                                                           size_t srcOffset,
                                                                           ze_event_handle_t hSignalEvent,
                                                                           uint32_t numWaitEvents,
                                                                           ze_event_handle_t *phWaitEvents) {

    auto builtinFunction = device->getBuiltinFunctionsLib()->getFunction(builtin);

    uint32_t groupSizeX = srcRegion->width;
    uint32_t groupSizeY = srcRegion->height;
    uint32_t groupSizeZ = 1u;

    if (builtinFunction->suggestGroupSize(groupSizeX, groupSizeY, groupSizeZ, &groupSizeX, &groupSizeY, &groupSizeZ) != ZE_RESULT_SUCCESS) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (builtinFunction->setGroupSize(groupSizeX, groupSizeY, groupSizeZ) != ZE_RESULT_SUCCESS) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (srcRegion->width % groupSizeX || srcRegion->height % groupSizeY) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ze_group_count_t dispatchFuncArgs{srcRegion->width / groupSizeX, srcRegion->height / groupSizeY, 1u};

    uint srcOrigin[2] = {(srcRegion->originX + static_cast<uint32_t>(srcOffset)), (srcRegion->originY)};
    uint dstOrigin[2] = {(dstRegion->originX + static_cast<uint32_t>(dstOffset)), (dstRegion->originY)};

    builtinFunction->setArgumentValue(0, sizeof(dstptr), &srcptr);
    builtinFunction->setArgumentValue(1, sizeof(srcptr), &dstptr);
    builtinFunction->setArgumentValue(2, sizeof(srcOrigin), &srcOrigin);
    builtinFunction->setArgumentValue(3, sizeof(dstOrigin), &dstOrigin);
    builtinFunction->setArgumentValue(4, sizeof(srcPitch), &srcPitch);
    builtinFunction->setArgumentValue(5, sizeof(dstPitch), &dstPitch);

    return CommandListCoreFamily<gfxCoreFamily>::appendLaunchFunction(builtinFunction->toHandle(), &dispatchFuncArgs, hSignalEvent, numWaitEvents,
                                                                      phWaitEvents);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemoryPrefetch(const void *ptr,
                                                                       size_t count) {
    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(ptr);
    if (allocData) {
        if (allocData->memoryType == InternalMemoryType::SHARED_UNIFIED_MEMORY) {
            return ZE_RESULT_SUCCESS;
        } else {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
    }
    return ZE_RESULT_ERROR_UNKNOWN;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMemoryFill(void *ptr, const void *pattern,
                                                                   size_t patternSize, size_t size,
                                                                   ze_event_handle_t hEvent) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    bool hostPointerNeedsFlush = false;

    NEO::SvmAllocationData *allocData = nullptr;
    bool dstAllocFound = device->getDriverHandle()->findAllocationDataForRange(ptr, size, &allocData);
    if (dstAllocFound == false) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    } else {
        if (allocData->memoryType == InternalMemoryType::HOST_UNIFIED_MEMORY ||
            allocData->memoryType == InternalMemoryType::SHARED_UNIFIED_MEMORY) {
            hostPointerNeedsFlush = true;
        }
    }

    uintptr_t dstPtr = reinterpret_cast<uintptr_t>(ptr);
    size_t dstOffset = 0;
    NEO::EncodeSurfaceState<GfxFamily>::getSshAlignedPointer(dstPtr, dstOffset);

    uintptr_t srcPtr = reinterpret_cast<uintptr_t>(const_cast<void *>(pattern));
    size_t srcOffset = 0;
    NEO::EncodeSurfaceState<GfxFamily>::getSshAlignedPointer(srcPtr, srcOffset);

    Kernel *builtinFunction = nullptr;
    uint32_t groupSizeX = 1u;

    if (patternSize == 1) {
        builtinFunction = device->getBuiltinFunctionsLib()->getFunction(Builtin::FillBufferImmediate);

        groupSizeX = builtinFunction->getImmutableData()->getDescriptor().kernelAttributes.simdSize;
        if (builtinFunction->setGroupSize(groupSizeX, 1u, 1u)) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }

        uint32_t value = *(reinterpret_cast<uint32_t *>(const_cast<void *>(pattern)));
        builtinFunction->setArgumentValue(0, sizeof(dstPtr), &dstPtr);
        builtinFunction->setArgumentValue(1, sizeof(dstOffset), &dstOffset);
        builtinFunction->setArgumentValue(2, sizeof(value), &value);

    } else {
        builtinFunction = device->getBuiltinFunctionsLib()->getFunction(Builtin::FillBufferSSHOffset);

        auto patternAlloc = device->getDriverHandle()->allocateManagedMemoryFromHostPtr(device,
                                                                                        reinterpret_cast<void *>(srcPtr),
                                                                                        srcOffset + patternSize, this);
        if (patternAlloc == nullptr) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }

        commandContainer.getDeallocationContainer().push_back(patternAlloc);

        groupSizeX = static_cast<uint32_t>(patternSize);
        if (builtinFunction->setGroupSize(groupSizeX, 1u, 1u)) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }

        builtinFunction->setArgumentValue(0, sizeof(dstPtr), &dstPtr);
        builtinFunction->setArgumentValue(1, sizeof(dstOffset), &dstOffset);
        builtinFunction->setArgumentValue(2, sizeof(srcPtr), &srcPtr);
        builtinFunction->setArgumentValue(3, sizeof(srcOffset), &srcOffset);
    }

    appendEventForProfiling(hEvent, true);

    uint32_t groups = static_cast<uint32_t>(size) / groupSizeX;
    ze_group_count_t dispatchFuncArgs{groups, 1u, 1u};
    ze_result_t res = CommandListCoreFamily<gfxCoreFamily>::appendLaunchFunction(builtinFunction->toHandle(),
                                                                                 &dispatchFuncArgs, nullptr,
                                                                                 0, nullptr);
    if (res) {
        return res;
    }

    uint32_t groupRemainderSizeX = static_cast<uint32_t>(size) % groupSizeX;
    if (groupRemainderSizeX) {
        if (builtinFunction->setGroupSize(groupRemainderSizeX, 1u, 1u)) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        ze_group_count_t dispatchFuncArgs{1u, 1u, 1u};

        dstPtr = dstPtr + (size - groupRemainderSizeX);
        dstOffset = 0;
        NEO::EncodeSurfaceState<GfxFamily>::getSshAlignedPointer(dstPtr, dstOffset);

        builtinFunction->setArgumentValue(0, sizeof(dstPtr), &dstPtr);
        builtinFunction->setArgumentValue(1, sizeof(dstOffset), &dstOffset);

        res = CommandListCoreFamily<gfxCoreFamily>::appendLaunchFunction(builtinFunction->toHandle(),
                                                                         &dispatchFuncArgs, nullptr,
                                                                         0, nullptr);
    }

    if (hEvent) {
        this->appendSignalEventPostWalker(hEvent);
    }

    if (hostPointerNeedsFlush) {
        NEO::MemorySynchronizationCommands<GfxFamily>::addPipeControl(*commandContainer.getCommandStream(), true);
    }

    return res;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::appendSignalEventPostWalker(ze_event_handle_t hEvent) {
    auto event = Event::fromHandle(hEvent);
    if (event->isTimestampEvent) {
        appendEventForProfiling(hEvent, false);
    } else {
        CommandListCoreFamily<gfxCoreFamily>::appendSignalEvent(hEvent);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline uint64_t CommandListCoreFamily<gfxCoreFamily>::getInputBufferSize(NEO::ImageType imageType, uint64_t bytesPerPixel, const ze_image_region_t *region) {
    switch (imageType) {
    default:
        UNRECOVERABLE_IF(true);
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
inline AlignedAllocationData CommandListCoreFamily<gfxCoreFamily>::getAlignedAllocation(Device *device, const void *buffer, uint64_t bufferSize) {
    NEO::SvmAllocationData *allocData = nullptr;
    bool srcAllocFound = device->getDriverHandle()->findAllocationDataForRange(const_cast<void *>(buffer), bufferSize, &allocData);
    NEO::GraphicsAllocation *alloc = nullptr;

    uintptr_t sourcePtr = reinterpret_cast<uintptr_t>(const_cast<void *>(buffer));
    size_t offset = 0;
    NEO::EncodeSurfaceState<GfxFamily>::getSshAlignedPointer(sourcePtr, offset);
    uintptr_t alignedPtr = 0u;
    bool hostPointerNeedsFlush = false;

    if (srcAllocFound == false) {
        alloc = device->getDriverHandle()->allocateMemoryFromHostPtr(device, buffer, bufferSize);
        hostPtrMap.insert(std::make_pair(buffer, alloc));

        alignedPtr = static_cast<uintptr_t>(alloc->getGpuAddress() - offset);
    } else {
        alloc = allocData->gpuAllocation;

        alignedPtr = reinterpret_cast<uintptr_t>(buffer) - offset;

        if (allocData->memoryType == InternalMemoryType::HOST_UNIFIED_MEMORY ||
            allocData->memoryType == InternalMemoryType::SHARED_UNIFIED_MEMORY) {
            hostPointerNeedsFlush = true;
        }
    }

    return {alignedPtr, offset, alloc, hostPointerNeedsFlush};
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline ze_result_t CommandListCoreFamily<gfxCoreFamily>::addEventsToCmdList(ze_event_handle_t hEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {
    if (numWaitEvents > 0) {
        if (phWaitEvents) {
            CommandListCoreFamily<gfxCoreFamily>::appendWaitOnEvents(numWaitEvents, phWaitEvents);
        } else {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
    }

    appendEventForProfiling(hEvent, true);

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendSignalEvent(ze_event_handle_t hEvent) {
    using POST_SYNC_OPERATION = typename GfxFamily::PIPE_CONTROL::POST_SYNC_OPERATION;
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    auto event = Event::fromHandle(hEvent);

    commandContainer.addToResidencyContainer(&event->getAllocation());

    bool dcFlushEnable = (event->signalScope == ZE_EVENT_SCOPE_FLAG_NONE) ? false : true;
    NEO::MemorySynchronizationCommands<GfxFamily>::obtainPipeControlAndProgramPostSyncOperation(
        *commandContainer.getCommandStream(), POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA,
        event->getGpuAddress(), Event::STATE_SIGNALED, dcFlushEnable, commandContainer.getDevice()->getHardwareInfo());

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendWaitOnEvents(uint32_t numEvents,
                                                                     ze_event_handle_t *phEvent) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    uint64_t gpuAddr = 0;

    for (uint32_t i = 0; i < numEvents; i++) {
        auto event = Event::fromHandle(phEvent[i]);
        commandContainer.addToResidencyContainer(&event->getAllocation());

        gpuAddr = event->getGpuAddress();
        if (event->isTimestampEvent) {
            gpuAddr += event->getOffsetOfProfilingEvent(ZE_EVENT_TIMESTAMP_CONTEXT_END);
        }

        NEO::HardwareCommandsHelper<GfxFamily>::programMiSemaphoreWait(*(commandContainer.getCommandStream()),
                                                                       gpuAddr,
                                                                       Event::STATE_CLEARED,
                                                                       COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);

        bool dcFlushEnable = (event->waitScope == ZE_EVENT_SCOPE_FLAG_NONE) ? false : true;
        if (dcFlushEnable) {
            NEO::MemorySynchronizationCommands<GfxFamily>::addPipeControl(*commandContainer.getCommandStream(), true);
        }
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
ze_result_t CommandListCoreFamily<gfxCoreFamily>::reset() {
    printfFunctionContainer.clear();
    removeDeallocationContainerData();
    removeHostPtrAllocations();
    commandContainer.reset();

    NEO::EncodeStateBaseAddress<GfxFamily>::encode(commandContainer);
    commandContainer.setDirtyStateForAllHeaps(false);

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::prepareIndirectParams(const ze_group_count_t *pThreadGroupDimensions) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(pThreadGroupDimensions);
    if (allocData) {
        auto alloc = allocData->gpuAllocation;
        commandContainer.addToResidencyContainer(alloc);

        NEO::EncodeSetMMIO<GfxFamily>::encodeMEM(commandContainer, GPUGPU_DISPATCHDIMX, ptrOffset(alloc->getGpuAddress(), offsetof(ze_group_count_t, groupCountX)));
        NEO::EncodeSetMMIO<GfxFamily>::encodeMEM(commandContainer, GPUGPU_DISPATCHDIMY, ptrOffset(alloc->getGpuAddress(), offsetof(ze_group_count_t, groupCountY)));
        NEO::EncodeSetMMIO<GfxFamily>::encodeMEM(commandContainer, GPUGPU_DISPATCHDIMZ, ptrOffset(alloc->getGpuAddress(), offsetof(ze_group_count_t, groupCountZ)));
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::setGroupSizeIndirect(uint32_t offsets[3], void *crossThreadAddress, uint32_t lws[3]) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;

    NEO::EncodeIndirectParams<GfxFamily>::setGroupSizeIndirect(commandContainer, offsets, crossThreadAddress, lws);

    return ZE_RESULT_SUCCESS;
}
} // namespace L0

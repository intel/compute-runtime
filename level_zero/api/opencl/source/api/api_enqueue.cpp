/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/ptr_math.h"

#include "level_zero/api/internal/l0_cmdlist.h"
#include "level_zero/api/opencl/source/api/api.h"
#include "level_zero/api/opencl/source/command_queue/command_queue.h"
#include "level_zero/api/opencl/source/context/context.h"
#include "level_zero/api/opencl/source/event/event.h"
#include "level_zero/api/opencl/source/helpers/base_object.h"
#include "level_zero/api/opencl/source/helpers/cl_validators.h"
#include "level_zero/api/opencl/source/helpers/convert_color.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/api/opencl/source/kernel/kernel.h"
#include "level_zero/api/opencl/source/mem_obj/buffer.h"
#include "level_zero/api/opencl/source/mem_obj/image.h"
#include "level_zero/core/source/builtin/builtin_functions_lib.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/image/internal_core_image_ext.h"
#include "level_zero/core/source/kernel/kernel.h"
#include <level_zero/ze_api.h>

#include "CL/cl.h"

#include <cstring>

inline uint32_t getOclMipLevel(NEO::LEO::Image *pImage, const size_t *origin) {
    if (pImage == nullptr || origin == nullptr) {
        return 0u;
    }
    auto numMipLevels = pImage->getL0Object()->getImageInfo().imgDesc.numMipLevels;
    if (numMipLevels <= 1u) {
        return 0u;
    }
    switch (pImage->getClObjectType()) {
    case CL_MEM_OBJECT_IMAGE1D:
    case CL_MEM_OBJECT_IMAGE1D_BUFFER:
        return static_cast<uint32_t>(origin[1]);
    case CL_MEM_OBJECT_IMAGE2D:
    case CL_MEM_OBJECT_IMAGE1D_ARRAY:
        return static_cast<uint32_t>(origin[2]);
    case CL_MEM_OBJECT_IMAGE3D:
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        return static_cast<uint32_t>(origin[3]);
    default:
        return 0u;
    }
}

inline L0::ze_image_region_mip_level_exp_desc_t createZeImageRegionWithMipLevel(
    NEO::LEO::Image *pImage,
    const size_t *origin,
    const size_t *region) {

    L0::ze_image_region_mip_level_exp_desc_t desc{};
    uint32_t mipLevel = getOclMipLevel(pImage, origin);

    switch (pImage->getClObjectType()) {
    case CL_MEM_OBJECT_IMAGE1D:
    case CL_MEM_OBJECT_IMAGE1D_BUFFER:
        desc.originX = static_cast<uint32_t>(origin[0]);
        desc.originY = 0u;
        desc.originZ = 0u;
        desc.width = static_cast<uint32_t>(region[0]);
        desc.height = 1u;
        desc.depth = 1u;
        break;
    case CL_MEM_OBJECT_IMAGE2D:
        desc.originX = static_cast<uint32_t>(origin[0]);
        desc.originY = static_cast<uint32_t>(origin[1]);
        desc.originZ = 0u;
        desc.width = static_cast<uint32_t>(region[0]);
        desc.height = static_cast<uint32_t>(region[1]);
        desc.depth = 1u;
        break;
    case CL_MEM_OBJECT_IMAGE1D_ARRAY:
        desc.originX = static_cast<uint32_t>(origin[0]);
        desc.originY = static_cast<uint32_t>(origin[1]);
        desc.originZ = 0u;
        desc.width = static_cast<uint32_t>(region[0]);
        desc.height = static_cast<uint32_t>(region[1]);
        desc.depth = 1u;
        break;
    case CL_MEM_OBJECT_IMAGE3D:
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        desc.originX = static_cast<uint32_t>(origin[0]);
        desc.originY = static_cast<uint32_t>(origin[1]);
        desc.originZ = static_cast<uint32_t>(origin[2]);
        desc.width = static_cast<uint32_t>(region[0]);
        desc.height = static_cast<uint32_t>(region[1]);
        desc.depth = static_cast<uint32_t>(region[2]);
        break;
    default:
        desc.originX = static_cast<uint32_t>(origin[0]);
        desc.originY = static_cast<uint32_t>(origin[1]);
        desc.originZ = static_cast<uint32_t>(origin[2]);
        desc.width = static_cast<uint32_t>(region[0]);
        desc.height = static_cast<uint32_t>(region[1]);
        desc.depth = static_cast<uint32_t>(region[2]);
        break;
    }
    desc.mipLevel = mipLevel;
    return desc;
}

cl_int CL_API_CALL clEnqueueReadBuffer(cl_command_queue commandQueue,
                                       cl_mem buffer,
                                       cl_bool blockingRead,
                                       size_t offset,
                                       size_t cb,
                                       void *ptr,
                                       cl_uint numEventsInWaitList,
                                       const cl_event *eventWaitList,
                                       cl_event *event) {
    auto [retVal, pCommandQueue, pBuffer] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::BufferObj{buffer}),
        std::make_tuple(ptr, NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    if (pBuffer->readMemObjFlagsInvalid()) [[unlikely]] {
        return CL_INVALID_OPERATION;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_READ_BUFFER, pCommandQueue);

    auto lock = pCommandQueue->takeOwnership();
    ze_result_t ret = zeCommandListAppendMemoryCopy(pCommandQueue->getL0Handle(),
                                                    ptr,
                                                    ptrOffset(pBuffer->getUsmPtr(), offset),
                                                    cb,
                                                    hSignalEvent,
                                                    waitEvents.size(),
                                                    waitEvents.data());
    if (blockingRead) {
        lock.unlock();
        ret = zeCommandListHostSynchronize(pCommandQueue->getL0Handle(), std::numeric_limits<uint64_t>::max());
    }

    return L0ToClResultMapper(ret);
}

cl_int CL_API_CALL clEnqueueReadBufferRect(cl_command_queue commandQueue,
                                           cl_mem buffer,
                                           cl_bool blockingRead,
                                           const size_t *bufferOrigin,
                                           const size_t *hostOrigin,
                                           const size_t *region,
                                           size_t bufferRowPitch,
                                           size_t bufferSlicePitch,
                                           size_t hostRowPitch,
                                           size_t hostSlicePitch,
                                           void *ptr,
                                           cl_uint numEventsInWaitList,
                                           const cl_event *eventWaitList,
                                           cl_event *event) {
    auto [retVal, pCommandQueue, pBuffer] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::BufferObj{buffer}),
        std::make_tuple(ptr, NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    if (pBuffer->readMemObjFlagsInvalid()) [[unlikely]] {
        return CL_INVALID_OPERATION;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_READ_BUFFER_RECT, pCommandQueue);

    ze_copy_region_t l0SrcRegion{static_cast<uint32_t>(bufferOrigin[0]), static_cast<uint32_t>(bufferOrigin[1]), static_cast<uint32_t>(bufferOrigin[2]), static_cast<uint32_t>(region[0]), static_cast<uint32_t>(region[1]), static_cast<uint32_t>(region[2])};
    ze_copy_region_t l0DstRegion{static_cast<uint32_t>(hostOrigin[0]), static_cast<uint32_t>(hostOrigin[1]), static_cast<uint32_t>(hostOrigin[2]), static_cast<uint32_t>(region[0]), static_cast<uint32_t>(region[1]), static_cast<uint32_t>(region[2])};

    auto lock = pCommandQueue->takeOwnership();
    ze_result_t ret = zeCommandListAppendMemoryCopyRegion(pCommandQueue->getL0Handle(),
                                                          ptr,
                                                          &l0DstRegion,
                                                          static_cast<uint32_t>(hostRowPitch),
                                                          static_cast<uint32_t>(hostSlicePitch),
                                                          pBuffer->getUsmPtr(),
                                                          &l0SrcRegion,
                                                          static_cast<uint32_t>(bufferRowPitch),
                                                          static_cast<uint32_t>(bufferSlicePitch),
                                                          hSignalEvent,
                                                          waitEvents.size(),
                                                          waitEvents.data());
    if (blockingRead) {
        lock.unlock();
        ret = zeCommandListHostSynchronize(pCommandQueue->getL0Handle(), std::numeric_limits<uint64_t>::max());
    }

    return L0ToClResultMapper(ret);
}

cl_int CL_API_CALL clEnqueueWriteBuffer(cl_command_queue commandQueue,
                                        cl_mem buffer,
                                        cl_bool blockingWrite,
                                        size_t offset,
                                        size_t cb,
                                        const void *ptr,
                                        cl_uint numEventsInWaitList,
                                        const cl_event *eventWaitList,
                                        cl_event *event) {
    auto [retVal, pCommandQueue, pBuffer] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::BufferObj{buffer}),
        std::make_tuple(const_cast<void *>(ptr), NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    if (pBuffer->writeMemObjFlagsInvalid()) [[unlikely]] {
        return CL_INVALID_OPERATION;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_WRITE_BUFFER, pCommandQueue);

    auto lock = pCommandQueue->takeOwnership();
    ze_result_t ret = zeCommandListAppendMemoryCopy(pCommandQueue->getL0Handle(),
                                                    ptrOffset(pBuffer->getUsmPtr(), offset),
                                                    ptr,
                                                    cb,
                                                    hSignalEvent,
                                                    waitEvents.size(),
                                                    waitEvents.data());
    if (blockingWrite) {
        lock.unlock();
        ret = zeCommandListHostSynchronize(pCommandQueue->getL0Handle(), std::numeric_limits<uint64_t>::max());
    }

    return L0ToClResultMapper(ret);
}

cl_int CL_API_CALL clEnqueueWriteBufferRect(cl_command_queue commandQueue,
                                            cl_mem buffer,
                                            cl_bool blockingWrite,
                                            const size_t *bufferOrigin,
                                            const size_t *hostOrigin,
                                            const size_t *region,
                                            size_t bufferRowPitch,
                                            size_t bufferSlicePitch,
                                            size_t hostRowPitch,
                                            size_t hostSlicePitch,
                                            const void *ptr,
                                            cl_uint numEventsInWaitList,
                                            const cl_event *eventWaitList,
                                            cl_event *event) {
    auto [retVal, pCommandQueue, pBuffer] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::BufferObj{buffer}),
        std::make_tuple(const_cast<void *>(ptr), NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    if (pBuffer->writeMemObjFlagsInvalid()) [[unlikely]] {
        return CL_INVALID_OPERATION;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_WRITE_BUFFER_RECT, pCommandQueue);

    ze_copy_region_t l0DstRegion{static_cast<uint32_t>(bufferOrigin[0]), static_cast<uint32_t>(bufferOrigin[1]), static_cast<uint32_t>(bufferOrigin[2]), static_cast<uint32_t>(region[0]), static_cast<uint32_t>(region[1]), static_cast<uint32_t>(region[2])};
    ze_copy_region_t l0SrcRegion{static_cast<uint32_t>(hostOrigin[0]), static_cast<uint32_t>(hostOrigin[1]), static_cast<uint32_t>(hostOrigin[2]), static_cast<uint32_t>(region[0]), static_cast<uint32_t>(region[1]), static_cast<uint32_t>(region[2])};

    auto lock = pCommandQueue->takeOwnership();
    ze_result_t ret = zeCommandListAppendMemoryCopyRegion(pCommandQueue->getL0Handle(),
                                                          pBuffer->getUsmPtr(),
                                                          &l0DstRegion,
                                                          static_cast<uint32_t>(bufferRowPitch),
                                                          static_cast<uint32_t>(bufferSlicePitch),
                                                          ptr,
                                                          &l0SrcRegion,
                                                          static_cast<uint32_t>(hostRowPitch),
                                                          static_cast<uint32_t>(hostSlicePitch),
                                                          hSignalEvent,
                                                          waitEvents.size(),
                                                          waitEvents.data());
    if (blockingWrite) {
        lock.unlock();
        ret = zeCommandListHostSynchronize(pCommandQueue->getL0Handle(), std::numeric_limits<uint64_t>::max());
    }

    return L0ToClResultMapper(ret);
}

cl_int CL_API_CALL clEnqueueFillBuffer(cl_command_queue commandQueue,
                                       cl_mem buffer,
                                       const void *pattern,
                                       size_t patternSize,
                                       size_t offset,
                                       size_t size,
                                       cl_uint numEventsInWaitList,
                                       const cl_event *eventWaitList,
                                       cl_event *event) {
    auto [retVal, pCommandQueue, pBuffer] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::BufferObj{buffer}),
        std::make_tuple(const_cast<void *>(pattern), NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_FILL_BUFFER, pCommandQueue);

    auto lock = pCommandQueue->takeOwnership();
    return L0ToClResultMapper(zeCommandListAppendMemoryFill(pCommandQueue->getL0Handle(),
                                                            ptrOffset(pBuffer->getUsmPtr(), offset),
                                                            pattern,
                                                            patternSize,
                                                            size,
                                                            hSignalEvent,
                                                            waitEvents.size(),
                                                            waitEvents.data()));
}

cl_int CL_API_CALL clEnqueueCopyBuffer(cl_command_queue commandQueue,
                                       cl_mem srcBuffer,
                                       cl_mem dstBuffer,
                                       size_t srcOffset,
                                       size_t dstOffset,
                                       size_t cb,
                                       cl_uint numEventsInWaitList,
                                       const cl_event *eventWaitList,
                                       cl_event *event) {
    auto [retVal, pCommandQueue, pSrcBuffer, pDstBuffer] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::BufferObj{srcBuffer}, NEO::LEO::BufferObj{dstBuffer}),
        std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_COPY_BUFFER, pCommandQueue);

    auto lock = pCommandQueue->takeOwnership();
    return L0ToClResultMapper(zeCommandListAppendMemoryCopy(pCommandQueue->getL0Handle(),
                                                            ptrOffset(pDstBuffer->getUsmPtr(), dstOffset),
                                                            ptrOffset(pSrcBuffer->getUsmPtr(), srcOffset),
                                                            cb,
                                                            hSignalEvent,
                                                            waitEvents.size(),
                                                            waitEvents.data()));
}

cl_int CL_API_CALL clEnqueueCopyBufferRect(cl_command_queue commandQueue,
                                           cl_mem srcBuffer,
                                           cl_mem dstBuffer,
                                           const size_t *srcOrigin,
                                           const size_t *dstOrigin,
                                           const size_t *region,
                                           size_t srcRowPitch,
                                           size_t srcSlicePitch,
                                           size_t dstRowPitch,
                                           size_t dstSlicePitch,
                                           cl_uint numEventsInWaitList,
                                           const cl_event *eventWaitList,
                                           cl_event *event) {
    auto [retVal, pCommandQueue, pSrcBuffer, pDstBuffer] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::BufferObj{srcBuffer}, NEO::LEO::BufferObj{dstBuffer}),
        std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_COPY_BUFFER_RECT, pCommandQueue);

    ze_copy_region_t l0DstRegion{static_cast<uint32_t>(dstOrigin[0]), static_cast<uint32_t>(dstOrigin[1]), static_cast<uint32_t>(dstOrigin[2]), static_cast<uint32_t>(region[0]), static_cast<uint32_t>(region[1]), static_cast<uint32_t>(region[2])};
    ze_copy_region_t l0SrcRegion{static_cast<uint32_t>(srcOrigin[0]), static_cast<uint32_t>(srcOrigin[1]), static_cast<uint32_t>(srcOrigin[2]), static_cast<uint32_t>(region[0]), static_cast<uint32_t>(region[1]), static_cast<uint32_t>(region[2])};

    auto lock = pCommandQueue->takeOwnership();
    ze_result_t ret = zeCommandListAppendMemoryCopyRegion(pCommandQueue->getL0Handle(),
                                                          pDstBuffer->getUsmPtr(),
                                                          &l0DstRegion,
                                                          static_cast<uint32_t>(dstRowPitch),
                                                          static_cast<uint32_t>(dstSlicePitch),
                                                          pSrcBuffer->getUsmPtr(),
                                                          &l0SrcRegion,
                                                          static_cast<uint32_t>(srcRowPitch),
                                                          static_cast<uint32_t>(srcSlicePitch),
                                                          hSignalEvent,
                                                          waitEvents.size(),
                                                          waitEvents.data());

    return L0ToClResultMapper(ret);
}

cl_int CL_API_CALL clEnqueueReadImage(cl_command_queue commandQueue,
                                      cl_mem image,
                                      cl_bool blockingRead,
                                      const size_t *origin,
                                      const size_t *region,
                                      size_t rowPitch,
                                      size_t slicePitch,
                                      void *ptr,
                                      cl_uint numEventsInWaitList,
                                      const cl_event *eventWaitList,
                                      cl_event *event) {
    auto [retVal, pCommandQueue, pImage] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::ImageObj{image}),
        std::make_tuple(ptr, NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    if (pImage->readMemObjFlagsInvalid()) [[unlikely]] {
        return CL_INVALID_OPERATION;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_READ_IMAGE, pCommandQueue);

    auto mipDesc = createZeImageRegionWithMipLevel(pImage, origin, region);

    auto lock = pCommandQueue->takeOwnership();
    ze_result_t ret = zeCommandListAppendImageCopyToMemoryExt(pCommandQueue->getL0Handle(),
                                                              ptr,
                                                              pImage->getL0Handle(),
                                                              &mipDesc,
                                                              static_cast<uint32_t>(rowPitch),
                                                              static_cast<uint32_t>(slicePitch),
                                                              hSignalEvent,
                                                              waitEvents.size(),
                                                              waitEvents.data());
    if (blockingRead) {
        lock.unlock();
        ret = zeCommandListHostSynchronize(pCommandQueue->getL0Handle(), std::numeric_limits<uint64_t>::max());
    }

    return L0ToClResultMapper(ret);
}

cl_int CL_API_CALL clEnqueueWriteImage(cl_command_queue commandQueue,
                                       cl_mem image,
                                       cl_bool blockingWrite,
                                       const size_t *origin,
                                       const size_t *region,
                                       size_t inputRowPitch,
                                       size_t inputSlicePitch,
                                       const void *ptr,
                                       cl_uint numEventsInWaitList,
                                       const cl_event *eventWaitList,
                                       cl_event *event) {
    auto [retVal, pCommandQueue, pImage] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::ImageObj{image}),
        std::make_tuple(const_cast<void *>(ptr), NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    if (pImage->writeMemObjFlagsInvalid()) [[unlikely]] {
        return CL_INVALID_OPERATION;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_WRITE_IMAGE, pCommandQueue);

    auto mipDesc = createZeImageRegionWithMipLevel(pImage, origin, region);

    auto lock = pCommandQueue->takeOwnership();
    ze_result_t ret = zeCommandListAppendImageCopyFromMemoryExt(pCommandQueue->getL0Handle(),
                                                                pImage->getL0Handle(),
                                                                ptr,
                                                                &mipDesc,
                                                                static_cast<uint32_t>(inputRowPitch),
                                                                static_cast<uint32_t>(inputSlicePitch),
                                                                hSignalEvent,
                                                                waitEvents.size(),
                                                                waitEvents.data());
    if (blockingWrite) {
        lock.unlock();
        ret = zeCommandListHostSynchronize(pCommandQueue->getL0Handle(), std::numeric_limits<uint64_t>::max());
    }

    return L0ToClResultMapper(ret);
}

cl_int CL_API_CALL clEnqueueFillImage(cl_command_queue commandQueue,
                                      cl_mem image,
                                      const void *fillColor,
                                      const size_t *origin,
                                      const size_t *region,
                                      cl_uint numEventsInWaitList,
                                      const cl_event *eventWaitList,
                                      cl_event *event) {
    auto [retVal, pCommandQueue, pImage] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::ImageObj{image}),
        std::make_tuple(const_cast<void *>(fillColor), NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    auto l0ImageImp = pImage->getL0Object();
    auto imgInfo = l0ImageImp->getImageInfo();

    int32_t convertedFillColor[4] = {0};
    NEO::convertFillColor(fillColor, convertedFillColor, pImage->getOriginalFormat(), NEO::redescribeFillColor(imgInfo));

    auto numChannels = imgInfo.surfaceFormat->numChannels;
    auto perChannelSize = imgInfo.surfaceFormat->perChannelSizeInBytes;
    uint8_t packedFillColor[16] = {0};
    for (uint32_t ch = 0; ch < numChannels; ch++) {
        memcpy(&packedFillColor[ch * perChannelSize], &convertedFillColor[ch], perChannelSize);
    }

    auto l0Device = pCommandQueue->getDevice()->getL0Object();
    auto builtInMode = l0Device->getCompilerProductHelper().getDefaultBuiltInAddressingMode(
        NEO::ApiSpecificConfig::getBindlessMode(*l0Device->getNEODevice()));

    auto lock = l0Device->getBuiltinFunctionsLib()->obtainUniqueOwnership();
    auto *builtinKernel = l0Device->getBuiltinFunctionsLib()->getImageFunction(L0::ImageBuiltIn::fillImage3d, builtInMode);

    builtinKernel->setArgRedescribedImage(0u, pImage->getL0Handle(), false, 0u);
    builtinKernel->setArgumentValue(1u, sizeof(packedFillColor), packedFillColor);

    auto fillRegion = createZeImageRegionWithMipLevel(pImage, origin, region);
    uint32_t dstOffset[] = {fillRegion.originX, fillRegion.originY, fillRegion.originZ, fillRegion.mipLevel};
    builtinKernel->setArgumentValue(2u, sizeof(dstOffset), dstOffset);

    uint32_t groupSizeX = static_cast<uint32_t>(region[0]);
    uint32_t groupSizeY = static_cast<uint32_t>(std::max(region[1], static_cast<size_t>(1)));
    uint32_t groupSizeZ = static_cast<uint32_t>(std::max(region[2], static_cast<size_t>(1)));

    builtinKernel->suggestGroupSize(groupSizeX, groupSizeY, groupSizeZ, &groupSizeX, &groupSizeY, &groupSizeZ);
    builtinKernel->setGroupSize(groupSizeX, groupSizeY, groupSizeZ);

    uint32_t dispatchX = static_cast<uint32_t>(region[0]) / groupSizeX;
    uint32_t dispatchY = static_cast<uint32_t>(std::max(region[1], static_cast<size_t>(1))) / groupSizeY;
    uint32_t dispatchZ = static_cast<uint32_t>(std::max(region[2], static_cast<size_t>(1))) / groupSizeZ;
    ze_group_count_t groupCount{dispatchX, dispatchY, dispatchZ};

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_FILL_IMAGE, pCommandQueue);

    auto queueLock = pCommandQueue->takeOwnership();
    ze_result_t ret = zeCommandListAppendLaunchKernel(pCommandQueue->getL0Handle(),
                                                      builtinKernel->toHandle(),
                                                      &groupCount,
                                                      hSignalEvent,
                                                      waitEvents.size(),
                                                      waitEvents.data());

    return L0ToClResultMapper(ret);
}

cl_int CL_API_CALL clEnqueueCopyImage(cl_command_queue commandQueue,
                                      cl_mem srcImage,
                                      cl_mem dstImage,
                                      const size_t *srcOrigin,
                                      const size_t *dstOrigin,
                                      const size_t *region,
                                      cl_uint numEventsInWaitList,
                                      const cl_event *eventWaitList,
                                      cl_event *event) {
    auto [retVal, pCommandQueue, pSrcImage, pDstImage] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::ImageObj{srcImage}, NEO::LEO::ImageObj{dstImage}),
        std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_COPY_IMAGE, pCommandQueue);

    auto srcMipDesc = createZeImageRegionWithMipLevel(pSrcImage, srcOrigin, region);
    auto dstMipDesc = createZeImageRegionWithMipLevel(pDstImage, dstOrigin, region);

    auto lock = pCommandQueue->takeOwnership();
    ze_result_t ret = zeCommandListAppendImageCopyRegion(pCommandQueue->getL0Handle(),
                                                         pDstImage->getL0Handle(),
                                                         pSrcImage->getL0Handle(),
                                                         &dstMipDesc,
                                                         &srcMipDesc,
                                                         hSignalEvent,
                                                         waitEvents.size(),
                                                         waitEvents.data());
    return L0ToClResultMapper(ret);
}

cl_int CL_API_CALL clEnqueueCopyImageToBuffer(cl_command_queue commandQueue,
                                              cl_mem srcImage,
                                              cl_mem dstBuffer,
                                              const size_t *srcOrigin,
                                              const size_t *region,
                                              const size_t dstOffset,
                                              cl_uint numEventsInWaitList,
                                              const cl_event *eventWaitList,
                                              cl_event *event) {
    auto [retVal, pCommandQueue, pSrcImage, pDstBuffer] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::ImageObj{srcImage}, NEO::LEO::BufferObj{dstBuffer}),
        std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_COPY_IMAGE_TO_BUFFER, pCommandQueue);

    auto mipDesc = createZeImageRegionWithMipLevel(pSrcImage, srcOrigin, region);

    auto lock = pCommandQueue->takeOwnership();
    ze_result_t ret = zeCommandListAppendImageCopyToMemory(pCommandQueue->getL0Handle(),
                                                           ptrOffset(pDstBuffer->getUsmPtr(), dstOffset),
                                                           pSrcImage->getL0Handle(),
                                                           &mipDesc,
                                                           hSignalEvent,
                                                           waitEvents.size(),
                                                           waitEvents.data());

    return L0ToClResultMapper(ret);
}

cl_int CL_API_CALL clEnqueueCopyBufferToImage(cl_command_queue commandQueue,
                                              cl_mem srcBuffer,
                                              cl_mem dstImage,
                                              size_t srcOffset,
                                              const size_t *dstOrigin,
                                              const size_t *region,
                                              cl_uint numEventsInWaitList,
                                              const cl_event *eventWaitList,
                                              cl_event *event) {
    auto [retVal, pCommandQueue, pSrcBuffer, pDstImage] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::BufferObj{srcBuffer}, NEO::LEO::ImageObj{dstImage}),
        std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_COPY_BUFFER_TO_IMAGE, pCommandQueue);

    auto mipDesc = createZeImageRegionWithMipLevel(pDstImage, dstOrigin, region);

    auto lock = pCommandQueue->takeOwnership();
    ze_result_t ret = zeCommandListAppendImageCopyFromMemory(pCommandQueue->getL0Handle(),
                                                             pDstImage->getL0Handle(),
                                                             ptrOffset(pSrcBuffer->getUsmPtr(), srcOffset),
                                                             &mipDesc,
                                                             hSignalEvent,
                                                             waitEvents.size(),
                                                             waitEvents.data());

    return L0ToClResultMapper(ret);
}

void *CL_API_CALL clEnqueueMapBuffer(cl_command_queue commandQueue,
                                     cl_mem buffer,
                                     cl_bool blockingMap,
                                     cl_map_flags mapFlags,
                                     size_t offset,
                                     size_t cb,
                                     cl_uint numEventsInWaitList,
                                     const cl_event *eventWaitList,
                                     cl_event *event,
                                     cl_int *errcodeRet) {
    ErrorCodeHelper errcodeHelper(errcodeRet, CL_SUCCESS);

    auto [retVal, pCommandQueue, pBuffer] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::BufferObj{buffer}),
        std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        errcodeHelper.set(retVal);
        return nullptr;
    }

    if (pBuffer->mapMemObjFlagsInvalid(mapFlags)) [[unlikely]] {
        errcodeHelper.set(CL_INVALID_OPERATION);
        return nullptr;
    }

    if (pBuffer->getCpuPtr() != pBuffer->getUsmPtr()) {
        if (!pBuffer->getCpuPtr()) {
            errcodeHelper.set(pBuffer->createMapAllocation());
            if (errcodeHelper.localErrcode != CL_SUCCESS) {
                return nullptr;
            }
        }
        if (NEO::isValueSet(mapFlags, CL_MAP_WRITE_INVALIDATE_REGION)) {
            errcodeHelper.set(clEnqueueMarkerWithWaitList(commandQueue, numEventsInWaitList, eventWaitList, event));
        } else {
            auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_MAP_BUFFER, pCommandQueue);

            {
                auto lock = pCommandQueue->takeOwnership();
                errcodeHelper.set(L0ToClResultMapper(zeCommandListAppendMemoryCopy(pCommandQueue->getL0Handle(),
                                                                                   ptrOffset(pBuffer->getCpuPtr(), offset),
                                                                                   ptrOffset(pBuffer->getUsmPtr(), offset),
                                                                                   cb,
                                                                                   hSignalEvent,
                                                                                   waitEvents.size(),
                                                                                   waitEvents.data())));
            }
        }
    } else {
        errcodeHelper.set(clEnqueueMarkerWithWaitList(commandQueue, numEventsInWaitList, eventWaitList, event));
    }

    if (errcodeHelper.localErrcode != CL_SUCCESS) {
        return nullptr;
    }

    NEO::LEO::MemObjSizeArray sizes{cb, 0, 0};
    NEO::LEO::MemObjOffsetArray offsets{offset, 0, 0};
    if (!pBuffer->getMapOperationsHandler().add(ptrOffset(pBuffer->getCpuPtr(), offset), cb, mapFlags, sizes, offsets)) {
        errcodeHelper.set(CL_OUT_OF_HOST_MEMORY);
        return nullptr;
    }

    if (blockingMap) {
        errcodeHelper.set(clFinish(commandQueue));
    }

    return ptrOffset(pBuffer->getCpuPtr(), offset);
}

void *CL_API_CALL clEnqueueMapImage(cl_command_queue commandQueue,
                                    cl_mem image,
                                    cl_bool blockingMap,
                                    cl_map_flags mapFlags,
                                    const size_t *origin,
                                    const size_t *region,
                                    size_t *imageRowPitch,
                                    size_t *imageSlicePitch,
                                    cl_uint numEventsInWaitList,
                                    const cl_event *eventWaitList,
                                    cl_event *event,
                                    cl_int *errcodeRet) {
    ErrorCodeHelper errcodeHelper(errcodeRet, CL_SUCCESS);

    auto [retVal, pCommandQueue, pImage] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::ImageObj{image}),
        std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        errcodeHelper.set(retVal);
        return nullptr;
    }

    if (pImage->mapMemObjFlagsInvalid(mapFlags)) [[unlikely]] {
        errcodeHelper.set(CL_INVALID_OPERATION);
        return nullptr;
    }
    if (!pImage->getCpuPtr()) {
        ze_host_mem_alloc_desc_t hostDesc{ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC, nullptr, 0};
        void *cpuPtr = nullptr;
        errcodeHelper.set(L0ToClResultMapper(zeMemAllocHost(pCommandQueue->getL0Object()->getCmdListContext(),
                                                            &hostDesc,
                                                            pImage->getHostptrSize(),
                                                            1,
                                                            &cpuPtr)));
        pImage->setCpuPtr(cpuPtr);
        if (errcodeHelper.localErrcode != CL_SUCCESS) {
            return nullptr;
        }
    }

    NEO::LEO::MemObjSizeArray sizes{region[0], region[1], region[2]};
    NEO::LEO::MemObjOffsetArray offsets{origin[0], origin[1], origin[2]};

    size_t size = pImage->calculateTotalSizeForImage(sizes);
    size_t offset = pImage->calculateTotalSizeForImage(offsets);

    if (NEO::isValueSet(mapFlags, CL_MAP_WRITE_INVALIDATE_REGION)) {
        errcodeHelper.set(clEnqueueMarkerWithWaitList(commandQueue, numEventsInWaitList, eventWaitList, event));
    } else {
        auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_MAP_IMAGE, pCommandQueue);

        auto mipDesc = createZeImageRegionWithMipLevel(pImage, origin, region);

        uint32_t mapRowPitch = 0;
        uint32_t mapSlicePitch = 0;
        if (pImage->getHostPtrRowPitch() != 0) {
            mapRowPitch = static_cast<uint32_t>(pImage->getHostPtrRowPitch());
        } else {
            mapRowPitch = static_cast<uint32_t>(static_cast<L0::ImageImp *>(L0::Image::fromHandle(pImage->getL0Handle()))->getImageInfo().rowPitch);
        }
        if (pImage->getHostPtrSlicePitch() != 0) {
            mapSlicePitch = static_cast<uint32_t>(pImage->getHostPtrSlicePitch());
        } else {
            mapSlicePitch = static_cast<uint32_t>(static_cast<L0::ImageImp *>(L0::Image::fromHandle(pImage->getL0Handle()))->getImageInfo().slicePitch);
        }

        {
            auto lock = pCommandQueue->takeOwnership();
            errcodeHelper.set(L0ToClResultMapper(zeCommandListAppendImageCopyToMemoryExt(pCommandQueue->getL0Handle(),
                                                                                         ptrOffset(pImage->getCpuPtr(), offset),
                                                                                         pImage->getL0Handle(),
                                                                                         &mipDesc,
                                                                                         mapRowPitch,
                                                                                         mapSlicePitch,
                                                                                         hSignalEvent,
                                                                                         waitEvents.size(),
                                                                                         waitEvents.data())));
        }
        if (blockingMap && errcodeHelper.localErrcode == CL_SUCCESS) {
            errcodeHelper.set(L0ToClResultMapper(zeCommandListHostSynchronize(pCommandQueue->getL0Handle(), std::numeric_limits<uint64_t>::max())));
        }
    }
    if (errcodeHelper.localErrcode != CL_SUCCESS) {
        return nullptr;
    }

    if (!pImage->getMapOperationsHandler().add(ptrOffset(pImage->getCpuPtr(), offset), size, mapFlags, sizes, offsets, getOclMipLevel(pImage, origin))) {
        errcodeHelper.set(CL_OUT_OF_HOST_MEMORY);
        return nullptr;
    }

    if (imageRowPitch) {
        auto hostPtrRowPitch = pImage->getHostPtrRowPitch();
        *imageRowPitch = hostPtrRowPitch != 0 ? hostPtrRowPitch : static_cast<L0::ImageImp *>(L0::Image::fromHandle(pImage->getL0Handle()))->getImageInfo().rowPitch;
    }

    if (imageSlicePitch) {
        auto hostPtrSlicePitch = pImage->getHostPtrSlicePitch();
        *imageSlicePitch = hostPtrSlicePitch != 0 ? hostPtrSlicePitch : static_cast<L0::ImageImp *>(L0::Image::fromHandle(pImage->getL0Handle()))->getImageInfo().slicePitch;
    }

    return ptrOffset(pImage->getCpuPtr(), offset);
}

cl_int CL_API_CALL clEnqueueUnmapMemObject(cl_command_queue commandQueue,
                                           cl_mem memObj,
                                           void *mappedPtr,
                                           cl_uint numEventsInWaitList,
                                           const cl_event *eventWaitList,
                                           cl_event *event) {
    auto [retVal, pCommandQueue, pMemObj] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, memObj),
        std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    NEO::LEO::MapInfo mapInfo{};
    pMemObj->getMapOperationsHandler().find(mappedPtr, mapInfo);
    pMemObj->getMapOperationsHandler().remove(mappedPtr);

    if (pMemObj->isImage()) {
        // Validate and cast to Image type
        auto pImage = static_cast<NEO::LEO::Image *>(pMemObj);
        if (!pImage) [[unlikely]] {
            return CL_INVALID_MEM_OBJECT;
        }

        if (mapInfo.readOnly) {
            return clEnqueueMarkerWithWaitList(commandQueue, numEventsInWaitList, eventWaitList, event);
        }

        size_t origin[4] = {mapInfo.offset[0], mapInfo.offset[1], mapInfo.offset[2], 0};
        size_t region[3] = {mapInfo.size[0], mapInfo.size[1], mapInfo.size[2]};

        if (mapInfo.mipLevel != 0) {
            switch (pImage->getClObjectType()) {
            case CL_MEM_OBJECT_IMAGE1D:
            case CL_MEM_OBJECT_IMAGE1D_BUFFER:
                origin[1] = mapInfo.mipLevel;
                break;
            case CL_MEM_OBJECT_IMAGE2D:
            case CL_MEM_OBJECT_IMAGE1D_ARRAY:
                origin[2] = mapInfo.mipLevel;
                break;
            case CL_MEM_OBJECT_IMAGE3D:
            case CL_MEM_OBJECT_IMAGE2D_ARRAY:
                origin[3] = mapInfo.mipLevel;
                break;
            default:
                break;
            }
        }

        size_t unmapRowPitch = pImage->getHostPtrRowPitch() != 0 ? pImage->getHostPtrRowPitch()
                                                                 : static_cast<L0::ImageImp *>(L0::Image::fromHandle(pImage->getL0Handle()))->getImageInfo().rowPitch;
        size_t unmapSlicePitch = pImage->getHostPtrSlicePitch() != 0 ? pImage->getHostPtrSlicePitch()
                                                                     : static_cast<L0::ImageImp *>(L0::Image::fromHandle(pImage->getL0Handle()))->getImageInfo().slicePitch;

        return clEnqueueWriteImage(commandQueue,
                                   memObj,
                                   false,
                                   origin,
                                   region,
                                   unmapRowPitch,
                                   unmapSlicePitch,
                                   mappedPtr,
                                   numEventsInWaitList,
                                   eventWaitList,
                                   event);
    } else {
        // Validate and cast to Buffer type
        auto pBuffer = static_cast<NEO::LEO::Buffer *>(pMemObj);
        if (!pBuffer) [[unlikely]] {
            return CL_INVALID_MEM_OBJECT;
        }

        if (pBuffer->getCpuPtr() != pBuffer->getUsmPtr() && !mapInfo.readOnly) {
            auto mappedOffset = mapInfo.offset[0];
            auto mappedSize = mapInfo.ptrLength;
            return clEnqueueWriteBuffer(commandQueue, memObj, false, mappedOffset, mappedSize, mappedPtr, numEventsInWaitList, eventWaitList, event);
        } else {
            return clEnqueueMarkerWithWaitList(commandQueue, numEventsInWaitList, eventWaitList, event);
        }
    }
}

cl_int CL_API_CALL clEnqueueMigrateMemObjects(cl_command_queue commandQueue,
                                              cl_uint numMemObjects,
                                              const cl_mem *memObjects,
                                              cl_mem_migration_flags flags,
                                              cl_uint numEventsInWaitList,
                                              const cl_event *eventWaitList,
                                              cl_event *event) {
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue), std::make_tuple(NEO::LEO::MemObjList{memObjects, numMemObjects}, NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    if (numMemObjects == 0 || memObjects == nullptr) {
        return CL_INVALID_VALUE;
    }

    for (cl_uint object = 0; object < numMemObjects; ++object) {
        auto memObject = NEO::LEO::castToObject<NEO::LEO::MemObj>(memObjects[object]);
        if (!memObject) {
            return CL_INVALID_MEM_OBJECT;
        }
    }

    const cl_mem_migration_flags allValidFlags = CL_MIGRATE_MEM_OBJECT_CONTENT_UNDEFINED | CL_MIGRATE_MEM_OBJECT_HOST;

    if ((flags & (~allValidFlags)) != 0) [[unlikely]] {
        return CL_INVALID_VALUE;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_MIGRATE_MEM_OBJECTS, pCommandQueue);

    auto lock = pCommandQueue->takeOwnership();
    return L0ToClResultMapper(zeCommandListAppendBarrier(pCommandQueue->getL0Handle(), hSignalEvent, waitEvents.size(), waitEvents.data()));
}

cl_int CL_API_CALL clEnqueueNDRangeKernel(cl_command_queue commandQueue,
                                          cl_kernel kernel,
                                          cl_uint workDim,
                                          const size_t *globalWorkOffset,
                                          const size_t *globalWorkSize,
                                          const size_t *localWorkSize,
                                          cl_uint numEventsInWaitList,
                                          const cl_event *eventWaitList,
                                          cl_event *event) {
    auto [retVal, pCommandQueue, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue, kernel), std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    auto kernelHandle = pKernel->getL0Handle();
    ze_result_t ret = ZE_RESULT_SUCCESS;

    if (!pKernel->areAllArgsSet()) [[unlikely]] {
        return CL_INVALID_KERNEL_ARGS;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_NDRANGE_KERNEL, pCommandQueue);
    auto cmdlistHandle = pCommandQueue->getL0Handle();
    auto lock = pCommandQueue->takeOwnership();

    if (!globalWorkSize || globalWorkSize[0] == 0) {
        return L0ToClResultMapper(zeCommandListAppendBarrier(cmdlistHandle, hSignalEvent, waitEvents.size(), waitEvents.data()));
    }

    uint32_t gwo[3] = {globalWorkOffset ? static_cast<uint32_t>(globalWorkOffset[0]) : 0,
                       workDim > 1 ? static_cast<uint32_t>(globalWorkOffset ? globalWorkOffset[1] : 0) : 0,
                       workDim > 2 ? static_cast<uint32_t>(globalWorkOffset ? globalWorkOffset[2] : 0) : 0};

    zeKernelSetGlobalOffsetExp(kernelHandle, gwo[0], gwo[1], gwo[2]);

    uint32_t lws[3] = {1, 1, 1};

    if (localWorkSize) {
        lws[0] = static_cast<uint32_t>(localWorkSize[0]);
        if (workDim > 1) {
            lws[1] = static_cast<uint32_t>(localWorkSize[1]);
        }
        if (workDim > 2) {
            lws[2] = static_cast<uint32_t>(localWorkSize[2]);
        }
    } else if (pKernel->getL0Object()->getKernelDescriptor().kernelAttributes.requiredWorkgroupSize[0]) {
        lws[0] = pKernel->getL0Object()->getKernelDescriptor().kernelAttributes.requiredWorkgroupSize[0];
        lws[1] = workDim > 1 ? pKernel->getL0Object()->getKernelDescriptor().kernelAttributes.requiredWorkgroupSize[1] : 1u;
        lws[2] = workDim > 2 ? pKernel->getL0Object()->getKernelDescriptor().kernelAttributes.requiredWorkgroupSize[2] : 1u;
    } else {
        uint32_t gws[3] = {static_cast<uint32_t>(globalWorkSize[0]),
                           workDim > 1 ? static_cast<uint32_t>(globalWorkSize[1]) : 1u,
                           workDim > 2 ? static_cast<uint32_t>(globalWorkSize[2]) : 1u};
        ret = zeKernelSuggestGroupSize(kernelHandle, gws[0], gws[1], gws[2], &lws[0], &lws[1], &lws[2]);
        if (ret != ZE_RESULT_SUCCESS) {
            return L0ToClResultMapper(ret);
        }
    }

    ret = zeKernelSetGroupSize(kernelHandle, lws[0], lws[1], lws[2]);
    if (ret != ZE_RESULT_SUCCESS) {
        return L0ToClResultMapper(ret);
    }

    ze_group_count_t wgc{static_cast<uint32_t>(globalWorkSize[0] / lws[0]),
                         workDim > 1 ? static_cast<uint32_t>(globalWorkSize[1] / lws[1]) : 1u,
                         workDim > 2 ? static_cast<uint32_t>(globalWorkSize[2] / lws[2]) : 1u};

    for (cl_uint i = 0; i < workDim; ++i) {
        if (static_cast<uint32_t>(globalWorkSize[i]) % lws[i] != 0) [[unlikely]] {
            return CL_INVALID_WORK_GROUP_SIZE;
        }
    }

    return L0ToClResultMapper(zeCommandListAppendLaunchKernel(cmdlistHandle, kernelHandle, &wgc, hSignalEvent, waitEvents.size(), waitEvents.data()));
}

cl_int CL_API_CALL clEnqueueWaitForEvents(cl_command_queue commandQueue,
                                          cl_uint numEvents,
                                          const cl_event *eventList) {
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue), std::make_tuple(NEO::LEO::EventWaitList{eventList, numEvents}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    if (pCommandQueue->getContext()->isTerminated()) [[unlikely]] {
        return CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST;
    }
    NEO::LEO::EventHandleSpan waitEvents{numEvents, eventList};
    auto lock = pCommandQueue->takeOwnership();
    return L0ToClResultMapper(zeCommandListAppendWaitOnEvents(pCommandQueue->getL0Handle(), waitEvents.size(), waitEvents.data()));
}

cl_int CL_API_CALL clEnqueueMarkerWithWaitList(cl_command_queue commandQueue,
                                               cl_uint numEventsInWaitList,
                                               const cl_event *eventWaitList,
                                               cl_event *event) {
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue), std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    if ((numEventsInWaitList > 0 && !eventWaitList) ||
        (numEventsInWaitList == 0 && eventWaitList)) {
        return CL_INVALID_EVENT_WAIT_LIST;
    }

    for (cl_uint i = 0; i < numEventsInWaitList; ++i) {
        auto pEvent = NEO::LEO::castToObject<NEO::LEO::Event>(eventWaitList[i]);

        if (!pEvent) {
            return CL_INVALID_EVENT_WAIT_LIST;
        }

        if (pCommandQueue->getContext() != pEvent->getContext()) {
            return CL_INVALID_CONTEXT;
        }
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_MARKER, pCommandQueue);

    auto lock = pCommandQueue->takeOwnership();
    return L0ToClResultMapper(zeCommandListAppendBarrier(pCommandQueue->getL0Handle(), hSignalEvent, waitEvents.size(), waitEvents.data()));
}

cl_int CL_API_CALL clEnqueueBarrierWithWaitList(cl_command_queue commandQueue,
                                                cl_uint numEventsInWaitList,
                                                const cl_event *eventWaitList,
                                                cl_event *event) {
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue), std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    if (eventWaitList) {
        if (std::any_of(eventWaitList, eventWaitList + numEventsInWaitList, [pCommandQueue](cl_event event) { return NEO::LEO::castToObject<NEO::LEO::Event>(event)->getContext() != pCommandQueue->getContext(); })) {
            return CL_INVALID_CONTEXT;
        }
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_BARRIER, pCommandQueue);

    auto lock = pCommandQueue->takeOwnership();
    return L0ToClResultMapper(zeCommandListAppendBarrier(pCommandQueue->getL0Handle(), hSignalEvent, waitEvents.size(), waitEvents.data()));
}

cl_int CL_API_CALL clEnqueueSVMMigrateMem(cl_command_queue commandQueue,
                                          cl_uint numSvmPointers,
                                          const void **svmPointers,
                                          const size_t *sizes,
                                          const cl_mem_migration_flags flags,
                                          cl_uint numEventsInWaitList,
                                          const cl_event *eventWaitList,
                                          cl_event *event) {
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue), std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_SVM_MIGRATE_MEM, pCommandQueue);

    auto lock = pCommandQueue->takeOwnership();
    return L0ToClResultMapper(zeCommandListAppendBarrier(pCommandQueue->getL0Handle(), hSignalEvent, waitEvents.size(), waitEvents.data()));
}

cl_int CL_API_CALL clEnqueueSVMFree(cl_command_queue commandQueue,
                                    cl_uint numSvmPointers,
                                    void *svmPointers[],
                                    void(CL_CALLBACK *pfnFreeFunc)(cl_command_queue queue,
                                                                   cl_uint numSvmPointers,
                                                                   void *svmPointers[],
                                                                   void *userData),
                                    void *userData,
                                    cl_uint numEventsInWaitList,
                                    const cl_event *eventWaitList,
                                    cl_event *event) {
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue), std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    if (numSvmPointers == 0 || svmPointers == nullptr) [[unlikely]] {
        return CL_INVALID_VALUE;
    }

    cl_event evntForCallback = nullptr;
    bool ownsEventDeletion = false;
    if (event == nullptr) {
        ownsEventDeletion = true;
        event = &evntForCallback;
    }

    clEnqueueMarkerWithWaitList(commandQueue, numEventsInWaitList, eventWaitList, event);

    struct ClEnqueueSVMFreeUserData {
        cl_command_queue commandQueue = nullptr;

        void(CL_CALLBACK *pfnFreeFunc)(cl_command_queue queue,
                                       cl_uint numSvmPointers,
                                       void *svmPointers[],
                                       void *userData) = nullptr;
        void *userData = nullptr;

        cl_uint numSvmPointers = 0;
        void **svmPointers = nullptr;

        bool ownsEventDeletion = false;
    };
    auto clEnqueueSVMFreeUserData = new ClEnqueueSVMFreeUserData{commandQueue, pfnFreeFunc, userData, numSvmPointers, svmPointers, ownsEventDeletion};

    auto clEnqueueSVMFreeCallbackWrapper = [](cl_event event, cl_int, void *userData) {
        auto clEnqueueSVMFreeUserData = static_cast<ClEnqueueSVMFreeUserData *>(userData);

        auto pEvent = NEO::LEO::castToObject<NEO::LEO::Event>(event);

        if (clEnqueueSVMFreeUserData->pfnFreeFunc) {
            clEnqueueSVMFreeUserData->pfnFreeFunc(clEnqueueSVMFreeUserData->commandQueue, clEnqueueSVMFreeUserData->numSvmPointers, clEnqueueSVMFreeUserData->svmPointers, clEnqueueSVMFreeUserData->userData);
        } else {
            for (cl_uint i = 0; i < clEnqueueSVMFreeUserData->numSvmPointers; ++i) {
                clSVMFree(pEvent->getContext(), clEnqueueSVMFreeUserData->svmPointers[i]);
            }
        }

        if (clEnqueueSVMFreeUserData->ownsEventDeletion) {
            clReleaseEvent(event);
        } else {
            pEvent->updateCommandType(CL_COMMAND_SVM_FREE);
        }

        delete clEnqueueSVMFreeUserData;
    };

    return clSetEventCallback(*event, CL_COMPLETE, clEnqueueSVMFreeCallbackWrapper, clEnqueueSVMFreeUserData);
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueMemsetINTEL(
    cl_command_queue commandQueue,
    void *dstPtr,
    cl_int value,
    size_t size,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue), std::make_tuple(dstPtr, NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_MEMSET_INTEL, pCommandQueue);

    auto lock = pCommandQueue->takeOwnership();
    return L0ToClResultMapper(zeCommandListAppendMemoryFill(pCommandQueue->getL0Handle(),
                                                            dstPtr,
                                                            &value,
                                                            sizeof(cl_int),
                                                            size,
                                                            hSignalEvent,
                                                            waitEvents.size(),
                                                            waitEvents.data()));
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueMemFillINTEL(
    cl_command_queue commandQueue,
    void *dstPtr,
    const void *pattern,
    size_t patternSize,
    size_t size,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue), std::make_tuple(dstPtr, const_cast<void *>(pattern), NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_MEMFILL_INTEL, pCommandQueue);

    auto lock = pCommandQueue->takeOwnership();
    return L0ToClResultMapper(zeCommandListAppendMemoryFill(pCommandQueue->getL0Handle(),
                                                            dstPtr,
                                                            pattern,
                                                            patternSize,
                                                            size,
                                                            hSignalEvent,
                                                            waitEvents.size(),
                                                            waitEvents.data()));
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueMemcpyINTEL(
    cl_command_queue commandQueue,
    cl_bool blocking,
    void *dstPtr,
    const void *srcPtr,
    size_t size,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue), std::make_tuple(dstPtr, const_cast<void *>(srcPtr), NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_MEMCPY_INTEL, pCommandQueue);

    auto lock = pCommandQueue->takeOwnership();
    ze_result_t ret = zeCommandListAppendMemoryCopy(pCommandQueue->getL0Handle(),
                                                    dstPtr,
                                                    srcPtr,
                                                    size,
                                                    hSignalEvent,
                                                    waitEvents.size(),
                                                    waitEvents.data());
    if (blocking) {
        lock.unlock();
        ret = zeCommandListHostSynchronize(pCommandQueue->getL0Handle(), std::numeric_limits<uint64_t>::max());
    }

    return L0ToClResultMapper(ret);
}

cl_int CL_API_CALL clEnqueueTask(cl_command_queue commandQueue,
                                 cl_kernel kernel,
                                 cl_uint numEventsInWaitList,
                                 const cl_event *eventWaitList,
                                 cl_event *event) {
    size_t globalWorkOffset = 0;
    size_t globalWorkSize = 1u;
    size_t localWorkSize = 1u;
    return clEnqueueNDRangeKernel(commandQueue, kernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, numEventsInWaitList, eventWaitList, event);
}

cl_int CL_API_CALL clEnqueueNativeKernel(cl_command_queue commandQueue,
                                         void(CL_CALLBACK *userFunc)(void *),
                                         void *args,
                                         size_t cbArgs,
                                         cl_uint numMemObjects,
                                         const cl_mem *memList,
                                         const void **argsMemLoc,
                                         cl_uint numEventsInWaitList,
                                         const cl_event *eventWaitList,
                                         cl_event *event) {
    return CL_INVALID_OPERATION;
}

cl_int CL_API_CALL clEnqueueMarker(cl_command_queue commandQueue,
                                   cl_event *event) {
    return clEnqueueMarkerWithWaitList(commandQueue, 0, nullptr, event);
}

cl_int CL_API_CALL clEnqueueBarrier(cl_command_queue commandQueue) {
    return clEnqueueBarrierWithWaitList(commandQueue, 0, nullptr, nullptr);
}

cl_int CL_API_CALL clEnqueueSVMMemcpy(cl_command_queue commandQueue,
                                      cl_bool blockingCopy,
                                      void *dstPtr,
                                      const void *srcPtr,
                                      size_t size,
                                      cl_uint numEventsInWaitList,
                                      const cl_event *eventWaitList,
                                      cl_event *event) {
    auto ret = clEnqueueMemcpyINTEL(commandQueue, blockingCopy, dstPtr, srcPtr, size, numEventsInWaitList, eventWaitList, event);
    if (event) {
        NEO::LEO::castToObject<NEO::LEO::Event>(*event)->updateCommandType(CL_COMMAND_SVM_MEMCPY);
    }
    return ret;
}

cl_int CL_API_CALL clEnqueueSVMMemFill(cl_command_queue commandQueue,
                                       void *svmPtr,
                                       const void *pattern,
                                       size_t patternSize,
                                       size_t size,
                                       cl_uint numEventsInWaitList,
                                       const cl_event *eventWaitList,
                                       cl_event *event) {
    auto ret = clEnqueueMemFillINTEL(commandQueue, svmPtr, pattern, patternSize, size, numEventsInWaitList, eventWaitList, event);
    if (event) {
        NEO::LEO::castToObject<NEO::LEO::Event>(*event)->updateCommandType(CL_COMMAND_SVM_MEMFILL);
    }
    return ret;
}

cl_int CL_API_CALL clEnqueueSVMMap(cl_command_queue commandQueue,
                                   cl_bool blockingMap,
                                   cl_map_flags mapFlags,
                                   void *svmPtr,
                                   size_t size,
                                   cl_uint numEventsInWaitList,
                                   const cl_event *eventWaitList,
                                   cl_event *event) {
    auto ret = clEnqueueMarkerWithWaitList(commandQueue, numEventsInWaitList, eventWaitList, event);
    if (event) {
        NEO::LEO::castToObject<NEO::LEO::Event>(*event)->updateCommandType(CL_COMMAND_SVM_MAP);
    }
    return ret;
}

cl_int CL_API_CALL clEnqueueSVMUnmap(cl_command_queue commandQueue,
                                     void *svmPtr,
                                     cl_uint numEventsInWaitList,
                                     const cl_event *eventWaitList,
                                     cl_event *event) {
    auto ret = clEnqueueMarkerWithWaitList(commandQueue, numEventsInWaitList, eventWaitList, event);
    if (event) {
        NEO::LEO::castToObject<NEO::LEO::Event>(*event)->updateCommandType(CL_COMMAND_SVM_UNMAP);
    }
    return ret;
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueVerifyMemoryINTEL(
    cl_command_queue commandQueue,
    const void *allocationPtr,
    const void *expectedData,
    size_t sizeOfComparison,
    cl_uint comparisonMode) {

    if (sizeOfComparison == 0 || expectedData == nullptr || allocationPtr == nullptr) {
        return CL_INVALID_VALUE;
    }

    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue),
        std::make_tuple());
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    auto l0CmdList = pCommandQueue->getL0Object();
    auto status = l0CmdList->verifyMemory(allocationPtr, expectedData, sizeOfComparison, comparisonMode);
    return status ? CL_SUCCESS : CL_INVALID_VALUE;
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueMigrateMemINTEL(
    cl_command_queue commandQueue,
    const void *ptr,
    size_t size,
    cl_mem_migration_flags flags,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue), std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_MIGRATEMEM_INTEL, pCommandQueue);
    auto cmdlistHandle = pCommandQueue->getL0Handle();
    auto lock = pCommandQueue->takeOwnership();

    ze_result_t ret = ZE_RESULT_SUCCESS;

    if (waitEvents.size() > 0) {
        ret = zeCommandListAppendWaitOnEvents(cmdlistHandle, waitEvents.size(), waitEvents.data());
        if (ret != ZE_RESULT_SUCCESS) [[unlikely]] {
            return L0ToClResultMapper(ret);
        }
    }

    ret = zeCommandListAppendMemoryPrefetch(cmdlistHandle, ptr, size);
    if (ret != ZE_RESULT_SUCCESS) [[unlikely]] {
        return L0ToClResultMapper(ret);
    }

    if (hSignalEvent) {
        ret = zeCommandListAppendSignalEvent(cmdlistHandle, hSignalEvent);
        if (ret != ZE_RESULT_SUCCESS) [[unlikely]] {
            return L0ToClResultMapper(ret);
        }
    }

    return CL_SUCCESS;
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueMemAdviseINTEL(
    cl_command_queue commandQueue,
    const void *ptr,
    size_t size,
    cl_mem_advice_intel advice,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue), std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_MEMADVISE_INTEL, pCommandQueue);
    auto cmdlistHandle = pCommandQueue->getL0Handle();
    auto lock = pCommandQueue->takeOwnership();
    auto deviceHandle = pCommandQueue->getDevice()->getL0Handle();

    ze_result_t ret = ZE_RESULT_SUCCESS;

    if (waitEvents.size() > 0) {
        ret = zeCommandListAppendWaitOnEvents(cmdlistHandle, waitEvents.size(), waitEvents.data());
        if (ret != ZE_RESULT_SUCCESS) {
            return L0ToClResultMapper(ret);
        }
    }

    ret = zeCommandListAppendMemAdvise(cmdlistHandle, deviceHandle, ptr, size, static_cast<ze_memory_advice_t>(advice));
    if (ret != ZE_RESULT_SUCCESS) {
        return L0ToClResultMapper(ret);
    }

    if (hSignalEvent) {
        ret = zeCommandListAppendSignalEvent(cmdlistHandle, hSignalEvent);
        if (ret != ZE_RESULT_SUCCESS) {
            return L0ToClResultMapper(ret);
        }
    }

    return CL_SUCCESS;
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueNDCountKernelINTEL(
    cl_command_queue commandQueue,
    cl_kernel kernel,
    cl_uint workDim,
    const size_t *globalWorkOffset,
    const size_t *workgroupCount,
    const size_t *localWorkSize,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {
    auto [retVal, pCommandQueue, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue, kernel), std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    auto kernelHandle = pKernel->getL0Handle();

    if (!pKernel->areAllArgsSet()) [[unlikely]] {
        return CL_INVALID_KERNEL_ARGS;
    }

    if (!workgroupCount) [[unlikely]] {
        return CL_INVALID_VALUE;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_NDRANGE_KERNEL, pCommandQueue);
    auto cmdlistHandle = pCommandQueue->getL0Handle();
    auto lock = pCommandQueue->takeOwnership();

    uint32_t gwo[3] = {globalWorkOffset ? static_cast<uint32_t>(globalWorkOffset[0]) : 0,
                       workDim > 1 ? static_cast<uint32_t>(globalWorkOffset ? globalWorkOffset[1] : 0) : 0,
                       workDim > 2 ? static_cast<uint32_t>(globalWorkOffset ? globalWorkOffset[2] : 0) : 0};

    zeKernelSetGlobalOffsetExp(kernelHandle, gwo[0], gwo[1], gwo[2]);

    ze_group_count_t wgc{static_cast<uint32_t>(workgroupCount[0]),
                         workDim > 1 ? static_cast<uint32_t>(workgroupCount[1]) : 1u,
                         workDim > 2 ? static_cast<uint32_t>(workgroupCount[2]) : 1u};

    uint32_t lws[3] = {1, 1, 1};
    ze_result_t ret = ZE_RESULT_SUCCESS;

    if (localWorkSize) {
        lws[0] = static_cast<uint32_t>(localWorkSize[0]);
        if (workDim > 1) {
            lws[1] = static_cast<uint32_t>(localWorkSize[1]);
        }
        if (workDim > 2) {
            lws[2] = static_cast<uint32_t>(localWorkSize[2]);
        }
    } else if (pKernel->getL0Object()->getKernelDescriptor().kernelAttributes.requiredWorkgroupSize[0]) {
        lws[0] = pKernel->getL0Object()->getKernelDescriptor().kernelAttributes.requiredWorkgroupSize[0];
        lws[1] = workDim > 1 ? pKernel->getL0Object()->getKernelDescriptor().kernelAttributes.requiredWorkgroupSize[1] : 1u;
        lws[2] = workDim > 2 ? pKernel->getL0Object()->getKernelDescriptor().kernelAttributes.requiredWorkgroupSize[2] : 1u;
    } else {
        ret = zeKernelSuggestGroupSize(kernelHandle, wgc.groupCountX, wgc.groupCountY, wgc.groupCountZ, &lws[0], &lws[1], &lws[2]);
        if (ret != ZE_RESULT_SUCCESS) {
            return L0ToClResultMapper(ret);
        }
    }

    ret = zeKernelSetGroupSize(kernelHandle, lws[0], lws[1], lws[2]);
    if (ret != ZE_RESULT_SUCCESS) [[unlikely]] {
        return L0ToClResultMapper(ret);
    }

    return L0ToClResultMapper(zeCommandListAppendLaunchCooperativeKernel(cmdlistHandle, kernelHandle, &wgc, hSignalEvent, waitEvents.size(), waitEvents.data()));
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueAcquireExternalMemObjectsKHR(
    cl_command_queue commandQueue,
    cl_uint numMemObjects,
    const cl_mem *memObjects,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {
    return CL_INVALID_OPERATION;
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueReleaseExternalMemObjectsKHR(
    cl_command_queue commandQueue,
    cl_uint numMemObjects,
    const cl_mem *memObjects,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {
    return CL_INVALID_OPERATION;
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueExternalMemObjectsKHR(
    cl_command_queue commandQueue,
    cl_uint numMemObjects,
    const cl_mem *memObjects,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {
    return CL_INVALID_OPERATION;
}

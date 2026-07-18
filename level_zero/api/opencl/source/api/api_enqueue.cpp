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

#include "level_zero/api/opencl/source/api/leo_api.h"
#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/event/leo_event.h"
#include "level_zero/api/opencl/source/helpers/image_enqueue_helpers.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/source/helpers/leo_cl_validators.h"
#include "level_zero/api/opencl/source/helpers/leo_convert_color.h"
#include "level_zero/api/opencl/source/kernel/leo_kernel.h"
#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"
#include "level_zero/api/opencl/source/mem_obj/leo_image.h"
#include "level_zero/api/opencl/source/tracing/leo_tracing_notify.h"
#include "level_zero/core/source/builtin/builtin_functions_lib.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/image/internal_core_image_ext.h"
#include "level_zero/core/source/kernel/kernel.h"
#include <level_zero/ze_api.h>

#include "CL/cl.h"

#include <cstring>

inline void applyDefaultRectPitches(const size_t *region, size_t &rowPitch, size_t &slicePitch) {
    if (rowPitch == 0) {
        rowPitch = region[0];
    }
    if (slicePitch == 0) {
        slicePitch = region[1] * rowPitch;
    }
}

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
    TRACING_ENTER(ClEnqueueReadBuffer, &commandQueue, &buffer, &blockingRead, &offset, &cb, &ptr, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue, pBuffer] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::BufferObj{buffer}),
        std::make_tuple(ptr, NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClEnqueueReadBuffer, &retVal);
        return retVal;
    }

    if (pBuffer->readMemObjFlagsInvalid()) [[unlikely]] {
        cl_int tracingRetVal = CL_INVALID_OPERATION;
        TRACING_EXIT(ClEnqueueReadBuffer, &tracingRetVal);
        return tracingRetVal;
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
        ret = pCommandQueue->hostSynchronize(std::numeric_limits<uint64_t>::max());
    }

    cl_int tracingRetVal = L0ToClResultMapper(ret);
    TRACING_EXIT(ClEnqueueReadBuffer, &tracingRetVal);
    return tracingRetVal;
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
    TRACING_ENTER(ClEnqueueReadBufferRect, &commandQueue, &buffer, &blockingRead, &bufferOrigin, &hostOrigin, &region, &bufferRowPitch, &bufferSlicePitch, &hostRowPitch, &hostSlicePitch, &ptr, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue, pBuffer] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::BufferObj{buffer}),
        std::make_tuple(ptr, NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClEnqueueReadBufferRect, &retVal);
        return retVal;
    }

    if (pBuffer->readMemObjFlagsInvalid()) [[unlikely]] {
        cl_int tracingRetVal = CL_INVALID_OPERATION;
        TRACING_EXIT(ClEnqueueReadBufferRect, &tracingRetVal);
        return tracingRetVal;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_READ_BUFFER_RECT, pCommandQueue);

    applyDefaultRectPitches(region, bufferRowPitch, bufferSlicePitch);
    applyDefaultRectPitches(region, hostRowPitch, hostSlicePitch);

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
        ret = pCommandQueue->hostSynchronize(std::numeric_limits<uint64_t>::max());
    }

    cl_int tracingRetVal = L0ToClResultMapper(ret);
    TRACING_EXIT(ClEnqueueReadBufferRect, &tracingRetVal);
    return tracingRetVal;
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
    TRACING_ENTER(ClEnqueueWriteBuffer, &commandQueue, &buffer, &blockingWrite, &offset, &cb, &ptr, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue, pBuffer] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::BufferObj{buffer}),
        std::make_tuple(const_cast<void *>(ptr), NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClEnqueueWriteBuffer, &retVal);
        return retVal;
    }

    if (pBuffer->writeMemObjFlagsInvalid()) [[unlikely]] {
        cl_int tracingRetVal = CL_INVALID_OPERATION;
        TRACING_EXIT(ClEnqueueWriteBuffer, &tracingRetVal);
        return tracingRetVal;
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
        ret = pCommandQueue->hostSynchronize(std::numeric_limits<uint64_t>::max());
    }

    cl_int tracingRetVal = L0ToClResultMapper(ret);
    TRACING_EXIT(ClEnqueueWriteBuffer, &tracingRetVal);
    return tracingRetVal;
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
    TRACING_ENTER(ClEnqueueWriteBufferRect, &commandQueue, &buffer, &blockingWrite, &bufferOrigin, &hostOrigin, &region, &bufferRowPitch, &bufferSlicePitch, &hostRowPitch, &hostSlicePitch, &ptr, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue, pBuffer] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::BufferObj{buffer}),
        std::make_tuple(const_cast<void *>(ptr), NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClEnqueueWriteBufferRect, &retVal);
        return retVal;
    }

    if (pBuffer->writeMemObjFlagsInvalid()) [[unlikely]] {
        cl_int tracingRetVal = CL_INVALID_OPERATION;
        TRACING_EXIT(ClEnqueueWriteBufferRect, &tracingRetVal);
        return tracingRetVal;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_WRITE_BUFFER_RECT, pCommandQueue);

    applyDefaultRectPitches(region, bufferRowPitch, bufferSlicePitch);
    applyDefaultRectPitches(region, hostRowPitch, hostSlicePitch);

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
        ret = pCommandQueue->hostSynchronize(std::numeric_limits<uint64_t>::max());
    }

    cl_int tracingRetVal = L0ToClResultMapper(ret);
    TRACING_EXIT(ClEnqueueWriteBufferRect, &tracingRetVal);
    return tracingRetVal;
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
    TRACING_ENTER(ClEnqueueFillBuffer, &commandQueue, &buffer, &pattern, &patternSize, &offset, &size, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue, pBuffer] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::BufferObj{buffer}),
        std::make_tuple(const_cast<void *>(pattern), NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClEnqueueFillBuffer, &retVal);
        return retVal;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_FILL_BUFFER, pCommandQueue);

    auto lock = pCommandQueue->takeOwnership();
    cl_int tracingRetVal = L0ToClResultMapper(zeCommandListAppendMemoryFill(pCommandQueue->getL0Handle(),
                                                                            ptrOffset(pBuffer->getUsmPtr(), offset),
                                                                            pattern,
                                                                            patternSize,
                                                                            size,
                                                                            hSignalEvent,
                                                                            waitEvents.size(),
                                                                            waitEvents.data()));
    TRACING_EXIT(ClEnqueueFillBuffer, &tracingRetVal);
    return tracingRetVal;
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
    TRACING_ENTER(ClEnqueueCopyBuffer, &commandQueue, &srcBuffer, &dstBuffer, &srcOffset, &dstOffset, &cb, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue, pSrcBuffer, pDstBuffer] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::BufferObj{srcBuffer}, NEO::LEO::BufferObj{dstBuffer}),
        std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClEnqueueCopyBuffer, &retVal);
        return retVal;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_COPY_BUFFER, pCommandQueue);

    auto lock = pCommandQueue->takeOwnership();
    cl_int tracingRetVal = L0ToClResultMapper(zeCommandListAppendMemoryCopy(pCommandQueue->getL0Handle(),
                                                                            ptrOffset(pDstBuffer->getUsmPtr(), dstOffset),
                                                                            ptrOffset(pSrcBuffer->getUsmPtr(), srcOffset),
                                                                            cb,
                                                                            hSignalEvent,
                                                                            waitEvents.size(),
                                                                            waitEvents.data()));
    TRACING_EXIT(ClEnqueueCopyBuffer, &tracingRetVal);
    return tracingRetVal;
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
    TRACING_ENTER(ClEnqueueCopyBufferRect, &commandQueue, &srcBuffer, &dstBuffer, &srcOrigin, &dstOrigin, &region, &srcRowPitch, &srcSlicePitch, &dstRowPitch, &dstSlicePitch, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue, pSrcBuffer, pDstBuffer] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::BufferObj{srcBuffer}, NEO::LEO::BufferObj{dstBuffer}),
        std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClEnqueueCopyBufferRect, &retVal);
        return retVal;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_COPY_BUFFER_RECT, pCommandQueue);

    applyDefaultRectPitches(region, srcRowPitch, srcSlicePitch);
    applyDefaultRectPitches(region, dstRowPitch, dstSlicePitch);

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

    cl_int tracingRetVal = L0ToClResultMapper(ret);
    TRACING_EXIT(ClEnqueueCopyBufferRect, &tracingRetVal);
    return tracingRetVal;
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
    TRACING_ENTER(ClEnqueueReadImage, &commandQueue, &image, &blockingRead, &origin, &region, &rowPitch, &slicePitch, &ptr, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue, pImage] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::ImageObj{image}),
        std::make_tuple(ptr, NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClEnqueueReadImage, &retVal);
        return retVal;
    }

    if (pImage->readMemObjFlagsInvalid()) [[unlikely]] {
        cl_int tracingRetVal = CL_INVALID_OPERATION;
        TRACING_EXIT(ClEnqueueReadImage, &tracingRetVal);
        return tracingRetVal;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_READ_IMAGE, pCommandQueue);

    auto mipDesc = createZeImageRegionWithMipLevel(pImage, origin, region);

    auto lock = pCommandQueue->takeOwnership();
    pImage->migrateTo(pCommandQueue->getL0Handle(), pCommandQueue->getDevice()->getRootDeviceIndex(), pCommandQueue->isOutOfOrder(), static_cast<uint32_t>(waitEvents.size()), waitEvents.data());
    ze_result_t ret = zeCommandListAppendImageCopyToMemoryExt(pCommandQueue->getL0Handle(),
                                                              ptr,
                                                              pImage->getL0Handle(pCommandQueue->getDevice()->getRootDeviceIndex()),
                                                              &mipDesc,
                                                              getL0ImageRowPitch(pImage->getClObjectType(), rowPitch, slicePitch),
                                                              static_cast<uint32_t>(slicePitch),
                                                              hSignalEvent,
                                                              waitEvents.size(),
                                                              waitEvents.data());
    if (blockingRead) {
        lock.unlock();
        ret = pCommandQueue->hostSynchronize(std::numeric_limits<uint64_t>::max());
    }

    cl_int tracingRetVal = L0ToClResultMapper(ret);
    TRACING_EXIT(ClEnqueueReadImage, &tracingRetVal);
    return tracingRetVal;
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
    TRACING_ENTER(ClEnqueueWriteImage, &commandQueue, &image, &blockingWrite, &origin, &region, &inputRowPitch, &inputSlicePitch, &ptr, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue, pImage] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::ImageObj{image}),
        std::make_tuple(const_cast<void *>(ptr), NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClEnqueueWriteImage, &retVal);
        return retVal;
    }

    if (pImage->writeMemObjFlagsInvalid()) [[unlikely]] {
        cl_int tracingRetVal = CL_INVALID_OPERATION;
        TRACING_EXIT(ClEnqueueWriteImage, &tracingRetVal);
        return tracingRetVal;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_WRITE_IMAGE, pCommandQueue);

    auto mipDesc = createZeImageRegionWithMipLevel(pImage, origin, region);

    auto lock = pCommandQueue->takeOwnership();
    pImage->migrateTo(pCommandQueue->getL0Handle(), pCommandQueue->getDevice()->getRootDeviceIndex(), pCommandQueue->isOutOfOrder(), static_cast<uint32_t>(waitEvents.size()), waitEvents.data());
    ze_result_t ret = zeCommandListAppendImageCopyFromMemoryExt(pCommandQueue->getL0Handle(),
                                                                pImage->getL0Handle(pCommandQueue->getDevice()->getRootDeviceIndex()),
                                                                ptr,
                                                                &mipDesc,
                                                                getL0ImageRowPitch(pImage->getClObjectType(), inputRowPitch, inputSlicePitch),
                                                                static_cast<uint32_t>(inputSlicePitch),
                                                                hSignalEvent,
                                                                waitEvents.size(),
                                                                waitEvents.data());
    if (blockingWrite) {
        lock.unlock();
        ret = pCommandQueue->hostSynchronize(std::numeric_limits<uint64_t>::max());
    }

    cl_int tracingRetVal = L0ToClResultMapper(ret);
    TRACING_EXIT(ClEnqueueWriteImage, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clEnqueueFillImage(cl_command_queue commandQueue,
                                      cl_mem image,
                                      const void *fillColor,
                                      const size_t *origin,
                                      const size_t *region,
                                      cl_uint numEventsInWaitList,
                                      const cl_event *eventWaitList,
                                      cl_event *event) {
    TRACING_ENTER(ClEnqueueFillImage, &commandQueue, &image, &fillColor, &origin, &region, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue, pImage] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::ImageObj{image}),
        std::make_tuple(const_cast<void *>(fillColor), NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClEnqueueFillImage, &retVal);
        return retVal;
    }

    auto l0ImageImp = pImage->getL0Object(pCommandQueue->getDevice()->getRootDeviceIndex());
    auto imgInfo = l0ImageImp->getImageInfo();

    int32_t convertedFillColor[4] = {0};
    NEO::convertFillColor(fillColor, convertedFillColor, pImage->getOriginalFormat(), NEO::redescribeFillColor(imgInfo));

    auto numChannels = imgInfo.surfaceFormat->numChannels;
    auto perChannelSize = imgInfo.surfaceFormat->perChannelSizeInBytes;
    uint8_t packedFillColor[16] = {0};
    for (uint32_t ch = 0; ch < numChannels; ch++) {
        memcpy(&packedFillColor[ch * perChannelSize], &convertedFillColor[ch], perChannelSize);
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_FILL_IMAGE, pCommandQueue);

    if (pImage->getClObjectType() == CL_MEM_OBJECT_IMAGE1D_BUFFER) {
        auto elementSize = imgInfo.surfaceFormat->imageElementSizeInBytes;
        auto dstPtr = reinterpret_cast<void *>(l0ImageImp->getAllocation()->getGpuAddress() + imgInfo.offset + origin[0] * elementSize);
        auto fillSize = region[0] * elementSize;

        auto queueLock = pCommandQueue->takeOwnership();
        ze_result_t ret = zeCommandListAppendMemoryFill(pCommandQueue->getL0Handle(),
                                                        dstPtr,
                                                        packedFillColor,
                                                        elementSize,
                                                        fillSize,
                                                        hSignalEvent,
                                                        waitEvents.size(),
                                                        waitEvents.data());
        cl_int tracingRetVal = L0ToClResultMapper(ret);
        TRACING_EXIT(ClEnqueueFillImage, &tracingRetVal);
        return tracingRetVal;
    }

    auto l0Device = pCommandQueue->getDevice()->getL0Object();
    auto builtInMode = l0Device->getCompilerProductHelper().getDefaultBuiltInAddressingMode(
        NEO::ApiSpecificConfig::getBindlessMode(*l0Device->getNEODevice()));

    auto lock = l0Device->getBuiltinFunctionsLib()->obtainUniqueOwnership();
    auto *builtinKernel = l0Device->getBuiltinFunctionsLib()->getImageFunction(L0::ImageBuiltIn::fillImage3d, builtInMode);

    builtinKernel->setArgRedescribedImage(0u, pImage->getL0Handle(pCommandQueue->getDevice()->getRootDeviceIndex()), false, 0u);
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

    auto queueLock = pCommandQueue->takeOwnership();
    pImage->migrateTo(pCommandQueue->getL0Handle(), pCommandQueue->getDevice()->getRootDeviceIndex(), pCommandQueue->isOutOfOrder(), static_cast<uint32_t>(waitEvents.size()), waitEvents.data());
    ze_result_t ret = zeCommandListAppendLaunchKernel(pCommandQueue->getL0Handle(),
                                                      builtinKernel->toHandle(),
                                                      &groupCount,
                                                      hSignalEvent,
                                                      waitEvents.size(),
                                                      waitEvents.data());

    cl_int tracingRetVal = L0ToClResultMapper(ret);
    TRACING_EXIT(ClEnqueueFillImage, &tracingRetVal);
    return tracingRetVal;
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
    TRACING_ENTER(ClEnqueueCopyImage, &commandQueue, &srcImage, &dstImage, &srcOrigin, &dstOrigin, &region, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue, pSrcImage, pDstImage] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::ImageObj{srcImage}, NEO::LEO::ImageObj{dstImage}),
        std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClEnqueueCopyImage, &retVal);
        return retVal;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_COPY_IMAGE, pCommandQueue);

    auto srcMipDesc = createZeImageRegionWithMipLevel(pSrcImage, srcOrigin, region);
    auto dstMipDesc = createZeImageRegionWithMipLevel(pDstImage, dstOrigin, region);

    const auto copyImageRootDeviceIndex = pCommandQueue->getDevice()->getRootDeviceIndex();
    auto lock = pCommandQueue->takeOwnership();
    pSrcImage->migrateTo(pCommandQueue->getL0Handle(), copyImageRootDeviceIndex, pCommandQueue->isOutOfOrder(), static_cast<uint32_t>(waitEvents.size()), waitEvents.data());
    pDstImage->migrateTo(pCommandQueue->getL0Handle(), copyImageRootDeviceIndex, pCommandQueue->isOutOfOrder(), static_cast<uint32_t>(waitEvents.size()), waitEvents.data());
    ze_result_t ret = zeCommandListAppendImageCopyRegion(pCommandQueue->getL0Handle(),
                                                         pDstImage->getL0Handle(copyImageRootDeviceIndex),
                                                         pSrcImage->getL0Handle(copyImageRootDeviceIndex),
                                                         &dstMipDesc,
                                                         &srcMipDesc,
                                                         hSignalEvent,
                                                         waitEvents.size(),
                                                         waitEvents.data());
    cl_int tracingRetVal = L0ToClResultMapper(ret);
    TRACING_EXIT(ClEnqueueCopyImage, &tracingRetVal);
    return tracingRetVal;
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
    TRACING_ENTER(ClEnqueueCopyImageToBuffer, &commandQueue, &srcImage, &dstBuffer, &srcOrigin, &region, &dstOffset, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue, pSrcImage, pDstBuffer] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::ImageObj{srcImage}, NEO::LEO::BufferObj{dstBuffer}),
        std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClEnqueueCopyImageToBuffer, &retVal);
        return retVal;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_COPY_IMAGE_TO_BUFFER, pCommandQueue);

    auto mipDesc = createZeImageRegionWithMipLevel(pSrcImage, srcOrigin, region);

    auto lock = pCommandQueue->takeOwnership();
    pSrcImage->migrateTo(pCommandQueue->getL0Handle(), pCommandQueue->getDevice()->getRootDeviceIndex(), pCommandQueue->isOutOfOrder(), static_cast<uint32_t>(waitEvents.size()), waitEvents.data());
    ze_result_t ret = zeCommandListAppendImageCopyToMemory(pCommandQueue->getL0Handle(),
                                                           ptrOffset(pDstBuffer->getUsmPtr(), dstOffset),
                                                           pSrcImage->getL0Handle(pCommandQueue->getDevice()->getRootDeviceIndex()),
                                                           &mipDesc,
                                                           hSignalEvent,
                                                           waitEvents.size(),
                                                           waitEvents.data());

    cl_int tracingRetVal = L0ToClResultMapper(ret);
    TRACING_EXIT(ClEnqueueCopyImageToBuffer, &tracingRetVal);
    return tracingRetVal;
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
    TRACING_ENTER(ClEnqueueCopyBufferToImage, &commandQueue, &srcBuffer, &dstImage, &srcOffset, &dstOrigin, &region, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue, pSrcBuffer, pDstImage] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::BufferObj{srcBuffer}, NEO::LEO::ImageObj{dstImage}),
        std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClEnqueueCopyBufferToImage, &retVal);
        return retVal;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_COPY_BUFFER_TO_IMAGE, pCommandQueue);

    auto mipDesc = createZeImageRegionWithMipLevel(pDstImage, dstOrigin, region);

    auto lock = pCommandQueue->takeOwnership();
    pDstImage->migrateTo(pCommandQueue->getL0Handle(), pCommandQueue->getDevice()->getRootDeviceIndex(), pCommandQueue->isOutOfOrder(), static_cast<uint32_t>(waitEvents.size()), waitEvents.data());
    ze_result_t ret = zeCommandListAppendImageCopyFromMemory(pCommandQueue->getL0Handle(),
                                                             pDstImage->getL0Handle(pCommandQueue->getDevice()->getRootDeviceIndex()),
                                                             ptrOffset(pSrcBuffer->getUsmPtr(), srcOffset),
                                                             &mipDesc,
                                                             hSignalEvent,
                                                             waitEvents.size(),
                                                             waitEvents.data());

    cl_int tracingRetVal = L0ToClResultMapper(ret);
    TRACING_EXIT(ClEnqueueCopyBufferToImage, &tracingRetVal);
    return tracingRetVal;
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
    TRACING_ENTER(ClEnqueueMapBuffer, &commandQueue, &buffer, &blockingMap, &mapFlags, &offset, &cb, &numEventsInWaitList, &eventWaitList, &event, &errcodeRet);
    ErrorCodeHelper errcodeHelper(errcodeRet, CL_SUCCESS);

    auto [retVal, pCommandQueue, pBuffer] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::BufferObj{buffer}),
        std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        errcodeHelper.set(retVal);
        void *tracingRetVal = nullptr;
        TRACING_EXIT(ClEnqueueMapBuffer, &tracingRetVal);
        return tracingRetVal;
    }

    if (pBuffer->mapMemObjFlagsInvalid(mapFlags)) [[unlikely]] {
        errcodeHelper.set(CL_INVALID_OPERATION);
        void *tracingRetVal = nullptr;
        TRACING_EXIT(ClEnqueueMapBuffer, &tracingRetVal);
        return tracingRetVal;
    }

    if (pBuffer->getCpuPtr() != pBuffer->getUsmPtr()) {
        if (!pBuffer->getCpuPtr()) {
            errcodeHelper.set(pBuffer->createMapAllocation());
            if (errcodeHelper.localErrcode != CL_SUCCESS) {
                void *tracingRetVal = nullptr;
                TRACING_EXIT(ClEnqueueMapBuffer, &tracingRetVal);
                return tracingRetVal;
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
        void *tracingRetVal = nullptr;
        TRACING_EXIT(ClEnqueueMapBuffer, &tracingRetVal);
        return tracingRetVal;
    }

    NEO::LEO::MemObjSizeArray sizes{cb, 0, 0};
    NEO::LEO::MemObjOffsetArray offsets{offset, 0, 0};
    if (!pBuffer->getMapOperationsHandler().add(ptrOffset(pBuffer->getCpuPtr(), offset), cb, mapFlags, sizes, offsets)) {
        errcodeHelper.set(CL_OUT_OF_HOST_MEMORY);
        void *tracingRetVal = nullptr;
        TRACING_EXIT(ClEnqueueMapBuffer, &tracingRetVal);
        return tracingRetVal;
    }

    if (blockingMap) {
        errcodeHelper.set(clFinish(commandQueue));
    }

    void *tracingRetVal = ptrOffset(pBuffer->getCpuPtr(), offset);
    TRACING_EXIT(ClEnqueueMapBuffer, &tracingRetVal);
    return tracingRetVal;
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
    TRACING_ENTER(ClEnqueueMapImage, &commandQueue, &image, &blockingMap, &mapFlags, &origin, &region, &imageRowPitch, &imageSlicePitch, &numEventsInWaitList, &eventWaitList, &event, &errcodeRet);
    ErrorCodeHelper errcodeHelper(errcodeRet, CL_SUCCESS);

    auto [retVal, pCommandQueue, pImage] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, NEO::LEO::ImageObj{image}),
        std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        errcodeHelper.set(retVal);
        void *tracingRetVal = nullptr;
        TRACING_EXIT(ClEnqueueMapImage, &tracingRetVal);
        return tracingRetVal;
    }

    if (pImage->mapMemObjFlagsInvalid(mapFlags)) [[unlikely]] {
        errcodeHelper.set(CL_INVALID_OPERATION);
        void *tracingRetVal = nullptr;
        TRACING_EXIT(ClEnqueueMapImage, &tracingRetVal);
        return tracingRetVal;
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
            void *tracingRetVal = nullptr;
            TRACING_EXIT(ClEnqueueMapImage, &tracingRetVal);
            return tracingRetVal;
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
            pImage->migrateTo(pCommandQueue->getL0Handle(), pCommandQueue->getDevice()->getRootDeviceIndex(), pCommandQueue->isOutOfOrder(), static_cast<uint32_t>(waitEvents.size()), waitEvents.data());
            errcodeHelper.set(L0ToClResultMapper(zeCommandListAppendImageCopyToMemoryExt(pCommandQueue->getL0Handle(),
                                                                                         ptrOffset(pImage->getCpuPtr(), offset),
                                                                                         pImage->getL0Handle(pCommandQueue->getDevice()->getRootDeviceIndex()),
                                                                                         &mipDesc,
                                                                                         getL0ImageRowPitch(pImage->getClObjectType(), mapRowPitch, mapSlicePitch),
                                                                                         mapSlicePitch,
                                                                                         hSignalEvent,
                                                                                         waitEvents.size(),
                                                                                         waitEvents.data())));
        }
        if (blockingMap && errcodeHelper.localErrcode == CL_SUCCESS) {
            errcodeHelper.set(L0ToClResultMapper(pCommandQueue->hostSynchronize(std::numeric_limits<uint64_t>::max())));
        }
    }
    if (errcodeHelper.localErrcode != CL_SUCCESS) {
        void *tracingRetVal = nullptr;
        TRACING_EXIT(ClEnqueueMapImage, &tracingRetVal);
        return tracingRetVal;
    }

    if (!pImage->getMapOperationsHandler().add(ptrOffset(pImage->getCpuPtr(), offset), size, mapFlags, sizes, offsets, getOclMipLevel(pImage, origin))) {
        errcodeHelper.set(CL_OUT_OF_HOST_MEMORY);
        void *tracingRetVal = nullptr;
        TRACING_EXIT(ClEnqueueMapImage, &tracingRetVal);
        return tracingRetVal;
    }

    if (imageRowPitch) {
        auto hostPtrRowPitch = pImage->getHostPtrRowPitch();
        *imageRowPitch = hostPtrRowPitch != 0 ? hostPtrRowPitch : static_cast<L0::ImageImp *>(L0::Image::fromHandle(pImage->getL0Handle()))->getImageInfo().rowPitch;
    }

    if (imageSlicePitch) {
        auto hostPtrSlicePitch = pImage->getHostPtrSlicePitch();
        *imageSlicePitch = hostPtrSlicePitch != 0 ? hostPtrSlicePitch : static_cast<L0::ImageImp *>(L0::Image::fromHandle(pImage->getL0Handle()))->getImageInfo().slicePitch;
    }

    void *tracingRetVal = ptrOffset(pImage->getCpuPtr(), offset);
    TRACING_EXIT(ClEnqueueMapImage, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clEnqueueUnmapMemObject(cl_command_queue commandQueue,
                                           cl_mem memObj,
                                           void *mappedPtr,
                                           cl_uint numEventsInWaitList,
                                           const cl_event *eventWaitList,
                                           cl_event *event) {
    TRACING_ENTER(ClEnqueueUnmapMemObject, &commandQueue, &memObj, &mappedPtr, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue, pMemObj] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue, memObj),
        std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClEnqueueUnmapMemObject, &retVal);
        return retVal;
    }

    NEO::LEO::MapInfo mapInfo{};
    pMemObj->getMapOperationsHandler().find(mappedPtr, mapInfo);
    pMemObj->getMapOperationsHandler().remove(mappedPtr);

    if (pMemObj->isImage()) {
        // Validate and cast to Image type
        auto pImage = static_cast<NEO::LEO::Image *>(pMemObj);
        if (!pImage) [[unlikely]] {
            cl_int tracingRetVal = CL_INVALID_MEM_OBJECT;
            TRACING_EXIT(ClEnqueueUnmapMemObject, &tracingRetVal);
            return tracingRetVal;
        }

        if (mapInfo.readOnly) {
            cl_int tracingRetVal = clEnqueueMarkerWithWaitList(commandQueue, numEventsInWaitList, eventWaitList, event);
            TRACING_EXIT(ClEnqueueUnmapMemObject, &tracingRetVal);
            return tracingRetVal;
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

        cl_int tracingRetVal = clEnqueueWriteImage(commandQueue,
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
        TRACING_EXIT(ClEnqueueUnmapMemObject, &tracingRetVal);
        return tracingRetVal;
    } else {
        // Validate and cast to Buffer type
        auto pBuffer = static_cast<NEO::LEO::Buffer *>(pMemObj);
        if (!pBuffer) [[unlikely]] {
            cl_int tracingRetVal = CL_INVALID_MEM_OBJECT;
            TRACING_EXIT(ClEnqueueUnmapMemObject, &tracingRetVal);
            return tracingRetVal;
        }

        if (pBuffer->getCpuPtr() != pBuffer->getUsmPtr() && !mapInfo.readOnly) {
            auto mappedOffset = mapInfo.offset[0];
            auto mappedSize = mapInfo.ptrLength;
            cl_int tracingRetVal = clEnqueueWriteBuffer(commandQueue, memObj, false, mappedOffset, mappedSize, mappedPtr, numEventsInWaitList, eventWaitList, event);
            TRACING_EXIT(ClEnqueueUnmapMemObject, &tracingRetVal);
            return tracingRetVal;
        } else {
            cl_int tracingRetVal = clEnqueueMarkerWithWaitList(commandQueue, numEventsInWaitList, eventWaitList, event);
            TRACING_EXIT(ClEnqueueUnmapMemObject, &tracingRetVal);
            return tracingRetVal;
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
    TRACING_ENTER(ClEnqueueMigrateMemObjects, &commandQueue, &numMemObjects, &memObjects, &flags, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue), std::make_tuple(NEO::LEO::MemObjList{memObjects, numMemObjects}, NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClEnqueueMigrateMemObjects, &retVal);
        return retVal;
    }

    if (numMemObjects == 0 || memObjects == nullptr) {
        cl_int tracingRetVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClEnqueueMigrateMemObjects, &tracingRetVal);
        return tracingRetVal;
    }

    for (cl_uint object = 0; object < numMemObjects; ++object) {
        auto memObject = NEO::LEO::castToObject<NEO::LEO::MemObj>(memObjects[object]);
        if (!memObject) {
            cl_int tracingRetVal = CL_INVALID_MEM_OBJECT;
            TRACING_EXIT(ClEnqueueMigrateMemObjects, &tracingRetVal);
            return tracingRetVal;
        }
    }

    const cl_mem_migration_flags allValidFlags = CL_MIGRATE_MEM_OBJECT_CONTENT_UNDEFINED | CL_MIGRATE_MEM_OBJECT_HOST;

    if ((flags & (~allValidFlags)) != 0) [[unlikely]] {
        cl_int tracingRetVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClEnqueueMigrateMemObjects, &tracingRetVal);
        return tracingRetVal;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_MIGRATE_MEM_OBJECTS, pCommandQueue);

    auto lock = pCommandQueue->takeOwnership();
    cl_int tracingRetVal = L0ToClResultMapper(zeCommandListAppendBarrier(pCommandQueue->getL0Handle(), hSignalEvent, waitEvents.size(), waitEvents.data()));
    TRACING_EXIT(ClEnqueueMigrateMemObjects, &tracingRetVal);
    return tracingRetVal;
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
    TRACING_ENTER(ClEnqueueNdRangeKernel, &commandQueue, &kernel, &workDim, &globalWorkOffset, &globalWorkSize, &localWorkSize, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue, kernel), std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClEnqueueNdRangeKernel, &retVal);
        return retVal;
    }

    auto kernelHandle = pKernel->getL0Handle(pCommandQueue->getDevice()->getRootDeviceIndex());
    ze_result_t ret = ZE_RESULT_SUCCESS;

    if (!pKernel->areAllArgsSet()) [[unlikely]] {
        cl_int tracingRetVal = CL_INVALID_KERNEL_ARGS;
        TRACING_EXIT(ClEnqueueNdRangeKernel, &tracingRetVal);
        return tracingRetVal;
    }

    if (pKernel->getExecutionType() != NEO::KernelExecutionType::defaultType ||
        pKernel->getL0Object()->getKernelDescriptor().kernelAttributes.flags.usesSyncBuffer) [[unlikely]] {
        cl_int tracingRetVal = CL_INVALID_KERNEL;
        TRACING_EXIT(ClEnqueueNdRangeKernel, &tracingRetVal);
        return tracingRetVal;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_NDRANGE_KERNEL, pCommandQueue);
    auto cmdlistHandle = pCommandQueue->getL0Handle();
    auto lock = pCommandQueue->takeOwnership();

    if (!globalWorkSize || globalWorkSize[0] == 0) {
        cl_int tracingRetVal = L0ToClResultMapper(zeCommandListAppendBarrier(cmdlistHandle, hSignalEvent, waitEvents.size(), waitEvents.data()));
        TRACING_EXIT(ClEnqueueNdRangeKernel, &tracingRetVal);
        return tracingRetVal;
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
            cl_int tracingRetVal = L0ToClResultMapper(ret);
            TRACING_EXIT(ClEnqueueNdRangeKernel, &tracingRetVal);
            return tracingRetVal;
        }
    }

    ret = zeKernelSetGroupSize(kernelHandle, lws[0], lws[1], lws[2]);
    if (ret != ZE_RESULT_SUCCESS) {
        cl_int tracingRetVal = L0ToClResultMapper(ret);
        TRACING_EXIT(ClEnqueueNdRangeKernel, &tracingRetVal);
        return tracingRetVal;
    }

    ze_group_count_t wgc{static_cast<uint32_t>(globalWorkSize[0] / lws[0]),
                         workDim > 1 ? static_cast<uint32_t>(globalWorkSize[1] / lws[1]) : 1u,
                         workDim > 2 ? static_cast<uint32_t>(globalWorkSize[2] / lws[2]) : 1u};

    for (cl_uint i = 0; i < workDim; ++i) {
        if (static_cast<uint32_t>(globalWorkSize[i]) % lws[i] != 0) [[unlikely]] {
            cl_int tracingRetVal = CL_INVALID_WORK_GROUP_SIZE;
            TRACING_EXIT(ClEnqueueNdRangeKernel, &tracingRetVal);
            return tracingRetVal;
        }
    }

    if (pCommandQueue->isPerfCountersEnabled() && event) {
        auto pEvent = NEO::LEO::castToObject<NEO::LEO::Event>(*event);
        auto perfCounterNode = pEvent->getHwPerfCounterNode();
        auto perfCounters = pCommandQueue->getPerfCounters();

        if (perfCounterNode && perfCounters) {
            auto l0Event = L0::Event::fromHandle(hSignalEvent);
            l0Event->setPerfCounterNode(perfCounterNode);
        }
    }

    const auto queueRootDeviceIndex = pCommandQueue->getDevice()->getRootDeviceIndex();
    const bool outOfOrder = pCommandQueue->isOutOfOrder();
    for (const auto &[argIndex, pImage] : pKernel->getImageArgs()) {
        pImage->migrateTo(cmdlistHandle, queueRootDeviceIndex, outOfOrder, static_cast<uint32_t>(waitEvents.size()), waitEvents.data());
    }

    ret = zeCommandListAppendLaunchKernel(cmdlistHandle, kernelHandle, &wgc, hSignalEvent, waitEvents.size(), waitEvents.data());
    cl_int tracingRetVal = L0ToClResultMapper(ret);
    TRACING_EXIT(ClEnqueueNdRangeKernel, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clEnqueueWaitForEvents(cl_command_queue commandQueue,
                                          cl_uint numEvents,
                                          const cl_event *eventList) {
    TRACING_ENTER(ClEnqueueWaitForEvents, &commandQueue, &numEvents, &eventList);
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue), std::make_tuple(NEO::LEO::EventWaitList{eventList, numEvents}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClEnqueueWaitForEvents, &retVal);
        return retVal;
    }

    if (pCommandQueue->getContext()->isTerminated()) [[unlikely]] {
        cl_int tracingRetVal = CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST;
        TRACING_EXIT(ClEnqueueWaitForEvents, &tracingRetVal);
        return tracingRetVal;
    }
    NEO::LEO::EventHandleSpan waitEvents{numEvents, eventList};
    pCommandQueue->storeDependencies(numEvents, eventList);
    auto lock = pCommandQueue->takeOwnership();
    cl_int tracingRetVal = L0ToClResultMapper(zeCommandListAppendWaitOnEvents(pCommandQueue->getL0Handle(), waitEvents.size(), waitEvents.data()));
    TRACING_EXIT(ClEnqueueWaitForEvents, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clEnqueueMarkerWithWaitList(cl_command_queue commandQueue,
                                               cl_uint numEventsInWaitList,
                                               const cl_event *eventWaitList,
                                               cl_event *event) {
    TRACING_ENTER(ClEnqueueMarkerWithWaitList, &commandQueue, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue), std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClEnqueueMarkerWithWaitList, &retVal);
        return retVal;
    }

    if ((numEventsInWaitList > 0 && !eventWaitList) ||
        (numEventsInWaitList == 0 && eventWaitList)) {
        cl_int tracingRetVal = CL_INVALID_EVENT_WAIT_LIST;
        TRACING_EXIT(ClEnqueueMarkerWithWaitList, &tracingRetVal);
        return tracingRetVal;
    }

    for (cl_uint i = 0; i < numEventsInWaitList; ++i) {
        auto pEvent = NEO::LEO::castToObject<NEO::LEO::Event>(eventWaitList[i]);

        if (!pEvent) {
            cl_int tracingRetVal = CL_INVALID_EVENT_WAIT_LIST;
            TRACING_EXIT(ClEnqueueMarkerWithWaitList, &tracingRetVal);
            return tracingRetVal;
        }

        if (pCommandQueue->getContext() != pEvent->getContext()) {
            cl_int tracingRetVal = CL_INVALID_CONTEXT;
            TRACING_EXIT(ClEnqueueMarkerWithWaitList, &tracingRetVal);
            return tracingRetVal;
        }
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_MARKER, pCommandQueue);

    auto lock = pCommandQueue->takeOwnership();
    cl_int tracingRetVal = L0ToClResultMapper(zeCommandListAppendBarrier(pCommandQueue->getL0Handle(), hSignalEvent, waitEvents.size(), waitEvents.data()));
    TRACING_EXIT(ClEnqueueMarkerWithWaitList, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clEnqueueBarrierWithWaitList(cl_command_queue commandQueue,
                                                cl_uint numEventsInWaitList,
                                                const cl_event *eventWaitList,
                                                cl_event *event) {
    TRACING_ENTER(ClEnqueueBarrierWithWaitList, &commandQueue, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue), std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClEnqueueBarrierWithWaitList, &retVal);
        return retVal;
    }

    if (eventWaitList) {
        if (std::any_of(eventWaitList, eventWaitList + numEventsInWaitList, [pCommandQueue](cl_event event) { return NEO::LEO::castToObject<NEO::LEO::Event>(event)->getContext() != pCommandQueue->getContext(); })) {
            cl_int tracingRetVal = CL_INVALID_CONTEXT;
            TRACING_EXIT(ClEnqueueBarrierWithWaitList, &tracingRetVal);
            return tracingRetVal;
        }
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_BARRIER, pCommandQueue);

    auto lock = pCommandQueue->takeOwnership();
    cl_int tracingRetVal = L0ToClResultMapper(zeCommandListAppendBarrier(pCommandQueue->getL0Handle(), hSignalEvent, waitEvents.size(), waitEvents.data()));
    TRACING_EXIT(ClEnqueueBarrierWithWaitList, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clEnqueueSVMMigrateMem(cl_command_queue commandQueue,
                                          cl_uint numSvmPointers,
                                          const void **svmPointers,
                                          const size_t *sizes,
                                          const cl_mem_migration_flags flags,
                                          cl_uint numEventsInWaitList,
                                          const cl_event *eventWaitList,
                                          cl_event *event) {
    TRACING_ENTER(ClEnqueueSvmMigrateMem, &commandQueue, &numSvmPointers, &svmPointers, &sizes, &flags, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue), std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClEnqueueSvmMigrateMem, &retVal);
        return retVal;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_SVM_MIGRATE_MEM, pCommandQueue);

    auto lock = pCommandQueue->takeOwnership();
    cl_int tracingRetVal = L0ToClResultMapper(zeCommandListAppendBarrier(pCommandQueue->getL0Handle(), hSignalEvent, waitEvents.size(), waitEvents.data()));
    TRACING_EXIT(ClEnqueueSvmMigrateMem, &tracingRetVal);
    return tracingRetVal;
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
    TRACING_ENTER(ClEnqueueSvmFree, &commandQueue, &numSvmPointers, &svmPointers, &pfnFreeFunc, &userData, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue), std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClEnqueueSvmFree, &retVal);
        return retVal;
    }

    if (numSvmPointers == 0 || svmPointers == nullptr) [[unlikely]] {
        cl_int tracingRetVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClEnqueueSvmFree, &tracingRetVal);
        return tracingRetVal;
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

    cl_int tracingRetVal = clSetEventCallback(*event, CL_COMPLETE, clEnqueueSVMFreeCallbackWrapper, clEnqueueSVMFreeUserData);
    TRACING_EXIT(ClEnqueueSvmFree, &tracingRetVal);
    return tracingRetVal;
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueMemsetINTEL(
    cl_command_queue commandQueue,
    void *dstPtr,
    cl_int value,
    size_t size,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {
    TRACING_ENTER(ClEnqueueMemsetINTEL, &commandQueue, &dstPtr, &value, &size, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue), std::make_tuple(dstPtr, NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClEnqueueMemsetINTEL, &retVal);
        return retVal;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_MEMSET_INTEL, pCommandQueue);

    auto lock = pCommandQueue->takeOwnership();
    cl_int tracingRetVal = L0ToClResultMapper(zeCommandListAppendMemoryFill(pCommandQueue->getL0Handle(),
                                                                            dstPtr,
                                                                            &value,
                                                                            sizeof(cl_int),
                                                                            size,
                                                                            hSignalEvent,
                                                                            waitEvents.size(),
                                                                            waitEvents.data()));
    TRACING_EXIT(ClEnqueueMemsetINTEL, &tracingRetVal);
    return tracingRetVal;
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
    TRACING_ENTER(ClEnqueueMemFillINTEL, &commandQueue, &dstPtr, &pattern, &patternSize, &size, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue), std::make_tuple(dstPtr, const_cast<void *>(pattern), NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClEnqueueMemFillINTEL, &retVal);
        return retVal;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_MEMFILL_INTEL, pCommandQueue);

    auto lock = pCommandQueue->takeOwnership();
    cl_int tracingRetVal = L0ToClResultMapper(zeCommandListAppendMemoryFill(pCommandQueue->getL0Handle(),
                                                                            dstPtr,
                                                                            pattern,
                                                                            patternSize,
                                                                            size,
                                                                            hSignalEvent,
                                                                            waitEvents.size(),
                                                                            waitEvents.data()));
    TRACING_EXIT(ClEnqueueMemFillINTEL, &tracingRetVal);
    return tracingRetVal;
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
    TRACING_ENTER(ClEnqueueMemcpyINTEL, &commandQueue, &blocking, &dstPtr, &srcPtr, &size, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue), std::make_tuple(dstPtr, const_cast<void *>(srcPtr), NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClEnqueueMemcpyINTEL, &retVal);
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
        ret = pCommandQueue->hostSynchronize(std::numeric_limits<uint64_t>::max());
    }

    cl_int tracingRetVal = L0ToClResultMapper(ret);
    TRACING_EXIT(ClEnqueueMemcpyINTEL, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clEnqueueTask(cl_command_queue commandQueue,
                                 cl_kernel kernel,
                                 cl_uint numEventsInWaitList,
                                 const cl_event *eventWaitList,
                                 cl_event *event) {
    TRACING_ENTER(ClEnqueueTask, &commandQueue, &kernel, &numEventsInWaitList, &eventWaitList, &event);
    size_t globalWorkOffset = 0;
    size_t globalWorkSize = 1u;
    size_t localWorkSize = 1u;
    cl_int tracingRetVal = clEnqueueNDRangeKernel(commandQueue, kernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, numEventsInWaitList, eventWaitList, event);
    TRACING_EXIT(ClEnqueueTask, &tracingRetVal);
    return tracingRetVal;
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
    TRACING_ENTER(ClEnqueueNativeKernel, &commandQueue, &userFunc, &args, &cbArgs, &numMemObjects, &memList, &argsMemLoc, &numEventsInWaitList, &eventWaitList, &event);
    cl_int tracingRetVal = CL_INVALID_OPERATION;
    TRACING_EXIT(ClEnqueueNativeKernel, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clEnqueueMarker(cl_command_queue commandQueue,
                                   cl_event *event) {
    TRACING_ENTER(ClEnqueueMarker, &commandQueue, &event);
    cl_int tracingRetVal = clEnqueueMarkerWithWaitList(commandQueue, 0, nullptr, event);
    TRACING_EXIT(ClEnqueueMarker, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clEnqueueBarrier(cl_command_queue commandQueue) {
    TRACING_ENTER(ClEnqueueBarrier, &commandQueue);
    cl_int tracingRetVal = clEnqueueBarrierWithWaitList(commandQueue, 0, nullptr, nullptr);
    TRACING_EXIT(ClEnqueueBarrier, &tracingRetVal);
    return tracingRetVal;
}

cl_int CL_API_CALL clEnqueueSVMMemcpy(cl_command_queue commandQueue,
                                      cl_bool blockingCopy,
                                      void *dstPtr,
                                      const void *srcPtr,
                                      size_t size,
                                      cl_uint numEventsInWaitList,
                                      const cl_event *eventWaitList,
                                      cl_event *event) {
    TRACING_ENTER(ClEnqueueSvmMemcpy, &commandQueue, &blockingCopy, &dstPtr, &srcPtr, &size, &numEventsInWaitList, &eventWaitList, &event);
    auto ret = clEnqueueMemcpyINTEL(commandQueue, blockingCopy, dstPtr, srcPtr, size, numEventsInWaitList, eventWaitList, event);
    if (event) {
        NEO::LEO::castToObject<NEO::LEO::Event>(*event)->updateCommandType(CL_COMMAND_SVM_MEMCPY);
    }
    TRACING_EXIT(ClEnqueueSvmMemcpy, &ret);
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
    TRACING_ENTER(ClEnqueueSvmMemFill, &commandQueue, &svmPtr, &pattern, &patternSize, &size, &numEventsInWaitList, &eventWaitList, &event);
    auto ret = clEnqueueMemFillINTEL(commandQueue, svmPtr, pattern, patternSize, size, numEventsInWaitList, eventWaitList, event);
    if (event) {
        NEO::LEO::castToObject<NEO::LEO::Event>(*event)->updateCommandType(CL_COMMAND_SVM_MEMFILL);
    }
    TRACING_EXIT(ClEnqueueSvmMemFill, &ret);
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
    TRACING_ENTER(ClEnqueueSvmMap, &commandQueue, &blockingMap, &mapFlags, &svmPtr, &size, &numEventsInWaitList, &eventWaitList, &event);
    auto ret = clEnqueueMarkerWithWaitList(commandQueue, numEventsInWaitList, eventWaitList, event);
    if (event) {
        NEO::LEO::castToObject<NEO::LEO::Event>(*event)->updateCommandType(CL_COMMAND_SVM_MAP);
    }
    if (blockingMap && ret == CL_SUCCESS) {
        ret = clFinish(commandQueue);
    }
    TRACING_EXIT(ClEnqueueSvmMap, &ret);
    return ret;
}

cl_int CL_API_CALL clEnqueueSVMUnmap(cl_command_queue commandQueue,
                                     void *svmPtr,
                                     cl_uint numEventsInWaitList,
                                     const cl_event *eventWaitList,
                                     cl_event *event) {
    TRACING_ENTER(ClEnqueueSvmUnmap, &commandQueue, &svmPtr, &numEventsInWaitList, &eventWaitList, &event);
    auto ret = clEnqueueMarkerWithWaitList(commandQueue, numEventsInWaitList, eventWaitList, event);
    if (event) {
        NEO::LEO::castToObject<NEO::LEO::Event>(*event)->updateCommandType(CL_COMMAND_SVM_UNMAP);
    }
    TRACING_EXIT(ClEnqueueSvmUnmap, &ret);
    return ret;
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueVerifyMemoryINTEL(
    cl_command_queue commandQueue,
    const void *allocationPtr,
    const void *expectedData,
    size_t sizeOfComparison,
    cl_uint comparisonMode) {
    TRACING_ENTER(ClEnqueueVerifyMemoryINTEL, &commandQueue, &allocationPtr, &expectedData, &sizeOfComparison, &comparisonMode);

    if (sizeOfComparison == 0 || expectedData == nullptr || allocationPtr == nullptr) {
        cl_int tracingRetVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClEnqueueVerifyMemoryINTEL, &tracingRetVal);
        return tracingRetVal;
    }

    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue),
        std::make_tuple());
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClEnqueueVerifyMemoryINTEL, &retVal);
        return retVal;
    }

    auto l0CmdList = pCommandQueue->getL0Object();
    auto status = l0CmdList->verifyMemory(allocationPtr, expectedData, sizeOfComparison, comparisonMode);
    cl_int tracingRetVal = status ? CL_SUCCESS : CL_INVALID_VALUE;
    TRACING_EXIT(ClEnqueueVerifyMemoryINTEL, &tracingRetVal);
    return tracingRetVal;
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueMigrateMemINTEL(
    cl_command_queue commandQueue,
    const void *ptr,
    size_t size,
    cl_mem_migration_flags flags,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {
    TRACING_ENTER(ClEnqueueMigrateMemINTEL, &commandQueue, &ptr, &size, &flags, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue), std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClEnqueueMigrateMemINTEL, &retVal);
        return retVal;
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_MIGRATEMEM_INTEL, pCommandQueue);
    auto cmdlistHandle = pCommandQueue->getL0Handle();
    auto lock = pCommandQueue->takeOwnership();

    ze_result_t ret = ZE_RESULT_SUCCESS;

    if (waitEvents.size() > 0) {
        ret = zeCommandListAppendWaitOnEvents(cmdlistHandle, waitEvents.size(), waitEvents.data());
        if (ret != ZE_RESULT_SUCCESS) [[unlikely]] {
            cl_int tracingRetVal = L0ToClResultMapper(ret);
            TRACING_EXIT(ClEnqueueMigrateMemINTEL, &tracingRetVal);
            return tracingRetVal;
        }
    }

    ret = zeCommandListAppendMemoryPrefetch(cmdlistHandle, ptr, size);
    if (ret != ZE_RESULT_SUCCESS) [[unlikely]] {
        cl_int tracingRetVal = L0ToClResultMapper(ret);
        TRACING_EXIT(ClEnqueueMigrateMemINTEL, &tracingRetVal);
        return tracingRetVal;
    }

    if (hSignalEvent) {
        ret = zeCommandListAppendSignalEvent(cmdlistHandle, hSignalEvent);
        if (ret != ZE_RESULT_SUCCESS) [[unlikely]] {
            cl_int tracingRetVal = L0ToClResultMapper(ret);
            TRACING_EXIT(ClEnqueueMigrateMemINTEL, &tracingRetVal);
            return tracingRetVal;
        }
    }

    cl_int tracingRetVal = CL_SUCCESS;
    TRACING_EXIT(ClEnqueueMigrateMemINTEL, &tracingRetVal);
    return tracingRetVal;
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueMemAdviseINTEL(
    cl_command_queue commandQueue,
    const void *ptr,
    size_t size,
    cl_mem_advice_intel advice,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {
    TRACING_ENTER(ClEnqueueMemAdviseINTEL, &commandQueue, &ptr, &size, &advice, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue), std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClEnqueueMemAdviseINTEL, &retVal);
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
            cl_int tracingRetVal = L0ToClResultMapper(ret);
            TRACING_EXIT(ClEnqueueMemAdviseINTEL, &tracingRetVal);
            return tracingRetVal;
        }
    }

    ret = zeCommandListAppendMemAdvise(cmdlistHandle, deviceHandle, ptr, size, static_cast<ze_memory_advice_t>(advice));
    if (ret != ZE_RESULT_SUCCESS) {
        cl_int tracingRetVal = L0ToClResultMapper(ret);
        TRACING_EXIT(ClEnqueueMemAdviseINTEL, &tracingRetVal);
        return tracingRetVal;
    }

    if (hSignalEvent) {
        ret = zeCommandListAppendSignalEvent(cmdlistHandle, hSignalEvent);
        if (ret != ZE_RESULT_SUCCESS) {
            cl_int tracingRetVal = L0ToClResultMapper(ret);
            TRACING_EXIT(ClEnqueueMemAdviseINTEL, &tracingRetVal);
            return tracingRetVal;
        }
    }

    cl_int tracingRetVal = CL_SUCCESS;
    TRACING_EXIT(ClEnqueueMemAdviseINTEL, &tracingRetVal);
    return tracingRetVal;
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
    TRACING_ENTER(ClEnqueueNDCountKernelINTEL, &commandQueue, &kernel, &workDim, &globalWorkOffset, &workgroupCount, &localWorkSize, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue, kernel), std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        TRACING_EXIT(ClEnqueueNDCountKernelINTEL, &retVal);
        return retVal;
    }

    auto kernelHandle = pKernel->getL0Handle(pCommandQueue->getDevice()->getRootDeviceIndex());

    if (!pKernel->areAllArgsSet()) [[unlikely]] {
        cl_int tracingRetVal = CL_INVALID_KERNEL_ARGS;
        TRACING_EXIT(ClEnqueueNDCountKernelINTEL, &tracingRetVal);
        return tracingRetVal;
    }

    if (!workgroupCount) [[unlikely]] {
        cl_int tracingRetVal = CL_INVALID_VALUE;
        TRACING_EXIT(ClEnqueueNDCountKernelINTEL, &tracingRetVal);
        return tracingRetVal;
    }

    const bool isConcurrent = pKernel->getExecutionType() == NEO::KernelExecutionType::concurrent;

    if (pKernel->getL0Object()->getKernelDescriptor().kernelAttributes.flags.usesSyncBuffer && !isConcurrent) [[unlikely]] {
        cl_int tracingRetVal = CL_INVALID_KERNEL;
        TRACING_EXIT(ClEnqueueNDCountKernelINTEL, &tracingRetVal);
        return tracingRetVal;
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
            cl_int tracingRetVal = L0ToClResultMapper(ret);
            TRACING_EXIT(ClEnqueueNDCountKernelINTEL, &tracingRetVal);
            return tracingRetVal;
        }
    }

    ret = zeKernelSetGroupSize(kernelHandle, lws[0], lws[1], lws[2]);
    if (ret != ZE_RESULT_SUCCESS) [[unlikely]] {
        cl_int tracingRetVal = L0ToClResultMapper(ret);
        TRACING_EXIT(ClEnqueueNDCountKernelINTEL, &tracingRetVal);
        return tracingRetVal;
    }

    if (isConcurrent) {
        const size_t resolvedLws[3] = {lws[0], lws[1], lws[2]};
        size_t maximalNumberOfWorkgroupsAllowed = 0;
        auto countRet = pKernel->getMaxConcurrentWorkGroupCount(workDim, resolvedLws, &maximalNumberOfWorkgroupsAllowed);
        if (countRet != CL_SUCCESS) [[unlikely]] {
            cl_int tracingRetVal = countRet;
            TRACING_EXIT(ClEnqueueNDCountKernelINTEL, &tracingRetVal);
            return tracingRetVal;
        }
        size_t requestedNumberOfWorkgroups = 1;
        for (cl_uint i = 0; i < workDim; i++) {
            requestedNumberOfWorkgroups *= workgroupCount[i];
        }
        if (requestedNumberOfWorkgroups > maximalNumberOfWorkgroupsAllowed) [[unlikely]] {
            cl_int tracingRetVal = CL_INVALID_VALUE;
            TRACING_EXIT(ClEnqueueNDCountKernelINTEL, &tracingRetVal);
            return tracingRetVal;
        }
    }

    const auto queueRootDeviceIndex = pCommandQueue->getDevice()->getRootDeviceIndex();
    const bool outOfOrder = pCommandQueue->isOutOfOrder();
    for (const auto &[argIndex, pImage] : pKernel->getImageArgs()) {
        pImage->migrateTo(cmdlistHandle, queueRootDeviceIndex, outOfOrder, static_cast<uint32_t>(waitEvents.size()), waitEvents.data());
    }

    ret = isConcurrent
              ? zeCommandListAppendLaunchCooperativeKernel(cmdlistHandle, kernelHandle, &wgc, hSignalEvent, waitEvents.size(), waitEvents.data())
              : zeCommandListAppendLaunchKernel(cmdlistHandle, kernelHandle, &wgc, hSignalEvent, waitEvents.size(), waitEvents.data());

    cl_int tracingRetVal = L0ToClResultMapper(ret);
    TRACING_EXIT(ClEnqueueNDCountKernelINTEL, &tracingRetVal);
    return tracingRetVal;
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueAcquireExternalMemObjectsKHR(
    cl_command_queue commandQueue,
    cl_uint numMemObjects,
    const cl_mem *memObjects,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {
    TRACING_ENTER(ClEnqueueAcquireExternalMemObjectsKHR, &commandQueue, &numMemObjects, &memObjects, &numEventsInWaitList, &eventWaitList, &event);
    cl_int tracingRetVal = CL_INVALID_OPERATION;
    TRACING_EXIT(ClEnqueueAcquireExternalMemObjectsKHR, &tracingRetVal);
    return tracingRetVal;
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueReleaseExternalMemObjectsKHR(
    cl_command_queue commandQueue,
    cl_uint numMemObjects,
    const cl_mem *memObjects,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {
    TRACING_ENTER(ClEnqueueReleaseExternalMemObjectsKHR, &commandQueue, &numMemObjects, &memObjects, &numEventsInWaitList, &eventWaitList, &event);
    cl_int tracingRetVal = CL_INVALID_OPERATION;
    TRACING_EXIT(ClEnqueueReleaseExternalMemObjectsKHR, &tracingRetVal);
    return tracingRetVal;
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueExternalMemObjectsKHR(
    cl_command_queue commandQueue,
    cl_uint numMemObjects,
    const cl_mem *memObjects,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {
    TRACING_ENTER(ClEnqueueExternalMemObjectsKHR, &commandQueue, &numMemObjects, &memObjects, &numEventsInWaitList, &eventWaitList, &event);
    cl_int tracingRetVal = CL_INVALID_OPERATION;
    TRACING_EXIT(ClEnqueueExternalMemObjectsKHR, &tracingRetVal);
    return tracingRetVal;
}

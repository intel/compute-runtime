/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/utilities/stackvec.h"

#include "level_zero/api/opencl/source/api/leo_api.h"
#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/event/leo_event.h"
#include "level_zero/api/opencl/source/helpers/image_enqueue_helpers.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/source/helpers/leo_cl_validators.h"
#include "level_zero/api/opencl/source/kernel/leo_kernel.h"
#include "level_zero/api/opencl/source/l0_dispatch/leo_l0_dispatch.h"
#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"
#include "level_zero/api/opencl/source/mem_obj/leo_image.h"
#include "level_zero/api/opencl/source/tracing/leo_tracing_notify.h"
#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdlist/cmdlist_memory_copy_params.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/image/internal_core_image_ext.h"
#include "level_zero/core/source/kernel/kernel.h"
#include <level_zero/ze_api.h>

#include "CL/cl.h"

#include <algorithm>

namespace NEO {
namespace LEO {

extern "C" {

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
        return tracingScope.exit(retVal);
    }

    if (pBuffer->readMemObjFlagsInvalid()) [[unlikely]] {
        return tracingScope.exit(CL_INVALID_OPERATION);
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_READ_BUFFER, pCommandQueue);

    auto lock = pCommandQueue->takeOwnership();
    ze_result_t ret = pCommandQueue->appendHostMemoryCopy(ptr,
                                                          ptrOffset(pBuffer->getUsmPtr(), offset),
                                                          cb,
                                                          hSignalEvent,
                                                          waitEvents.size(),
                                                          waitEvents.data());
    if (blockingRead && ret == ZE_RESULT_SUCCESS) {
        lock.unlock();
        ret = pCommandQueue->hostSynchronize(std::numeric_limits<uint64_t>::max());
    }

    return tracingScope.exit(L0ToClResultMapper(ret));
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
        return tracingScope.exit(retVal);
    }

    if (pBuffer->readMemObjFlagsInvalid()) [[unlikely]] {
        return tracingScope.exit(CL_INVALID_OPERATION);
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_READ_BUFFER_RECT, pCommandQueue);

    applyDefaultRectPitches(region, bufferRowPitch, bufferSlicePitch);
    applyDefaultRectPitches(region, hostRowPitch, hostSlicePitch);

    UNRECOVERABLE_IF(!NEO::LEO::rectArgsFitInUint32(bufferOrigin, region, bufferRowPitch, bufferSlicePitch));
    UNRECOVERABLE_IF(!NEO::LEO::rectArgsFitInUint32(hostOrigin, region, hostRowPitch, hostSlicePitch));

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
    if (blockingRead && ret == ZE_RESULT_SUCCESS) {
        lock.unlock();
        ret = pCommandQueue->hostSynchronize(std::numeric_limits<uint64_t>::max());
    }

    return tracingScope.exit(L0ToClResultMapper(ret));
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
        return tracingScope.exit(retVal);
    }

    if (pBuffer->writeMemObjFlagsInvalid()) [[unlikely]] {
        return tracingScope.exit(CL_INVALID_OPERATION);
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_WRITE_BUFFER, pCommandQueue);

    auto lock = pCommandQueue->takeOwnership();
    ze_result_t ret = pCommandQueue->appendHostMemoryCopy(ptrOffset(pBuffer->getUsmPtr(), offset),
                                                          ptr,
                                                          cb,
                                                          hSignalEvent,
                                                          waitEvents.size(),
                                                          waitEvents.data());
    if (blockingWrite && ret == ZE_RESULT_SUCCESS) {
        lock.unlock();
        ret = pCommandQueue->hostSynchronize(std::numeric_limits<uint64_t>::max());
    }

    return tracingScope.exit(L0ToClResultMapper(ret));
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
        return tracingScope.exit(retVal);
    }

    if (pBuffer->writeMemObjFlagsInvalid()) [[unlikely]] {
        return tracingScope.exit(CL_INVALID_OPERATION);
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_WRITE_BUFFER_RECT, pCommandQueue);

    applyDefaultRectPitches(region, bufferRowPitch, bufferSlicePitch);
    applyDefaultRectPitches(region, hostRowPitch, hostSlicePitch);

    UNRECOVERABLE_IF(!NEO::LEO::rectArgsFitInUint32(bufferOrigin, region, bufferRowPitch, bufferSlicePitch));
    UNRECOVERABLE_IF(!NEO::LEO::rectArgsFitInUint32(hostOrigin, region, hostRowPitch, hostSlicePitch));

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
    if (blockingWrite && ret == ZE_RESULT_SUCCESS) {
        lock.unlock();
        ret = pCommandQueue->hostSynchronize(std::numeric_limits<uint64_t>::max());
    }

    return tracingScope.exit(L0ToClResultMapper(ret));
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
        return tracingScope.exit(retVal);
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
    return tracingScope.exit(tracingRetVal);
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
        return tracingScope.exit(retVal);
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
    return tracingScope.exit(tracingRetVal);
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
        return tracingScope.exit(retVal);
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_COPY_BUFFER_RECT, pCommandQueue);

    applyDefaultRectPitches(region, srcRowPitch, srcSlicePitch);
    applyDefaultRectPitches(region, dstRowPitch, dstSlicePitch);

    UNRECOVERABLE_IF(!NEO::LEO::rectArgsFitInUint32(srcOrigin, region, srcRowPitch, srcSlicePitch));
    UNRECOVERABLE_IF(!NEO::LEO::rectArgsFitInUint32(dstOrigin, region, dstRowPitch, dstSlicePitch));

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

    return tracingScope.exit(L0ToClResultMapper(ret));
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
        return tracingScope.exit(retVal);
    }

    if (pImage->readMemObjFlagsInvalid()) [[unlikely]] {
        return tracingScope.exit(CL_INVALID_OPERATION);
    }

    const auto readFormat = pImage->getOriginalFormat();
    if (NEO::LEO::isPackedYuvImage(&readFormat)) {
        retVal = NEO::LEO::validateYuvOperation(origin, region);
        if (retVal != CL_SUCCESS) {
            return tracingScope.exit(retVal);
        }
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_READ_IMAGE, pCommandQueue);

    auto mipDesc = createZeImageRegionWithMipLevel(pImage, origin, region);
    auto l0Image = pImage->getL0Object();
    resolveHostPitchesForCustomPitchImage(l0Image->hasCustomPitch(), l0Image->getImageInfo().surfaceFormat->imageElementSizeInBytes, region, rowPitch, slicePitch);

    UNRECOVERABLE_IF(!NEO::LEO::fitsInUint32(rowPitch) || !NEO::LEO::fitsInUint32(slicePitch));

    auto lock = pCommandQueue->takeOwnership();
    pImage->migrateTo(pCommandQueue->getL0Handle(), pCommandQueue->getDevice()->getRootDeviceIndex(), pCommandQueue->isOutOfOrder(), static_cast<uint32_t>(waitEvents.size()), waitEvents.data());
    ze_result_t ret = pCommandQueue->appendHostImageCopyToMemory(ptr,
                                                                 pImage->getL0Handle(pCommandQueue->getDevice()->getRootDeviceIndex()),
                                                                 &mipDesc,
                                                                 getL0ImageRowPitch(pImage->getClObjectType(), rowPitch, slicePitch),
                                                                 static_cast<uint32_t>(slicePitch),
                                                                 hSignalEvent,
                                                                 waitEvents.size(),
                                                                 waitEvents.data());
    if (blockingRead && ret == ZE_RESULT_SUCCESS) {
        lock.unlock();
        ret = pCommandQueue->hostSynchronize(std::numeric_limits<uint64_t>::max());
    }

    return tracingScope.exit(L0ToClResultMapper(ret));
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
        return tracingScope.exit(retVal);
    }

    if (pImage->writeMemObjFlagsInvalid()) [[unlikely]] {
        return tracingScope.exit(CL_INVALID_OPERATION);
    }

    const auto writeFormat = pImage->getOriginalFormat();
    if (NEO::LEO::isPackedYuvImage(&writeFormat)) {
        retVal = NEO::LEO::validateYuvOperation(origin, region);
        if (retVal != CL_SUCCESS) {
            return tracingScope.exit(retVal);
        }
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_WRITE_IMAGE, pCommandQueue);

    auto mipDesc = createZeImageRegionWithMipLevel(pImage, origin, region);
    auto l0Image = pImage->getL0Object();
    resolveHostPitchesForCustomPitchImage(l0Image->hasCustomPitch(), l0Image->getImageInfo().surfaceFormat->imageElementSizeInBytes, region, inputRowPitch, inputSlicePitch);

    UNRECOVERABLE_IF(!NEO::LEO::fitsInUint32(inputRowPitch) || !NEO::LEO::fitsInUint32(inputSlicePitch));

    auto lock = pCommandQueue->takeOwnership();
    pImage->migrateTo(pCommandQueue->getL0Handle(), pCommandQueue->getDevice()->getRootDeviceIndex(), pCommandQueue->isOutOfOrder(), static_cast<uint32_t>(waitEvents.size()), waitEvents.data());
    ze_result_t ret = pCommandQueue->appendHostImageCopyFromMemory(pImage->getL0Handle(pCommandQueue->getDevice()->getRootDeviceIndex()),
                                                                   ptr,
                                                                   &mipDesc,
                                                                   getL0ImageRowPitch(pImage->getClObjectType(), inputRowPitch, inputSlicePitch),
                                                                   static_cast<uint32_t>(inputSlicePitch),
                                                                   hSignalEvent,
                                                                   waitEvents.size(),
                                                                   waitEvents.data());
    if (blockingWrite && ret == ZE_RESULT_SUCCESS) {
        lock.unlock();
        ret = pCommandQueue->hostSynchronize(std::numeric_limits<uint64_t>::max());
    }

    return tracingScope.exit(L0ToClResultMapper(ret));
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
        return tracingScope.exit(retVal);
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_FILL_IMAGE, pCommandQueue);

    auto queueLock = pCommandQueue->takeOwnership();
    if (pImage->getClObjectType() != CL_MEM_OBJECT_IMAGE1D_BUFFER) {
        pImage->migrateTo(pCommandQueue->getL0Handle(), pCommandQueue->getDevice()->getRootDeviceIndex(), pCommandQueue->isOutOfOrder(), static_cast<uint32_t>(waitEvents.size()), waitEvents.data());
    }
    ze_result_t ret = pImage->appendFill(pCommandQueue->getL0Handle(), pCommandQueue->getDevice(), fillColor, origin, region,
                                         hSignalEvent, static_cast<uint32_t>(waitEvents.size()), waitEvents.data());

    return tracingScope.exit(L0ToClResultMapper(ret));
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
        return tracingScope.exit(retVal);
    }

    retVal = NEO::LEO::validateImageCopy(pSrcImage->getOriginalFormat(), pDstImage->getOriginalFormat(), srcOrigin, dstOrigin, region);
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return tracingScope.exit(retVal);
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
    return tracingScope.exit(L0ToClResultMapper(ret));
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
        return tracingScope.exit(retVal);
    }

    const auto srcFormat = pSrcImage->getOriginalFormat();
    if (NEO::LEO::isPackedYuvImage(&srcFormat)) {
        retVal = NEO::LEO::validateYuvOperation(srcOrigin, region);
        if (retVal != CL_SUCCESS) {
            return tracingScope.exit(retVal);
        }
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_COPY_IMAGE_TO_BUFFER, pCommandQueue);

    auto mipDesc = createZeImageRegionWithMipLevel(pSrcImage, srcOrigin, region);

    auto lock = pCommandQueue->takeOwnership();
    pSrcImage->migrateTo(pCommandQueue->getL0Handle(), pCommandQueue->getDevice()->getRootDeviceIndex(), pCommandQueue->isOutOfOrder(), static_cast<uint32_t>(waitEvents.size()), waitEvents.data());
    ze_result_t ret = pCommandQueue->appendImageCopyToBuffer(ptrOffset(pDstBuffer->getUsmPtr(), dstOffset),
                                                             pSrcImage->getL0Handle(pCommandQueue->getDevice()->getRootDeviceIndex()),
                                                             &mipDesc,
                                                             hSignalEvent,
                                                             static_cast<uint32_t>(waitEvents.size()),
                                                             waitEvents.data());

    return tracingScope.exit(L0ToClResultMapper(ret));
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
        return tracingScope.exit(retVal);
    }

    const auto dstFormat = pDstImage->getOriginalFormat();
    if (NEO::LEO::isPackedYuvImage(&dstFormat)) {
        retVal = NEO::LEO::validateYuvOperation(dstOrigin, region);
        if (retVal != CL_SUCCESS) {
            return tracingScope.exit(retVal);
        }
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_COPY_BUFFER_TO_IMAGE, pCommandQueue);

    auto mipDesc = createZeImageRegionWithMipLevel(pDstImage, dstOrigin, region);

    auto lock = pCommandQueue->takeOwnership();
    pDstImage->migrateTo(pCommandQueue->getL0Handle(), pCommandQueue->getDevice()->getRootDeviceIndex(), pCommandQueue->isOutOfOrder(), static_cast<uint32_t>(waitEvents.size()), waitEvents.data());
    ze_result_t ret = pCommandQueue->appendBufferCopyToImage(pDstImage->getL0Handle(pCommandQueue->getDevice()->getRootDeviceIndex()),
                                                             ptrOffset(pSrcBuffer->getUsmPtr(), srcOffset),
                                                             &mipDesc,
                                                             hSignalEvent,
                                                             static_cast<uint32_t>(waitEvents.size()),
                                                             waitEvents.data());

    return tracingScope.exit(L0ToClResultMapper(ret));
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
        return tracingScope.exit(nullptr);
    }

    if (pBuffer->mapMemObjFlagsInvalid(mapFlags)) [[unlikely]] {
        errcodeHelper.set(CL_INVALID_OPERATION);
        return tracingScope.exit(nullptr);
    }

    if ((pBuffer->getCpuPtr() != pBuffer->getUsmPtr() && !pBuffer->isZeroCopy()) || pBuffer->getUsesSvm()) {
        if (!pBuffer->getCpuPtr()) {
            errcodeHelper.set(pBuffer->createMapAllocation());
            if (errcodeHelper.localErrcode != CL_SUCCESS) {
                return tracingScope.exit(nullptr);
            }
        }
        if (pBuffer->getUsesSvm()) {
            errcodeHelper.set(clEnqueueSVMMap(commandQueue, false, mapFlags, ptrOffset(pBuffer->getCpuPtr(), offset), cb, numEventsInWaitList, eventWaitList, event));
        } else if (NEO::isValueSet(mapFlags, CL_MAP_WRITE_INVALIDATE_REGION)) {
            errcodeHelper.set(clEnqueueMarkerWithWaitList(commandQueue, numEventsInWaitList, eventWaitList, event));
        } else {
            auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_MAP_BUFFER, pCommandQueue);

            {
                auto lock = pCommandQueue->takeOwnership();
                errcodeHelper.set(L0ToClResultMapper(pCommandQueue->appendHostMemoryCopy(ptrOffset(pBuffer->getCpuPtr(), offset),
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
        return tracingScope.exit(nullptr);
    }

    NEO::LEO::MemObjSizeArray sizes{cb, 0, 0};
    NEO::LEO::MemObjOffsetArray offsets{offset, 0, 0};
    if (!pBuffer->getMapOperationsHandler().add(ptrOffset(pBuffer->getCpuPtr(), offset), cb, mapFlags, sizes, offsets)) {
        errcodeHelper.set(CL_INVALID_OPERATION);
        return tracingScope.exit(nullptr);
    }

    if (blockingMap) {
        errcodeHelper.set(clFinish(commandQueue));
    }

    return tracingScope.exit(ptrOffset(pBuffer->getCpuPtr(), offset));
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
        return tracingScope.exit(nullptr);
    }

    if (pImage->mapMemObjFlagsInvalid(mapFlags)) [[unlikely]] {
        errcodeHelper.set(CL_INVALID_OPERATION);
        return tracingScope.exit(nullptr);
    }

    const uint32_t mipLevel = getOclMipLevel(pImage, origin);
    if (mipLevel >= std::max(pImage->getL0Object()->getImageInfo().imgDesc.numMipLevels, 1u)) [[unlikely]] {
        errcodeHelper.set(CL_INVALID_MIP_LEVEL);
        return tracingScope.exit(nullptr);
    }

    const auto mapFormat = pImage->getOriginalFormat();
    if (NEO::LEO::isPackedYuvImage(&mapFormat)) {
        retVal = NEO::LEO::validateYuvOperation(origin, region);
        if (retVal != CL_SUCCESS) {
            errcodeHelper.set(retVal);
            return tracingScope.exit(nullptr);
        }
    }
    {
        auto memObjLock = pImage->takeOwnership();
        if (!pImage->getCpuPtr()) {
            ze_relaxed_allocation_limits_exp_desc_t relaxedSizeDesc{ZE_STRUCTURE_TYPE_RELAXED_ALLOCATION_LIMITS_EXP_DESC, nullptr, ZE_RELAXED_ALLOCATION_LIMITS_EXP_FLAG_MAX_SIZE};
            ze_host_mem_alloc_desc_t hostDesc{ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC, &relaxedSizeDesc, 0};
            void *cpuPtr = nullptr;
            errcodeHelper.set(L0ToClResultMapper(zeMemAllocHost(pCommandQueue->getL0Object()->getCmdListContext(),
                                                                &hostDesc,
                                                                pImage->getHostptrSize(),
                                                                1,
                                                                &cpuPtr)));
            pImage->setCpuPtr(cpuPtr);
            if (errcodeHelper.localErrcode != CL_SUCCESS) {
                return tracingScope.exit(nullptr);
            }
        }
    }

    NEO::LEO::MemObjSizeArray sizes{region[0], region[1], region[2]};
    NEO::LEO::MemObjOffsetArray offsets{origin[0], origin[1], origin[2]};

    const auto mipLevelHostLayout = pImage->getMipLevelHostLayout(mipLevel);
    const size_t size = pImage->calculateHostPtrSizeForImage(sizes, mipLevelHostLayout);
    const size_t offset = pImage->calculateHostPtrOffsetForImage(offsets, mipLevelHostLayout);
    const size_t hostRowPitch = mipLevelHostLayout.rowPitch;
    const size_t hostSlicePitch = mipLevelHostLayout.slicePitch;

    if (NEO::isValueSet(mapFlags, CL_MAP_WRITE_INVALIDATE_REGION)) {
        errcodeHelper.set(clEnqueueMarkerWithWaitList(commandQueue, numEventsInWaitList, eventWaitList, event));
    } else {
        auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_MAP_IMAGE, pCommandQueue);

        auto mipDesc = createZeImageRegionWithMipLevel(pImage, origin, region);

        const auto mapRowPitch = static_cast<uint32_t>(hostRowPitch);
        const auto mapSlicePitch = static_cast<uint32_t>(hostSlicePitch);

        {
            auto lock = pCommandQueue->takeOwnership();
            pImage->migrateTo(pCommandQueue->getL0Handle(), pCommandQueue->getDevice()->getRootDeviceIndex(), pCommandQueue->isOutOfOrder(), static_cast<uint32_t>(waitEvents.size()), waitEvents.data());
            errcodeHelper.set(L0ToClResultMapper(pCommandQueue->appendHostImageCopyToMemory(ptrOffset(pImage->getCpuPtr(), offset),
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
        return tracingScope.exit(nullptr);
    }

    if (!pImage->getMapOperationsHandler().add(ptrOffset(pImage->getCpuPtr(), offset), size, mapFlags, sizes, offsets, mipLevel)) {
        errcodeHelper.set(CL_INVALID_OPERATION);
        return tracingScope.exit(nullptr);
    }

    if (imageRowPitch) {
        *imageRowPitch = hostRowPitch;
    }

    if (imageSlicePitch) {
        *imageSlicePitch = hostSlicePitch;
    }

    return tracingScope.exit(ptrOffset(pImage->getCpuPtr(), offset));
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
        return tracingScope.exit(retVal);
    }

    NEO::LEO::MapInfo mapInfo{};
    pMemObj->getMapOperationsHandler().find(mappedPtr, mapInfo);
    pMemObj->getMapOperationsHandler().remove(mappedPtr);

    if (pMemObj->isImage()) {
        // Validate and cast to Image type
        auto pImage = static_cast<NEO::LEO::Image *>(pMemObj);
        if (!pImage) [[unlikely]] {
            return tracingScope.exit(CL_INVALID_MEM_OBJECT);
        }

        if (mapInfo.readOnly) {
            return tracingScope.exit(clEnqueueMarkerWithWaitList(commandQueue, numEventsInWaitList, eventWaitList, event));
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

        const auto mipLevelHostLayout = pImage->getMipLevelHostLayout(mapInfo.mipLevel);
        cl_int tracingRetVal = clEnqueueWriteImage(commandQueue,
                                                   memObj,
                                                   false,
                                                   origin,
                                                   region,
                                                   mipLevelHostLayout.rowPitch,
                                                   mipLevelHostLayout.slicePitch,
                                                   mappedPtr,
                                                   numEventsInWaitList,
                                                   eventWaitList,
                                                   event);
        return tracingScope.exit(tracingRetVal);
    } else {
        // Validate and cast to Buffer type
        auto pBuffer = static_cast<NEO::LEO::Buffer *>(pMemObj);
        if (!pBuffer) [[unlikely]] {
            return tracingScope.exit(CL_INVALID_MEM_OBJECT);
        }

        if (pBuffer->getUsesSvm()) {
            return tracingScope.exit(clEnqueueSVMUnmap(commandQueue, mappedPtr, numEventsInWaitList, eventWaitList, event));
        } else if (pBuffer->getCpuPtr() != pBuffer->getUsmPtr() && !pBuffer->isZeroCopy() && !mapInfo.readOnly) {
            auto mappedOffset = mapInfo.offset[0];
            auto mappedSize = mapInfo.ptrLength;
            return tracingScope.exit(clEnqueueWriteBuffer(commandQueue, memObj, false, mappedOffset, mappedSize, mappedPtr, numEventsInWaitList, eventWaitList, event));
        } else {
            return tracingScope.exit(clEnqueueMarkerWithWaitList(commandQueue, numEventsInWaitList, eventWaitList, event));
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
        return tracingScope.exit(retVal);
    }

    if (numMemObjects == 0 || memObjects == nullptr) {
        return tracingScope.exit(CL_INVALID_VALUE);
    }

    for (cl_uint object = 0; object < numMemObjects; ++object) {
        auto memObject = NEO::LEO::castToObject<NEO::LEO::MemObj>(memObjects[object]);
        if (!memObject) {
            return tracingScope.exit(CL_INVALID_MEM_OBJECT);
        }
    }

    const cl_mem_migration_flags allValidFlags = CL_MIGRATE_MEM_OBJECT_CONTENT_UNDEFINED | CL_MIGRATE_MEM_OBJECT_HOST;

    if ((flags & (~allValidFlags)) != 0) [[unlikely]] {
        return tracingScope.exit(CL_INVALID_VALUE);
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_MIGRATE_MEM_OBJECTS, pCommandQueue);

    auto lock = pCommandQueue->takeOwnership();
    return tracingScope.exit(L0ToClResultMapper(zeCommandListAppendBarrier(pCommandQueue->getL0Handle(), hSignalEvent, waitEvents.size(), waitEvents.data())));
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
        return tracingScope.exit(retVal);
    }

    if (0u == workDim || workDim > pCommandQueue->getDevice()->getDeviceInfo().maxWorkItemDimensions) [[unlikely]] {
        return tracingScope.exit(CL_INVALID_WORK_DIMENSION);
    }

    auto kernelHandle = pKernel->getL0Handle(pCommandQueue->getDevice()->getRootDeviceIndex());
    ze_result_t ret = ZE_RESULT_SUCCESS;

    auto kernelLock = pKernel->takeOwnership();

    if (!pKernel->areAllArgsSet()) [[unlikely]] {
        return tracingScope.exit(CL_INVALID_KERNEL_ARGS);
    }

    if (pKernel->getExecutionType() != NEO::KernelExecutionType::defaultType) [[unlikely]] {
        return tracingScope.exit(CL_INVALID_KERNEL);
    }

    if (pKernel->isUsingSharedObjArgs()) {
        pKernel->resetSharedObjectsPatchAddresses();
    }

    auto cmdlistHandle = pCommandQueue->getL0Handle();

    if (!globalWorkSize || globalWorkSize[0] == 0) {
        auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_NDRANGE_KERNEL, pCommandQueue);
        kernelLock.unlock();
        auto lock = pCommandQueue->takeOwnership();
        return tracingScope.exit(L0ToClResultMapper(zeCommandListAppendBarrier(cmdlistHandle, hSignalEvent, waitEvents.size(), waitEvents.data())));
    }

    auto lock = pCommandQueue->takeOwnership();

    ze_group_count_t wgc{};
    std::optional<L0::CmdListNonUniformLaunchArgs> nonUniformLaunchArgs;
    cl_int setupRetVal = pKernel->setupDispatch(*pCommandQueue->getDevice(), workDim, globalWorkOffset, globalWorkSize, localWorkSize, wgc, &nonUniformLaunchArgs);
    if (setupRetVal != CL_SUCCESS) {
        return tracingScope.exit(setupRetVal);
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_NDRANGE_KERNEL, pCommandQueue);

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

    pKernel->addExecInfoAllocationsToResidency(cmdlistHandle, queueRootDeviceIndex);

    if (nonUniformLaunchArgs.has_value()) [[unlikely]] {
        ret = pCommandQueue->getL0Object()->appendLaunchKernelNonUniform(kernelHandle, *nonUniformLaunchArgs, hSignalEvent, static_cast<uint32_t>(waitEvents.size()), waitEvents.data(), false);
    } else {
        ret = zeCommandListAppendLaunchKernel(cmdlistHandle, kernelHandle, &wgc, hSignalEvent, waitEvents.size(), waitEvents.data());
    }
    return tracingScope.exit(L0ToClResultMapper(ret));
}

cl_int CL_API_CALL clEnqueueWaitForEvents(cl_command_queue commandQueue,
                                          cl_uint numEvents,
                                          const cl_event *eventList) {
    TRACING_ENTER(ClEnqueueWaitForEvents, &commandQueue, &numEvents, &eventList);
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue), std::make_tuple(NEO::LEO::EventWaitList{eventList, numEvents}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return tracingScope.exit(retVal);
    }

    if (pCommandQueue->getContext()->isTerminated()) [[unlikely]] {
        return tracingScope.exit(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST);
    }
    NEO::LEO::EventHandleSpan waitEvents{numEvents, eventList};
    pCommandQueue->storeDependencies(numEvents, eventList);
    auto lock = pCommandQueue->takeOwnership();
    return tracingScope.exit(L0ToClResultMapper(zeCommandListAppendWaitOnEvents(pCommandQueue->getL0Handle(), waitEvents.size(), waitEvents.data())));
}

cl_int CL_API_CALL clEnqueueMarkerWithWaitList(cl_command_queue commandQueue,
                                               cl_uint numEventsInWaitList,
                                               const cl_event *eventWaitList,
                                               cl_event *event) {
    TRACING_ENTER(ClEnqueueMarkerWithWaitList, &commandQueue, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue), std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return tracingScope.exit(retVal);
    }

    if ((numEventsInWaitList > 0 && !eventWaitList) ||
        (numEventsInWaitList == 0 && eventWaitList)) {
        return tracingScope.exit(CL_INVALID_EVENT_WAIT_LIST);
    }

    for (cl_uint i = 0; i < numEventsInWaitList; ++i) {
        auto pEvent = NEO::LEO::castToObject<NEO::LEO::Event>(eventWaitList[i]);

        if (!pEvent) {
            return tracingScope.exit(CL_INVALID_EVENT_WAIT_LIST);
        }

        if (pCommandQueue->getContext() != pEvent->getContext()) {
            return tracingScope.exit(CL_INVALID_CONTEXT);
        }
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_MARKER, pCommandQueue);

    auto lock = pCommandQueue->takeOwnership();
    return tracingScope.exit(L0ToClResultMapper(zeCommandListAppendBarrier(pCommandQueue->getL0Handle(), hSignalEvent, waitEvents.size(), waitEvents.data())));
}

cl_int CL_API_CALL clEnqueueBarrierWithWaitList(cl_command_queue commandQueue,
                                                cl_uint numEventsInWaitList,
                                                const cl_event *eventWaitList,
                                                cl_event *event) {
    TRACING_ENTER(ClEnqueueBarrierWithWaitList, &commandQueue, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue), std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return tracingScope.exit(retVal);
    }

    if (eventWaitList) {
        if (std::any_of(eventWaitList, eventWaitList + numEventsInWaitList, [pCommandQueue](cl_event event) { return NEO::LEO::castToObject<NEO::LEO::Event>(event)->getContext() != pCommandQueue->getContext(); })) {
            return tracingScope.exit(CL_INVALID_CONTEXT);
        }
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_BARRIER, pCommandQueue);

    auto lock = pCommandQueue->takeOwnership();
    return tracingScope.exit(L0ToClResultMapper(zeCommandListAppendBarrier(pCommandQueue->getL0Handle(), hSignalEvent, waitEvents.size(), waitEvents.data())));
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
        return tracingScope.exit(retVal);
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_SVM_MIGRATE_MEM, pCommandQueue);

    auto lock = pCommandQueue->takeOwnership();
    return tracingScope.exit(L0ToClResultMapper(zeCommandListAppendBarrier(pCommandQueue->getL0Handle(), hSignalEvent, waitEvents.size(), waitEvents.data())));
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
        return tracingScope.exit(retVal);
    }

    if (numSvmPointers == 0 || svmPointers == nullptr) [[unlikely]] {
        return tracingScope.exit(CL_INVALID_VALUE);
    }

    cl_event evntForCallback = nullptr;
    bool ownsEventDeletion = false;
    if (event == nullptr) {
        ownsEventDeletion = true;
        event = &evntForCallback;
    }

    retVal = clEnqueueMarkerWithWaitList(commandQueue, numEventsInWaitList, eventWaitList, event);
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return tracingScope.exit(retVal);
    }

    // Marker labels the event CL_COMMAND_MARKER; relabel here.
    NEO::LEO::castToObject<NEO::LEO::Event>(*event)->updateCommandType(CL_COMMAND_SVM_FREE);

    struct ClEnqueueSVMFreeUserData {
        cl_command_queue commandQueue = nullptr;

        void(CL_CALLBACK *pfnFreeFunc)(cl_command_queue queue,
                                       cl_uint numSvmPointers,
                                       void *svmPointers[],
                                       void *userData) = nullptr;
        void *userData = nullptr;

        StackVec<void *, 8> svmPointers{};

        bool ownsEventDeletion = false;
    };
    auto clEnqueueSVMFreeUserData = new ClEnqueueSVMFreeUserData{commandQueue, pfnFreeFunc, userData,
                                                                 StackVec<void *, 8>(svmPointers, svmPointers + numSvmPointers),
                                                                 ownsEventDeletion};

    auto clEnqueueSVMFreeCallbackWrapper = [](cl_event event, cl_int, void *userData) {
        auto clEnqueueSVMFreeUserData = static_cast<ClEnqueueSVMFreeUserData *>(userData);

        auto pEvent = NEO::LEO::castToObject<NEO::LEO::Event>(event);

        if (clEnqueueSVMFreeUserData->pfnFreeFunc) {
            clEnqueueSVMFreeUserData->pfnFreeFunc(clEnqueueSVMFreeUserData->commandQueue, static_cast<cl_uint>(clEnqueueSVMFreeUserData->svmPointers.size()), clEnqueueSVMFreeUserData->svmPointers.begin(), clEnqueueSVMFreeUserData->userData);
        } else {
            for (auto svmPointer : clEnqueueSVMFreeUserData->svmPointers) {
                clSVMFree(pEvent->getContext(), svmPointer);
            }
        }

        if (clEnqueueSVMFreeUserData->ownsEventDeletion) {
            clReleaseEvent(event);
        }

        delete clEnqueueSVMFreeUserData;
    };

    return tracingScope.exit(clSetEventCallback(*event, CL_COMPLETE, clEnqueueSVMFreeCallbackWrapper, clEnqueueSVMFreeUserData));
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
        return tracingScope.exit(retVal);
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_MEMSET_INTEL, pCommandQueue);

    auto lock = pCommandQueue->takeOwnership();
    cl_int tracingRetVal = L0ToClResultMapper(zeCommandListAppendMemoryFill(pCommandQueue->getL0Handle(),
                                                                            dstPtr,
                                                                            &value,
                                                                            1u,
                                                                            size,
                                                                            hSignalEvent,
                                                                            waitEvents.size(),
                                                                            waitEvents.data()));
    return tracingScope.exit(tracingRetVal);
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
        return tracingScope.exit(retVal);
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
    return tracingScope.exit(tracingRetVal);
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
        return tracingScope.exit(retVal);
    }

    const auto dstStart = reinterpret_cast<uintptr_t>(dstPtr);
    const auto srcStart = reinterpret_cast<uintptr_t>(srcPtr);
    if ((srcStart < dstStart + size) && (dstStart < srcStart + size)) [[unlikely]] {
        return tracingScope.exit(CL_MEM_COPY_OVERLAP);
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_MEMCPY_INTEL, pCommandQueue);

    auto lock = pCommandQueue->takeOwnership();
    ze_result_t ret = pCommandQueue->appendHostMemoryCopy(dstPtr,
                                                          srcPtr,
                                                          size,
                                                          hSignalEvent,
                                                          waitEvents.size(),
                                                          waitEvents.data());
    if (blocking && ret == ZE_RESULT_SUCCESS) {
        lock.unlock();
        ret = pCommandQueue->hostSynchronize(std::numeric_limits<uint64_t>::max());
    }

    return tracingScope.exit(L0ToClResultMapper(ret));
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
    return tracingScope.exit(clEnqueueNDRangeKernel(commandQueue, kernel, 1, &globalWorkOffset, &globalWorkSize, &localWorkSize, numEventsInWaitList, eventWaitList, event));
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
    return tracingScope.exit(CL_INVALID_OPERATION);
}

cl_int CL_API_CALL clEnqueueMarker(cl_command_queue commandQueue,
                                   cl_event *event) {
    TRACING_ENTER(ClEnqueueMarker, &commandQueue, &event);
    return tracingScope.exit(clEnqueueMarkerWithWaitList(commandQueue, 0, nullptr, event));
}

cl_int CL_API_CALL clEnqueueBarrier(cl_command_queue commandQueue) {
    TRACING_ENTER(ClEnqueueBarrier, &commandQueue);
    return tracingScope.exit(clEnqueueBarrierWithWaitList(commandQueue, 0, nullptr, nullptr));
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
    if (event && ret == CL_SUCCESS) {
        NEO::LEO::castToObject<NEO::LEO::Event>(*event)->updateCommandType(CL_COMMAND_SVM_MEMCPY);
    }
    return tracingScope.exit(ret);
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
    if (event && ret == CL_SUCCESS) {
        NEO::LEO::castToObject<NEO::LEO::Event>(*event)->updateCommandType(CL_COMMAND_SVM_MEMFILL);
    }
    return tracingScope.exit(ret);
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
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue), std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return tracingScope.exit(retVal);
    }

    auto svmAllocsManager = pCommandQueue->getContext()->getL0Object()->getDriverHandle()->getSvmAllocsManager();
    auto svmData = svmAllocsManager->getSVMAlloc(svmPtr);
    const bool deviceStorage = svmData && svmData->cpuAllocation &&
                               (svmData->gpuAllocations.getAllocationType() != NEO::AllocationType::svmZeroCopy);

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_SVM_MAP, pCommandQueue);

    auto lock = pCommandQueue->takeOwnership();
    auto cmdListHandle = pCommandQueue->getL0Handle();
    ze_result_t ret = ZE_RESULT_SUCCESS;

    if (deviceStorage) {
        auto cpuAllocation = svmData->cpuAllocation;
        auto svmBasePtr = cpuAllocation->getUnderlyingBuffer();
        const size_t svmOffset = ptrDiff(svmPtr, svmBasePtr);
        if (svmAllocsManager->getSvmMapOperation(svmPtr) == nullptr) {
            L0::CmdListMemoryCopyParams memoryCopyParams{};
            memoryCopyParams.dstAllocInfo.explicitAlloc = cpuAllocation;
            auto dstPtr = reinterpret_cast<void *>(cpuAllocation->getGpuAddress() + svmOffset);

            ret = pCommandQueue->getL0Object()->appendMemoryCopy(dstPtr, svmPtr, size, hSignalEvent,
                                                                 static_cast<uint32_t>(waitEvents.size()), waitEvents.data(),
                                                                 memoryCopyParams);
            svmAllocsManager->insertSvmMapOperation(svmPtr, size, svmBasePtr, svmOffset, mapFlags == CL_MAP_READ);
        } else {
            ret = zeCommandListAppendBarrier(cmdListHandle, hSignalEvent, waitEvents.size(), waitEvents.data());
        }
    } else {
        ret = zeCommandListAppendBarrier(cmdListHandle, hSignalEvent, waitEvents.size(), waitEvents.data());
    }

    if (event) {
        NEO::LEO::castToObject<NEO::LEO::Event>(*event)->updateCommandType(CL_COMMAND_SVM_MAP);
    }

    if (blockingMap && ret == ZE_RESULT_SUCCESS) {
        lock.unlock();
        ret = pCommandQueue->hostSynchronize(std::numeric_limits<uint64_t>::max());
    }

    return tracingScope.exit(L0ToClResultMapper(ret));
}

cl_int CL_API_CALL clEnqueueSVMUnmap(cl_command_queue commandQueue,
                                     void *svmPtr,
                                     cl_uint numEventsInWaitList,
                                     const cl_event *eventWaitList,
                                     cl_event *event) {
    TRACING_ENTER(ClEnqueueSvmUnmap, &commandQueue, &svmPtr, &numEventsInWaitList, &eventWaitList, &event);
    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(commandQueue), std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return tracingScope.exit(retVal);
    }

    auto svmAllocsManager = pCommandQueue->getContext()->getL0Object()->getDriverHandle()->getSvmAllocsManager();
    auto svmData = svmAllocsManager->getSVMAlloc(svmPtr);
    const bool deviceStorage = svmData && svmData->cpuAllocation &&
                               (svmData->gpuAllocations.getAllocationType() != NEO::AllocationType::svmZeroCopy);

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_SVM_UNMAP, pCommandQueue);

    auto lock = pCommandQueue->takeOwnership();
    auto cmdListHandle = pCommandQueue->getL0Handle();
    ze_result_t ret = ZE_RESULT_SUCCESS;

    if (deviceStorage) {
        auto mapOperation = svmAllocsManager->getSvmMapOperation(svmPtr);
        if (mapOperation && !mapOperation->readOnlyMap) {
            auto cpuAllocation = svmData->cpuAllocation;
            cpuAllocation->setAubWritable(true, NEO::GraphicsAllocation::defaultBank);
            cpuAllocation->setTbxWritable(true, NEO::GraphicsAllocation::defaultBank);
            L0::CmdListMemoryCopyParams memoryCopyParams{};
            memoryCopyParams.srcAllocInfo.explicitAlloc = cpuAllocation;
            auto srcPtr = reinterpret_cast<const void *>(cpuAllocation->getGpuAddress() + mapOperation->offset);

            ret = pCommandQueue->getL0Object()->appendMemoryCopy(svmPtr, srcPtr, mapOperation->regionSize, hSignalEvent,
                                                                 static_cast<uint32_t>(waitEvents.size()), waitEvents.data(),
                                                                 memoryCopyParams);
        } else {
            ret = zeCommandListAppendBarrier(cmdListHandle, hSignalEvent, waitEvents.size(), waitEvents.data());
        }
        if (mapOperation) {
            svmAllocsManager->removeSvmMapOperation(svmPtr);
        }
    } else {
        ret = zeCommandListAppendBarrier(cmdListHandle, hSignalEvent, waitEvents.size(), waitEvents.data());
    }

    if (event) {
        NEO::LEO::castToObject<NEO::LEO::Event>(*event)->updateCommandType(CL_COMMAND_SVM_UNMAP);
    }

    return tracingScope.exit(L0ToClResultMapper(ret));
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueVerifyMemoryINTEL(
    cl_command_queue commandQueue,
    const void *allocationPtr,
    const void *expectedData,
    size_t sizeOfComparison,
    cl_uint comparisonMode) {
    TRACING_ENTER(ClEnqueueVerifyMemoryINTEL, &commandQueue, &allocationPtr, &expectedData, &sizeOfComparison, &comparisonMode);

    if (sizeOfComparison == 0 || expectedData == nullptr || allocationPtr == nullptr) {
        return tracingScope.exit(CL_INVALID_VALUE);
    }

    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(
        std::make_tuple(commandQueue),
        std::make_tuple());
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return tracingScope.exit(retVal);
    }

    auto l0CmdList = pCommandQueue->getL0Object();
    auto status = l0CmdList->verifyMemory(allocationPtr, expectedData, sizeOfComparison, comparisonMode);
    return tracingScope.exit(status ? CL_SUCCESS : CL_INVALID_VALUE);
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
        return tracingScope.exit(retVal);
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_MIGRATEMEM_INTEL, pCommandQueue);
    auto cmdlistHandle = pCommandQueue->getL0Handle();
    auto lock = pCommandQueue->takeOwnership();

    ze_result_t ret = ZE_RESULT_SUCCESS;

    if (waitEvents.size() > 0) {
        ret = zeCommandListAppendWaitOnEvents(cmdlistHandle, waitEvents.size(), waitEvents.data());
        if (ret != ZE_RESULT_SUCCESS) [[unlikely]] {
            return tracingScope.exit(L0ToClResultMapper(ret));
        }
    }

    ret = zeCommandListAppendMemoryPrefetch(cmdlistHandle, ptr, size);
    if (ret != ZE_RESULT_SUCCESS) [[unlikely]] {
        return tracingScope.exit(L0ToClResultMapper(ret));
    }

    if (hSignalEvent) {
        ret = zeCommandListAppendSignalEvent(cmdlistHandle, hSignalEvent);
        if (ret != ZE_RESULT_SUCCESS) [[unlikely]] {
            return tracingScope.exit(L0ToClResultMapper(ret));
        }
    }

    return tracingScope.exit(CL_SUCCESS);
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
        return tracingScope.exit(retVal);
    }

    auto [waitEvents, hSignalEvent] = NEO::LEO::Event::setupEvents(numEventsInWaitList, eventWaitList, event, CL_COMMAND_MEMADVISE_INTEL, pCommandQueue);
    auto cmdlistHandle = pCommandQueue->getL0Handle();
    auto lock = pCommandQueue->takeOwnership();
    auto deviceHandle = pCommandQueue->getDevice()->getL0Handle();

    ze_result_t ret = ZE_RESULT_SUCCESS;

    if (waitEvents.size() > 0) {
        ret = zeCommandListAppendWaitOnEvents(cmdlistHandle, waitEvents.size(), waitEvents.data());
        if (ret != ZE_RESULT_SUCCESS) {
            return tracingScope.exit(L0ToClResultMapper(ret));
        }
    }

    ret = zeCommandListAppendMemAdvise(cmdlistHandle, deviceHandle, ptr, size, static_cast<ze_memory_advice_t>(advice));
    if (ret != ZE_RESULT_SUCCESS) {
        return tracingScope.exit(L0ToClResultMapper(ret));
    }

    if (hSignalEvent) {
        ret = zeCommandListAppendSignalEvent(cmdlistHandle, hSignalEvent);
        if (ret != ZE_RESULT_SUCCESS) {
            return tracingScope.exit(L0ToClResultMapper(ret));
        }
    }

    return tracingScope.exit(CL_SUCCESS);
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
        return tracingScope.exit(retVal);
    }

    auto kernelHandle = pKernel->getL0Handle(pCommandQueue->getDevice()->getRootDeviceIndex());

    auto kernelLock = pKernel->takeOwnership();

    if (!pKernel->areAllArgsSet()) [[unlikely]] {
        return tracingScope.exit(CL_INVALID_KERNEL_ARGS);
    }

    if (!workgroupCount) [[unlikely]] {
        return tracingScope.exit(CL_INVALID_VALUE);
    }

    const bool isConcurrent = pKernel->getExecutionType() == NEO::KernelExecutionType::concurrent;

    if (pKernel->isUsingSharedObjArgs()) {
        pKernel->resetSharedObjectsPatchAddresses();
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
            return tracingScope.exit(L0ToClResultMapper(ret));
        }
    }

    ret = zeKernelSetGroupSize(kernelHandle, lws[0], lws[1], lws[2]);
    if (ret != ZE_RESULT_SUCCESS) [[unlikely]] {
        return tracingScope.exit(L0ToClResultMapper(ret));
    }

    if (isConcurrent) {
        const size_t resolvedLws[3] = {lws[0], lws[1], lws[2]};
        size_t maximalNumberOfWorkgroupsAllowed = 0;
        auto countRet = pKernel->getMaxConcurrentWorkGroupCount(workDim, resolvedLws, &maximalNumberOfWorkgroupsAllowed);
        if (countRet != CL_SUCCESS) [[unlikely]] {
            return tracingScope.exit(countRet);
        }
        size_t requestedNumberOfWorkgroups = 1;
        for (cl_uint i = 0; i < workDim; i++) {
            requestedNumberOfWorkgroups *= workgroupCount[i];
        }
        if (requestedNumberOfWorkgroups > maximalNumberOfWorkgroupsAllowed) [[unlikely]] {
            return tracingScope.exit(CL_INVALID_VALUE);
        }
    }

    const auto queueRootDeviceIndex = pCommandQueue->getDevice()->getRootDeviceIndex();
    const bool outOfOrder = pCommandQueue->isOutOfOrder();
    for (const auto &[argIndex, pImage] : pKernel->getImageArgs()) {
        pImage->migrateTo(cmdlistHandle, queueRootDeviceIndex, outOfOrder, static_cast<uint32_t>(waitEvents.size()), waitEvents.data());
    }

    pKernel->addExecInfoAllocationsToResidency(cmdlistHandle, queueRootDeviceIndex);

    ret = isConcurrent
              ? zeCommandListAppendLaunchCooperativeKernel(cmdlistHandle, kernelHandle, &wgc, hSignalEvent, waitEvents.size(), waitEvents.data())
              : zeCommandListAppendLaunchKernel(cmdlistHandle, kernelHandle, &wgc, hSignalEvent, waitEvents.size(), waitEvents.data());

    return tracingScope.exit(L0ToClResultMapper(ret));
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueAcquireExternalMemObjectsKHR(
    cl_command_queue commandQueue,
    cl_uint numMemObjects,
    const cl_mem *memObjects,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {
    TRACING_ENTER(ClEnqueueAcquireExternalMemObjectsKHR, &commandQueue, &numMemObjects, &memObjects, &numEventsInWaitList, &eventWaitList, &event);
    return tracingScope.exit(CL_INVALID_OPERATION);
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueReleaseExternalMemObjectsKHR(
    cl_command_queue commandQueue,
    cl_uint numMemObjects,
    const cl_mem *memObjects,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {
    TRACING_ENTER(ClEnqueueReleaseExternalMemObjectsKHR, &commandQueue, &numMemObjects, &memObjects, &numEventsInWaitList, &eventWaitList, &event);
    return tracingScope.exit(CL_INVALID_OPERATION);
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueExternalMemObjectsKHR(
    cl_command_queue commandQueue,
    cl_uint numMemObjects,
    const cl_mem *memObjects,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {
    TRACING_ENTER(ClEnqueueExternalMemObjectsKHR, &commandQueue, &numMemObjects, &memObjects, &numEventsInWaitList, &eventWaitList, &event);
    return tracingScope.exit(CL_INVALID_OPERATION);
}

} // extern "C"

} // namespace LEO
} // namespace NEO

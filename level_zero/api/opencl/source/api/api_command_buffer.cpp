/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/ptr_math.h"

#include "level_zero/api/opencl/source/api/leo_api.h"
#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/command_buffer/leo_command_buffer.h"
#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/cl_to_l0_handles.h"
#include "level_zero/api/opencl/source/helpers/l0_to_cl_return_types_mapper.h"
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/source/helpers/leo_cl_validators.h"
#include "level_zero/api/opencl/source/kernel/leo_kernel.h"
#include "level_zero/api/opencl/source/l0_dispatch/leo_l0_dispatch.h"
#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"
#include <level_zero/ze_api.h>

#include "CL/cl.h"

#include <algorithm>
#include <initializer_list>

namespace NEO {
namespace LEO {

namespace {

// Only CL_COMMAND_BUFFER_FLAGS_KHR is defined by the extension. No flag value is supported:
// CL_COMMAND_BUFFER_SIMULTANEOUS_USE_KHR and CL_COMMAND_BUFFER_MUTABLE_KHR both belong to
// cl_khr_command_buffer_mutable_dispatch.
cl_int validateCommandBufferProperties(const cl_command_buffer_properties_khr *properties) {
    if (properties == nullptr) {
        return CL_SUCCESS;
    }

    bool foundFlags = false;
    while (*properties != 0) {
        if (*properties != CL_COMMAND_BUFFER_FLAGS_KHR) {
            return CL_INVALID_PROPERTY;
        }
        if (foundFlags) {
            return CL_INVALID_PROPERTY;
        }
        foundFlags = true;

        ++properties;
        if (*properties != 0) {
            return CL_INVALID_PROPERTY;
        }
        ++properties;
    }

    return CL_SUCCESS;
}

bool isInContext(const NEO::LEO::CommandBuffer &commandBuffer, std::initializer_list<const NEO::LEO::MemObj *> memObjs) {
    return std::all_of(memObjs.begin(), memObjs.end(), [&commandBuffer](const NEO::LEO::MemObj *memObj) { return memObj->getContext() == commandBuffer.getContext(); });
}

cl_int recordAndRetain(NEO::LEO::CommandBuffer &commandBuffer, ze_result_t appendResult, cl_sync_point_khr *syncPoint,
                       std::initializer_list<NEO::LEO::MemObj *> memObjs) {
    auto retVal = commandBuffer.recordCommand(appendResult, syncPoint);
    if (retVal == CL_SUCCESS) {
        for (auto memObj : memObjs) {
            commandBuffer.keepAlive(memObj);
        }
    }
    return retVal;
}

} // namespace

extern "C" {

CL_API_ENTRY cl_command_buffer_khr CL_API_CALL clCreateCommandBufferKHR(
    cl_uint numQueues,
    const cl_command_queue *queues,
    const cl_command_buffer_properties_khr *properties,
    cl_int *errcodeRet) {
    ErrorCodeHelper err(errcodeRet, CL_SUCCESS);

    if (false == NEO::LEO::CommandBuffer::isSupported()) {
        err.set(CL_INVALID_OPERATION);
        return nullptr;
    }

    // A single queue per command buffer. Multiple queues require cl_khr_command_buffer_multi_device.
    if ((numQueues != 1u) || (queues == nullptr)) {
        err.set(CL_INVALID_VALUE);
        return nullptr;
    }

    auto [retVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(queues[0]));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        err.set(retVal);
        return nullptr;
    }

    retVal = validateCommandBufferProperties(properties);
    if (retVal != CL_SUCCESS) [[unlikely]] {
        err.set(retVal);
        return nullptr;
    }

    auto contextHandle = pCommandQueue->getContext()->getL0ContextHandle();
    auto deviceHandle = NEO::LEO::ConvertTo::zeDeviceHandle(pCommandQueue->getDevice());

    // Recorded in order even for an out-of-order queue: a sync point can only name an earlier command,
    // so recording order satisfies every sync point without an event per command.
    ze_command_list_desc_t cmdListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC,
                                          nullptr,
                                          pCommandQueue->getL0Object()->getOrdinal(),
                                          ZE_COMMAND_LIST_FLAG_COPY_OFFLOAD_HINT | ZE_COMMAND_LIST_FLAG_IN_ORDER};

    ze_command_list_handle_t cmdListHandle = nullptr;
    auto result = zeCommandListCreate(contextHandle, deviceHandle, &cmdListDesc, &cmdListHandle);
    if (result != ZE_RESULT_SUCCESS) {
        err.set(L0ToClResultMapper(result));
        return nullptr;
    }

    // A command buffer may be enqueued again while an earlier replay is still executing. Without the
    // patch preamble each execution resets the in-order counter from the host, under the running replay.
    L0::CommandList::fromHandle(cmdListHandle)->setupPatchPreambleEnabled(true);

    return new NEO::LEO::CommandBuffer(pCommandQueue->getContext(), pCommandQueue, cmdListHandle, properties);
}

CL_API_ENTRY cl_int CL_API_CALL clFinalizeCommandBufferKHR(
    cl_command_buffer_khr commandBuffer) {
    auto [retVal, pCommandBuffer] = NEO::LEO::validateAndCast(std::make_tuple(commandBuffer));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    auto lock = pCommandBuffer->takeOwnership();
    return pCommandBuffer->finalize();
}

CL_API_ENTRY cl_int CL_API_CALL clRetainCommandBufferKHR(
    cl_command_buffer_khr commandBuffer) {
    auto [retVal, pCommandBuffer] = NEO::LEO::validateAndCast(std::make_tuple(commandBuffer));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    pCommandBuffer->incRefApi();
    return CL_SUCCESS;
}

CL_API_ENTRY cl_int CL_API_CALL clReleaseCommandBufferKHR(
    cl_command_buffer_khr commandBuffer) {
    auto [retVal, pCommandBuffer] = NEO::LEO::validateAndCast(std::make_tuple(commandBuffer));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    pCommandBuffer->decRefApi();
    return CL_SUCCESS;
}

CL_API_ENTRY cl_int CL_API_CALL clEnqueueCommandBufferKHR(
    cl_uint numQueues,
    cl_command_queue *queues,
    cl_command_buffer_khr commandBuffer,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {
    auto [retVal, pCommandBuffer] = NEO::LEO::validateAndCast(std::make_tuple(commandBuffer),
                                                              std::make_tuple(NEO::LEO::EventWaitList{eventWaitList, numEventsInWaitList}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    // A single queue per replay. Multiple queues require cl_khr_command_buffer_multi_device.
    if (numQueues > 1u) {
        return CL_INVALID_VALUE;
    }
    // numQueues and queues have to agree: either both are empty or both are given.
    if ((numQueues == 0u) != (queues == nullptr)) {
        return CL_INVALID_VALUE;
    }

    // With no queue given the buffer replays onto the queue it was created with. A given queue may
    // be a different one, as long as it is on the same device and in the same context.
    auto pTargetQueue = pCommandBuffer->getCommandQueue();
    if (numQueues == 1u) {
        auto [queueRetVal, pCommandQueue] = NEO::LEO::validateAndCast(std::make_tuple(queues[0]));
        if (queueRetVal != CL_SUCCESS) [[unlikely]] {
            return queueRetVal;
        }
        if (pCommandQueue->getContext() != pTargetQueue->getContext()) {
            return CL_INVALID_CONTEXT;
        }
        if (pCommandQueue->getDevice() != pTargetQueue->getDevice()) {
            return CL_INVALID_DEVICE;
        }
        if (pCommandQueue->getL0Object()->getOrdinal() != pCommandBuffer->getOrdinal()) {
            return CL_INCOMPATIBLE_COMMAND_QUEUE_KHR;
        }
        pTargetQueue = pCommandQueue;
    }

    auto lock = pCommandBuffer->takeOwnership();
    return pCommandBuffer->enqueue(pTargetQueue, numEventsInWaitList, eventWaitList, event);
}

CL_API_ENTRY cl_int CL_API_CALL clGetCommandBufferInfoKHR(
    cl_command_buffer_khr commandBuffer,
    cl_command_buffer_info_khr paramName,
    size_t paramValueSize,
    void *paramValue,
    size_t *paramValueSizeRet) {
    auto [retVal, pCommandBuffer] = NEO::LEO::validateAndCast(std::make_tuple(commandBuffer));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    auto lock = pCommandBuffer->takeOwnership();
    return pCommandBuffer->getInfo(paramName, paramValueSize, paramValue, paramValueSizeRet);
}

CL_API_ENTRY cl_int CL_API_CALL clCommandBarrierWithWaitListKHR(
    cl_command_buffer_khr commandBuffer,
    cl_command_queue commandQueue,
    const cl_command_properties_khr *properties,
    cl_uint numSyncPointsInWaitList,
    const cl_sync_point_khr *syncPointWaitList,
    cl_sync_point_khr *syncPoint,
    cl_mutable_command_khr *mutableHandle) {
    auto [retVal, pCommandBuffer] = NEO::LEO::validateAndCast(std::make_tuple(commandBuffer));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    auto lock = pCommandBuffer->takeOwnership();
    retVal = pCommandBuffer->validateCommand(commandQueue, properties, numSyncPointsInWaitList, syncPointWaitList, mutableHandle);
    if (retVal != CL_SUCCESS) {
        return retVal;
    }

    return pCommandBuffer->recordCommand(zeCommandListAppendBarrier(pCommandBuffer->getL0Handle(), nullptr, 0u, nullptr), syncPoint);
}

CL_API_ENTRY cl_int CL_API_CALL clCommandCopyBufferKHR(
    cl_command_buffer_khr commandBuffer,
    cl_command_queue commandQueue,
    const cl_command_properties_khr *properties,
    cl_mem srcBuffer,
    cl_mem dstBuffer,
    size_t srcOffset,
    size_t dstOffset,
    size_t size,
    cl_uint numSyncPointsInWaitList,
    const cl_sync_point_khr *syncPointWaitList,
    cl_sync_point_khr *syncPoint,
    cl_mutable_command_khr *mutableHandle) {
    auto [retVal, pCommandBuffer, pSrcBuffer, pDstBuffer] = NEO::LEO::validateAndCast(
        std::make_tuple(commandBuffer, NEO::LEO::BufferObj{srcBuffer}, NEO::LEO::BufferObj{dstBuffer}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    auto lock = pCommandBuffer->takeOwnership();
    retVal = pCommandBuffer->validateCommand(commandQueue, properties, numSyncPointsInWaitList, syncPointWaitList, mutableHandle);
    if (retVal != CL_SUCCESS) {
        return retVal;
    }
    if (!isInContext(*pCommandBuffer, {pSrcBuffer, pDstBuffer})) {
        return CL_INVALID_CONTEXT;
    }

    auto result = zeCommandListAppendMemoryCopy(pCommandBuffer->getL0Handle(),
                                                ptrOffset(pDstBuffer->getUsmPtr(), dstOffset),
                                                ptrOffset(pSrcBuffer->getUsmPtr(), srcOffset),
                                                size, nullptr, 0u, nullptr);
    return recordAndRetain(*pCommandBuffer, result, syncPoint, {pSrcBuffer, pDstBuffer});
}

CL_API_ENTRY cl_int CL_API_CALL clCommandCopyBufferRectKHR(
    cl_command_buffer_khr commandBuffer,
    cl_command_queue commandQueue,
    const cl_command_properties_khr *properties,
    cl_mem srcBuffer,
    cl_mem dstBuffer,
    const size_t *srcOrigin,
    const size_t *dstOrigin,
    const size_t *region,
    size_t srcRowPitch,
    size_t srcSlicePitch,
    size_t dstRowPitch,
    size_t dstSlicePitch,
    cl_uint numSyncPointsInWaitList,
    const cl_sync_point_khr *syncPointWaitList,
    cl_sync_point_khr *syncPoint,
    cl_mutable_command_khr *mutableHandle) {
    auto [retVal, pCommandBuffer, pSrcBuffer, pDstBuffer] = NEO::LEO::validateAndCast(
        std::make_tuple(commandBuffer, NEO::LEO::BufferObj{srcBuffer}, NEO::LEO::BufferObj{dstBuffer}));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    auto lock = pCommandBuffer->takeOwnership();
    retVal = pCommandBuffer->validateCommand(commandQueue, properties, numSyncPointsInWaitList, syncPointWaitList, mutableHandle);
    if (retVal != CL_SUCCESS) {
        return retVal;
    }
    if (!isInContext(*pCommandBuffer, {pSrcBuffer, pDstBuffer})) {
        return CL_INVALID_CONTEXT;
    }

    NEO::LEO::applyDefaultRectPitches(region, srcRowPitch, srcSlicePitch);
    NEO::LEO::applyDefaultRectPitches(region, dstRowPitch, dstSlicePitch);

    UNRECOVERABLE_IF(!NEO::LEO::rectArgsFitInUint32(srcOrigin, region, srcRowPitch, srcSlicePitch));
    UNRECOVERABLE_IF(!NEO::LEO::rectArgsFitInUint32(dstOrigin, region, dstRowPitch, dstSlicePitch));

    ze_copy_region_t l0DstRegion{static_cast<uint32_t>(dstOrigin[0]), static_cast<uint32_t>(dstOrigin[1]), static_cast<uint32_t>(dstOrigin[2]), static_cast<uint32_t>(region[0]), static_cast<uint32_t>(region[1]), static_cast<uint32_t>(region[2])};
    ze_copy_region_t l0SrcRegion{static_cast<uint32_t>(srcOrigin[0]), static_cast<uint32_t>(srcOrigin[1]), static_cast<uint32_t>(srcOrigin[2]), static_cast<uint32_t>(region[0]), static_cast<uint32_t>(region[1]), static_cast<uint32_t>(region[2])};

    auto result = zeCommandListAppendMemoryCopyRegion(pCommandBuffer->getL0Handle(),
                                                      pDstBuffer->getUsmPtr(),
                                                      &l0DstRegion,
                                                      static_cast<uint32_t>(dstRowPitch),
                                                      static_cast<uint32_t>(dstSlicePitch),
                                                      pSrcBuffer->getUsmPtr(),
                                                      &l0SrcRegion,
                                                      static_cast<uint32_t>(srcRowPitch),
                                                      static_cast<uint32_t>(srcSlicePitch),
                                                      nullptr, 0u, nullptr);
    return recordAndRetain(*pCommandBuffer, result, syncPoint, {pSrcBuffer, pDstBuffer});
}

CL_API_ENTRY cl_int CL_API_CALL clCommandFillBufferKHR(
    cl_command_buffer_khr commandBuffer,
    cl_command_queue commandQueue,
    const cl_command_properties_khr *properties,
    cl_mem buffer,
    const void *pattern,
    size_t patternSize,
    size_t offset,
    size_t size,
    cl_uint numSyncPointsInWaitList,
    const cl_sync_point_khr *syncPointWaitList,
    cl_sync_point_khr *syncPoint,
    cl_mutable_command_khr *mutableHandle) {
    auto [retVal, pCommandBuffer, pBuffer] = NEO::LEO::validateAndCast(
        std::make_tuple(commandBuffer, NEO::LEO::BufferObj{buffer}),
        std::make_tuple(const_cast<void *>(pattern)));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    auto lock = pCommandBuffer->takeOwnership();
    retVal = pCommandBuffer->validateCommand(commandQueue, properties, numSyncPointsInWaitList, syncPointWaitList, mutableHandle);
    if (retVal != CL_SUCCESS) {
        return retVal;
    }
    if (!isInContext(*pCommandBuffer, {pBuffer})) {
        return CL_INVALID_CONTEXT;
    }

    auto result = zeCommandListAppendMemoryFill(pCommandBuffer->getL0Handle(), ptrOffset(pBuffer->getUsmPtr(), offset),
                                                pattern, patternSize, size, nullptr, 0u, nullptr);
    return recordAndRetain(*pCommandBuffer, result, syncPoint, {pBuffer});
}

CL_API_ENTRY cl_int CL_API_CALL clCommandSVMMemcpyKHR(
    cl_command_buffer_khr commandBuffer,
    cl_command_queue commandQueue,
    const cl_command_properties_khr *properties,
    void *dstPtr,
    const void *srcPtr,
    size_t size,
    cl_uint numSyncPointsInWaitList,
    const cl_sync_point_khr *syncPointWaitList,
    cl_sync_point_khr *syncPoint,
    cl_mutable_command_khr *mutableHandle) {
    auto [retVal, pCommandBuffer] = NEO::LEO::validateAndCast(std::make_tuple(commandBuffer),
                                                              std::make_tuple(dstPtr, const_cast<void *>(srcPtr)));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    auto lock = pCommandBuffer->takeOwnership();
    retVal = pCommandBuffer->validateCommand(commandQueue, properties, numSyncPointsInWaitList, syncPointWaitList, mutableHandle);
    if (retVal != CL_SUCCESS) {
        return retVal;
    }

    return pCommandBuffer->recordCommand(zeCommandListAppendMemoryCopy(pCommandBuffer->getL0Handle(), dstPtr, srcPtr, size, nullptr, 0u, nullptr),
                                         syncPoint);
}

CL_API_ENTRY cl_int CL_API_CALL clCommandSVMMemFillKHR(
    cl_command_buffer_khr commandBuffer,
    cl_command_queue commandQueue,
    const cl_command_properties_khr *properties,
    void *svmPtr,
    const void *pattern,
    size_t patternSize,
    size_t size,
    cl_uint numSyncPointsInWaitList,
    const cl_sync_point_khr *syncPointWaitList,
    cl_sync_point_khr *syncPoint,
    cl_mutable_command_khr *mutableHandle) {
    auto [retVal, pCommandBuffer] = NEO::LEO::validateAndCast(std::make_tuple(commandBuffer),
                                                              std::make_tuple(svmPtr, const_cast<void *>(pattern)));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    auto lock = pCommandBuffer->takeOwnership();
    retVal = pCommandBuffer->validateCommand(commandQueue, properties, numSyncPointsInWaitList, syncPointWaitList, mutableHandle);
    if (retVal != CL_SUCCESS) {
        return retVal;
    }

    return pCommandBuffer->recordCommand(zeCommandListAppendMemoryFill(pCommandBuffer->getL0Handle(), svmPtr, pattern, patternSize, size, nullptr, 0u, nullptr),
                                         syncPoint);
}

CL_API_ENTRY cl_int CL_API_CALL clCommandNDRangeKernelKHR(
    cl_command_buffer_khr commandBuffer,
    cl_command_queue commandQueue,
    const cl_command_properties_khr *properties,
    cl_kernel kernel,
    cl_uint workDim,
    const size_t *globalWorkOffset,
    const size_t *globalWorkSize,
    const size_t *localWorkSize,
    cl_uint numSyncPointsInWaitList,
    const cl_sync_point_khr *syncPointWaitList,
    cl_sync_point_khr *syncPoint,
    cl_mutable_command_khr *mutableHandle) {
    auto [retVal, pCommandBuffer, pKernel] = NEO::LEO::validateAndCast(std::make_tuple(commandBuffer, kernel));
    if (retVal != CL_SUCCESS) [[unlikely]] {
        return retVal;
    }

    auto lock = pCommandBuffer->takeOwnership();
    retVal = pCommandBuffer->validateCommand(commandQueue, properties, numSyncPointsInWaitList, syncPointWaitList, mutableHandle);
    if (retVal != CL_SUCCESS) {
        return retVal;
    }
    if (pKernel->getContext() != pCommandBuffer->getContext()) {
        return CL_INVALID_CONTEXT;
    }

    auto pDevice = pCommandBuffer->getCommandQueue()->getDevice();
    if (0u == workDim || workDim > pDevice->getDeviceInfo().maxWorkItemDimensions) [[unlikely]] {
        return CL_INVALID_WORK_DIMENSION;
    }

    auto kernelLock = pKernel->takeOwnership();

    if (!pKernel->areAllArgsSet()) [[unlikely]] {
        return CL_INVALID_KERNEL_ARGS;
    }

    if (pKernel->getExecutionType() != NEO::KernelExecutionType::defaultType ||
        pKernel->getL0Object()->getKernelDescriptor().kernelAttributes.flags.usesSyncBuffer) [[unlikely]] {
        return CL_INVALID_KERNEL;
    }

    // Image migration and shared object patching are decided when the command is appended, so a
    // recorded command would replay the state of recording time.
    if (!pKernel->getImageArgs().empty() || pKernel->isUsingSharedObjArgs()) {
        return CL_INVALID_OPERATION;
    }

    // Required while CL_COMMAND_BUFFER_CAPABILITY_KERNEL_PRINTF_KHR is not reported.
    if (pKernel->getL0Object()->getKernelDescriptor().kernelAttributes.flags.usesPrintf) {
        return CL_INVALID_OPERATION;
    }

    ze_result_t result = ZE_RESULT_SUCCESS;
    if (!globalWorkSize || globalWorkSize[0] == 0) {
        result = zeCommandListAppendBarrier(pCommandBuffer->getL0Handle(), nullptr, 0u, nullptr);
    } else {
        ze_group_count_t wgc{};
        retVal = pKernel->setupDispatch(*pDevice, workDim, globalWorkOffset, globalWorkSize, localWorkSize, wgc);
        if (retVal != CL_SUCCESS) {
            return retVal;
        }
        result = zeCommandListAppendLaunchKernel(pCommandBuffer->getL0Handle(), pKernel->getL0Handle(pDevice->getRootDeviceIndex()),
                                                 &wgc, nullptr, 0u, nullptr);
    }

    retVal = pCommandBuffer->recordCommand(result, syncPoint);
    if (retVal == CL_SUCCESS) {
        pCommandBuffer->keepAlive(pKernel);
    }
    return retVal;
}

} // extern "C"

} // namespace LEO
} // namespace NEO

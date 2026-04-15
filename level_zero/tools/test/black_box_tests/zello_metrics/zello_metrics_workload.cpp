/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/black_box_tests/zello_metrics/zello_metrics.h"
#include "level_zero/tools/test/black_box_tests/zello_metrics/zello_metrics_util.h"

#include <cstring>

namespace zmu = ZelloMetricsUtility;

///////////////////////////////////////////////////
/////AppendMemoryCopyFromHeapToDeviceAndBackToHost
///////////////////////////////////////////////////
void AppendMemoryCopyFromHeapToDeviceAndBackToHost::initialize() {
    heapBuffer1 = new char[allocSize];
    heapBuffer2 = new char[allocSize];
    for (size_t i = 0; i < allocSize; ++i) {
        heapBuffer1[i] = static_cast<char>(i + 1);
    }

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceDesc.ordinal = 0;
    deviceDesc.flags = 0;
    deviceDesc.pNext = nullptr;

    VALIDATECALL(zeMemAllocDevice(executionCtxt->getContextHandle(0), &deviceDesc, allocSize, 1, executionCtxt->getDeviceHandle(0), &zeBuffer));
    VALIDATECALL(zeContextMakeMemoryResident(executionCtxt->getContextHandle(0), executionCtxt->getDeviceHandle(0), zeBuffer, allocSize));
}

bool AppendMemoryCopyFromHeapToDeviceAndBackToHost::appendCommands() {

    // Counter to increase complexity of the workload
    int32_t repeatCount = 3;

    while (repeatCount-- > 0) {
        // Copy from heap to device-allocated memory
        VALIDATECALL(zeCommandListAppendMemoryCopy(executionCtxt->getCommandList(0), zeBuffer, heapBuffer1, allocSize,
                                                   nullptr, 0, nullptr));

        VALIDATECALL(zeCommandListAppendBarrier(executionCtxt->getCommandList(0), nullptr, 0, nullptr));
        // Copy from device-allocated memory to heap2
        VALIDATECALL(zeCommandListAppendMemoryCopy(executionCtxt->getCommandList(0), heapBuffer2, zeBuffer, allocSize,
                                                   nullptr, 0, nullptr));
    }

    return true;
}

bool AppendMemoryCopyFromHeapToDeviceAndBackToHost::validate() {
    // Validate heap and ze buffers have the original data from heapBuffer1
    return (0 == memcmp(heapBuffer1, heapBuffer2, allocSize));
}

void AppendMemoryCopyFromHeapToDeviceAndBackToHost::finalize() {

    VALIDATECALL(zeContextEvictMemory(executionCtxt->getContextHandle(0), executionCtxt->getDeviceHandle(0), zeBuffer, allocSize));
    VALIDATECALL(zeMemFree(executionCtxt->getContextHandle(0), zeBuffer));
    delete[] heapBuffer1;
    delete[] heapBuffer2;
}

////////////////////////
/////CopyBufferToBuffer
////////////////////////
void CopyBufferToBuffer::initialize(void *src, void *dst, uint32_t size) {

    sourceBuffer = src;
    destinationBuffer = dst;
    allocationSize = size != 0u ? size : allocationSize;

    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED;
    deviceDesc.ordinal = 0;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    hostDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED;

    if (sourceBuffer == nullptr) {
        VALIDATECALL(zeMemAllocShared(executionCtxt->getContextHandle(0), &deviceDesc, &hostDesc,
                                      allocationSize, 1, executionCtxt->getDeviceHandle(0),
                                      &sourceBuffer));
        isSourceBufferAllocated = true;
        memset(sourceBuffer, 55, allocationSize);
    }

    if (destinationBuffer == nullptr) {
        VALIDATECALL(zeMemAllocShared(executionCtxt->getContextHandle(0), &deviceDesc, &hostDesc,
                                      allocationSize, 1, executionCtxt->getDeviceHandle(0),
                                      &destinationBuffer));
        isDestinationBufferAllocated = true;
        memset(destinationBuffer, 0, allocationSize);
    }
}

bool CopyBufferToBuffer::appendCommands() {

    LOG(zmu::LogLevel::DEBUG) << "Using zeCommandListAppendMemoryCopy" << std::endl;
    VALIDATECALL(zeCommandListAppendMemoryCopy(executionCtxt->getCommandList(0), destinationBuffer,
                                               sourceBuffer, allocationSize, nullptr, 0, nullptr));

    return true;
}

bool CopyBufferToBuffer::validate() {

    if (!isValidationEnabled) {
        return true;
    }

    // Validate.
    const bool outputValidationSuccessful = (memcmp(destinationBuffer, sourceBuffer, allocationSize) == 0);

    if (!outputValidationSuccessful) {
        // Validate
        uint8_t *srcCharBuffer = static_cast<uint8_t *>(sourceBuffer);
        uint8_t *dstCharBuffer = static_cast<uint8_t *>(destinationBuffer);
        for (size_t i = 0; i < allocationSize; i++) {
            if (srcCharBuffer[i] != dstCharBuffer[i]) {
                LOG(zmu::LogLevel::ERROR) << "srcBuffer[" << i << "] = " << static_cast<unsigned int>(srcCharBuffer[i]) << " not equal to "
                                          << "dstBuffer[" << i << "] = " << static_cast<unsigned int>(dstCharBuffer[i]) << "\n";
                break;
            }
        }
    }

    LOG(zmu::LogLevel::DEBUG) << "\nResults validation "
                              << (outputValidationSuccessful ? "PASSED" : "FAILED")
                              << std::endl;

    return outputValidationSuccessful;
}

void CopyBufferToBuffer::finalize() {

    if (isSourceBufferAllocated) {
        VALIDATECALL(zeMemFree(executionCtxt->getContextHandle(0), sourceBuffer));
    }

    if (isDestinationBufferAllocated) {
        VALIDATECALL(zeMemFree(executionCtxt->getContextHandle(0), destinationBuffer));
    }
}

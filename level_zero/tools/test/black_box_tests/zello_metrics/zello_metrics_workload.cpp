/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/test/black_box_tests/zello_metrics/zello_metrics.h"
#include "level_zero/tools/test/black_box_tests/zello_metrics/zello_metrics_util.h"

#include <cstring>
#include <fstream>
#include <memory>

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

    VALIDATECALL(zeMemAllocDevice(executionCtxt->getContextHandle(0), &deviceDesc, allocSize, allocSize, executionCtxt->getDeviceHandle(0), &zeBuffer));
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

    VALIDATECALL(zeMemFree(executionCtxt->getContextHandle(0), zeBuffer));
    delete[] heapBuffer1;
    delete[] heapBuffer2;
}

////////////////////////
/////CopyBufferToBuffer
////////////////////////
void CopyBufferToBuffer::initialize() {

    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED;
    deviceDesc.ordinal = 0;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    hostDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED;

    VALIDATECALL(zeMemAllocShared(executionCtxt->getContextHandle(0), &deviceDesc, &hostDesc,
                                  allocationSize, 1, executionCtxt->getDeviceHandle(0),
                                  &sourceBuffer));
    VALIDATECALL(zeMemAllocShared(executionCtxt->getContextHandle(0), &deviceDesc, &hostDesc,
                                  allocationSize, 1, executionCtxt->getDeviceHandle(0),
                                  &destinationBuffer));

    // Initialize memory
    memset(sourceBuffer, 55, allocationSize);
    memset(destinationBuffer, 0, allocationSize);

    std::ifstream file("copy_buffer_to_buffer.spv", std::ios::binary);

    if (file.is_open()) {
        file.seekg(0, file.end);
        auto length = file.tellg();
        file.seekg(0, file.beg);

        LOG(zmu::LogLevel::DEBUG) << "Using copy_buffer_to_buffer.spv" << std::endl;

        std::unique_ptr<char[]> spirvInput(new char[length]);
        file.read(spirvInput.get(), length);

        ze_module_desc_t moduleDesc = {};
        ze_module_build_log_handle_t buildlog;
        moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
        moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(spirvInput.get());
        moduleDesc.inputSize = length;
        moduleDesc.pBuildFlags = "";

        if (zeModuleCreate(executionCtxt->getContextHandle(0), executionCtxt->getDeviceHandle(0), &moduleDesc, &module, &buildlog) != ZE_RESULT_SUCCESS) {
            size_t szLog = 0;
            zeModuleBuildLogGetString(buildlog, &szLog, nullptr);

            char *strLog = (char *)malloc(szLog);
            zeModuleBuildLogGetString(buildlog, &szLog, strLog);
            LOG(zmu::LogLevel::DEBUG) << "Build log:" << strLog << std::endl;
            free(strLog);
        }
        VALIDATECALL(zeModuleBuildLogDestroy(buildlog));

        ze_kernel_desc_t kernelDesc = {};
        kernelDesc.pKernelName = "CopyBufferToBufferBytes";
        VALIDATECALL(zeKernelCreate(module, &kernelDesc, &kernel));
        file.close();
        executeFromSpirv = true;
    }
}

bool CopyBufferToBuffer::appendCommands() {

    if (executeFromSpirv) {
        uint32_t groupSizeX = 32u;
        uint32_t groupSizeY = 1u;
        uint32_t groupSizeZ = 1u;
        VALIDATECALL(zeKernelSuggestGroupSize(kernel, allocationSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
        VALIDATECALL(zeKernelSetGroupSize(kernel, groupSizeX, groupSizeY, groupSizeZ));

        uint32_t offset = 0;
        VALIDATECALL(zeKernelSetArgumentValue(kernel, 1, sizeof(destinationBuffer), &destinationBuffer));
        VALIDATECALL(zeKernelSetArgumentValue(kernel, 0, sizeof(sourceBuffer), &sourceBuffer));
        VALIDATECALL(zeKernelSetArgumentValue(kernel, 2, sizeof(uint32_t), &offset));
        VALIDATECALL(zeKernelSetArgumentValue(kernel, 3, sizeof(uint32_t), &offset));
        VALIDATECALL(zeKernelSetArgumentValue(kernel, 4, sizeof(uint32_t), &offset));

        ze_group_count_t dispatchTraits;
        dispatchTraits.groupCountX = allocationSize / groupSizeX;
        dispatchTraits.groupCountY = 1u;
        dispatchTraits.groupCountZ = 1u;

        VALIDATECALL(zeCommandListAppendLaunchKernel(executionCtxt->getCommandList(0), kernel, &dispatchTraits,
                                                     nullptr, 0, nullptr));
    } else {
        LOG(zmu::LogLevel::DEBUG) << "Using zeCommandListAppendMemoryCopy" << std::endl;
        VALIDATECALL(zeCommandListAppendMemoryCopy(executionCtxt->getCommandList(0), destinationBuffer,
                                                   sourceBuffer, allocationSize, nullptr, 0, nullptr));
    }

    return true;
}

bool CopyBufferToBuffer::validate() {

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

    VALIDATECALL(zeMemFree(executionCtxt->getContextHandle(0), sourceBuffer));
    VALIDATECALL(zeMemFree(executionCtxt->getContextHandle(0), destinationBuffer));

    if (kernel) {
        zeKernelDestroy(kernel);
        kernel = nullptr;
    }

    if (module) {
        zeModuleDestroy(module);
        module = nullptr;
    }
}

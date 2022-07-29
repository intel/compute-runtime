/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>

#include "zello_common.h"

#include <fstream>
#include <iostream>
#include <memory>

bool useSyncQueue = false;

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello World USM";
    verbose = isVerbose(argc, argv);
    bool aubMode = isAubMode(argc, argv);

    useSyncQueue = isSyncQueueEnabled(argc, argv);
    bool outputValidationSuccessful = false;
    // 1. Set-up
    constexpr char srcInitValue = 7;
    constexpr char dstInitValue = 3;
    constexpr size_t bytesPerThread = sizeof(char);
    uint32_t allocSize = getBufferLength(argc, argv, 4096 + 7);
    uint32_t numThreads = allocSize / bytesPerThread;
    ze_module_handle_t module;
    ze_kernel_handle_t kernel;
    ze_command_queue_handle_t cmdQueue;
    void *srcBuffer = nullptr;
    void *dstBuffer = nullptr;

    std::ifstream file("copy_buffer_to_buffer.spv", std::ios::binary);
    if (!file.is_open()) {
        std::cout << "Unable to open copy_buffer_to_buffer.spv file" << std::endl;
        std::cout << "\nZello World USM Results validation " << (outputValidationSuccessful ? "PASSED" : "FAILED") << "\n";
        return -1;
    }

    ze_context_handle_t context = nullptr;
    ze_driver_handle_t driverHandle = nullptr;
    auto devices = zelloInitContextAndGetDevices(context, driverHandle);
    auto device = devices[0];

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    printDeviceProperties(deviceProperties);

    file.seekg(0, file.end);
    auto length = file.tellg();
    file.seekg(0, file.beg);
    std::unique_ptr<char[]> spirvInput(new char[length]);
    file.read(spirvInput.get(), length);

    ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    ze_module_build_log_handle_t buildlog;
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(spirvInput.get());
    moduleDesc.inputSize = length;
    moduleDesc.pBuildFlags = "";
    if (zeModuleCreate(context, device, &moduleDesc, &module, &buildlog) != ZE_RESULT_SUCCESS) {
        size_t szLog = 0;
        zeModuleBuildLogGetString(buildlog, &szLog, nullptr);

        char *strLog = (char *)malloc(szLog);
        zeModuleBuildLogGetString(buildlog, &szLog, strLog);
        std::cout << "Build log:" << strLog << std::endl;

        free(strLog);
        SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));
        std::cout << "\nZello World Usm Results validation FAILED. Module creation error."
                  << std::endl;
        SUCCESS_OR_TERMINATE_BOOL(false);
    }
    SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));

    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "CopyBufferToBufferBytes";
    SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &kernel));

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(kernel, numThreads, 1U, 1U, &groupSizeX,
                                                  &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE_BOOL(numThreads % groupSizeX == 0);
    if (verbose) {
        std::cout << "Group size : (" << groupSizeX << ", " << groupSizeY << ", " << groupSizeZ
                  << ")" << std::endl;
    }
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernel, groupSizeX, groupSizeY, groupSizeZ));

    ze_command_queue_desc_t cmdQueueDesc = {};
    cmdQueueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    cmdQueueDesc.pNext = nullptr;
    cmdQueueDesc.flags = 0;
    selectQueueMode(cmdQueueDesc, useSyncQueue);

    cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    cmdQueueDesc.ordinal = getCommandQueueOrdinal(device);
    cmdQueueDesc.index = 0;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));

    ze_command_list_handle_t cmdList;
    SUCCESS_OR_TERMINATE(createCommandList(context, device, cmdList));

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceDesc.pNext = nullptr;
    deviceDesc.ordinal = 0;
    deviceDesc.flags = 0;

    ze_host_mem_alloc_desc_t hostDesc = {};
    hostDesc.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
    hostDesc.pNext = nullptr;
    hostDesc.flags = 0;

    SUCCESS_OR_TERMINATE(
        zeMemAllocShared(context, &deviceDesc, &hostDesc,
                         allocSize, 1, device, &srcBuffer));

    SUCCESS_OR_TERMINATE(
        zeMemAllocShared(context, &deviceDesc, &hostDesc,
                         allocSize, 1, device, &dstBuffer));

    ze_memory_allocation_properties_t memProperties = {ZE_STRUCTURE_TYPE_MEMORY_ALLOCATION_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeMemGetAllocProperties(context, srcBuffer, &memProperties, &device));

    SUCCESS_OR_TERMINATE_BOOL(memProperties.type == ZE_MEMORY_TYPE_SHARED);

    // initialize the src buffer
    memset(srcBuffer, srcInitValue, allocSize);

    // Encode run user kernel
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 0, sizeof(srcBuffer), &srcBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 1, sizeof(dstBuffer), &dstBuffer));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = numThreads / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;
    if (verbose) {
        std::cerr << "Number of groups : (" << dispatchTraits.groupCountX << ", "
                  << dispatchTraits.groupCountY << ", " << dispatchTraits.groupCountZ << ")"
                  << std::endl;
    }
    SUCCESS_OR_TERMINATE_BOOL(dispatchTraits.groupCountX * groupSizeX == allocSize);
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatchTraits,
                                                         nullptr, 0, nullptr));

    // initialize the dst buffer after appending the kernel but before executing the lists, to
    // ensure page-fault manager is correctly making resident the buffers in the GPU at
    // execution time
    memset(dstBuffer, dstInitValue, allocSize);

    // Dispatch and wait
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));

    // if using async command queue, explicit sync must be used for correctness
    if (useSyncQueue == false)
        SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    // Validate input / output
    outputValidationSuccessful = true;
    uint8_t *srcCharBuffer = static_cast<uint8_t *>(srcBuffer);
    uint8_t *dstCharBuffer = static_cast<uint8_t *>(dstBuffer);
    for (size_t i = 0; i < allocSize; i++) {
        if (dstCharBuffer[i] != srcCharBuffer[i]) {
            outputValidationSuccessful = false;
            std::cout << "dstBuffer[" << i << "] = " << static_cast<unsigned int>(dstCharBuffer[i]) << " not equal to "
                      << "srcBuffer[" << i << "] = " << static_cast<unsigned int>(srcCharBuffer[i]) << "\n";
        }
        if (srcCharBuffer[i] != srcInitValue) {
            outputValidationSuccessful = false;
            std::cout << "srcBuffer[" << i << "] = " << static_cast<unsigned int>(srcCharBuffer[i]) << " not equal to "
                      << "value = " << static_cast<unsigned int>(srcInitValue) << "\n";
        }
        if (dstCharBuffer[i] != srcInitValue) {
            outputValidationSuccessful = false;
            std::cout << "dstBuffer[" << i << "] = " << static_cast<unsigned int>(dstCharBuffer[i]) << " not equal to "
                      << "value = " << static_cast<unsigned int>(srcInitValue) << "\n";
        }
        if (!outputValidationSuccessful) {
            break;
        }
    }

    // Cleanup
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));

    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));
    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    printResult(aubMode, outputValidationSuccessful, blackBoxName);
    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;
    return outputValidationSuccessful ? 0 : 1;
}

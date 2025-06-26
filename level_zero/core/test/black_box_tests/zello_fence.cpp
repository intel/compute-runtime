/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>

#include "zello_common.h"
#include "zello_compile.h"

#include <cstring>
#include <iostream>

void createModule(ze_context_handle_t &context, ze_module_handle_t &module, ze_device_handle_t &device) {
    // Prepare spirV
    std::string buildLog;
    auto spirV = LevelZeroBlackBoxTests::compileToSpirV(LevelZeroBlackBoxTests::openCLKernelsSource, "", buildLog);
    LevelZeroBlackBoxTests::printBuildLog(buildLog);
    SUCCESS_OR_TERMINATE((0 == spirV.size()));

    ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    ze_module_build_log_handle_t buildlog;
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = spirV.data();
    moduleDesc.inputSize = spirV.size();
    moduleDesc.pBuildFlags = "";
    if (zeModuleCreate(context, device, &moduleDesc, &module, &buildlog) != ZE_RESULT_SUCCESS) {
        size_t szLog = 0;
        zeModuleBuildLogGetString(buildlog, &szLog, nullptr);

        char *strLog = (char *)malloc(szLog);
        zeModuleBuildLogGetString(buildlog, &szLog, strLog);
        LevelZeroBlackBoxTests::printBuildLog(strLog);

        free(strLog);
        SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));
        std::cerr << "\nZello Fence Results validation FAILED. Module creation error."
                  << std::endl;
        SUCCESS_OR_TERMINATE_BOOL(false);
    }
    SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));
}

void createKernel(ze_module_handle_t &module, ze_kernel_handle_t &kernel,
                  uint32_t numThreads, uint32_t &sizeX, uint32_t &sizeY,
                  uint32_t &sizeZ) {

    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "increment_by_one";
    SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &kernel));
    ze_kernel_properties_t kernProps{ZE_STRUCTURE_TYPE_KERNEL_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeKernelGetProperties(kernel, &kernProps));
    LevelZeroBlackBoxTests::printKernelProperties(kernProps, kernelDesc.pKernelName);

    uint32_t groupSizeX = sizeX;
    uint32_t groupSizeY = sizeY;
    uint32_t groupSizeZ = sizeZ;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(kernel, numThreads, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE_BOOL(numThreads % groupSizeX == 0);
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "Group size : (" << groupSizeX << ", " << groupSizeY << ", " << groupSizeZ
                  << ")" << std::endl;
    }
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernel, groupSizeX, groupSizeY, groupSizeZ));

    sizeX = groupSizeX;
    sizeY = groupSizeY;
    sizeZ = groupSizeZ;
}

bool testFence(ze_context_handle_t &context, ze_device_handle_t &device) {
    constexpr size_t allocSize = 4096;
    constexpr size_t bytesPerThread = sizeof(char);
    constexpr size_t numThreads = allocSize / bytesPerThread;
    ze_module_handle_t module;
    ze_kernel_handle_t kernel;
    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;

    void *srcBuffer;
    void *dstBuffer;
    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;

    // Create commandQueue and cmdList
    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.ordinal = LevelZeroBlackBoxTests::getCommandQueueOrdinal(device, false);
    cmdQueueDesc.index = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));
    SUCCESS_OR_TERMINATE(LevelZeroBlackBoxTests::createCommandList(context, device, cmdList, false));

    // Create module and kernel
    createModule(context, module, device);
    createKernel(module, kernel, numThreads, groupSizeX, groupSizeY,
                 groupSizeZ);

    // Alloc buffers
    srcBuffer = nullptr;
    dstBuffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceDesc.ordinal = 0;
    deviceDesc.flags = 0;
    deviceDesc.pNext = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, allocSize, 1, device, &srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, allocSize, 1, device, &dstBuffer));

    // Init data and copy to device
    uint8_t initDataSrc[allocSize];
    memset(initDataSrc, 7, sizeof(initDataSrc));
    uint8_t initDataDst[allocSize];
    memset(initDataDst, 3, sizeof(initDataDst));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(
        cmdList, srcBuffer, initDataSrc, sizeof(initDataSrc), nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(
        cmdList, dstBuffer, initDataDst, sizeof(initDataDst), nullptr, 0, nullptr));

    // copying of data must finish before running the user kernel
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));

    // Set kernel args and get ready to dispatch
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 0, sizeof(dstBuffer), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 1, sizeof(srcBuffer), &srcBuffer));
    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = numThreads / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;
    LevelZeroBlackBoxTests::printGroupCount(dispatchTraits);

    SUCCESS_OR_TERMINATE_BOOL(dispatchTraits.groupCountX * groupSizeX == allocSize);
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(
        cmdList, kernel, &dispatchTraits, nullptr, 0, nullptr));

    // Create Fence
    ze_fence_handle_t fence;
    ze_fence_desc_t fenceDesc = {};
    fenceDesc.stype = ZE_STRUCTURE_TYPE_FENCE_DESC;
    fenceDesc.pNext = nullptr;
    fenceDesc.flags = 0;
    SUCCESS_OR_TERMINATE(zeFenceCreate(cmdQueue, &fenceDesc, &fence));

    // Execute CommandList
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, fence));

    // Wait for fence to be signaled
    SUCCESS_OR_TERMINATE(zeFenceHostSynchronize(fence, std::numeric_limits<uint64_t>::max()));
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "zeFenceHostSynchronize success" << std::endl;
    }

    // Tear down
    SUCCESS_OR_TERMINATE(zeFenceReset(fence));
    SUCCESS_OR_TERMINATE(zeCommandListReset(cmdList));
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeFenceDestroy(fence));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));

    return true;
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello Fence";
    bool outputValidationSuccessful;
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);

    ze_context_handle_t context = nullptr;
    ze_driver_handle_t driverHandle = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context, driverHandle);
    auto device = devices[0];

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    LevelZeroBlackBoxTests::printDeviceProperties(deviceProperties);

    outputValidationSuccessful = testFence(context, device);

    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName);
    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;
    return outputValidationSuccessful ? 0 : 1;
}

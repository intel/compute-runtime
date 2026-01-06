/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"
#include "zello_compile.h"

#include <cstring>
#include <iostream>
#include <vector>

void createModule(ze_module_handle_t &module, ze_context_handle_t &context, ze_device_handle_t &device) {
    // Prepare spirV
    std::string buildLog;

    auto binaryModule = LevelZeroBlackBoxTests::compileToSpirV(LevelZeroBlackBoxTests::dynLocalBarrierArgSrc, "", buildLog);
    if (buildLog.size() > 0) {
        std::cerr << "CL->spirV compilation log : " << buildLog
                  << std::endl;
    }
    SUCCESS_OR_TERMINATE(0 == binaryModule.size());

    ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(binaryModule.data());
    moduleDesc.inputSize = binaryModule.size();

    SUCCESS_OR_TERMINATE(zeModuleCreate(context, device, &moduleDesc, &module, nullptr));
}

void createKernel(ze_module_handle_t &module, ze_kernel_handle_t &kernel, size_t numThreads, uint32_t groupSizeX, uint32_t groupSizeY, uint32_t groupSizeZ) {
    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "local_barrier_arg";
    SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &kernel));

    // Set group sizes
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(kernel, static_cast<uint32_t>(numThreads), 1U, 1U, &groupSizeX,
                                                  &groupSizeY, &groupSizeZ));
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "Group size : (" << groupSizeX << ", " << groupSizeY << ", " << groupSizeZ
                  << ")" << std::endl;
    }
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernel, groupSizeX, groupSizeY, groupSizeZ));
}

void createCmdQueueAndCmdList(ze_context_handle_t &context, ze_device_handle_t &device,
                              ze_command_queue_handle_t &cmdqueue,
                              ze_command_list_handle_t &cmdList,
                              ze_command_queue_desc_t *cmdQueueDesc,
                              ze_command_list_desc_t *cmdListDesc) {
    cmdQueueDesc->ordinal = LevelZeroBlackBoxTests::getCommandQueueOrdinal(device, false);
    cmdQueueDesc->mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, cmdQueueDesc, &cmdqueue));

    SUCCESS_OR_TERMINATE(zeCommandListCreate(context, device, cmdListDesc, &cmdList));
}

// Test Local Argument Allocated and Barrier used
bool testLocalBarrier(ze_context_handle_t &context, ze_device_handle_t &device) {
    constexpr size_t allocSize = sizeof(int);
    ze_module_handle_t module;
    ze_kernel_handle_t kernel;
    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;
    int *realResult = nullptr;
    // Expected Result is a Sum of the local dynamic buffers added into a single global
    // with the results expected to be local_dst1 = 6, local_dst2 = 7, local_dst3 = 8, local_dst4 = 9.
    // This should result in a sum of 30.
    int expectedResult = 30;
    bool outputValidationSuccess = true;
    size_t numThreadsPerGroup = 3;
    uint32_t groupSizeX = 3u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;

    void *dstBuffer;

    // Create commandQueue and cmdList
    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    ze_command_list_desc_t cmdListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    createCmdQueueAndCmdList(context, device, cmdQueue, cmdList, &cmdQueueDesc, &cmdListDesc);

    // Create module and kernel
    createModule(module, context, device);
    createKernel(module, kernel, numThreadsPerGroup, groupSizeX, groupSizeY, groupSizeZ);

    // Alloc buffers
    dstBuffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.ordinal = 0;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    SUCCESS_OR_TERMINATE(
        zeMemAllocShared(context, &deviceDesc, &hostDesc, allocSize, 1, device, &dstBuffer));

    uint8_t initDataDst[allocSize];
    memset(initDataDst, 0, sizeof(initDataDst));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, dstBuffer, initDataDst,
                                                       allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));

    realResult = reinterpret_cast<int *>(dstBuffer);
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "Initial Global Memory Value " << *realResult << std::endl;
    }

    // Set kernel args and get ready to dispatch
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 0, sizeof(int), nullptr));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 1, sizeof(dstBuffer), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 2, sizeof(int), nullptr));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 3, sizeof(unsigned long), nullptr));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 4, sizeof(int), nullptr));
    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = 3u;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;
    LevelZeroBlackBoxTests::printGroupCount(dispatchTraits);

    SUCCESS_OR_TERMINATE(
        zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatchTraits, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    realResult = reinterpret_cast<int *>(dstBuffer);
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "Final Global Memory Value " << *realResult << std::endl;
    }

    if (*realResult != expectedResult) {
        outputValidationSuccess = false;
    }

    // Tear down
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));

    return outputValidationSuccess;
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello Dyn Local Arg";
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

    outputValidationSuccessful = testLocalBarrier(context, device);

    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName);

    int resultOnFailure = aubMode ? 0 : 1;
    return outputValidationSuccessful ? 0 : resultOnFailure;
}

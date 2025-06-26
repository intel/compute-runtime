/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>

#include "zello_common.h"
#include "zello_compile.h"

#include <cstring>
#include <iostream>

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello Function Pointers CL";

    constexpr size_t allocSize = 4096;

    // 1. Setup
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

    std::string buildLog;
    auto spirV = LevelZeroBlackBoxTests::compileToSpirV(LevelZeroBlackBoxTests::functionPointersProgram, "", buildLog);
    LevelZeroBlackBoxTests::printBuildLog(buildLog);
    SUCCESS_OR_TERMINATE((0 == spirV.size()));

    ze_module_handle_t module;
    ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    ze_module_build_log_handle_t buildlog;
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = spirV.data();
    moduleDesc.inputSize = spirV.size();
    moduleDesc.pBuildFlags = "-cl-take-global-address";
    if (zeModuleCreate(context, device, &moduleDesc, &module, &buildlog) != ZE_RESULT_SUCCESS) {
        size_t szLog = 0;
        zeModuleBuildLogGetString(buildlog, &szLog, nullptr);

        char *strLog = (char *)malloc(szLog);
        zeModuleBuildLogGetString(buildlog, &szLog, strLog);
        LevelZeroBlackBoxTests::printBuildLog(strLog);

        free(strLog);
        SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));
        std::cerr << "\nZello Function Pointers CL Results validation FAILED. Module creation error."
                  << std::endl;
        return 1;
    }
    SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));

    ze_kernel_handle_t kernel;
    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "memcpy_bytes";
    SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &kernel));
    ze_kernel_properties_t kernProps = {ZE_STRUCTURE_TYPE_KERNEL_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeKernelGetProperties(kernel, &kernProps));
    std::cout << "function pointer memcpy_bytes Private size = " << std::dec << kernProps.privateMemSize << std::endl;
    std::cout << "function pointer memcpy_bytes Spill size = " << std::dec << kernProps.spillMemSize << std::endl;

    uint32_t groupSizeX = 1u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(kernel, allocSize, 1U, 1U,
                                                  &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernel, groupSizeX, groupSizeY, groupSizeZ));

    ze_command_queue_handle_t cmdQueue;
    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.ordinal = LevelZeroBlackBoxTests::getCommandQueueOrdinal(device, false);
    cmdQueueDesc.index = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));

    ze_command_list_handle_t cmdList;
    SUCCESS_OR_TERMINATE(LevelZeroBlackBoxTests::createCommandList(context, device, cmdList, false));

    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.ordinal = 0;
    deviceDesc.flags = 0;
    deviceDesc.pNext = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    void *srcBuffer = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc, allocSize, 1, device, &srcBuffer));

    void *dstBuffer = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc, allocSize, 1, device, &dstBuffer));

    // 2. Initialize memory
    uint8_t initDataSrc[allocSize];
    memset(initDataSrc, 7, sizeof(initDataSrc));
    uint8_t initDataDst[allocSize];
    memset(initDataDst, 3, sizeof(initDataDst));

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, srcBuffer, initDataSrc,
                                                       sizeof(initDataSrc), nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, dstBuffer, initDataDst,
                                                       sizeof(initDataDst), nullptr, 0, nullptr));

    void *copyHelperFunction = nullptr;
    SUCCESS_OR_TERMINATE(zeModuleGetFunctionPointer(module, "copy_helper", &copyHelperFunction));
    if (nullptr == copyHelperFunction) {
        std::cerr << "Pointer to function helper not found\n";
        std::terminate();
    }

    void *bufferWithFunctionPointer = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, sizeof(bufferWithFunctionPointer), 1, device, &bufferWithFunctionPointer));

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, bufferWithFunctionPointer, &copyHelperFunction,
                                                       sizeof(void *), nullptr, 0, nullptr));

    // Copying of data must finish before running the user kernel
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));

    // 3. Encode run user kernel
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 0,
                                                  sizeof(dstBuffer), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 1,
                                                  sizeof(srcBuffer), &srcBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 2,
                                                  sizeof(bufferWithFunctionPointer),
                                                  &bufferWithFunctionPointer));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = allocSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    SUCCESS_OR_TERMINATE_BOOL(dispatchTraits.groupCountX * groupSizeX == allocSize);
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatchTraits,
                                                         nullptr, 0, nullptr));

    // 4. Encode read back memory
    uint8_t readBackData[allocSize];
    memset(readBackData, 2, sizeof(readBackData));
    // user kernel must finish before we start copying data
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, readBackData, dstBuffer,
                                                       sizeof(readBackData), nullptr, 0, nullptr));

    // 5. Dispatch and wait
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    // 6. Validate
    outputValidationSuccessful = LevelZeroBlackBoxTests::validate(initDataSrc, readBackData, sizeof(readBackData));
    SUCCESS_OR_WARNING_BOOL(outputValidationSuccessful);

    // 7. Cleanup
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, bufferWithFunctionPointer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));
    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName);
    int resultOnFailure = aubMode ? 0 : 1;
    return outputValidationSuccessful ? 0 : resultOnFailure;
}

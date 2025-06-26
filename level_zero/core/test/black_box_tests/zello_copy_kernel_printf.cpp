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
    const std::string blackBoxName = "Zello Copy With Printf";
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);

    // X. Prepare spirV
    std::string buildLog;
    auto moduleBinary = LevelZeroBlackBoxTests::compileToSpirV(LevelZeroBlackBoxTests::memcpyBytesWithPrintfTestKernelSrc, "-g", buildLog);
    LevelZeroBlackBoxTests::printBuildLog(buildLog);
    SUCCESS_OR_TERMINATE((0 == moduleBinary.size()));

    // 1. Set-up
    size_t allocSize = 4096;
    if (LevelZeroBlackBoxTests::verbose) {
        allocSize = 32;
    }
    constexpr size_t bytesPerThread = sizeof(char);
    size_t numThreads = allocSize / bytesPerThread;
    ze_module_handle_t module;
    ze_kernel_handle_t kernel;
    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;
    void *srcBuffer = nullptr;
    void *dstBuffer = nullptr;

    ze_context_handle_t context = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context);
    auto device = devices[0];

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    LevelZeroBlackBoxTests::printDeviceProperties(deviceProperties);

    ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(moduleBinary.data());
    moduleDesc.inputSize = moduleBinary.size();
    SUCCESS_OR_TERMINATE(zeModuleCreate(context, device, &moduleDesc, &module, nullptr));

    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "memcpy_bytes";
    SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &kernel));

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(kernel, static_cast<uint32_t>(numThreads), 1U, 1U, &groupSizeX,
                                                  &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE_BOOL(numThreads % groupSizeX == 0);
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "Group size : (" << groupSizeX << ", " << groupSizeY << ", " << groupSizeZ
                  << ")" << std::endl;
    }
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernel, groupSizeX, groupSizeY, groupSizeZ));

    cmdQueue = LevelZeroBlackBoxTests::createCommandQueue(context, device, nullptr, ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, false);
    SUCCESS_OR_TERMINATE(LevelZeroBlackBoxTests::createCommandList(context, device, cmdList, false));

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

    // 2. Encode initialize memory
    uint8_t *initDataSrc = new uint8_t[allocSize];
    memset(initDataSrc, 7, allocSize);
    uint8_t *initDataDst = new uint8_t[allocSize];
    memset(initDataDst, 3, allocSize);

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, srcBuffer, initDataSrc,
                                                       allocSize, nullptr,
                                                       0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, dstBuffer, initDataDst,
                                                       allocSize, nullptr,
                                                       0, nullptr));

    // copying of data must finish before running the user kernel
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(
        cmdList, nullptr, 0, nullptr));

    // 3. Set arguments for user kernel
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 0, sizeof(dstBuffer), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 1, sizeof(srcBuffer), &srcBuffer));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = static_cast<uint32_t>(numThreads) / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;
    LevelZeroBlackBoxTests::printGroupCount(dispatchTraits);

    SUCCESS_OR_TERMINATE_BOOL(dispatchTraits.groupCountX * groupSizeX == allocSize);
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatchTraits,
                                                         nullptr, 0, nullptr));

    // 4. Encode read back memory
    uint8_t *readBackData = new uint8_t[allocSize];
    memset(readBackData, 2, allocSize);

    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(
        cmdList, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, readBackData, dstBuffer,
                                                       allocSize, nullptr,
                                                       0, nullptr));

    // 5. Dispatch and wait
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    // 6. Validate
    bool outputValidationSuccessful = true;
    for (size_t i = 0; i < allocSize; ++i) {
        uint8_t expectedData = static_cast<uint8_t>(initDataSrc[i] + i);
        outputValidationSuccessful &= (expectedData == readBackData[i]);
        if ((LevelZeroBlackBoxTests::verbose || (outputValidationSuccessful == false)) && (aubMode == false)) {
            std::cout << "readBackData[" << i << "] = "
                      << static_cast<uint32_t>(readBackData[i])
                      << ", expected " << static_cast<uint32_t>(expectedData) << "\n";
            if (outputValidationSuccessful == false) {
                break;
            }
        }
    }
    SUCCESS_OR_WARNING_BOOL(outputValidationSuccessful);

    // X. Cleanup
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));

    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));
    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    delete[] initDataSrc;
    delete[] initDataDst;
    delete[] readBackData;

    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName);
    int resultOnFailure = aubMode ? 0 : 1;
    return outputValidationSuccessful ? 0 : resultOnFailure;
}

/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"
#include "zello_compile.h"

#include <cstring>

void executeGpuKernelAndValidate(ze_context_handle_t &context,
                                 ze_device_handle_t &device,
                                 ze_module_handle_t &module,
                                 ze_kernel_handle_t &kernel,
                                 bool &outputValidationSuccessful,
                                 bool useImmediateCommandList,
                                 bool useAsync,
                                 int allocFlagValue) {
    ze_command_queue_handle_t cmdQueue = nullptr;
    ze_command_list_handle_t cmdList = nullptr;
    ze_event_pool_handle_t eventPool = nullptr;
    ze_event_handle_t event = nullptr;

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.ordinal = LevelZeroBlackBoxTests::getCommandQueueOrdinal(device);
    cmdQueueDesc.index = 0;
    if (useAsync) {
        cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
        LevelZeroBlackBoxTests::createEventPoolAndEvents(context, device, eventPool, ZE_EVENT_POOL_FLAG_HOST_VISIBLE, false, 0, 1, &event, ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST);
    } else {
        cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    }

    if (useImmediateCommandList) {
        SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdList));
    } else {
        SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));
        SUCCESS_OR_TERMINATE(LevelZeroBlackBoxTests::createCommandList(context, device, cmdList));
    }

    // Create two shared buffers
    uint32_t arraySize = 32;
    uint32_t vectorSize = 16;
    uint32_t typeSize = sizeof(uint32_t);
    uint32_t srcAdditionalMul = 3u;

    uint32_t expectedMemorySize = arraySize * vectorSize * typeSize * 2;
    uint32_t srcMemorySize = expectedMemorySize * srcAdditionalMul;
    uint32_t idxMemorySize = arraySize * sizeof(uint32_t);

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    if (allocFlagValue != 0) {
        hostDesc.flags = static_cast<ze_host_mem_alloc_flag_t>(allocFlagValue);
    }

    void *srcBuffer = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, srcMemorySize, 1, &srcBuffer));

    void *dstBuffer = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, expectedMemorySize, 1, &dstBuffer));

    void *idxBuffer = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, idxMemorySize, 1, &idxBuffer));

    void *expectedMemory = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, expectedMemorySize, 1, &expectedMemory));

    // Initialize memory
    constexpr uint8_t val = 0;
    memset(srcBuffer, val, srcMemorySize);
    memset(idxBuffer, 0, idxMemorySize);
    memset(dstBuffer, 0, expectedMemorySize);
    memset(expectedMemory, 0, expectedMemorySize);

    auto srcBufferLong = static_cast<uint64_t *>(srcBuffer);
    auto expectedMemoryLong = static_cast<uint64_t *>(expectedMemory);

    for (uint32_t i = 0; i < arraySize; ++i) {
        static_cast<uint32_t *>(idxBuffer)[i] = 2;
        for (uint32_t vecIdx = 0; vecIdx < vectorSize; ++vecIdx) {
            for (uint32_t srcMulIdx = 0; srcMulIdx < srcAdditionalMul; ++srcMulIdx) {
                srcBufferLong[(i * vectorSize * srcAdditionalMul) + srcMulIdx * vectorSize + vecIdx] = 1l;
            }
            expectedMemoryLong[i * vectorSize + vecIdx] = 2l;
        }
    }

    uint32_t groupSizeX = arraySize;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(kernel, groupSizeX, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 2, sizeof(dstBuffer), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 1, sizeof(srcBuffer), &srcBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 0, sizeof(idxBuffer), &idxBuffer));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = 1u;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatchTraits,
                                                         event,
                                                         0, nullptr));

    if (!useImmediateCommandList) {
        // Close list and submit for execution
        SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
        SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    }

    if (useAsync) {
        SUCCESS_OR_TERMINATE(zeEventHostSynchronize(event, std::numeric_limits<uint64_t>::max()));
    }

    // Validate
    outputValidationSuccessful = LevelZeroBlackBoxTests::validate(expectedMemory, dstBuffer, expectedMemorySize);

    // Cleanup
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, idxBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, expectedMemory));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    if (useAsync) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
        SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool));
    }
    if (!useImmediateCommandList) {
        SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    }
}

void createModuleKernel(ze_context_handle_t &context,
                        ze_device_handle_t &device,
                        ze_module_handle_t &module,
                        ze_kernel_handle_t &kernel) {
    std::string buildLog;
    auto spirV = LevelZeroBlackBoxTests::compileToSpirV(LevelZeroBlackBoxTests::scratchKernelSrc, "", buildLog);
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
        std::cerr << "\nZello Scratch Results validation FAILED. Module creation error."
                  << std::endl;
        SUCCESS_OR_TERMINATE_BOOL(false);
    }
    SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));

    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "scratch_kernel";
    SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &kernel));

    ze_kernel_properties_t kernelProperties{ZE_STRUCTURE_TYPE_KERNEL_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeKernelGetProperties(kernel, &kernelProperties));
    std::cout << "Scratch size = " << std::dec << kernelProperties.spillMemSize << "\n";
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello Scratch";
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);
    bool immediateFirst = LevelZeroBlackBoxTests::isImmediateFirst(argc, argv);
    bool useAsync = LevelZeroBlackBoxTests::isAsyncQueueEnabled(argc, argv);
    int allocFlagValue = LevelZeroBlackBoxTests::getAllocationFlag(argc, argv, 0);

    ze_context_handle_t context = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context);
    auto device = devices[0];
    bool outputValidationSuccessful;

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    LevelZeroBlackBoxTests::printDeviceProperties(deviceProperties);

    ze_module_handle_t module = nullptr;
    ze_kernel_handle_t kernel = nullptr;

    createModuleKernel(context, device, module, kernel);

    const std::string regularCaseName = "Regular Command List";
    const std::string immediateCaseName = "Immediate Command List";

    std::string caseName;

    auto selectCaseName = [&regularCaseName, &immediateCaseName](bool immediate) {
        if (immediate) {
            return immediateCaseName;
        } else {
            return regularCaseName;
        }
    };

    executeGpuKernelAndValidate(context, device, module, kernel, outputValidationSuccessful, immediateFirst, useAsync, allocFlagValue);
    caseName = selectCaseName(immediateFirst);
    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName, caseName);

    if (outputValidationSuccessful || aubMode) {
        immediateFirst = !immediateFirst;
        executeGpuKernelAndValidate(context, device, module, kernel, outputValidationSuccessful, immediateFirst, useAsync, allocFlagValue);
        caseName = selectCaseName(immediateFirst);
        LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName, caseName);
    }

    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));
    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;
    return (outputValidationSuccessful ? 0 : 1);
}

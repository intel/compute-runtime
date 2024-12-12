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
    cmdQueueDesc.ordinal = LevelZeroBlackBoxTests::getCommandQueueOrdinal(device, false);
    cmdQueueDesc.index = 0;
    if (useAsync) {
        cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
        LevelZeroBlackBoxTests::createEventPoolAndEvents(context, device, eventPool, ZE_EVENT_POOL_FLAG_HOST_VISIBLE, false, nullptr, nullptr, 1, &event, ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST);
    } else {
        cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    }

    if (useImmediateCommandList) {
        SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdList));
    } else {
        SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));
        SUCCESS_OR_TERMINATE(LevelZeroBlackBoxTests::createCommandList(context, device, cmdList, false));
    }

    uint32_t arraySize = 0;
    uint32_t vectorSize = 0;
    uint32_t srcAdditionalMul = 0;

    uint32_t expectedMemorySize = 0;
    uint32_t srcMemorySize = 0;
    uint32_t idxMemorySize = 0;

    LevelZeroBlackBoxTests::prepareScratchTestValues(arraySize, vectorSize, expectedMemorySize, srcAdditionalMul, srcMemorySize, idxMemorySize);

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

    LevelZeroBlackBoxTests::prepareScratchTestBuffers(srcBuffer, idxBuffer, expectedMemory, arraySize, vectorSize, expectedMemorySize, srcAdditionalMul);

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

    // Synchronize queue
    if (!useImmediateCommandList) {
        SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));
    }

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

    LevelZeroBlackBoxTests::createScratchModuleKernel(context, device, module, kernel, nullptr);

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

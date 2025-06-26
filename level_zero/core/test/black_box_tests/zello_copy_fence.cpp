/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>

#include "zello_common.h"

#include <cstring>
#include <iostream>
#include <limits>

void testAppendMemoryCopy(ze_context_handle_t &context, ze_device_handle_t &device, bool &validRet) {
    const size_t allocSize = 4096;
    char *heapBuffer = new char[allocSize];
    void *zeBuffer = nullptr;
    char stackBuffer[allocSize];

    ze_command_queue_handle_t cmdQueue = nullptr;
    ze_command_list_handle_t cmdList = nullptr;

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.pNext = nullptr;
    cmdQueueDesc.flags = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    cmdQueueDesc.ordinal = 0;
    cmdQueueDesc.index = 0;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));

    ze_command_list_desc_t cmdListDesc = {};
    cmdListDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
    cmdListDesc.pNext = nullptr;
    cmdListDesc.flags = 0;
    SUCCESS_OR_TERMINATE(zeCommandListCreate(context, device, &cmdListDesc, &cmdList));

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceDesc.ordinal = 0;
    deviceDesc.flags = 0;
    deviceDesc.pNext = nullptr;
    deviceDesc.ordinal = 0;
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, allocSize, allocSize, device, &zeBuffer));

    for (size_t i = 0; i < allocSize; ++i) {
        heapBuffer[i] = static_cast<char>(i + 1);
    }
    memset(stackBuffer, 0, allocSize);

    ze_fence_handle_t fence = {};
    ze_fence_desc_t fenceDesc = {};
    fenceDesc.stype = ZE_STRUCTURE_TYPE_FENCE_DESC;
    fenceDesc.pNext = nullptr;
    fenceDesc.flags = 0;
    SUCCESS_OR_TERMINATE(zeFenceCreate(cmdQueue, &fenceDesc, &fence));
    for (int i = 0; i < 2; i++) {
        if (LevelZeroBlackBoxTests::verbose)
            std::cout << "zeFenceHostSynchronize start iter:" << i << std::endl;
        // Copy from heap to device-allocated memory
        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, zeBuffer, heapBuffer, allocSize,
                                                           nullptr, 0, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));
        // Copy from device-allocated memory to stack
        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, stackBuffer, zeBuffer, allocSize,
                                                           nullptr, 0, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
        SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, fence));
        SUCCESS_OR_TERMINATE(zeFenceHostSynchronize(fence, std::numeric_limits<uint64_t>::max()));
        if (LevelZeroBlackBoxTests::verbose)
            std::cout << "zeFenceHostSynchronize success iter:" << i << std::endl;
        SUCCESS_OR_TERMINATE(zeFenceReset(fence));
        SUCCESS_OR_TERMINATE(zeCommandListReset(cmdList));
    }
    // Validate stack and ze buffers have the original data from heapBuffer
    validRet = LevelZeroBlackBoxTests::validate(heapBuffer, stackBuffer, allocSize);

    delete[] heapBuffer;
    SUCCESS_OR_TERMINATE(zeMemFree(context, zeBuffer));
    SUCCESS_OR_TERMINATE(zeFenceDestroy(fence));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello Copy Fence";
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);

    ze_context_handle_t context = nullptr;
    ze_driver_handle_t driverHandle = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context, driverHandle);
    auto device = devices[0];
    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    LevelZeroBlackBoxTests::printDeviceProperties(deviceProperties);

    bool outputValidationSuccessful;
    testAppendMemoryCopy(context, device, outputValidationSuccessful);

    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName);
    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;
    return (outputValidationSuccessful ? 0 : 1);
}

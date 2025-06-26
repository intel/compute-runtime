/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"

#include <cstring>

void testAppendMemoryCopyFill(ze_context_handle_t &context, ze_device_handle_t &device, bool &validRet, ze_command_queue_handle_t &cmdQueue, size_t maxElemenets, bool useInitFill) {
    size_t numElements = maxElemenets;
    size_t devBufferSize = numElements * sizeof(uint16_t);
    uint16_t pattern = 16;
    void *devBuffer = nullptr;
    void *hostBuffer = nullptr;

    size_t maxOffset = numElements - 1;

    uint16_t initPattern = 0;
    if (useInitFill) {
        initPattern = static_cast<uint16_t>(maxElemenets * 2);
    }
    bool dataFail = false;

    hostBuffer = malloc(devBufferSize);
    SUCCESS_OR_TERMINATE_BOOL(hostBuffer != nullptr);

    for (uint32_t offset = 0; offset < maxOffset; offset++) {
        ze_device_mem_alloc_desc_t deviceDesc = {};
        deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
        deviceDesc.ordinal = 0;
        deviceDesc.flags = 0;
        deviceDesc.pNext = nullptr;

        SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, devBufferSize, devBufferSize, device, &devBuffer));

        memset(hostBuffer, 0xFF, devBufferSize);

        ze_command_list_handle_t cmdListInit = nullptr;
        if (useInitFill) {
            SUCCESS_OR_TERMINATE(LevelZeroBlackBoxTests::createCommandList(context, device, cmdListInit, false));
            SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdListInit, devBuffer, &initPattern, sizeof(initPattern), devBufferSize, nullptr, 0, nullptr));
            SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdListInit, nullptr, 0, nullptr));
            SUCCESS_OR_TERMINATE(zeCommandListClose(cmdListInit));
            SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdListInit, nullptr));
            SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));
        }

        ze_command_list_handle_t cmdListFill;
        SUCCESS_OR_TERMINATE(LevelZeroBlackBoxTests::createCommandList(context, device, cmdListFill, false));

        void *dst = reinterpret_cast<void *>(reinterpret_cast<uint16_t *>(devBuffer) + offset);
        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdListFill, dst, &pattern, sizeof(pattern), (numElements - offset) * sizeof(uint16_t), nullptr, 0, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdListFill, nullptr, 0, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandListClose(cmdListFill));

        ze_command_list_handle_t cmdListCopy;
        SUCCESS_OR_TERMINATE(LevelZeroBlackBoxTests::createCommandList(context, device, cmdListCopy, false));
        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdListCopy, hostBuffer, devBuffer, devBufferSize, nullptr, 0, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdListCopy, nullptr, 0, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandListClose(cmdListCopy));

        SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdListFill, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

        SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdListCopy, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

        uint16_t *data = reinterpret_cast<uint16_t *>(hostBuffer);
        uint16_t value;
        for (uint32_t i = 0; i < numElements; i++) {
            if (i < offset) {
                value = initPattern;
            } else {
                value = pattern;
            }

            if (value != data[i]) {
                std::cout << "!!! Data validation error for offset: " << offset << " at #" << i << " memory: " << data[i] << " expected pattern: " << value << std::endl;
                dataFail |= true;
            }
        }

        if (cmdListInit != nullptr) {
            SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdListInit));
        }
        SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdListFill));
        SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdListCopy));
        SUCCESS_OR_TERMINATE(zeMemFree(context, devBuffer));
    }

    free(hostBuffer);
    validRet = !dataFail;
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello Fill";
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);
    size_t maxElemenets = static_cast<uint32_t>(LevelZeroBlackBoxTests::getParamValue(argc, argv, "-e", "--max-elements", 10));
    bool useInitFill = LevelZeroBlackBoxTests::getParamValue(argc, argv, "-f", "--fill", 1) == 1 ? true : false;

    ze_context_handle_t context = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context);
    auto device = devices[0];
    bool outputValidationSuccessful = false;

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    LevelZeroBlackBoxTests::printDeviceProperties(deviceProperties);

    ze_command_queue_handle_t cmdQueue;
    cmdQueue = LevelZeroBlackBoxTests::createCommandQueue(context, device, nullptr, false);

    testAppendMemoryCopyFill(context, device, outputValidationSuccessful, cmdQueue, maxElemenets, useInitFill);

    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName);

    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;
    return (outputValidationSuccessful ? 0 : 1);
}

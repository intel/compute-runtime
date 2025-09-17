/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/driver_experimental/zex_cmdlist.h"

#include "zello_common.h"

struct FunctionData {
    uint16_t *array;
    size_t nElements;
};

extern "C" void ZE_APICALL hostFunction1(void *pUserData) {
    auto *data = static_cast<FunctionData *>(pUserData);

    for (auto i = 0u; i < data->nElements; ++i) {
        data->array[i] = data->array[i] + i;
    }
}

extern "C" void ZE_APICALL hostFunction2(void *pUserData) {
    auto *data = static_cast<FunctionData *>(pUserData);

    for (auto i = 0u; i < data->nElements; ++i) {
        data->array[i] = data->array[i] + 1;
    }
}

decltype(&zexCommandListAppendHostFunction) zexCommandListAppendHostFunctionFunc = nullptr;

void testHostFunction(ze_driver_handle_t &driver, ze_context_handle_t &context, ze_device_handle_t &device, bool useImmediate) {

    ze_command_queue_handle_t cmdQueue = nullptr;

    ze_event_pool_handle_t eventPool;
    uint32_t numEvents = 6;
    std::vector<ze_event_handle_t> events(numEvents);
    LevelZeroBlackBoxTests::createEventPoolAndEvents(context, device, eventPool,
                                                     ZE_EVENT_POOL_FLAG_HOST_VISIBLE, false, nullptr, nullptr,
                                                     numEvents, events.data(),
                                                     ZE_EVENT_SCOPE_FLAG_HOST,
                                                     ZE_EVENT_SCOPE_FLAG_HOST);

    ze_command_list_handle_t cmdList = nullptr;

    if (useImmediate) {
        ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
        cmdQueueDesc.pNext = nullptr;
        cmdQueueDesc.flags = 0;
        cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
        cmdQueueDesc.ordinal = LevelZeroBlackBoxTests::getCommandQueueOrdinal(device, false);
        cmdQueueDesc.index = 0;
        cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
        SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdList));
    } else {
        cmdQueue = LevelZeroBlackBoxTests::createCommandQueue(context, device, nullptr, ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, false);
        SUCCESS_OR_TERMINATE(LevelZeroBlackBoxTests::createCommandList(context, device, cmdList, false));
    }

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceDesc.ordinal = 0;
    deviceDesc.flags = 0;
    deviceDesc.pNext = nullptr;

    size_t numElements = 1024u;
    size_t bufferSize = numElements * sizeof(uint16_t);
    void *buffer = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, bufferSize, bufferSize, device, &buffer));

    uint16_t pattern = 5;
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, buffer, &pattern, sizeof(pattern), bufferSize, events[0], 0, nullptr));

    void *hostBuffer = nullptr;
    hostBuffer = malloc(bufferSize);

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, hostBuffer, buffer, bufferSize, events[1], 1, &events[0]));

    FunctionData callbackData;
    callbackData.array = static_cast<uint16_t *>(hostBuffer);
    callbackData.nElements = numElements;

    SUCCESS_OR_TERMINATE(zexCommandListAppendHostFunctionFunc(cmdList, reinterpret_cast<void *>(hostFunction1), static_cast<void *>(&callbackData), nullptr, events[2], 1, &events[1]));
    SUCCESS_OR_TERMINATE(zexCommandListAppendHostFunctionFunc(cmdList, reinterpret_cast<void *>(hostFunction2), static_cast<void *>(&callbackData), nullptr, events[3], 1, &events[2]));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, buffer, hostBuffer, bufferSize, events[4], 1, &events[3]));

    void *resultHostBuffer = nullptr;
    resultHostBuffer = malloc(bufferSize);

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, resultHostBuffer, buffer, bufferSize, events[5], 1, &events[4]));

    if (!useImmediate) {
        SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
        SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));
    }
    SUCCESS_OR_TERMINATE(zeEventHostSynchronize(events[5], std::numeric_limits<uint64_t>::max()));

    // validate data
    uint16_t *result = static_cast<uint16_t *>(resultHostBuffer);

    if (result) {
        for (auto i = 0u; i < numElements; i++) {
            auto expectedResult = (pattern + i) + 1;
            if (result[i] != expectedResult) {
                std::cout << "Incorrect data found = [" << result[i] << "], expected result = [" << expectedResult << "]" << std::endl;
                break;
            }
        }
    }

    for (auto event : events) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }

    SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool));
    SUCCESS_OR_TERMINATE(zeMemFree(context, buffer));
    free(hostBuffer);
    free(resultHostBuffer);
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));

    if (!useImmediate) {
        SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    }
}

int main(int argc, char *argv[]) {

    ze_driver_handle_t driverHandle = nullptr;
    ze_context_handle_t context = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context, driverHandle);
    auto device = devices[0];

    SUCCESS_OR_TERMINATE(zeDriverGetExtensionFunctionAddress(driverHandle, "zexCommandListAppendHostFunction", reinterpret_cast<void **>(&zexCommandListAppendHostFunctionFunc)));

    int testCase = LevelZeroBlackBoxTests::getParamValue(argc, argv, "", "--test-case", -1);
    uint32_t nTest = 2;

    if (testCase == -1) {
        // Run all tests
        testHostFunction(driverHandle, context, device, true);
        testHostFunction(driverHandle, context, device, false);
    } else {
        // Run specific test
        switch (testCase) {
        case 1:
            testHostFunction(driverHandle, context, device, true);
            break;
        case 2:
            testHostFunction(driverHandle, context, device, false);
            break;
        default:
            std::cerr << "Invalid test case number: " << testCase << ". Valid range is 1 to " << nTest << "." << std::endl;
            return -1;
        }
    }
    return 0;
}

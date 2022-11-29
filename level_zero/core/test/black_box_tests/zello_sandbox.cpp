/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"

#include <bitset>
#include <sstream>

void executeMemoryTransferAndValidate(ze_context_handle_t &context, ze_device_handle_t &device,
                                      uint32_t flags, bool useImmediate, bool asyncMode, bool &outputValidationSuccessful) {
    ze_command_queue_handle_t cmdQueue = nullptr;
    ze_command_list_handle_t cmdList = nullptr;

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.ordinal = getCommandQueueOrdinal(device);
    cmdQueueDesc.index = 0;
    selectQueueMode(cmdQueueDesc, !asyncMode);
    if (useImmediate) {
        SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdList));
    } else {
        SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));
        SUCCESS_OR_TERMINATE(createCommandList(context, device, cmdList));
    }

    ze_event_pool_handle_t eventPool;
    uint32_t numEvents = 10;
    std::vector<ze_event_handle_t> events(numEvents);
    ze_event_pool_flag_t eventPoolFlags = static_cast<ze_event_pool_flag_t>(flags);
    createEventPoolAndEvents(context, device, eventPool,
                             eventPoolFlags,
                             numEvents, events.data(),
                             ZE_EVENT_SCOPE_FLAG_HOST,
                             ZE_EVENT_SCOPE_FLAG_HOST);

    constexpr size_t allocSize = 10000;
    const uint8_t value = 0x55;

    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.flags = 0;
    deviceDesc.ordinal = 0;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    hostDesc.flags = 0;

    void *buffer1 = nullptr;
    void *buffer2 = nullptr;
    void *buffer3 = nullptr;
    void *buffer4 = nullptr;
    void *buffer5 = nullptr;

    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, allocSize, 1, device, &buffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, allocSize, 1, device, &buffer2));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, 1, &buffer3));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, allocSize, 1, device, &buffer4));
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, 1, &buffer5));

    memset(buffer3, 0, allocSize);
    memset(buffer5, 0, allocSize);

    ze_event_handle_t eventHandle = nullptr;

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, buffer1, &value, sizeof(value), allocSize, events[1], 0, nullptr));

    eventHandle = events[1];
    SUCCESS_OR_TERMINATE(zeCommandListAppendWaitOnEvents(cmdList, 1, &eventHandle));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, buffer2, buffer1, allocSize, events[2], 0, nullptr));

    eventHandle = events[2];
    SUCCESS_OR_TERMINATE(zeCommandListAppendWaitOnEvents(cmdList, 1, &eventHandle));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, buffer3, buffer2, allocSize, events[3], 0, nullptr));

    eventHandle = events[3];
    SUCCESS_OR_TERMINATE(zeCommandListAppendWaitOnEvents(cmdList, 1, &eventHandle));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, buffer4, buffer3, allocSize, events[4], 0, nullptr));

    eventHandle = events[4];
    SUCCESS_OR_TERMINATE(zeCommandListAppendWaitOnEvents(cmdList, 1, &eventHandle));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, buffer5, buffer4, allocSize, events[5], 0, nullptr));

    eventHandle = events[5];

    // Close list and submit for execution
    if (!useImmediate) {
        SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
        SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    }

    SUCCESS_OR_TERMINATE(zeEventHostSynchronize(eventHandle, std::numeric_limits<uint64_t>::max()));

    auto output = std::make_unique<uint8_t[]>(allocSize);
    memcpy(output.get(), buffer5, allocSize);

    // Validate
    outputValidationSuccessful = true;
    for (size_t i = 0; i < allocSize; i++) {
        if (output.get()[i] != value) {
            std::cout << "output[" << i << "] = " << static_cast<unsigned int>(output.get()[i]) << " not equal to "
                      << "reference = " << static_cast<unsigned int>(value) << "\n";
            outputValidationSuccessful = false;
            break;
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(context, buffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(context, buffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(context, buffer3));
    SUCCESS_OR_TERMINATE(zeMemFree(context, buffer4));
    SUCCESS_OR_TERMINATE(zeMemFree(context, buffer5));

    for (auto &event : events) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }
    SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool));

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    if (!useImmediate) {
        SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    }
}

std::string testMemoryTransfer5xString(bool asyncMode, bool immediateFirst, bool tsEvent) {
    std::ostringstream testStream;

    testStream << "MemoryTransfer 5x "
               << (immediateFirst ? "immediate" : "regular")
               << " command list "
               << (asyncMode ? "async" : "sync")
               << " mode and event pool flags: "
               << (tsEvent ? "ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP | ZE_EVENT_POOL_FLAG_HOST_VISIBLE" : "ZE_EVENT_POOL_FLAG_HOST_VISIBLE");

    return testStream.str();
}

using TestBitMask = std::bitset<32>;

TestBitMask getTestMask(int argc, char *argv[], uint32_t defaultValue) {
    uint32_t value = static_cast<uint32_t>(getParamValue(argc, argv, "-m", "-mask", static_cast<int>(defaultValue)));
    std::cerr << "Test mask ";
    if (value != defaultValue) {
        std::cerr << "override ";
    } else {
        std::cerr << "default ";
    }
    TestBitMask bitValue(value);
    std::cerr << "value 0b" << bitValue << std::endl;

    return bitValue;
}

int main(int argc, char *argv[]) {
    constexpr uint32_t bitNumberTestMemoryTransfer5x = 0u;

    const std::string blackBoxName = "Zello Sandbox";
    std::string currentTest;
    verbose = isVerbose(argc, argv);
    bool aubMode = isAubMode(argc, argv);
    bool asyncMode = !isSyncQueueEnabled(argc, argv);
    bool immediateFirst = isImmediateFirst(argc, argv);
    TestBitMask testMask = getTestMask(argc, argv, std::numeric_limits<uint32_t>::max());

    ze_context_handle_t context = nullptr;
    auto devices = zelloInitContextAndGetDevices(context);
    auto device = devices[0];

    bool outputValidationSuccessful = true;

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    printDeviceProperties(deviceProperties);

    if (testMask.test(bitNumberTestMemoryTransfer5x)) {
        bool useImmediate = immediateFirst;
        currentTest = testMemoryTransfer5xString(asyncMode, useImmediate, false);
        uint32_t testFlag = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

        executeMemoryTransferAndValidate(context, device,
                                         testFlag, useImmediate, asyncMode,
                                         outputValidationSuccessful);

        if (outputValidationSuccessful || aubMode) {
            printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);

            currentTest = testMemoryTransfer5xString(asyncMode, useImmediate, true);
            testFlag = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
            executeMemoryTransferAndValidate(context, device,
                                             testFlag, useImmediate, asyncMode,
                                             outputValidationSuccessful);
        }

        useImmediate = !useImmediate;
        if (outputValidationSuccessful || aubMode) {
            printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);

            currentTest = testMemoryTransfer5xString(asyncMode, useImmediate, false);
            testFlag = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
            executeMemoryTransferAndValidate(context, device,
                                             testFlag, useImmediate, asyncMode,
                                             outputValidationSuccessful);
        }

        if (outputValidationSuccessful || aubMode) {
            printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);

            currentTest = testMemoryTransfer5xString(asyncMode, useImmediate, true);
            testFlag = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
            executeMemoryTransferAndValidate(context, device,
                                             testFlag, useImmediate, asyncMode,
                                             outputValidationSuccessful);
        }
    }

    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);

    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;
    return outputValidationSuccessful ? 0 : 1;
}

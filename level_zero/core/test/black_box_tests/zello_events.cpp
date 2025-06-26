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
#include <vector>

void createCmdQueueAndCmdList(ze_device_handle_t &device,
                              ze_context_handle_t &context,
                              ze_command_queue_handle_t &cmdqueue,
                              ze_command_list_handle_t &cmdList) {
    // Create commandQueue and cmdList
    cmdqueue = LevelZeroBlackBoxTests::createCommandQueue(context, device, nullptr, ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, false);
    SUCCESS_OR_TERMINATE(LevelZeroBlackBoxTests::createCommandList(context, device, cmdList, false));
}

void createCmdQueueAndCmdListWithOrdinal(ze_device_handle_t &device,
                                         ze_context_handle_t &context, uint32_t ordinal,
                                         ze_command_queue_handle_t &cmdqueue,
                                         ze_command_list_handle_t &cmdList) {
    // Create commandQueue and cmdList
    cmdqueue = LevelZeroBlackBoxTests::createCommandQueueWithOrdinal(context, device, ordinal, ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS, ZE_COMMAND_QUEUE_PRIORITY_NORMAL);
    SUCCESS_OR_TERMINATE(LevelZeroBlackBoxTests::createCommandList(context, device, cmdList, ordinal));
}

// Test Device Signal and Device wait followed by Host Wait
bool testEventsDeviceSignalDeviceWait(ze_context_handle_t &context, ze_device_handle_t &device) {
    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;

    // Create commandQueue and cmdList
    createCmdQueueAndCmdList(device, context, cmdQueue, cmdList);

    // Create two shared buffers
    constexpr size_t allocSize = 4096;
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED;
    deviceDesc.ordinal = 0;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    hostDesc.flags = ZE_HOST_MEM_ALLOC_FLAG_BIAS_UNCACHED;

    void *srcBuffer = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc, allocSize, 1, device, &srcBuffer));

    void *dstBuffer = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc, allocSize, 1, device, &dstBuffer));

    // Create Event Pool and kernel launch event
    ze_event_pool_handle_t eventPoolDevice, eventPoolHost;
    uint32_t numEvents = 2;
    std::vector<ze_event_handle_t> deviceEvents(numEvents), hostEvents(numEvents);
    LevelZeroBlackBoxTests::createEventPoolAndEvents(context, device, eventPoolDevice,
                                                     0, false, nullptr, nullptr,
                                                     numEvents, deviceEvents.data(),
                                                     ZE_EVENT_SCOPE_FLAG_SUBDEVICE,
                                                     0);
    LevelZeroBlackBoxTests::createEventPoolAndEvents(context, device, eventPoolHost,
                                                     ZE_EVENT_POOL_FLAG_HOST_VISIBLE, false, nullptr, nullptr,
                                                     numEvents, hostEvents.data(),
                                                     ZE_EVENT_SCOPE_FLAG_HOST,
                                                     0);

    // Initialize memory
    uint8_t dstValue = 0;
    uint8_t srcValue = 55;
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, dstBuffer, reinterpret_cast<void *>(&dstValue),
                                                       sizeof(dstValue), allocSize, deviceEvents[0], 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, srcBuffer, reinterpret_cast<void *>(&srcValue),
                                                       sizeof(srcValue), allocSize, deviceEvents[1], 1, &deviceEvents[0]));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, dstBuffer, srcBuffer, allocSize, hostEvents[0], 1, &deviceEvents[1]));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));

    SUCCESS_OR_TERMINATE(zeEventHostSynchronize(hostEvents[0], std::numeric_limits<uint64_t>::max()));

    // Validate
    bool outputValidationSuccessful = true;
    if (memcmp(dstBuffer, srcBuffer, allocSize)) {
        outputValidationSuccessful = false;
        uint8_t *srcCharBuffer = static_cast<uint8_t *>(srcBuffer);
        uint8_t *dstCharBuffer = static_cast<uint8_t *>(dstBuffer);
        for (size_t i = 0; i < allocSize; i++) {
            if (srcCharBuffer[i] != dstCharBuffer[i]) {
                std::cout << "srcBuffer[" << i << "] = " << static_cast<unsigned int>(srcCharBuffer[i]) << " not equal to "
                          << "dstBuffer[" << i << "] = " << static_cast<unsigned int>(dstCharBuffer[i]) << "\n";
                break;
            }
        }
    }

    // Cleanup
    for (auto event : hostEvents) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }
    for (auto event : deviceEvents) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }

    SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPoolHost));
    SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPoolDevice));
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));

    return outputValidationSuccessful;
}

// Test Device Signal and Host wait
bool testEventsDeviceSignalHostWait(ze_context_handle_t &context, ze_device_handle_t &device) {
    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;

    // Create commandQueue and cmdList
    createCmdQueueAndCmdList(device, context, cmdQueue, cmdList);

    // Create two shared buffers
    constexpr size_t allocSize = 4096;
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED;
    deviceDesc.ordinal = 0;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    hostDesc.flags = ZE_HOST_MEM_ALLOC_FLAG_BIAS_UNCACHED;

    void *srcBuffer = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc, allocSize, 1, device, &srcBuffer));

    void *dstBuffer = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc, allocSize, 1, device, &dstBuffer));

    // Initialize memory
    constexpr uint8_t val = 55;
    memset(srcBuffer, val, allocSize);
    memset(dstBuffer, 0, allocSize);

    // Create Event Pool and kernel launch event
    ze_event_pool_handle_t eventPool;
    uint32_t numEvents = 2;
    std::vector<ze_event_handle_t> events(numEvents);
    LevelZeroBlackBoxTests::createEventPoolAndEvents(context, device, eventPool,
                                                     ZE_EVENT_POOL_FLAG_HOST_VISIBLE, false, nullptr, nullptr,
                                                     numEvents, events.data(),
                                                     ZE_EVENT_SCOPE_FLAG_HOST,
                                                     0);

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, dstBuffer, srcBuffer, allocSize, events[0], 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));

    SUCCESS_OR_TERMINATE(zeEventHostSynchronize(events[0], std::numeric_limits<uint64_t>::max()));

    // Validate
    bool outputValidationSuccessful = true;
    if (memcmp(dstBuffer, srcBuffer, allocSize)) {
        outputValidationSuccessful = false;
        uint8_t *srcCharBuffer = static_cast<uint8_t *>(srcBuffer);
        uint8_t *dstCharBuffer = static_cast<uint8_t *>(dstBuffer);
        for (size_t i = 0; i < allocSize; i++) {
            if (srcCharBuffer[i] != dstCharBuffer[i]) {
                std::cout << "srcBuffer[" << i << "] = " << static_cast<unsigned int>(srcCharBuffer[i]) << " not equal to "
                          << "dstBuffer[" << i << "] = " << static_cast<unsigned int>(dstCharBuffer[i]) << "\n";
                break;
            }
        }
    }

    // Cleanup
    for (auto event : events) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }

    SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool));
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));

    return outputValidationSuccessful;
}

// Test Device Signal and Host wait
bool testEventsDeviceSignalHostWaitWithNonZeroOrdinal(ze_context_handle_t &context, ze_device_handle_t &device) {
    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;
    auto ordinals = LevelZeroBlackBoxTests::getComputeQueueOrdinals(device);

    if (ordinals.size() <= 1) {
        return true;
    }

    auto ordinal = ordinals[ordinals.size() - 1];

    // Create Event Pool and kernel launch event
    ze_event_pool_handle_t eventPool;
    uint32_t numEvents = 2;
    std::vector<ze_event_handle_t> events(numEvents);
    LevelZeroBlackBoxTests::createEventPoolAndEvents(context, device, eventPool,
                                                     ZE_EVENT_POOL_FLAG_HOST_VISIBLE, false, nullptr, nullptr,
                                                     numEvents, events.data(),
                                                     ZE_EVENT_SCOPE_FLAG_HOST,
                                                     0);

    bool outputValidationSuccessful = true;

    // Create commandQueue and cmdList
    createCmdQueueAndCmdListWithOrdinal(device, context, ordinal, cmdQueue, cmdList);

    SUCCESS_OR_TERMINATE(zeCommandListAppendSignalEvent(cmdList, events[0]));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeEventHostSynchronize(events[0], std::numeric_limits<uint64_t>::max()));

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));

    // Cleanup
    for (auto event : events) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }

    SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool));
    return outputValidationSuccessful;
}

// Test Host Signal and Host wait
bool testEventsHostSignalHostWait(ze_context_handle_t &context, ze_device_handle_t &device) {
    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;

    // Create commandQueue and cmdList
    createCmdQueueAndCmdList(device, context, cmdQueue, cmdList);

    // Create two shared buffers
    constexpr size_t allocSize = 4096;
    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED;
    deviceDesc.ordinal = 0;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    hostDesc.flags = ZE_HOST_MEM_ALLOC_FLAG_BIAS_UNCACHED;

    void *srcBuffer = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc, allocSize, 1, device, &srcBuffer));

    void *dstBuffer = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc, allocSize, 1, device, &dstBuffer));

    // Initialize memory
    constexpr uint8_t val = 55;
    memset(srcBuffer, val, allocSize);
    memset(dstBuffer, 0, allocSize);

    // Create Event Pool and kernel launch event
    ze_event_pool_handle_t eventPool;
    uint32_t numEvents = 3;
    std::vector<ze_event_handle_t> events(numEvents);
    LevelZeroBlackBoxTests::createEventPoolAndEvents(context, device, eventPool,
                                                     ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP,
                                                     false, nullptr, nullptr,
                                                     numEvents, events.data(),
                                                     ZE_EVENT_SCOPE_FLAG_HOST,
                                                     0);

    SUCCESS_OR_TERMINATE(zeCommandListAppendWaitOnEvents(cmdList, 2, &events[1]));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, dstBuffer, srcBuffer, allocSize, events[0], 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));

    SUCCESS_OR_TERMINATE(zeEventHostSignal(events[1]));
    SUCCESS_OR_TERMINATE(zeEventHostSignal(events[2]));

    SUCCESS_OR_TERMINATE(zeEventHostSynchronize(events[0], std::numeric_limits<uint64_t>::max()));
    SUCCESS_OR_TERMINATE(zeEventHostSynchronize(events[1], std::numeric_limits<uint64_t>::max()));
    SUCCESS_OR_TERMINATE(zeEventHostSynchronize(events[2], std::numeric_limits<uint64_t>::max()));

    // Validate
    bool outputValidationSuccessful = LevelZeroBlackBoxTests::validate(srcBuffer, dstBuffer, allocSize);

    // Cleanup
    for (auto event : events) {
        SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    }

    SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool));
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));

    return outputValidationSuccessful;
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName("Zello Events");

    bool outputValidationSuccessful = true;
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);

    ze_context_handle_t context = nullptr;
    ze_driver_handle_t driverHandle = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context, driverHandle);
    auto device = devices[0];

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    LevelZeroBlackBoxTests::printDeviceProperties(deviceProperties);

    std::string currentTest;

    currentTest = "Device signal and host wait test";
    outputValidationSuccessful = testEventsDeviceSignalHostWait(context, device);
    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);

    if (outputValidationSuccessful || aubMode) {
        currentTest = "Device signal and device wait test";
        outputValidationSuccessful = testEventsDeviceSignalDeviceWait(context, device);
        LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);
    }

    if (outputValidationSuccessful || aubMode) {
        currentTest = "Host signal and host wait test";
        outputValidationSuccessful = testEventsHostSignalHostWait(context, device);
        LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);
    }

    if (outputValidationSuccessful || aubMode) {
        currentTest = "Device signal and host wait with non-zero ordinal";
        outputValidationSuccessful = testEventsDeviceSignalHostWaitWithNonZeroOrdinal(context, device);
        LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);
    }

    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;
    return outputValidationSuccessful ? 0 : 1;
}

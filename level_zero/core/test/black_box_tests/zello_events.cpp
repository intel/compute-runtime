/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>

#include "zello_common.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

extern bool verbose;
bool verbose = false;

void createCmdQueueAndCmdList(ze_device_handle_t &device,
                              ze_context_handle_t &context,
                              ze_command_queue_handle_t &cmdqueue,
                              ze_command_list_handle_t &cmdList) {
    // Create commandQueue and cmdList
    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.ordinal = getCommandQueueOrdinal(device);
    cmdQueueDesc.index = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdqueue));
    SUCCESS_OR_TERMINATE(createCommandList(context, device, cmdList));
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
    createEventPoolAndEvents(context, device, eventPoolDevice,
                             (ze_event_pool_flag_t)0,
                             numEvents, deviceEvents.data(),
                             ZE_EVENT_SCOPE_FLAG_SUBDEVICE,
                             (ze_event_scope_flag_t)0);
    createEventPoolAndEvents(context, device, eventPoolHost,
                             (ze_event_pool_flag_t)(ZE_EVENT_POOL_FLAG_HOST_VISIBLE),
                             numEvents, hostEvents.data(),
                             ZE_EVENT_SCOPE_FLAG_HOST,
                             (ze_event_scope_flag_t)0);

    //Initialize memory
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
    createEventPoolAndEvents(context, device, eventPool,
                             (ze_event_pool_flag_t)(ZE_EVENT_POOL_FLAG_HOST_VISIBLE),
                             numEvents, events.data(),
                             ZE_EVENT_SCOPE_FLAG_HOST,
                             (ze_event_scope_flag_t)0);

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

void printResult(bool outputValidationSuccessful, std::string &currentTest) {
    std::cout << "\nZello Events: " << currentTest.c_str()
              << "  Results validation "
              << (outputValidationSuccessful ? "PASSED" : "FAILED")
              << std::endl
              << std::endl;
}

int main(int argc, char *argv[]) {
    bool outputValidationSuccessful;
    verbose = isVerbose(argc, argv);

    ze_context_handle_t context = nullptr;
    ze_driver_handle_t driverHandle = nullptr;
    auto devices = zelloInitContextAndGetDevices(context, driverHandle);
    auto device = devices[0];

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    std::cout << "Device : \n"
              << " * name : " << deviceProperties.name << "\n"
              << " * vendorId : " << std::hex << deviceProperties.vendorId << "\n";

    std::string currentTest;

    currentTest = "Device signal and host wait test";
    outputValidationSuccessful = testEventsDeviceSignalHostWait(context, device);
    printResult(outputValidationSuccessful, currentTest);

    currentTest = "Device signal and device wait test";
    outputValidationSuccessful = testEventsDeviceSignalDeviceWait(context, device);
    printResult(outputValidationSuccessful, currentTest);

    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    return outputValidationSuccessful ? 0 : 1;
}

/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"

#include <bitset>
#include <cstring>
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

void executeEventSyncForMultiTileAndCopy(ze_context_handle_t &context, ze_device_handle_t &device,
                                         uint32_t flags, bool useImmediate, bool &outputValidationSuccessful) {
    uint32_t numEvents = 10;

    ze_event_pool_handle_t eventPool = nullptr;
    ze_event_pool_flag_t eventPoolFlags = static_cast<ze_event_pool_flag_t>(flags);

    ze_event_pool_desc_t eventPoolDesc{ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    eventPoolDesc.count = numEvents;
    eventPoolDesc.flags = eventPoolFlags;

    std::vector<ze_device_handle_t> eventPoolDevices;
    ze_device_handle_t subDevice = nullptr;
    ze_device_handle_t copyDevice = nullptr;

    std::vector<ze_event_handle_t> events(numEvents, nullptr);
    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};

    ze_command_queue_handle_t cmdQueue = nullptr;
    ze_command_list_handle_t cmdList = nullptr;

    ze_command_queue_handle_t cmdQueueCopy = nullptr;
    ze_command_list_handle_t cmdListCopy = nullptr;

    ze_command_queue_handle_t cmdQueueSubDevice = nullptr;
    ze_command_list_handle_t cmdListSubDevice = nullptr;

    eventPoolDevices.push_back(device);

    uint32_t queueGroup = getCommandQueueOrdinal(device);
    uint32_t copyQueueGroup = getCopyOnlyCommandQueueOrdinal(device);
    uint32_t subDeviceCopyQueueGroup = std::numeric_limits<uint32_t>::max();

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.ordinal = queueGroup;
    cmdQueueDesc.index = 0;
    selectQueueMode(cmdQueueDesc, false);
    if (useImmediate) {
        SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdList));
    } else {
        SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));
        SUCCESS_OR_TERMINATE(createCommandList(context, device, cmdList, queueGroup));
    }

    uint32_t subDevCount = 0;
    auto subDevices = zelloGetSubDevices(device, subDevCount);
    if (subDevCount == 0) {
        if (verbose) {
            std::cout << "Skipping multi-tile - subdevice compute sync" << std::endl;
        }
    } else {
        subDevice = subDevices[0];
        eventPoolDevices.push_back(subDevice);
        uint32_t subDeviceQueueGroup = getCommandQueueOrdinal(subDevice);

        subDeviceCopyQueueGroup = getCopyOnlyCommandQueueOrdinal(subDevice);
        if (subDeviceCopyQueueGroup != std::numeric_limits<uint32_t>::max()) {
            copyQueueGroup = subDeviceCopyQueueGroup;
        }

        cmdQueueDesc.ordinal = subDeviceQueueGroup;
        if (useImmediate) {
            SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, subDevice, &cmdQueueDesc, &cmdListSubDevice));
        } else {
            SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, subDevice, &cmdQueueDesc, &cmdQueueSubDevice));
            SUCCESS_OR_TERMINATE(createCommandList(context, subDevice, cmdListSubDevice, subDeviceQueueGroup));
        }
    }

    if (copyQueueGroup == std::numeric_limits<uint32_t>::max()) {
        if (verbose) {
            std::cout << "Skipping compute - copy sync" << std::endl;
        }
    } else {
        copyDevice = device;
        if (subDeviceCopyQueueGroup != std::numeric_limits<uint32_t>::max()) {
            copyDevice = subDevice;
            if (verbose) {
                std::cout << "Using subdevice for copy engine" << std::endl;
            }
        }

        cmdQueueDesc.ordinal = copyQueueGroup;
        if (useImmediate) {
            SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, copyDevice, &cmdQueueDesc, &cmdListCopy));
        } else {
            SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, copyDevice, &cmdQueueDesc, &cmdQueueCopy));
            SUCCESS_OR_TERMINATE(createCommandList(context, copyDevice, cmdListCopy, copyQueueGroup));
        }
    }

    uint32_t eventIndex = 0;
    bool createEvents = (subDevice != nullptr) || (copyDevice != nullptr);

    if (createEvents) {
        uint32_t eventPoolDevicesNum = static_cast<uint32_t>(eventPoolDevices.size());
        auto eventPtr = events.data();
        SUCCESS_OR_TERMINATE(zeEventPoolCreate(context, &eventPoolDesc, eventPoolDevicesNum, eventPoolDevices.data(), &eventPool));
        for (uint32_t i = 0; i < numEvents; i++) {
            eventDesc.index = i;
            SUCCESS_OR_TERMINATE(zeEventCreate(eventPool, &eventDesc, eventPtr + i));
        }
    }

    if (subDevice) {
        if (verbose) {
            std::cout << "Running multi-tile - subdevice compute sync" << std::endl;
        }
        auto fromRootToSubEvent = events[eventIndex++];
        SUCCESS_OR_TERMINATE(zeCommandListAppendSignalEvent(cmdList, fromRootToSubEvent));
        auto subEvent = events[eventIndex++];

        SUCCESS_OR_TERMINATE(zeCommandListAppendWaitOnEvents(cmdListSubDevice, 1, &fromRootToSubEvent));
        SUCCESS_OR_TERMINATE(zeCommandListAppendSignalEvent(cmdListSubDevice, subEvent));

        if (!useImmediate) {
            SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
            SUCCESS_OR_TERMINATE(zeCommandListClose(cmdListSubDevice));

            SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
            SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueueSubDevice, 1, &cmdListSubDevice, nullptr));
        }

        SUCCESS_OR_TERMINATE(zeEventHostSynchronize(subEvent, std::numeric_limits<uint64_t>::max()));
        if (!useImmediate) {
            SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));
            SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueueSubDevice, std::numeric_limits<uint64_t>::max()));

            SUCCESS_OR_TERMINATE(zeCommandListReset(cmdList));
            SUCCESS_OR_TERMINATE(zeCommandListReset(cmdListSubDevice));
        }

        auto fromSubToRootEvent = events[eventIndex++];
        SUCCESS_OR_TERMINATE(zeCommandListAppendSignalEvent(cmdListSubDevice, fromSubToRootEvent));
        auto rootEvent = events[eventIndex++];

        SUCCESS_OR_TERMINATE(zeCommandListAppendWaitOnEvents(cmdList, 1, &fromSubToRootEvent));
        SUCCESS_OR_TERMINATE(zeCommandListAppendSignalEvent(cmdList, rootEvent));

        if (!useImmediate) {
            SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
            SUCCESS_OR_TERMINATE(zeCommandListClose(cmdListSubDevice));

            SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueueSubDevice, 1, &cmdListSubDevice, nullptr));
            SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
        }

        SUCCESS_OR_TERMINATE(zeEventHostSynchronize(rootEvent, std::numeric_limits<uint64_t>::max()));
        if (!useImmediate) {
            SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));
            SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueueSubDevice, std::numeric_limits<uint64_t>::max()));

            SUCCESS_OR_TERMINATE(zeCommandListReset(cmdList));
        }
    }

    if (copyDevice) {
        if (verbose) {
            std::cout << "Running compute - copy sync" << std::endl;
        }
        auto fromComputeToCopyEvent = events[eventIndex++];
        SUCCESS_OR_TERMINATE(zeCommandListAppendSignalEvent(cmdList, fromComputeToCopyEvent));
        auto copyEvent = events[eventIndex++];

        SUCCESS_OR_TERMINATE(zeCommandListAppendWaitOnEvents(cmdListCopy, 1, &fromComputeToCopyEvent));
        SUCCESS_OR_TERMINATE(zeCommandListAppendSignalEvent(cmdListCopy, copyEvent));

        if (!useImmediate) {
            SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
            SUCCESS_OR_TERMINATE(zeCommandListClose(cmdListCopy));

            SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
            SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueueCopy, 1, &cmdListCopy, nullptr));
        }

        SUCCESS_OR_TERMINATE(zeEventHostSynchronize(copyEvent, std::numeric_limits<uint64_t>::max()));

        if (!useImmediate) {
            SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));
            SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueueCopy, std::numeric_limits<uint64_t>::max()));

            SUCCESS_OR_TERMINATE(zeCommandListReset(cmdList));
            SUCCESS_OR_TERMINATE(zeCommandListReset(cmdListCopy));
        }

        auto fromCopyToComputeEvent = events[eventIndex++];
        SUCCESS_OR_TERMINATE(zeCommandListAppendSignalEvent(cmdListCopy, fromCopyToComputeEvent));
        auto computeEvent = events[eventIndex++];

        SUCCESS_OR_TERMINATE(zeCommandListAppendWaitOnEvents(cmdList, 1, &fromCopyToComputeEvent));
        SUCCESS_OR_TERMINATE(zeCommandListAppendSignalEvent(cmdList, computeEvent));

        if (!useImmediate) {
            SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
            SUCCESS_OR_TERMINATE(zeCommandListClose(cmdListCopy));

            SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueueCopy, 1, &cmdListCopy, nullptr));
            SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
        }

        SUCCESS_OR_TERMINATE(zeEventHostSynchronize(computeEvent, std::numeric_limits<uint64_t>::max()));

        if (!useImmediate) {
            SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));
            SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueueCopy, std::numeric_limits<uint64_t>::max()));
        }
    }

    if (createEvents) {
        for (uint32_t i = 0; i < numEvents; i++) {
            SUCCESS_OR_TERMINATE(zeEventDestroy(events[i]));
        }
        SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool));
    }
    if (cmdListCopy) {
        SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdListCopy));
    }
    if (cmdQueueCopy) {
        SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueueCopy));
    }
    if (cmdListSubDevice) {
        SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdListSubDevice));
    }
    if (cmdQueueSubDevice) {
        SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueueSubDevice));
    }
    if (cmdList) {
        SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    }
    if (cmdQueue) {
        SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    }
}

std::string testEventSyncForMultiTileAndCopy(bool immediate, bool tsEvent) {
    std::ostringstream testStream;

    testStream << "Event Sync For Multi-Tile And Copy "
               << (immediate ? "immediate" : "regular")
               << " command list"
               << " and event pool flags: "
               << (tsEvent ? "ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP" : "ZE_EVENT_POOL_FLAG_HOST_VISIBLE");

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
    constexpr uint32_t bitNumberTestEventSyncForMultiTileAndCopy = 1u;

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

    uint32_t testFlag = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    if (testMask.test(bitNumberTestMemoryTransfer5x)) {
        bool useImmediate = immediateFirst;
        currentTest = testMemoryTransfer5xString(asyncMode, useImmediate, false);

        executeMemoryTransferAndValidate(context, device,
                                         testFlag, useImmediate, asyncMode,
                                         outputValidationSuccessful);
        printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);

        if (outputValidationSuccessful || aubMode) {
            currentTest = testMemoryTransfer5xString(asyncMode, useImmediate, true);
            testFlag = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
            executeMemoryTransferAndValidate(context, device,
                                             testFlag, useImmediate, asyncMode,
                                             outputValidationSuccessful);
            printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);
        }

        useImmediate = !useImmediate;
        if (outputValidationSuccessful || aubMode) {
            currentTest = testMemoryTransfer5xString(asyncMode, useImmediate, false);
            testFlag = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
            executeMemoryTransferAndValidate(context, device,
                                             testFlag, useImmediate, asyncMode,
                                             outputValidationSuccessful);
            printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);
        }

        if (outputValidationSuccessful || aubMode) {
            currentTest = testMemoryTransfer5xString(asyncMode, useImmediate, true);
            testFlag = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
            executeMemoryTransferAndValidate(context, device,
                                             testFlag, useImmediate, asyncMode,
                                             outputValidationSuccessful);
            printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);
        }
    }

    if (testMask.test(bitNumberTestEventSyncForMultiTileAndCopy)) {
        bool useImmediate = true;
        if (outputValidationSuccessful || aubMode) {
            currentTest = testEventSyncForMultiTileAndCopy(useImmediate, false);
            testFlag = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
            executeEventSyncForMultiTileAndCopy(context, device, testFlag, useImmediate, outputValidationSuccessful);
            printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);
        }
        if (outputValidationSuccessful || aubMode) {
            currentTest = testEventSyncForMultiTileAndCopy(useImmediate, true);
            testFlag = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
            executeEventSyncForMultiTileAndCopy(context, device, testFlag, useImmediate, outputValidationSuccessful);
            printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);
        }

        useImmediate = false;
        if (outputValidationSuccessful || aubMode) {
            currentTest = testEventSyncForMultiTileAndCopy(useImmediate, false);
            testFlag = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
            executeEventSyncForMultiTileAndCopy(context, device, testFlag, useImmediate, outputValidationSuccessful);
            printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);
        }
        if (outputValidationSuccessful || aubMode) {
            currentTest = testEventSyncForMultiTileAndCopy(useImmediate, true);
            testFlag = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
            executeEventSyncForMultiTileAndCopy(context, device, testFlag, useImmediate, outputValidationSuccessful);
            printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);
        }
    }

    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;
    return outputValidationSuccessful ? 0 : 1;
}

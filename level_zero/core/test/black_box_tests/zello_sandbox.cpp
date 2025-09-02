/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/include/level_zero/ze_intel_gpu.h"

#include "zello_common.h"
#include "zello_compile.h"

#include <cstring>
#include <sstream>

template <typename T, typename TNoRef = typename std::remove_reference<T>::type>
constexpr inline TNoRef alignUp(T before, size_t alignment) {
    TNoRef mask = static_cast<TNoRef>(alignment - 1);
    return (before + mask) & ~mask;
}
template <typename T>
constexpr inline T *alignUp(T *ptrBefore, size_t alignment) {
    return reinterpret_cast<T *>(alignUp(reinterpret_cast<uintptr_t>(ptrBefore), alignment));
}

void executeImmediateAndRegularCommandLists(ze_context_handle_t &context, ze_device_handle_t &device,
                                            bool &outputValidationSuccessful, bool aubMode, bool asyncMode) {
    ze_module_handle_t module = nullptr;
    ze_kernel_handle_t kernel = nullptr;

    std::string buildLog;
    auto spirV = LevelZeroBlackBoxTests::compileToSpirV(LevelZeroBlackBoxTests::openCLKernelsSource, "", buildLog);
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
        std::cerr << "\nZello Sandbox test Immediate and Regular concurrent execution validation FAILED. Module creation error."
                  << std::endl;
        SUCCESS_OR_TERMINATE_BOOL(false);
    }
    SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));

    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "add_constant";
    SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &kernel));

    ze_command_queue_handle_t cmdQueue = nullptr;
    ze_command_list_handle_t cmdList = nullptr;
    ze_command_list_handle_t immediateCmdList = nullptr;

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.ordinal = LevelZeroBlackBoxTests::getCommandQueueOrdinal(device, false);
    cmdQueueDesc.index = 0;
    LevelZeroBlackBoxTests::selectQueueMode(cmdQueueDesc, !asyncMode);

    SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &immediateCmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));
    SUCCESS_OR_TERMINATE(LevelZeroBlackBoxTests::createCommandList(context, device, cmdList, false));

    const size_t kernelDataSize = 32;
    const int numIteration = 5;
    int constValue = 10;
    const size_t totalKernelDataSize = kernelDataSize * sizeof(int);

    void *sharedBuffer = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceDesc.ordinal = 0;
    deviceDesc.flags = 0;
    deviceDesc.pNext = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    hostDesc.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
    hostDesc.pNext = nullptr;
    hostDesc.flags = 0;

    SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc,
                                          totalKernelDataSize, 1, device, &sharedBuffer));
    memset(sharedBuffer, 0x0, totalKernelDataSize);

    uint32_t groupSizeX = kernelDataSize;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 0, sizeof(sharedBuffer), &sharedBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 1, sizeof(constValue), &constValue));

    ze_group_count_t groupCount;
    groupCount.groupCountX = 1;
    groupCount.groupCountY = 1;
    groupCount.groupCountZ = 1;

    const size_t regularCmdlistBufSize = 4096;
    std::vector<uint8_t> sourceSystemMemory(regularCmdlistBufSize);
    std::vector<uint8_t> destSystemMemory(regularCmdlistBufSize, 0);

    memset(sourceSystemMemory.data(), 1, regularCmdlistBufSize);

    void *deviceMemory = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, regularCmdlistBufSize, regularCmdlistBufSize, device, &deviceMemory));

    ze_event_pool_handle_t eventPool;
    ze_event_handle_t events[3];
    LevelZeroBlackBoxTests::createEventPoolAndEvents(context, device, eventPool, ZE_EVENT_POOL_FLAG_HOST_VISIBLE, false, nullptr, nullptr, 3, events, ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST);

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, deviceMemory, sourceSystemMemory.data(), regularCmdlistBufSize, events[1], 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, destSystemMemory.data(), deviceMemory, regularCmdlistBufSize, events[2], 1, &events[1]));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));

    int valCheck = constValue;
    for (uint32_t iter = 0; iter < numIteration; iter++) {
        SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(immediateCmdList, kernel, &groupCount, events[0], 0, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));

        SUCCESS_OR_TERMINATE(zeEventHostSynchronize(events[0], std::numeric_limits<uint64_t>::max()));
        SUCCESS_OR_TERMINATE(zeEventHostReset(events[0]));

        SUCCESS_OR_TERMINATE(zeEventHostSynchronize(events[2], std::numeric_limits<uint64_t>::max()));
        SUCCESS_OR_TERMINATE(zeEventHostReset(events[1]));
        SUCCESS_OR_TERMINATE(zeEventHostReset(events[2]));

        if (!aubMode) {
            if (!LevelZeroBlackBoxTests::validateToValue<int>(valCheck, sharedBuffer, kernelDataSize)) {
                std::cerr << "regular cmdlist execution mismatch at iteration " << iter << " shared memory to const value\n";
                outputValidationSuccessful = false;
            }

            if (!LevelZeroBlackBoxTests::validate(sourceSystemMemory.data(), destSystemMemory.data(), regularCmdlistBufSize)) {
                std::cerr << "regular cmdlist execution mismatch at iteration " << iter << " source memory and destination memory\n";
                outputValidationSuccessful = false;
            }

            if (outputValidationSuccessful == false) {
                break;
            }
        }

        constValue += 5;
        valCheck += constValue;
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 1, sizeof(constValue), &constValue));

        uint8_t zero = 0;
        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(immediateCmdList, deviceMemory, reinterpret_cast<void *>(&zero),
                                                           sizeof(zero), regularCmdlistBufSize, events[0], 0, nullptr));

        SUCCESS_OR_TERMINATE(zeEventHostSynchronize(events[0], std::numeric_limits<uint64_t>::max()));
        SUCCESS_OR_TERMINATE(zeEventHostReset(events[0]));
    }

    SUCCESS_OR_TERMINATE(zeEventDestroy(events[0]));
    SUCCESS_OR_TERMINATE(zeEventDestroy(events[1]));
    SUCCESS_OR_TERMINATE(zeEventDestroy(events[2]));
    SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool));

    SUCCESS_OR_TERMINATE(zeMemFree(context, sharedBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, deviceMemory));

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(immediateCmdList));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));

    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));
}

std::string testNameImmediateAndRegularCommandLists(bool async) {
    std::ostringstream testStream;

    testStream << "Regular and immediate concurrent execution "
               << (async ? "asynchronous" : "synchronous")
               << " command list mode";

    return testStream.str();
}

void executeMemoryTransferAndValidate(ze_context_handle_t &context, ze_device_handle_t &device,
                                      uint32_t flags, bool useImmediate, bool asyncMode, bool &outputValidationSuccessful) {
    ze_command_queue_handle_t cmdQueue = nullptr;
    ze_command_list_handle_t cmdList = nullptr;

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.ordinal = LevelZeroBlackBoxTests::getCommandQueueOrdinal(device, false);
    cmdQueueDesc.index = 0;
    LevelZeroBlackBoxTests::selectQueueMode(cmdQueueDesc, !asyncMode);
    if (useImmediate) {
        SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdList));
    } else {
        SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));
        SUCCESS_OR_TERMINATE(LevelZeroBlackBoxTests::createCommandList(context, device, cmdList, false));
    }

    ze_event_pool_handle_t eventPool;
    uint32_t numEvents = 10;
    std::vector<ze_event_handle_t> events(numEvents);
    ze_event_pool_flags_t eventPoolFlags = flags;
    LevelZeroBlackBoxTests::createEventPoolAndEvents(context, device, eventPool,
                                                     eventPoolFlags, false, nullptr, nullptr,
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
    outputValidationSuccessful = LevelZeroBlackBoxTests::validateToValue<uint8_t>(value, output.get(), allocSize);

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

    uint32_t queueGroup = LevelZeroBlackBoxTests::getCommandQueueOrdinal(device, false);
    uint32_t copyQueueGroup = LevelZeroBlackBoxTests::getCopyOnlyCommandQueueOrdinal(device);
    uint32_t subDeviceCopyQueueGroup = std::numeric_limits<uint32_t>::max();

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.ordinal = queueGroup;
    cmdQueueDesc.index = 0;
    LevelZeroBlackBoxTests::selectQueueMode(cmdQueueDesc, false);
    if (useImmediate) {
        SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdList));
    } else {
        SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));
        SUCCESS_OR_TERMINATE(LevelZeroBlackBoxTests::createCommandList(context, device, cmdList, queueGroup));
    }

    uint32_t subDevCount = 0;
    auto subDevices = LevelZeroBlackBoxTests::zelloGetSubDevices(device, subDevCount);
    if (subDevCount == 0) {
        if (LevelZeroBlackBoxTests::verbose) {
            std::cout << "Skipping multi-tile - subdevice compute sync" << std::endl;
        }
    } else {
        subDevice = subDevices[0];
        eventPoolDevices.push_back(subDevice);
        uint32_t subDeviceQueueGroup = LevelZeroBlackBoxTests::getCommandQueueOrdinal(subDevice, false);

        subDeviceCopyQueueGroup = LevelZeroBlackBoxTests::getCopyOnlyCommandQueueOrdinal(subDevice);
        if (subDeviceCopyQueueGroup != std::numeric_limits<uint32_t>::max()) {
            copyQueueGroup = subDeviceCopyQueueGroup;
        }

        cmdQueueDesc.ordinal = subDeviceQueueGroup;
        if (useImmediate) {
            SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, subDevice, &cmdQueueDesc, &cmdListSubDevice));
        } else {
            SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, subDevice, &cmdQueueDesc, &cmdQueueSubDevice));
            SUCCESS_OR_TERMINATE(LevelZeroBlackBoxTests::createCommandList(context, subDevice, cmdListSubDevice, subDeviceQueueGroup));
        }
    }

    if (copyQueueGroup == std::numeric_limits<uint32_t>::max()) {
        if (LevelZeroBlackBoxTests::verbose) {
            std::cout << "Skipping compute - copy sync" << std::endl;
        }
    } else {
        copyDevice = device;
        if (subDeviceCopyQueueGroup != std::numeric_limits<uint32_t>::max()) {
            copyDevice = subDevice;
            if (LevelZeroBlackBoxTests::verbose) {
                std::cout << "Using subdevice for copy engine" << std::endl;
            }
        }

        cmdQueueDesc.ordinal = copyQueueGroup;
        if (useImmediate) {
            SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, copyDevice, &cmdQueueDesc, &cmdListCopy));
        } else {
            SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, copyDevice, &cmdQueueDesc, &cmdQueueCopy));
            SUCCESS_OR_TERMINATE(LevelZeroBlackBoxTests::createCommandList(context, copyDevice, cmdListCopy, copyQueueGroup));
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
            eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
            eventDesc.wait = ZE_EVENT_SCOPE_FLAG_DEVICE;
            SUCCESS_OR_TERMINATE(zeEventCreate(eventPool, &eventDesc, eventPtr + i));
        }
    }

    if (subDevice) {
        if (LevelZeroBlackBoxTests::verbose) {
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
        if (LevelZeroBlackBoxTests::verbose) {
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

void executeMemoryCopyOnSystemMemory(ze_context_handle_t &context, ze_device_handle_t &device,
                                     bool &outputValidationSuccessful, bool aubMode) {
    ze_command_list_handle_t cmdList = nullptr;

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.ordinal = LevelZeroBlackBoxTests::getCommandQueueOrdinal(device, false);
    cmdQueueDesc.index = 0;
    LevelZeroBlackBoxTests::selectQueueMode(cmdQueueDesc, true);

    SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdList));

    constexpr size_t allocSize = 0x1000;
    const uint8_t value = 0x55;

    auto memory = malloc(2 * allocSize);
    auto alignedMemory = alignUp(memory, 0x1000);

    ze_external_memmap_sysmem_ext_desc_t sysMemDesc = {ZE_STRUCTURE_TYPE_EXTERNAL_MEMMAP_SYSMEM_EXT_DESC,
                                                       nullptr, alignedMemory, allocSize};

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    hostDesc.flags = 0;
    hostDesc.pNext = &sysMemDesc;

    void *buffer1 = nullptr;
    void *buffer2 = nullptr;

    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, 1, &buffer1));
    hostDesc.pNext = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, 1, &buffer2));

    memset(buffer1, value, allocSize);
    memset(buffer2, 0, allocSize);

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, buffer2, buffer1, allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListHostSynchronize(cmdList, std::numeric_limits<uint64_t>::max()));

    // Validate
    if (!aubMode) {
        outputValidationSuccessful = LevelZeroBlackBoxTests::validateToValue<uint8_t>(value, buffer2, allocSize);
    }

    SUCCESS_OR_TERMINATE(zeMemFree(context, buffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(context, buffer2));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    free(memory);
}

int main(int argc, char *argv[]) {
    constexpr uint32_t bitNumberTestMemoryTransfer5x = 0u;
    constexpr uint32_t bitNumberTestEventSyncForMultiTileAndCopy = 1u;
    constexpr uint32_t bitNumberTestImmediateAndRegularCommandLists = 2u;
    constexpr uint32_t bitNumberTestSystemMemory = 3u;

    const std::string blackBoxName = "Zello Sandbox";
    std::string currentTest;
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);
    bool asyncMode = !LevelZeroBlackBoxTests::isSyncQueueEnabled(argc, argv);
    bool immediateFirst = LevelZeroBlackBoxTests::isImmediateFirst(argc, argv);
    LevelZeroBlackBoxTests::TestBitMask testMask = LevelZeroBlackBoxTests::getTestMask(argc, argv, std::numeric_limits<uint32_t>::max());

    ze_context_handle_t context = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context);
    auto device = devices[0];

    bool outputValidationSuccessful = true;

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    LevelZeroBlackBoxTests::printDeviceProperties(deviceProperties);

    uint32_t testFlag = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    if (testMask.test(bitNumberTestMemoryTransfer5x)) {
        bool useImmediate = immediateFirst;
        currentTest = testMemoryTransfer5xString(asyncMode, useImmediate, false);

        executeMemoryTransferAndValidate(context, device,
                                         testFlag, useImmediate, asyncMode,
                                         outputValidationSuccessful);
        LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);

        if (outputValidationSuccessful || aubMode) {
            currentTest = testMemoryTransfer5xString(asyncMode, useImmediate, true);
            testFlag = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
            executeMemoryTransferAndValidate(context, device,
                                             testFlag, useImmediate, asyncMode,
                                             outputValidationSuccessful);
            LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);
        }

        useImmediate = !useImmediate;
        if (outputValidationSuccessful || aubMode) {
            currentTest = testMemoryTransfer5xString(asyncMode, useImmediate, false);
            testFlag = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
            executeMemoryTransferAndValidate(context, device,
                                             testFlag, useImmediate, asyncMode,
                                             outputValidationSuccessful);
            LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);
        }

        if (outputValidationSuccessful || aubMode) {
            currentTest = testMemoryTransfer5xString(asyncMode, useImmediate, true);
            testFlag = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
            executeMemoryTransferAndValidate(context, device,
                                             testFlag, useImmediate, asyncMode,
                                             outputValidationSuccessful);
            LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);
        }
    }

    if (testMask.test(bitNumberTestEventSyncForMultiTileAndCopy)) {
        bool useImmediate = true;
        if (outputValidationSuccessful || aubMode) {
            currentTest = testEventSyncForMultiTileAndCopy(useImmediate, false);
            testFlag = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
            executeEventSyncForMultiTileAndCopy(context, device, testFlag, useImmediate, outputValidationSuccessful);
            LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);
        }
        if (outputValidationSuccessful || aubMode) {
            currentTest = testEventSyncForMultiTileAndCopy(useImmediate, true);
            testFlag = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
            executeEventSyncForMultiTileAndCopy(context, device, testFlag, useImmediate, outputValidationSuccessful);
            LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);
        }

        useImmediate = false;
        if (outputValidationSuccessful || aubMode) {
            currentTest = testEventSyncForMultiTileAndCopy(useImmediate, false);
            testFlag = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
            executeEventSyncForMultiTileAndCopy(context, device, testFlag, useImmediate, outputValidationSuccessful);
            LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);
        }
        if (outputValidationSuccessful || aubMode) {
            currentTest = testEventSyncForMultiTileAndCopy(useImmediate, true);
            testFlag = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
            executeEventSyncForMultiTileAndCopy(context, device, testFlag, useImmediate, outputValidationSuccessful);
            LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);
        }
    }

    if (testMask.test(bitNumberTestImmediateAndRegularCommandLists)) {
        if (outputValidationSuccessful || aubMode) {
            currentTest = testNameImmediateAndRegularCommandLists(asyncMode);
            executeImmediateAndRegularCommandLists(context, device, outputValidationSuccessful, aubMode, asyncMode);
            LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);
        }

        if (outputValidationSuccessful || aubMode) {
            currentTest = testNameImmediateAndRegularCommandLists(!asyncMode);
            executeImmediateAndRegularCommandLists(context, device, outputValidationSuccessful, aubMode, !asyncMode);
            LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);
        }
    }

    if (testMask.test(bitNumberTestSystemMemory)) {
        if (outputValidationSuccessful || aubMode) {
            currentTest = "executeMemoryCopyOnSystemMemory\n";
            executeMemoryCopyOnSystemMemory(context, device, outputValidationSuccessful, aubMode);
            LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);
        }
    }

    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;
    return outputValidationSuccessful ? 0 : 1;
}

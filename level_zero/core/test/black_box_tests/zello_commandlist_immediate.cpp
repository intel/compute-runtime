/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"
#include "zello_compile.h"

#include <iomanip>
#include <iostream>
#include <limits>

void testAppendMemoryCopy(ze_context_handle_t &context, ze_device_handle_t &device, bool &useSyncCmdQ, bool &validRet, ze_command_list_handle_t &sharedCmdList) {
    const size_t allocSize = 4096;
    char *heapBuffer = new char[allocSize];
    void *zeBuffer = nullptr;
    char stackBuffer[allocSize];
    ze_command_list_handle_t cmdList;
    ze_event_pool_handle_t eventPool, eventPool2;
    ze_event_handle_t event, event2;
    eventPool = eventPool2 = nullptr;
    event = event2 = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceDesc.ordinal = 0;
    deviceDesc.flags = 0;
    deviceDesc.pNext = nullptr;

    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, allocSize, allocSize, device, &zeBuffer));

    for (size_t i = 0; i < allocSize; ++i) {
        heapBuffer[i] = static_cast<char>(i + 1);
    }
    memset(stackBuffer, 0, allocSize);

    if (sharedCmdList == nullptr) {
        ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
        cmdQueueDesc.pNext = nullptr;
        cmdQueueDesc.flags = 0;
        cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
        cmdQueueDesc.ordinal = getCommandQueueOrdinal(device);
        cmdQueueDesc.index = 0;
        selectQueueMode(cmdQueueDesc, useSyncCmdQ);

        SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdList));
    } else {
        cmdList = sharedCmdList;
    }

    if (!useSyncCmdQ) {
        createEventPoolAndEvents(context, device, eventPool, ZE_EVENT_POOL_FLAG_HOST_VISIBLE, 1, &event, ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST);
        createEventPoolAndEvents(context, device, eventPool2, ZE_EVENT_POOL_FLAG_HOST_VISIBLE, 1, &event2, ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST);
    }
    // Copy from heap to device-allocated memory
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, zeBuffer, heapBuffer, allocSize,
                                                       useSyncCmdQ ? nullptr : event, 0, nullptr));
    // Copy from device-allocated memory to stack
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, stackBuffer, zeBuffer, allocSize,
                                                       useSyncCmdQ ? nullptr : event2, 0, nullptr));

    if (!useSyncCmdQ) {
        // If Async mode, use event for syncing copies
        SUCCESS_OR_TERMINATE(zeEventHostSynchronize(event, std::numeric_limits<uint64_t>::max()));
        SUCCESS_OR_TERMINATE(zeEventHostSynchronize(event2, std::numeric_limits<uint64_t>::max()));
    }
    // Validate stack and xe buffers have the original data from heapBuffer
    validRet = (0 == memcmp(heapBuffer, stackBuffer, allocSize));

    delete[] heapBuffer;
    SUCCESS_OR_TERMINATE(zeMemFree(context, zeBuffer));
    if (sharedCmdList == nullptr) {
        SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    }
    SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    SUCCESS_OR_TERMINATE(zeEventDestroy(event2));
    SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool));
    SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool2));
}

void testAppendMemoryCopyRegion(ze_context_handle_t &context, ze_device_handle_t &device, bool useSyncCmdQ, bool &validRet, ze_command_list_handle_t &sharedCmdList) {
    validRet = true;
    ze_command_list_handle_t cmdList;
    ze_event_pool_handle_t eventPool, eventPool2;
    ze_event_handle_t event, event2;
    eventPool = eventPool2 = nullptr;
    event = event2 = nullptr;

    if (sharedCmdList == nullptr) {
        ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
        cmdQueueDesc.pNext = nullptr;
        cmdQueueDesc.flags = 0;
        cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
        cmdQueueDesc.ordinal = getCommandQueueOrdinal(device);
        cmdQueueDesc.index = 0;
        selectQueueMode(cmdQueueDesc, useSyncCmdQ);

        SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdList));
    } else {
        cmdList = sharedCmdList;
    }

    void *dstBuffer = nullptr;
    uint32_t dstWidth = verbose ? 16 : 1024; // width of the dst 2D buffer in bytes
    uint32_t dstHeight = verbose ? 32 : 512; // height of the dst 2D buffer in bytes
    uint32_t dstOriginX = verbose ? 8 : 128; // Offset in bytes
    uint32_t dstOriginY = verbose ? 8 : 144; // Offset in rows
    uint32_t dstSize = dstHeight * dstWidth; // Size of the dst buffer

    void *srcBuffer = nullptr;
    uint32_t srcWidth = verbose ? 24 : 256;  // width of the src 2D buffer in bytes
    uint32_t srcHeight = verbose ? 16 : 384; // height of the src 2D buffer in bytes
    uint32_t srcOriginX = verbose ? 4 : 64;  // Offset in bytes
    uint32_t srcOriginY = verbose ? 4 : 128; // Offset in rows
    uint32_t srcSize = srcHeight * srcWidth; // Size of the src buffer

    uint32_t width = verbose ? 8 : 144;  // width of the region to copy
    uint32_t height = verbose ? 12 : 96; // height of the region to copy
    const ze_copy_region_t dstRegion = {dstOriginX, dstOriginY, 0, width, height, 0};
    const ze_copy_region_t srcRegion = {srcOriginX, srcOriginY, 0, width, height, 0};

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
                                          srcSize, 1, device, &srcBuffer));

    SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc,
                                          dstSize, 1, device, &dstBuffer));

    // Initialize buffers
    uint8_t *stackBuffer = new uint8_t[srcSize];
    for (uint32_t i = 0; i < srcHeight; i++) {
        for (uint32_t j = 0; j < srcWidth; j++) {
            stackBuffer[i * srcWidth + j] = static_cast<uint8_t>(i * srcWidth + j);
        }
    }

    if (!useSyncCmdQ) {
        // Create Event Pool and kernel launch event
        createEventPoolAndEvents(context, device, eventPool, ZE_EVENT_POOL_FLAG_HOST_VISIBLE, 1, &event, ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST);
        createEventPoolAndEvents(context, device, eventPool2, ZE_EVENT_POOL_FLAG_HOST_VISIBLE, 1, &event2, ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST);
    }

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, srcBuffer, stackBuffer,
                                                       srcSize,
                                                       useSyncCmdQ ? nullptr : event,
                                                       0, nullptr));

    int value = 0;
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, dstBuffer, reinterpret_cast<void *>(&value),
                                                       sizeof(value), dstSize, useSyncCmdQ ? nullptr : event2,
                                                       0, nullptr));

    if (!useSyncCmdQ) {
        // If Async mode, use event for syncing copies
        SUCCESS_OR_TERMINATE(zeEventHostSynchronize(event, std::numeric_limits<uint64_t>::max()));
        SUCCESS_OR_TERMINATE(zeEventHostSynchronize(event2, std::numeric_limits<uint64_t>::max()));
        SUCCESS_OR_TERMINATE(zeEventHostReset(event));
        SUCCESS_OR_TERMINATE(zeEventHostReset(event2));
    }

    // Perform the copy
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopyRegion(cmdList, dstBuffer, &dstRegion, dstWidth, 0,
                                                             srcBuffer, &srcRegion, srcWidth, 0,
                                                             useSyncCmdQ ? nullptr : event,
                                                             0, nullptr));

    if (!useSyncCmdQ) {
        // If Async mode, use event for syncing copies
        SUCCESS_OR_TERMINATE(zeEventHostSynchronize(event, std::numeric_limits<uint64_t>::max()));
    }

    uint8_t *dstBufferChar = reinterpret_cast<uint8_t *>(dstBuffer);
    if (verbose) {
        std::cout << "stackBuffer\n";
        for (uint32_t i = 0; i < srcHeight; i++) {
            for (uint32_t j = 0; j < srcWidth; j++) {
                std::cout << std::setw(3) << std::dec
                          << static_cast<unsigned int>(stackBuffer[i * srcWidth + j]) << " ";
            }
            std::cout << "\n";
        }

        std::cout << "dstBuffer\n";
        for (uint32_t i = 0; i < dstHeight; i++) {
            for (uint32_t j = 0; j < dstWidth; j++) {
                std::cout << std::setw(3) << std::dec
                          << static_cast<unsigned int>(dstBufferChar[i * dstWidth + j]) << " ";
            }
            std::cout << "\n";
        }
    }

    uint32_t dstOffset = dstOriginX + dstOriginY * dstWidth;
    uint32_t srcOffset = srcOriginX + srcOriginY * srcWidth;
    for (uint32_t i = 0; i < height; i++) {
        for (uint32_t j = 0; j < width; j++) {
            uint8_t dstVal = dstBufferChar[dstOffset + (i * dstWidth) + j];
            uint8_t srcVal = stackBuffer[srcOffset + (i * srcWidth) + j];
            if (dstVal != srcVal) {
                validRet = false;
            }
        }
    }

    delete[] stackBuffer;
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    if (sharedCmdList == nullptr) {
        SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    }
    SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    SUCCESS_OR_TERMINATE(zeEventDestroy(event2));
    SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool));
    SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool2));
}

void testAppendGpuKernel(ze_context_handle_t &context, ze_device_handle_t &device, bool useSyncCmdQ, bool &validRet, ze_command_list_handle_t &sharedCmdList) {
    constexpr size_t allocSize = 4096;
    constexpr size_t bytesPerThread = sizeof(char);
    constexpr size_t numThreads = allocSize / bytesPerThread;
    ze_module_handle_t module;
    ze_kernel_handle_t kernel;
    ze_command_list_handle_t cmdList;
    ze_event_pool_handle_t eventPool, eventPool2;
    ze_event_handle_t event, event2;
    eventPool = eventPool2 = nullptr;
    event = event2 = nullptr;
    void *srcBuffer = nullptr;
    void *dstBuffer = nullptr;

    std::string buildLog;
    auto moduleBinary = compileToSpirV(memcpyBytesTestKernelSrc, "", buildLog);
    if (buildLog.size() > 0) {
        std::cout << "Build log " << buildLog;
    }
    SUCCESS_OR_TERMINATE((0 == moduleBinary.size()));

    ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(moduleBinary.data());
    moduleDesc.inputSize = moduleBinary.size();
    SUCCESS_OR_TERMINATE(zeModuleCreate(context, device, &moduleDesc, &module, nullptr));

    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "memcpy_bytes";
    SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &kernel));

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(kernel, numThreads, 1U, 1U, &groupSizeX,
                                                  &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE_BOOL(numThreads % groupSizeX == 0);
    if (verbose) {
        std::cout << "Group size : (" << groupSizeX << ", " << groupSizeY << ", " << groupSizeZ
                  << ")" << std::endl;
    }
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernel, groupSizeX, groupSizeY, groupSizeZ));

    if (sharedCmdList == nullptr) {
        ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
        cmdQueueDesc.pNext = nullptr;
        cmdQueueDesc.flags = 0;
        cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
        cmdQueueDesc.ordinal = getCommandQueueOrdinal(device);
        cmdQueueDesc.index = 0;
        selectQueueMode(cmdQueueDesc, useSyncCmdQ);

        SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device, &cmdQueueDesc, &cmdList));
    } else {
        cmdList = sharedCmdList;
    }

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
                                          allocSize, 1, device, &srcBuffer));

    SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc,
                                          allocSize, 1, device, &dstBuffer));

    uint8_t initDataSrc[allocSize];
    memset(initDataSrc, 7, sizeof(initDataSrc));
    uint8_t initDataDst[allocSize];
    memset(initDataDst, 3, sizeof(initDataDst));

    if (!useSyncCmdQ) {
        // Create Event Pool and kernel launch event
        createEventPoolAndEvents(context, device, eventPool, ZE_EVENT_POOL_FLAG_HOST_VISIBLE, 1, &event, ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST);
        createEventPoolAndEvents(context, device, eventPool2, ZE_EVENT_POOL_FLAG_HOST_VISIBLE, 1, &event2, ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST);
    }

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, srcBuffer, initDataSrc,
                                                       sizeof(initDataSrc),
                                                       useSyncCmdQ ? nullptr : event,
                                                       0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, dstBuffer, initDataDst,
                                                       sizeof(initDataDst),
                                                       useSyncCmdQ ? nullptr : event2,
                                                       0, nullptr));

    if (!useSyncCmdQ) {
        // If Async mode, use event for syncing copies
        SUCCESS_OR_TERMINATE(zeEventHostSynchronize(event, std::numeric_limits<uint64_t>::max()));
        SUCCESS_OR_TERMINATE(zeEventHostSynchronize(event2, std::numeric_limits<uint64_t>::max()));
        SUCCESS_OR_TERMINATE(zeEventHostReset(event));
        SUCCESS_OR_TERMINATE(zeEventHostReset(event2));
    }

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 0, sizeof(dstBuffer), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 1, sizeof(srcBuffer), &srcBuffer));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = numThreads / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;
    if (verbose) {
        std::cerr << "Number of groups : (" << dispatchTraits.groupCountX << ", "
                  << dispatchTraits.groupCountY << ", " << dispatchTraits.groupCountZ << ")"
                  << std::endl;
    }
    SUCCESS_OR_TERMINATE_BOOL(dispatchTraits.groupCountX * groupSizeX == allocSize);
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatchTraits,
                                                         useSyncCmdQ ? nullptr : event, 0, nullptr));
    if (!useSyncCmdQ) {
        // If Async mode, use event for syncing copies
        SUCCESS_OR_TERMINATE(zeEventHostSynchronize(event, std::numeric_limits<uint64_t>::max()));
    }

    uint8_t readBackData[allocSize];
    memset(readBackData, 2, sizeof(readBackData));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, readBackData, dstBuffer,
                                                       sizeof(readBackData),
                                                       useSyncCmdQ ? nullptr : event2,
                                                       0, nullptr));
    if (!useSyncCmdQ) {
        // If Async mode, use event for syncing copies
        SUCCESS_OR_TERMINATE(zeEventHostSynchronize(event2, std::numeric_limits<uint64_t>::max()));
    }

    validRet =
        (0 == memcmp(initDataSrc, readBackData, sizeof(readBackData)));
    if (verbose && (false == validRet)) {
        validate(initDataSrc, readBackData, sizeof(readBackData));
    }

    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));

    if (sharedCmdList == nullptr) {
        SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    }

    SUCCESS_OR_TERMINATE(zeEventDestroy(event));
    SUCCESS_OR_TERMINATE(zeEventDestroy(event2));
    SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool));
    SUCCESS_OR_TERMINATE(zeEventPoolDestroy(eventPool2));
    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName("Zello Command List Immediate");
    verbose = isVerbose(argc, argv);
    bool useSyncQueue = isSyncQueueEnabled(argc, argv);
    bool commandListShared = isCommandListShared(argc, argv);
    bool aubMode = isAubMode(argc, argv);

    ze_context_handle_t context = nullptr;
    ze_driver_handle_t driverHandle = nullptr;
    auto devices = zelloInitContextAndGetDevices(context, driverHandle);
    auto device0 = devices[0];

    ze_device_properties_t device0Properties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device0, &device0Properties));
    printDeviceProperties(device0Properties);

    bool outputValidationSuccessful = false;

    ze_command_list_handle_t cmdList = nullptr;
    if (commandListShared) {
        ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
        cmdQueueDesc.pNext = nullptr;
        cmdQueueDesc.flags = 0;
        cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
        cmdQueueDesc.ordinal = getCommandQueueOrdinal(device0);
        cmdQueueDesc.index = 0;
        selectQueueMode(cmdQueueDesc, useSyncQueue);
        SUCCESS_OR_TERMINATE(zeCommandListCreateImmediate(context, device0, &cmdQueueDesc, &cmdList));
    }

    std::string currentTest;
    currentTest = "Standard Memory Copy";
    testAppendMemoryCopy(context, device0, useSyncQueue, outputValidationSuccessful, cmdList);
    printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);

    if (outputValidationSuccessful || aubMode) {
        currentTest = "Memory Copy Region";
        testAppendMemoryCopyRegion(context, device0, useSyncQueue, outputValidationSuccessful, cmdList);
        printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);
    }

    if (outputValidationSuccessful || aubMode) {
        currentTest = "Launch GPU Kernel";
        testAppendGpuKernel(context, device0, useSyncQueue, outputValidationSuccessful, cmdList);
        printResult(aubMode, outputValidationSuccessful, blackBoxName, currentTest);
    }

    if (commandListShared) {
        SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    }

    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    int resultOnFailure = aubMode ? 0 : 1;
    return outputValidationSuccessful ? 0 : resultOnFailure;
}

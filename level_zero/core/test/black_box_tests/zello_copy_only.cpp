/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>

#include "zello_common.h"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>

void testCopyBetweenHeapDeviceAndStack(ze_context_handle_t &context, ze_device_handle_t &device, bool &validRet) {
    const size_t allocSize = 4096 + 7; // +7 to brake alignment and make it harder
    auto heapBuffer = std::make_unique<char[]>(allocSize);
    void *buffer1 = nullptr;
    void *buffer2 = nullptr;
    auto stackBuffer = std::make_unique<char[]>(allocSize);

    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    uint32_t copyQueueGroup = LevelZeroBlackBoxTests::getCopyOnlyCommandQueueOrdinal(device);
    if (copyQueueGroup == std::numeric_limits<uint32_t>::max()) {
        std::cout << "No Copy queue group found. Skipping test run\n";
        validRet = true;
        return;
    }

    cmdQueueDesc.pNext = nullptr;
    cmdQueueDesc.flags = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    cmdQueueDesc.ordinal = copyQueueGroup;
    cmdQueueDesc.index = 0;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));

    ze_command_list_desc_t cmdListDesc = {};
    cmdListDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
    cmdListDesc.pNext = nullptr;
    cmdListDesc.flags = 0;
    cmdListDesc.commandQueueGroupOrdinal = copyQueueGroup;
    SUCCESS_OR_TERMINATE(zeCommandListCreate(context, device, &cmdListDesc, &cmdList));

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceDesc.ordinal = 0;
    deviceDesc.flags = 0;
    deviceDesc.pNext = nullptr;

    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, allocSize, allocSize, device, &buffer1));
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, allocSize, allocSize, device, &buffer2));

    for (size_t i = 0; i < allocSize; ++i) {
        heapBuffer[i] = static_cast<char>(i + 1);
    }
    memset(stackBuffer.get(), 0, allocSize);

    // Copy from heap to device-allocated memory
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, buffer1, heapBuffer.get(), allocSize,
                                                       nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, buffer2, buffer1, allocSize,
                                                       nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));

    // Copy from device-allocated memory to stack
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, stackBuffer.get(), buffer2, allocSize,
                                                       nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));
    // Validate stack and ze buffers have the original data from heapBuffer
    validRet = LevelZeroBlackBoxTests::validate(heapBuffer.get(), stackBuffer.get(), allocSize);

    SUCCESS_OR_TERMINATE(zeMemFree(context, buffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(context, buffer2));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
}

void testCopyBetweenHostMemAndDeviceMem(ze_context_handle_t &context, ze_device_handle_t &device, bool &validRet) {
    const size_t allocSize = 4096 + 7; // +7 to brake alignment and make it harder
    char *hostBuffer = nullptr;
    void *deviceBuffer = nullptr;
    auto stackBuffer = std::make_unique<char[]>(allocSize);
    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    uint32_t copyQueueGroup = LevelZeroBlackBoxTests::getCopyOnlyCommandQueueOrdinal(device);
    if (copyQueueGroup == std::numeric_limits<uint32_t>::max()) {
        std::cout << "No Copy queue group found. Skipping test run\n";
        validRet = true;
        return;
    }

    cmdQueueDesc.pNext = nullptr;
    cmdQueueDesc.flags = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    cmdQueueDesc.ordinal = copyQueueGroup;
    cmdQueueDesc.index = 0;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));

    ze_command_list_desc_t cmdListDesc = {};
    cmdListDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
    cmdListDesc.pNext = nullptr;
    cmdListDesc.flags = 0;
    cmdListDesc.commandQueueGroupOrdinal = copyQueueGroup;
    SUCCESS_OR_TERMINATE(zeCommandListCreate(context, device, &cmdListDesc, &cmdList));

    ze_host_mem_alloc_desc_t hostDesc = {};
    hostDesc.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
    hostDesc.pNext = nullptr;
    hostDesc.flags = 0;
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, 1, (void **)(&hostBuffer)));

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceDesc.ordinal = 0;
    deviceDesc.flags = 0;
    deviceDesc.pNext = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, allocSize, allocSize, device, &deviceBuffer));

    for (size_t i = 0; i < allocSize; ++i) {
        hostBuffer[i] = static_cast<char>(i + 1);
    }
    memset(stackBuffer.get(), 0, allocSize);

    // Copy from host-allocated to device-allocated memory
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, deviceBuffer, hostBuffer, allocSize,
                                                       nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));

    // Copy from device-allocated memory to stack
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, stackBuffer.get(), deviceBuffer, allocSize,
                                                       nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    // Validate stack and ze deviceBuffers have the original data from hostBuffer
    validRet = LevelZeroBlackBoxTests::validate(hostBuffer, stackBuffer.get(), allocSize);

    SUCCESS_OR_TERMINATE(zeMemFree(context, hostBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, deviceBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
}

void testRegionCopyOf2DSharedMem(ze_context_handle_t &context, ze_device_handle_t &device, bool &validRet) {
    validRet = true;

    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    uint32_t copyQueueGroup = LevelZeroBlackBoxTests::getCopyOnlyCommandQueueOrdinal(device);
    if (copyQueueGroup == std::numeric_limits<uint32_t>::max()) {
        std::cout << "No Copy queue group found. Skipping test run\n";
        validRet = true;
        return;
    }

    cmdQueueDesc.pNext = nullptr;
    cmdQueueDesc.flags = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    cmdQueueDesc.ordinal = copyQueueGroup;
    cmdQueueDesc.index = 0;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));

    ze_command_list_desc_t cmdListDesc = {};
    cmdListDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
    cmdListDesc.pNext = nullptr;
    cmdListDesc.flags = 0;
    cmdListDesc.commandQueueGroupOrdinal = copyQueueGroup;
    SUCCESS_OR_TERMINATE(zeCommandListCreate(context, device, &cmdListDesc, &cmdList));

    void *dstBuffer = nullptr;
    uint32_t dstWidth = LevelZeroBlackBoxTests::verbose ? 16 : 256;  // width of the dst 2D buffer in bytes
    uint32_t dstHeight = LevelZeroBlackBoxTests::verbose ? 32 : 128; // height of the dst 2D buffer in bytes
    uint32_t dstOriginX = LevelZeroBlackBoxTests::verbose ? 8 : 32;  // Offset in bytes
    uint32_t dstOriginY = LevelZeroBlackBoxTests::verbose ? 8 : 64;  // Offset in rows
    uint32_t dstSize = dstHeight * dstWidth;                         // Size of the dst buffer

    void *srcBuffer = nullptr;
    uint32_t srcWidth = LevelZeroBlackBoxTests::verbose ? 16 : 256;  // width of the dst 2D buffer in bytes
    uint32_t srcHeight = LevelZeroBlackBoxTests::verbose ? 32 : 128; // height of the dst 2D buffer in bytes
    uint32_t srcOriginX = LevelZeroBlackBoxTests::verbose ? 8 : 32;  // Offset in bytes
    uint32_t srcOriginY = LevelZeroBlackBoxTests::verbose ? 8 : 64;  // Offset in rows
    uint32_t srcSize = dstHeight * dstWidth;                         // Size of the dst buffer

    uint32_t width = LevelZeroBlackBoxTests::verbose ? 8 : 64;   // width of the region to copy
    uint32_t height = LevelZeroBlackBoxTests::verbose ? 12 : 32; // height of the region to copy
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
    SUCCESS_OR_TERMINATE(
        zeMemAllocShared(context, &deviceDesc, &hostDesc,
                         srcSize, 1, device, &srcBuffer));

    SUCCESS_OR_TERMINATE(
        zeMemAllocShared(context, &deviceDesc, &hostDesc,
                         dstSize, 1, device, &dstBuffer));

    // Initialize buffers
    // dstBuffer and srcBuffer are shared allocations, so they have UVA between host and device
    // and there's no need to perform explicit copies
    uint8_t *srcBufferChar = reinterpret_cast<uint8_t *>(srcBuffer);
    for (uint32_t i = 0; i < srcHeight; i++) {
        for (uint32_t j = 0; j < srcWidth; j++) {
            srcBufferChar[i * srcWidth + j] = static_cast<uint8_t>(i * srcWidth + j);
        }
    }

    int value = 0;
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, dstBuffer, reinterpret_cast<void *>(&value),
                                                       sizeof(value), dstSize, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));

    // Perform the copy
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopyRegion(cmdList, dstBuffer, &dstRegion, dstWidth, 0,
                                                             srcBuffer, &srcRegion, srcWidth, 0,
                                                             nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    uint8_t *dstBufferChar = reinterpret_cast<uint8_t *>(dstBuffer);
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "srcBufferChar\n";
        for (uint32_t i = 0; i < srcHeight; i++) {
            for (uint32_t j = 0; j < srcWidth; j++) {
                std::cout << std::setw(3) << std::dec << static_cast<unsigned int>(srcBufferChar[i * srcWidth + j]) << " ";
            }
            std::cout << "\n";
        }

        std::cout << "dstBuffer\n";
        for (uint32_t i = 0; i < dstHeight; i++) {
            for (uint32_t j = 0; j < dstWidth; j++) {
                std::cout << std::setw(3) << std::dec << static_cast<unsigned int>(dstBufferChar[i * dstWidth + j]) << " ";
            }
            std::cout << "\n";
        }
    }

    uint32_t dstOffset = dstOriginX + dstOriginY * dstWidth;
    uint32_t srcOffset = srcOriginX + srcOriginY * srcWidth;
    for (uint32_t i = 0; i < height; i++) {
        for (uint32_t j = 0; j < width; j++) {
            uint8_t dstVal = dstBufferChar[dstOffset + (i * dstWidth) + j];
            uint8_t srcVal = srcBufferChar[srcOffset + (i * srcWidth) + j];
            if (dstVal != srcVal) {
                validRet = false;
            }
        }
    }
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
}

void testSharedMemDataAccessWithoutCopy(ze_context_handle_t &context, ze_device_handle_t &device, bool &validRet) {
    const size_t allocSize = 4096;
    char pattern0 = 5;
    const size_t pattern1Size = 8;
    auto pattern1 = std::make_unique<char[]>(pattern1Size);
    void *buffer0 = nullptr;
    void *buffer1 = nullptr;

    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    uint32_t copyQueueGroup = LevelZeroBlackBoxTests::getCopyOnlyCommandQueueOrdinal(device);
    if (copyQueueGroup == std::numeric_limits<uint32_t>::max()) {
        std::cout << "No Copy queue group found. Skipping test run\n";
        validRet = true;
        return;
    }

    cmdQueueDesc.pNext = nullptr;
    cmdQueueDesc.flags = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    cmdQueueDesc.ordinal = copyQueueGroup;
    cmdQueueDesc.index = 0;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));

    ze_command_list_desc_t cmdListDesc = {};
    cmdListDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
    cmdListDesc.pNext = nullptr;
    cmdListDesc.flags = 0;
    cmdListDesc.commandQueueGroupOrdinal = copyQueueGroup;
    SUCCESS_OR_TERMINATE(zeCommandListCreate(context, device, &cmdListDesc, &cmdList));

    // Initialize buffers
    // buffer0 and buffer1 are shared allocations, so they have UVA between host and device
    // and there's no need to perform explicit copies
    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceDesc.ordinal = 0;
    deviceDesc.flags = 0;
    deviceDesc.pNext = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    hostDesc.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
    hostDesc.pNext = nullptr;
    hostDesc.flags = 0;

    SUCCESS_OR_TERMINATE(
        zeMemAllocShared(context, &deviceDesc, &hostDesc,
                         allocSize, 1, device, &buffer0));

    SUCCESS_OR_TERMINATE(
        zeMemAllocShared(context, &deviceDesc, &hostDesc,
                         allocSize, 1, device, &buffer1));

    // Fibonacci
    pattern1[0] = 1;
    pattern1[1] = 2;
    for (size_t i = 2; i < pattern1Size; i++) {
        pattern1[i] = pattern1[i - 1] + pattern1[i - 2];
    }

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, buffer0, &pattern0, sizeof(pattern0), allocSize,
                                                       nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, buffer1, pattern1.get(), pattern1Size, allocSize,
                                                       nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    validRet = true;
    uint8_t *bufferChar0 = reinterpret_cast<uint8_t *>(buffer0);
    for (size_t i = 0; i < allocSize; ++i) {
        if (bufferChar0[i] != pattern0) {
            validRet = false;
            if (LevelZeroBlackBoxTests::verbose) {
                std::cout << "dstBufferChar0[" << i << " ] "
                          << static_cast<unsigned int>(bufferChar0[i])
                          << "!= pattern0 " << pattern0 << "\n";
            }
            break;
        }
    }

    if (validRet == true) {
        uint8_t *bufferChar1 = reinterpret_cast<uint8_t *>(buffer1);
        size_t j = 0;
        for (size_t i = 0; i < allocSize; i++) {
            if (bufferChar1[i] != pattern1[j]) {
                validRet = false;
                if (LevelZeroBlackBoxTests::verbose) {
                    std::cout << "dstBufferChar1[" << i << " ] "
                              << static_cast<unsigned int>(bufferChar1[i])
                              << "!= pattern1[" << j << " ] "
                              << static_cast<unsigned int>(pattern1[j]) << "\n";
                }
                break;
            }
            j++;
            if (j >= pattern1Size) {
                j = 0;
            }
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(context, buffer0));
    SUCCESS_OR_TERMINATE(zeMemFree(context, buffer1));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
}

void testRegionCopyOf3DSharedMem(ze_context_handle_t &context, ze_device_handle_t &device, bool &validRet) {
    validRet = true;

    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    uint32_t copyQueueGroup = LevelZeroBlackBoxTests::getCopyOnlyCommandQueueOrdinal(device);
    if (copyQueueGroup == std::numeric_limits<uint32_t>::max()) {
        std::cout << "No Copy queue group found. Skipping test run\n";
        validRet = true;
        return;
    }

    cmdQueueDesc.pNext = nullptr;
    cmdQueueDesc.flags = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    cmdQueueDesc.ordinal = copyQueueGroup;
    cmdQueueDesc.index = 0;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));

    ze_command_list_desc_t cmdListDesc = {};
    cmdListDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
    cmdListDesc.pNext = nullptr;
    cmdListDesc.flags = 0;
    cmdListDesc.commandQueueGroupOrdinal = copyQueueGroup;
    SUCCESS_OR_TERMINATE(zeCommandListCreate(context, device, &cmdListDesc, &cmdList));

    void *dstBuffer = nullptr;
    uint32_t dstWidth = LevelZeroBlackBoxTests::verbose ? 8 : 64;  // width of the dst 3D buffer in bytes
    uint32_t dstHeight = LevelZeroBlackBoxTests::verbose ? 8 : 64; // height of the dst 3D buffer in bytes
    uint32_t dstDepth = LevelZeroBlackBoxTests::verbose ? 2 : 4;   // depth of the dst 3D buffer in bytes
    uint32_t dstOriginX = 0;                                       // Offset in bytes
    uint32_t dstOriginY = 0;                                       // Offset in rows
    uint32_t dstOriginZ = 0;                                       // Offset in rows
    uint32_t dstSize = dstHeight * dstWidth * dstDepth;            // Size of the dst buffer

    void *srcBuffer = nullptr;
    uint32_t srcWidth = LevelZeroBlackBoxTests::verbose ? 8 : 64;  // width of the src 3D buffer in bytes
    uint32_t srcHeight = LevelZeroBlackBoxTests::verbose ? 8 : 64; // height of the src 3D buffer in bytes
    uint32_t srcDepth = LevelZeroBlackBoxTests::verbose ? 2 : 4;   // depth of the src 3D buffer in bytes
    uint32_t srcOriginX = 0;                                       // Offset in bytes
    uint32_t srcOriginY = 0;                                       // Offset in rows
    uint32_t srcOriginZ = 0;                                       // Offset in rows
    uint32_t srcSize = srcHeight * srcWidth * srcDepth;            // Size of the src buffer

    uint32_t width = LevelZeroBlackBoxTests::verbose ? 8 : 64;  // width of the region to copy
    uint32_t height = LevelZeroBlackBoxTests::verbose ? 8 : 64; // height of the region to copy
    uint32_t depth = LevelZeroBlackBoxTests::verbose ? 2 : 4;   // height of the region to copy
    const ze_copy_region_t dstRegion = {dstOriginX, dstOriginY, dstOriginZ, width, height, depth};
    const ze_copy_region_t srcRegion = {srcOriginX, srcOriginY, dstOriginZ, width, height, depth};

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceDesc.ordinal = 0;
    deviceDesc.flags = 0;
    deviceDesc.pNext = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    hostDesc.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
    hostDesc.pNext = nullptr;
    hostDesc.flags = 0;
    SUCCESS_OR_TERMINATE(
        zeMemAllocShared(context, &deviceDesc, &hostDesc,
                         srcSize, 1, device, &srcBuffer));

    SUCCESS_OR_TERMINATE(
        zeMemAllocShared(context, &deviceDesc, &hostDesc,
                         dstSize, 1, device, &dstBuffer));

    // Initialize buffers
    // dstBuffer and srcBuffer are shared allocations, so they have UVA between host and device
    // and there's no need to perform explicit copies
    uint8_t *srcBufferChar = reinterpret_cast<uint8_t *>(srcBuffer);
    for (uint32_t i = 0; i < srcDepth; i++) {
        for (uint32_t j = 0; j < srcHeight; j++) {
            for (uint32_t k = 0; k < srcWidth; k++) {
                size_t index = (i * srcWidth * srcHeight) + (j * srcWidth) + k;
                srcBufferChar[index] = static_cast<uint8_t>(index);
            }
        }
    }

    int value = 0;
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, dstBuffer, reinterpret_cast<void *>(&value),
                                                       sizeof(value), dstSize, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));

    // Perform the copy
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopyRegion(cmdList, dstBuffer, &dstRegion, dstWidth, (dstWidth * dstHeight),
                                                             srcBuffer, &srcRegion, srcWidth, (srcWidth * srcHeight),
                                                             nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    uint8_t *dstBufferChar = reinterpret_cast<uint8_t *>(dstBuffer);
    if (LevelZeroBlackBoxTests::verbose) {
        std::cout << "srcBufferChar\n";
        for (uint32_t i = 0; i < srcDepth; i++) {
            for (uint32_t j = 0; j < srcHeight; j++) {
                for (uint32_t k = 0; k < srcWidth; k++) {
                    size_t index = (i * srcWidth * srcHeight) + (j * srcWidth) + k;
                    std::cout << std::setw(3) << std::dec << static_cast<unsigned int>(srcBufferChar[index]) << " ";
                }
                std::cout << "\n";
            }
            std::cout << "\n";
        }

        std::cout << "dstBuffer\n";
        for (uint32_t i = 0; i < dstDepth; i++) {
            for (uint32_t j = 0; j < dstHeight; j++) {
                for (uint32_t k = 0; k < dstWidth; k++) {
                    size_t index = (i * dstWidth * dstHeight) + (j * dstWidth) + k;
                    std::cout << std::setw(3) << std::dec << static_cast<unsigned int>(dstBufferChar[index]) << " ";
                }
                std::cout << "\n";
            }
            std::cout << "\n";
        }
    }

    uint32_t dstOffset = dstOriginX + dstOriginY * dstWidth + dstOriginZ * dstDepth * dstWidth;
    uint32_t srcOffset = srcOriginX + srcOriginY * srcWidth + srcOriginZ * srcDepth * srcWidth;
    for (uint32_t i = 0; i < depth; i++) {
        for (uint32_t j = 0; j < height; j++) {
            for (uint32_t k = 0; k < width; k++) {
                uint8_t dstVal = dstBufferChar[dstOffset + (i * dstWidth * dstHeight) + (j * dstWidth) + k];
                uint8_t srcVal = srcBufferChar[srcOffset + (i * srcWidth * srcHeight) + (j * srcWidth) + k];
                if (dstVal != srcVal) {
                    validRet = false;
                }
            }
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello Copy Only";
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);

    ze_context_handle_t context = nullptr;
    ze_driver_handle_t driverHandle = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context, driverHandle);
    auto device = devices[0];

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    LevelZeroBlackBoxTests::printDeviceProperties(deviceProperties);

    bool outputValidationSuccessful = true;
    testCopyBetweenHeapDeviceAndStack(context, device, outputValidationSuccessful);
    if (outputValidationSuccessful || aubMode) {
        testCopyBetweenHostMemAndDeviceMem(context, device, outputValidationSuccessful);
    }
    if (outputValidationSuccessful || aubMode) {
        testRegionCopyOf2DSharedMem(context, device, outputValidationSuccessful);
    }
    if (outputValidationSuccessful || aubMode) {
        testSharedMemDataAccessWithoutCopy(context, device, outputValidationSuccessful);
    }
    if (outputValidationSuccessful || aubMode) {
        testRegionCopyOf3DSharedMem(context, device, outputValidationSuccessful);
    }

    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName);
    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;
    return (outputValidationSuccessful ? 0 : 1);
}

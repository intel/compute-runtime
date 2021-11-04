/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"

#include <iomanip>

extern bool verbose;
bool verbose = false;

void testAppendMemoryCopyFromHeapToDeviceToStack(ze_context_handle_t context, ze_device_handle_t &device, bool &validRet) {
    const size_t allocSize = 4096 + 7; // +7 to break alignment and make it harder
    char *heapBuffer = new char[allocSize];
    void *zeBuffer = nullptr;
    char stackBuffer[allocSize];

    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;

    cmdQueue = createCommandQueue(context, device, nullptr);
    SUCCESS_OR_TERMINATE(createCommandList(context, device, cmdList));

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

    // Copy from heap to device-allocated memory
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, zeBuffer, heapBuffer, allocSize,
                                                       nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));

    // Copy from device-allocated memory to stack
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, stackBuffer, zeBuffer, allocSize,
                                                       nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint32_t>::max()));

    // Validate stack and ze buffers have the original data from heapBuffer
    validRet = (0 == memcmp(heapBuffer, stackBuffer, allocSize));

    delete[] heapBuffer;
    SUCCESS_OR_TERMINATE(zeMemFree(context, zeBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
}

void testAppendMemoryCopyFromHostToDeviceToStack(ze_context_handle_t context, ze_device_handle_t &device, bool &validRet) {
    const size_t allocSize = 4096 + 7; // +7 to break alignment and make it harder
    char *hostBuffer;
    void *zeBuffer = nullptr;
    char stackBuffer[allocSize];

    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;

    cmdQueue = createCommandQueue(context, device, nullptr);
    SUCCESS_OR_TERMINATE(createCommandList(context, device, cmdList));

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

    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, allocSize, allocSize, device, &zeBuffer));

    for (size_t i = 0; i < allocSize; ++i) {
        hostBuffer[i] = static_cast<char>(i + 1);
    }
    memset(stackBuffer, 0, allocSize);

    // Copy from host-allocated to device-allocated memory
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, zeBuffer, hostBuffer, allocSize,
                                                       nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));

    // Copy from device-allocated memory to stack
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, stackBuffer, zeBuffer, allocSize,
                                                       nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint32_t>::max()));

    // Validate stack and ze buffers have the original data from hostBuffer
    validRet = (0 == memcmp(hostBuffer, stackBuffer, allocSize));

    SUCCESS_OR_TERMINATE(zeMemFree(context, hostBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, zeBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
}

void testAppendMemoryCopy2DRegion(ze_context_handle_t context, ze_device_handle_t &device, bool &validRet) {
    validRet = true;

    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;

    cmdQueue = createCommandQueue(context, device, nullptr);
    SUCCESS_OR_TERMINATE(createCommandList(context, device, cmdList));

    void *dstBuffer = nullptr;
    uint32_t dstWidth = verbose ? 16 : 256;  // width of the dst 2D buffer in bytes
    uint32_t dstHeight = verbose ? 32 : 128; // height of the dst 2D buffer in bytes
    uint32_t dstOriginX = verbose ? 8 : 32;  // Offset in bytes
    uint32_t dstOriginY = verbose ? 8 : 64;  // Offset in rows
    uint32_t dstSize = dstHeight * dstWidth; // Size of the dst buffer

    void *srcBuffer = nullptr;
    uint32_t srcWidth = verbose ? 24 : 128;  // width of the src 2D buffer in bytes
    uint32_t srcHeight = verbose ? 16 : 96;  // height of the src 2D buffer in bytes
    uint32_t srcOriginX = verbose ? 4 : 16;  // Offset in bytes
    uint32_t srcOriginY = verbose ? 4 : 32;  // Offset in rows
    uint32_t srcSize = srcHeight * srcWidth; // Size of the src buffer

    uint32_t width = verbose ? 8 : 64;   // width of the region to copy
    uint32_t height = verbose ? 12 : 32; // height of the region to copy
    const ze_copy_region_t dstRegion = {dstOriginX, dstOriginY, 0, width, height, 0};
    const ze_copy_region_t srcRegion = {srcOriginX, srcOriginY, 0, width, height, 0};

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceDesc.pNext = nullptr;
    deviceDesc.ordinal = 0;
    deviceDesc.flags = 0;

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
                                                             const_cast<const void *>(srcBuffer), &srcRegion, srcWidth, 0,
                                                             nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint32_t>::max()));

    uint8_t *dstBufferChar = reinterpret_cast<uint8_t *>(dstBuffer);
    if (verbose) {
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

void testAppendMemoryFillWithSomePattern(ze_context_handle_t context, ze_device_handle_t &device, bool &validRet) {
    const size_t allocSize = 4096 + 7;

    char pattern0 = 5;
    const size_t pattern1Size = 8;
    char *pattern1 = new char[pattern1Size];
    void *zeBuffer0 = nullptr;
    void *zeBuffer1 = nullptr;

    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;

    cmdQueue = createCommandQueue(context, device, nullptr);
    SUCCESS_OR_TERMINATE(createCommandList(context, device, cmdList));

    // Initialize buffers
    // zeBuffer0 and zeBuffer1 are shared allocations, so they have UVA between host and device
    // and there's no need to perform explicit copies
    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceDesc.pNext = nullptr;
    deviceDesc.ordinal = 0;
    deviceDesc.flags = 0;

    ze_host_mem_alloc_desc_t hostDesc = {};
    hostDesc.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
    hostDesc.pNext = nullptr;
    hostDesc.flags = 0;

    SUCCESS_OR_TERMINATE(
        zeMemAllocShared(context, &deviceDesc, &hostDesc,
                         allocSize, 1, device, &zeBuffer0));

    SUCCESS_OR_TERMINATE(
        zeMemAllocShared(context, &deviceDesc, &hostDesc,
                         allocSize, 1, device, &zeBuffer1));

    // Fibonacci
    pattern1[0] = 1;
    pattern1[1] = 2;
    for (size_t i = 2; i < pattern1Size; i++) {
        pattern1[i] = pattern1[i - 1] + pattern1[i - 2];
    }

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, zeBuffer0, &pattern0, sizeof(pattern0), allocSize,
                                                       nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, zeBuffer1, pattern1, pattern1Size, allocSize,
                                                       nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint32_t>::max()));

    validRet = true;
    uint8_t *zeBufferChar0 = reinterpret_cast<uint8_t *>(zeBuffer0);
    for (size_t i = 0; i < allocSize; ++i) {
        if (zeBufferChar0[i] != pattern0) {
            validRet = false;
            if (verbose) {
                std::cout << "dstBufferChar0[" << i << " ] "
                          << static_cast<unsigned int>(zeBufferChar0[i])
                          << "!= pattern0 " << pattern0 << "\n";
            }
            break;
        }
    }

    if (validRet == true) {
        uint8_t *zeBufferChar1 = reinterpret_cast<uint8_t *>(zeBuffer1);
        for (size_t i = 0; i < allocSize; i++) {
            if (zeBufferChar1[i] != pattern1[i % pattern1Size]) {
                validRet = false;
                if (verbose) {
                    std::cout << "dstBufferChar1[" << i << " ] "
                              << static_cast<unsigned int>(zeBufferChar1[i])
                              << "!= pattern1[" << i % pattern1Size << " ] "
                              << static_cast<unsigned int>(pattern1[i % pattern1Size]) << "\n";
                }
                break;
            }
        }
    }

    delete[] pattern1;
    SUCCESS_OR_TERMINATE(zeMemFree(context, zeBuffer0));
    SUCCESS_OR_TERMINATE(zeMemFree(context, zeBuffer1));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
}

void testAppendMemoryCopy3DRegion(ze_context_handle_t context, ze_device_handle_t &device, bool &validRet) {
    validRet = true;

    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;

    cmdQueue = createCommandQueue(context, device, nullptr);
    SUCCESS_OR_TERMINATE(createCommandList(context, device, cmdList));

    void *dstBuffer = nullptr;
    uint32_t dstWidth = verbose ? 8 : 64;               // width of the dst 3D buffer in bytes
    uint32_t dstHeight = verbose ? 8 : 64;              // height of the dst 3D buffer in bytes
    uint32_t dstDepth = verbose ? 2 : 4;                // depth of the dst 3D buffer in bytes
    uint32_t dstOriginX = 0;                            // Offset in bytes
    uint32_t dstOriginY = 0;                            // Offset in rows
    uint32_t dstOriginZ = 0;                            // Offset in rows
    uint32_t dstSize = dstHeight * dstWidth * dstDepth; // Size of the dst buffer

    void *srcBuffer = nullptr;
    uint32_t srcWidth = verbose ? 8 : 64;               // width of the src 3D buffer in bytes
    uint32_t srcHeight = verbose ? 8 : 64;              // height of the src 3D buffer in bytes
    uint32_t srcDepth = verbose ? 2 : 4;                // depth of the src 3D buffer in bytes
    uint32_t srcOriginX = 0;                            // Offset in bytes
    uint32_t srcOriginY = 0;                            // Offset in rows
    uint32_t srcOriginZ = 0;                            // Offset in rows
    uint32_t srcSize = srcHeight * srcWidth * srcDepth; // Size of the src buffer

    uint32_t width = verbose ? 8 : 64;  // width of the region to copy
    uint32_t height = verbose ? 8 : 64; // height of the region to copy
    uint32_t depth = verbose ? 2 : 4;   // height of the region to copy
    const ze_copy_region_t dstRegion = {dstOriginX, dstOriginY, dstOriginZ, width, height, depth};
    const ze_copy_region_t srcRegion = {srcOriginX, srcOriginY, dstOriginZ, width, height, depth};

    ze_device_mem_alloc_desc_t deviceDesc = {};
    deviceDesc.stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC;
    deviceDesc.pNext = nullptr;
    deviceDesc.ordinal = 0;
    deviceDesc.flags = 0;

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
                                                             const_cast<const void *>(srcBuffer), &srcRegion, srcWidth, (srcWidth * srcHeight),
                                                             nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint32_t>::max()));

    uint8_t *dstBufferChar = reinterpret_cast<uint8_t *>(dstBuffer);
    if (verbose) {
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
    verbose = isVerbose(argc, argv);

    ze_context_handle_t context = nullptr;
    auto devices = zelloInitContextAndGetDevices(context);
    auto device = devices[0];
    bool outputValidationSuccessful = false;

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    std::cout << "Device : \n"
              << " * name : " << deviceProperties.name << "\n"
              << " * vendorId : " << std::hex << deviceProperties.vendorId << "\n";

    testAppendMemoryCopyFromHeapToDeviceToStack(context, device, outputValidationSuccessful);
    if (outputValidationSuccessful)
        testAppendMemoryCopyFromHostToDeviceToStack(context, device, outputValidationSuccessful);
    if (outputValidationSuccessful)
        testAppendMemoryCopy2DRegion(context, device, outputValidationSuccessful);
    if (outputValidationSuccessful)
        testAppendMemoryFillWithSomePattern(context, device, outputValidationSuccessful);
    if (outputValidationSuccessful)
        testAppendMemoryCopy3DRegion(context, device, outputValidationSuccessful);

    SUCCESS_OR_TERMINATE(zeContextDestroy(context));
    std::cout << "\nZello Copy Results validation " << (outputValidationSuccessful ? "PASSED" : "FAILED") << "\n";
    return (outputValidationSuccessful ? 0 : 1);
}

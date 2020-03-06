/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>

#include <cstring>
#include <iostream>
#include <limits>

int main(int argc, char *argv[]) {
    // Initialize driver
    ze_result_t res = zeInit(ZE_INIT_FLAG_NONE);
    if (res) {
        std::terminate();
    }

    // Retrieve driver
    uint32_t driverCount = 0;
    res = zeDriverGet(&driverCount, nullptr);
    if (res || driverCount == 0) {
        std::terminate();
    }
    ze_driver_handle_t driverHandle;
    res = zeDriverGet(&driverCount, &driverHandle);
    if (res) {
        std::terminate();
    }

    // Retrieve device
    uint32_t deviceCount = 0;
    res = zeDeviceGet(driverHandle, &deviceCount, nullptr);
    if (res || deviceCount == 0) {
        std::terminate();
    }
    ze_device_handle_t device;
    deviceCount = 1;
    res = zeDeviceGet(driverHandle, &deviceCount, &device);
    if (res) {
        std::terminate();
    }

    // Print some properties
    ze_device_properties_t deviceProperties = {ZE_DEVICE_PROPERTIES_VERSION_CURRENT};
    res = zeDeviceGetProperties(device, &deviceProperties);
    if (res) {
        std::terminate();
    }

    std::cout << "Device : \n"
              << " * name : " << deviceProperties.name << "\n"
              << " * type : " << ((deviceProperties.type == ZE_DEVICE_TYPE_GPU) ? "GPU" : "FPGA") << "\n"
              << " * vendorId : " << deviceProperties.vendorId << "\n";

    // Create command queue
    ze_command_queue_handle_t cmdQueue;
    ze_command_queue_desc_t cmdQueueDesc = {ZE_COMMAND_QUEUE_DESC_VERSION_CURRENT};
    cmdQueueDesc.ordinal = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    res = zeCommandQueueCreate(device, &cmdQueueDesc, &cmdQueue);
    if (res) {
        std::terminate();
    }

    // Create command list
    ze_command_list_handle_t cmdList;
    ze_command_list_desc_t cmdListDesc = {ZE_COMMAND_LIST_DESC_VERSION_CURRENT};
    res = zeCommandListCreate(device, &cmdListDesc, &cmdList);
    if (res) {
        std::terminate();
    }

    // Create two shared buffers
    constexpr size_t allocSize = 4096;
    ze_device_mem_alloc_desc_t deviceDesc;
    deviceDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_DEFAULT;
    deviceDesc.ordinal = 0;
    deviceDesc.version = ZE_DEVICE_MEM_ALLOC_DESC_VERSION_CURRENT;

    ze_host_mem_alloc_desc_t hostDesc;
    hostDesc.flags = ZE_HOST_MEM_ALLOC_FLAG_DEFAULT;
    hostDesc.version = ZE_HOST_MEM_ALLOC_DESC_VERSION_CURRENT;

    void *srcBuffer = nullptr;
    res = zeDriverAllocSharedMem(driverHandle, &deviceDesc, &hostDesc, allocSize, 1, device, &srcBuffer);
    if (res) {
        std::terminate();
    }

    void *dstBuffer = nullptr;
    res = zeDriverAllocSharedMem(driverHandle, &deviceDesc, &hostDesc, allocSize, 1, device, &dstBuffer);
    if (res) {
        std::terminate();
    }

    // Initialize memory
    constexpr uint8_t val = 55;
    memset(srcBuffer, val, allocSize);
    memset(dstBuffer, 0, allocSize);

    // Perform a GPU copy
    res = zeCommandListAppendMemoryCopy(cmdList, dstBuffer, srcBuffer, allocSize, nullptr);
    if (res) {
        std::terminate();
    }

    // Close list and submit for execution
    res = zeCommandListClose(cmdList);
    if (res) {
        std::terminate();
    }
    res = zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr);
    if (res) {
        std::terminate();
    }

    // Wait for completion
    res = zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint32_t>::max());
    if (res) {
        std::terminate();
    }

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
    res = zeDriverFreeMem(driverHandle, dstBuffer);
    if (res) {
        std::terminate();
    }
    res = zeDriverFreeMem(driverHandle, srcBuffer);
    if (res) {
        std::terminate();
    }
    res = zeCommandListDestroy(cmdList);
    if (res) {
        std::terminate();
    }
    res = zeCommandQueueDestroy(cmdQueue);
    if (res) {
        std::terminate();
    }

    std::cout << "\nZello World Results validation " << (outputValidationSuccessful ? "PASSED" : "FAILED") << "\n";

    return 0;
}

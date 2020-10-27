/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

#define VALIDATECALL(myZeCall)                \
    do {                                      \
        if (myZeCall != ZE_RESULT_SUCCESS) {  \
            std::cout << "Error at "          \
                      << #myZeCall << ": "    \
                      << __FUNCTION__ << ": " \
                      << __LINE__ << "\n";    \
            std::terminate();                 \
        }                                     \
    } while (0);

int main(int argc, char *argv[]) {
    // Initialize driver
    VALIDATECALL(zeInit(ZE_INIT_FLAG_GPU_ONLY));

    // Retrieve driver
    uint32_t driverCount = 0;
    VALIDATECALL(zeDriverGet(&driverCount, nullptr));

    ze_driver_handle_t driverHandle;
    VALIDATECALL(zeDriverGet(&driverCount, &driverHandle));

    ze_context_desc_t contextDesc = {};
    ze_context_handle_t context;
    VALIDATECALL(zeContextCreate(driverHandle, &contextDesc, &context));

    // Retrieve device
    uint32_t deviceCount = 0;
    VALIDATECALL(zeDeviceGet(driverHandle, &deviceCount, nullptr));

    ze_device_handle_t device;
    deviceCount = 1;
    VALIDATECALL(zeDeviceGet(driverHandle, &deviceCount, &device));

    // Print some properties
    ze_device_properties_t deviceProperties = {};
    VALIDATECALL(zeDeviceGetProperties(device, &deviceProperties));

    std::cout << "Device : \n"
              << " * name : " << deviceProperties.name << "\n"
              << " * type : " << ((deviceProperties.type == ZE_DEVICE_TYPE_GPU) ? "GPU" : "FPGA") << "\n"
              << " * vendorId : " << std::hex << deviceProperties.vendorId << "\n";

    // Create command queue
    uint32_t numQueueGroups = 0;
    VALIDATECALL(zeDeviceGetCommandQueueGroupProperties(device, &numQueueGroups, nullptr));
    if (numQueueGroups == 0) {
        std::cout << "No queue groups found!\n";
        std::terminate();
    }
    std::vector<ze_command_queue_group_properties_t> queueProperties(numQueueGroups);
    VALIDATECALL(zeDeviceGetCommandQueueGroupProperties(device, &numQueueGroups,
                                                        queueProperties.data()));

    ze_command_queue_handle_t cmdQueue;
    ze_command_queue_desc_t cmdQueueDesc = {};

    for (uint32_t i = 0; i < numQueueGroups; i++) {
        if (queueProperties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) {
            cmdQueueDesc.ordinal = i;
        }
    }
    cmdQueueDesc.index = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    VALIDATECALL(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));

    // Create command list
    ze_command_list_handle_t cmdList;
    ze_command_list_desc_t cmdListDesc = {};
    cmdListDesc.commandQueueGroupOrdinal = cmdQueueDesc.ordinal;
    VALIDATECALL(zeCommandListCreate(context, device, &cmdListDesc, &cmdList));

    // Create two shared buffers
    constexpr size_t allocSize = 4096;
    ze_device_mem_alloc_desc_t deviceDesc;
    deviceDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED;
    deviceDesc.ordinal = 0;

    ze_host_mem_alloc_desc_t hostDesc;
    hostDesc.flags = ZE_HOST_MEM_ALLOC_FLAG_BIAS_UNCACHED;

    void *srcBuffer = nullptr;
    VALIDATECALL(zeMemAllocShared(context, &deviceDesc, &hostDesc, allocSize, 1, device, &srcBuffer));

    void *dstBuffer = nullptr;
    VALIDATECALL(zeMemAllocShared(context, &deviceDesc, &hostDesc, allocSize, 1, device, &dstBuffer));

    // Initialize memory
    constexpr uint8_t val = 55;
    memset(srcBuffer, val, allocSize);
    memset(dstBuffer, 0, allocSize);

    ze_module_handle_t module = nullptr;
    ze_kernel_handle_t kernel = nullptr;

    std::ifstream file("copy_buffer_to_buffer.spv", std::ios::binary);

    if (file.is_open()) {
        file.seekg(0, file.end);
        auto length = file.tellg();
        file.seekg(0, file.beg);

        std::unique_ptr<char[]> spirvInput(new char[length]);
        file.read(spirvInput.get(), length);

        ze_module_desc_t moduleDesc = {};
        ze_module_build_log_handle_t buildlog;
        moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
        moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(spirvInput.get());
        moduleDesc.inputSize = length;
        moduleDesc.pBuildFlags = "";

        if (zeModuleCreate(context, device, &moduleDesc, &module, &buildlog) != ZE_RESULT_SUCCESS) {
            size_t szLog = 0;
            zeModuleBuildLogGetString(buildlog, &szLog, nullptr);

            char *strLog = (char *)malloc(szLog);
            zeModuleBuildLogGetString(buildlog, &szLog, strLog);
            std::cout << "Build log:" << strLog << std::endl;

            free(strLog);
        }
        VALIDATECALL(zeModuleBuildLogDestroy(buildlog));

        ze_kernel_desc_t kernelDesc = {};
        kernelDesc.pKernelName = "CopyBufferToBufferBytes";
        VALIDATECALL(zeKernelCreate(module, &kernelDesc, &kernel));

        uint32_t groupSizeX = 32u;
        uint32_t groupSizeY = 1u;
        uint32_t groupSizeZ = 1u;
        VALIDATECALL(zeKernelSuggestGroupSize(kernel, allocSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
        VALIDATECALL(zeKernelSetGroupSize(kernel, groupSizeX, groupSizeY, groupSizeZ));

        uint32_t offset = 0;
        VALIDATECALL(zeKernelSetArgumentValue(kernel, 1, sizeof(dstBuffer), &dstBuffer));
        VALIDATECALL(zeKernelSetArgumentValue(kernel, 0, sizeof(srcBuffer), &srcBuffer));
        VALIDATECALL(zeKernelSetArgumentValue(kernel, 2, sizeof(uint32_t), &offset));
        VALIDATECALL(zeKernelSetArgumentValue(kernel, 3, sizeof(uint32_t), &offset));
        VALIDATECALL(zeKernelSetArgumentValue(kernel, 4, sizeof(uint32_t), &offset));

        ze_group_count_t dispatchTraits;
        dispatchTraits.groupCountX = allocSize / groupSizeX;
        dispatchTraits.groupCountY = 1u;
        dispatchTraits.groupCountZ = 1u;

        VALIDATECALL(zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatchTraits,
                                                     nullptr, 0, nullptr));
        file.close();
    } else {
        // Perform a GPU copy
        VALIDATECALL(zeCommandListAppendMemoryCopy(cmdList, dstBuffer, srcBuffer, allocSize, nullptr, 0, nullptr));
    }

    // Close list and submit for execution
    VALIDATECALL(zeCommandListClose(cmdList));
    VALIDATECALL(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));

    VALIDATECALL(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

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
    VALIDATECALL(zeMemFree(context, dstBuffer));
    VALIDATECALL(zeMemFree(context, srcBuffer));
    VALIDATECALL(zeCommandListDestroy(cmdList));
    VALIDATECALL(zeCommandQueueDestroy(cmdQueue));
    VALIDATECALL(zeContextDestroy(context));

    std::cout << "\nZello World Results validation " << (outputValidationSuccessful ? "PASSED" : "FAILED") << "\n";

    return 0;
}

/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"

#include <cstring>

void executeGpuKernelAndValidate(ze_context_handle_t &context, ze_device_handle_t &device, bool &outputValidationSuccessful) {
    ze_command_queue_handle_t cmdQueue;
    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    ze_command_list_handle_t cmdList;

    cmdQueueDesc.ordinal = LevelZeroBlackBoxTests::getCommandQueueOrdinal(device, false);
    cmdQueueDesc.index = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));
    SUCCESS_OR_TERMINATE(LevelZeroBlackBoxTests::createCommandList(context, device, cmdList, false));

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

    ze_module_handle_t module = nullptr;
    ze_kernel_handle_t kernel = nullptr;

    std::ifstream file("copy_buffer_to_buffer.spv", std::ios::binary);

    if (file.is_open()) {
        file.seekg(0, file.end);
        auto length = file.tellg();
        file.seekg(0, file.beg);

        std::unique_ptr<char[]> spirvInput(new char[length]);
        file.read(spirvInput.get(), length);

        ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
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
            LevelZeroBlackBoxTests::printBuildLog(strLog);

            free(strLog);
            SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));
            std::cerr << "\nZello World Gpu Results validation FAILED. Module creation error."
                      << std::endl;
            SUCCESS_OR_TERMINATE_BOOL(false);
        }
        SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));

        ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
        kernelDesc.pKernelName = "CopyBufferToBufferBytes";
        SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &kernel));

        uint32_t groupSizeX = 32u;
        uint32_t groupSizeY = 1u;
        uint32_t groupSizeZ = 1u;
        SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(kernel, allocSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
        SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernel, groupSizeX, groupSizeY, groupSizeZ));

        uint32_t offset = 0;
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 1, sizeof(dstBuffer), &dstBuffer));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 0, sizeof(srcBuffer), &srcBuffer));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 2, sizeof(uint32_t), &offset));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 3, sizeof(uint32_t), &offset));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 4, sizeof(uint32_t), &offset));

        ze_group_count_t dispatchTraits;
        dispatchTraits.groupCountX = allocSize / groupSizeX;
        dispatchTraits.groupCountY = 1u;
        dispatchTraits.groupCountZ = 1u;

        SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatchTraits,
                                                             nullptr, 0, nullptr));
        file.close();
    } else {
        // Perform a GPU copy
        SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, dstBuffer, srcBuffer, allocSize, nullptr, 0, nullptr));
    }

    // Close list and submit for execution
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    // Validate
    outputValidationSuccessful = LevelZeroBlackBoxTests::validate(srcBuffer, dstBuffer, allocSize);

    // Cleanup
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));

    if (kernel != nullptr) {
        SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));
    }
    if (module != nullptr) {
        SUCCESS_OR_TERMINATE(zeModuleDestroy(module));
    }
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello World";
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);

    ze_context_handle_t context = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context);
    auto device = devices[0];

    bool outputValidationSuccessful;

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    LevelZeroBlackBoxTests::printDeviceProperties(deviceProperties);
    LevelZeroBlackBoxTests::printCommandQueueGroupsProperties(device);

    executeGpuKernelAndValidate(context, device, outputValidationSuccessful);

    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName);
    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;
    return outputValidationSuccessful ? 0 : 1;
}

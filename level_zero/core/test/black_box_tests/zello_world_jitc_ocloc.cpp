/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"
#include "zello_compile.h"

const char *moduleSrc = R"===(
__kernel void kernel_copy(__global char *dst, __global char *src){
    uint gid = get_global_id(0);
    dst[gid] = src[gid];
}
)===";

void executeKernelAndValidate(ze_context_handle_t &context, ze_device_handle_t &device, bool &outputValidationSuccessful) {
    ze_command_queue_handle_t cmdQueue;
    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    ze_command_list_handle_t cmdList;

    cmdQueueDesc.ordinal = getCommandQueueOrdinal(device);
    cmdQueueDesc.index = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));
    SUCCESS_OR_TERMINATE(createCommandList(context, device, cmdList));
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

    std::string buildLog;
    auto spirV = compileToSpirV(moduleSrc, "", buildLog);
    if (buildLog.size() > 0) {
        std::cout << "Build log " << buildLog;
    }
    SUCCESS_OR_TERMINATE((0 == spirV.size()));

    ze_module_handle_t module = nullptr;
    ze_kernel_handle_t kernel = nullptr;

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
        std::cout << "Build log:" << strLog << std::endl;

        free(strLog);
        SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));
        std::cout << "\nZello World Jitc Ocloc Results validation FAILED. Module creation error."
                  << std::endl;
        SUCCESS_OR_TERMINATE_BOOL(false);
    }
    SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));

    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "kernel_copy";
    SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &kernel));
    ze_kernel_properties_t kernProps{ZE_STRUCTURE_TYPE_KERNEL_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeKernelGetProperties(kernel, &kernProps));
    printKernelProperties(kernProps, kernelDesc.pKernelName);

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(kernel, allocSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 0, sizeof(dstBuffer), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 1, sizeof(srcBuffer), &srcBuffer));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = allocSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatchTraits,
                                                         nullptr, 0, nullptr));

    // Close list and submit for execution
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    // Validate
    outputValidationSuccessful = true;
    if (memcmp(dstBuffer, srcBuffer, allocSize)) {
        outputValidationSuccessful = false;
        uint8_t *srcCharBuffer = static_cast<uint8_t *>(srcBuffer);
        uint8_t *dstCharBuffer = static_cast<uint8_t *>(dstBuffer);
        for (size_t i = 0; i < allocSize; i++) {
            if (srcCharBuffer[i] != dstCharBuffer[i]) {
                std::cout << "srcBuffer[" << i << "] = " << std::dec << static_cast<unsigned int>(srcCharBuffer[i]) << " not equal to "
                          << "dstBuffer[" << i << "] = " << std::dec << static_cast<unsigned int>(dstCharBuffer[i]) << "\n";
                break;
            }
        }
    }

    // Cleanup
    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello World JIT";
    verbose = isVerbose(argc, argv);
    bool aubMode = isAubMode(argc, argv);

    ze_context_handle_t context = nullptr;
    auto devices = zelloInitContextAndGetDevices(context);
    auto device = devices[0];

    bool outputValidationSuccessful;

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    printDeviceProperties(deviceProperties);

    executeKernelAndValidate(context, device, outputValidationSuccessful);

    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    printResult(aubMode, outputValidationSuccessful, blackBoxName);
    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;
    return outputValidationSuccessful ? 0 : 1;
}

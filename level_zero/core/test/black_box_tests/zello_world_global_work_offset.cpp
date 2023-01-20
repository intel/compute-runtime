/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>
#include <level_zero/ze_ddi.h>

#include "zello_common.h"
#include "zello_compile.h"

const char *moduleSrc = R"===(
__kernel void kernel_copy(__global char *dst, __global char *src){
    uint gid = get_global_id(0);
    dst[gid] = src[gid];
}
)===";

void executeKernelAndValidate(ze_context_handle_t &context,
                              ze_device_handle_t &device,
                              ze_kernel_exp_dditable_t &kernelExpDdiTable,
                              bool &outputValidationSuccessful) {
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
    constexpr uint32_t bufferOffset = 8;
    constexpr uint8_t srcVal = 55;
    constexpr uint8_t dstVal = 77;
    memset(srcBuffer, srcVal, allocSize);
    memset(dstBuffer, 0, allocSize);

    uint8_t *dstCharBuffer = static_cast<uint8_t *>(dstBuffer);
    for (uint32_t i = 0; i < bufferOffset; i++) {
        dstCharBuffer[i] = dstVal;
    }

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
        std::cout << "\nZello World Global Work Offset Results validation FAILED. Module creation error."
                  << std::endl;
        SUCCESS_OR_TERMINATE_BOOL(false);
    }
    SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));

    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "kernel_copy";
    SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &kernel));
    ze_kernel_properties_t kernProps = {ZE_STRUCTURE_TYPE_KERNEL_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeKernelGetProperties(kernel, &kernProps));
    printKernelProperties(kernProps, kernelDesc.pKernelName);

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(kernel, allocSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 0, sizeof(dstBuffer), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 1, sizeof(srcBuffer), &srcBuffer));

    uint32_t offsetx = bufferOffset;
    uint32_t offsety = 0;
    uint32_t offsetz = 0;
    SUCCESS_OR_TERMINATE(kernelExpDdiTable.pfnSetGlobalOffsetExp(kernel, offsetx, offsety, offsetz));

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
    uint8_t *srcCharBuffer = static_cast<uint8_t *>(srcBuffer);
    for (size_t i = 0; i < allocSize; i++) {
        if (i < bufferOffset) {
            if (dstCharBuffer[i] != dstVal) {
                std::cout << "dstBuffer[" << i << "] = "
                          << std::dec << static_cast<unsigned int>(dstCharBuffer[i]) << " not equal to "
                          << dstVal << "\n";
                outputValidationSuccessful = false;
                break;
            }
        } else {
            if (dstCharBuffer[i] != srcCharBuffer[i]) {
                std::cout << "dstBuffer[" << i << "] = "
                          << std::dec << static_cast<unsigned int>(dstCharBuffer[i]) << " not equal to "
                          << "srcBuffer[" << i << "] = "
                          << std::dec << static_cast<unsigned int>(srcCharBuffer[i]) << "\n";
                outputValidationSuccessful = false;
                break;
            }
        }
    }

    // Cleanup
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName("Zello World Global Work Offset");
    verbose = isVerbose(argc, argv);
    bool aubMode = isAubMode(argc, argv);

    ze_driver_handle_t driverHandle;
    ze_context_handle_t context = nullptr;
    auto devices = zelloInitContextAndGetDevices(context, driverHandle);
    auto device = devices[0];

    ze_api_version_t apiVersion = ZE_API_VERSION_CURRENT;

    ze_kernel_exp_dditable_t kernelExpDdiTable;
    SUCCESS_OR_TERMINATE(zeGetKernelExpProcAddrTable(apiVersion, &kernelExpDdiTable));

    bool outputValidationSuccessful;

    uint32_t extensionsCount = 0;
    SUCCESS_OR_TERMINATE(zeDriverGetExtensionProperties(driverHandle, &extensionsCount, nullptr));
    if (extensionsCount == 0) {
        std::cout << "No extensions supported on this driver\n";
        std::terminate();
    }

    std::vector<ze_driver_extension_properties_t> extensionsSupported(extensionsCount);
    SUCCESS_OR_TERMINATE(zeDriverGetExtensionProperties(driverHandle, &extensionsCount, extensionsSupported.data()));
    bool globalOffsetExtensionFound = false;
    std::string globalOffsetName = "ZE_experimental_global_offset";
    for (uint32_t i = 0; i < extensionsSupported.size(); i++) {
        if (verbose) {
            std::cout << "Extension #" << i << " name : " << extensionsSupported[i].name << " version : " << extensionsSupported[i].version << std::endl;
        }
        if (strncmp(extensionsSupported[i].name, globalOffsetName.c_str(), globalOffsetName.size()) == 0) {
            if (extensionsSupported[i].version == ZE_GLOBAL_OFFSET_EXP_VERSION_1_0) {
                globalOffsetExtensionFound = true;
            }
        }
    }
    if (globalOffsetExtensionFound == false) {
        std::cout << "No global offset extension found on this driver\n";
        std::terminate();
    }

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    printDeviceProperties(deviceProperties);

    executeKernelAndValidate(context, device, kernelExpDdiTable, outputValidationSuccessful);

    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    printResult(aubMode, outputValidationSuccessful, blackBoxName);
    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;
    return (outputValidationSuccessful ? 0 : 1);
}

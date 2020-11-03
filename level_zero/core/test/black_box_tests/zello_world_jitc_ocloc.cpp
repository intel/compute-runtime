/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_api.h"

#include <level_zero/ze_api.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
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

const char *module = R"===(
__kernel void kernel_copy(__global int *dst, __global char *src){
    uint gid = get_global_id(0);
    dst[gid] = src[gid];
}
)===";

std::vector<uint8_t> compileToSpirV(const std::string &src, const std::string &options, std::string &outCompilerLog) {
    std::vector<uint8_t> ret;

    const char *mainFileName = "main.cl";
    const char *argv[] = {"ocloc", "-q", "-device", "skl", "-file", mainFileName};
    const unsigned char *sources[] = {reinterpret_cast<const unsigned char *>(src.c_str())};
    size_t sourcesLengths[] = {src.size() + 1};
    const char *sourcesNames[] = {mainFileName};
    unsigned int numOutputs = 0U;
    unsigned char **outputs = nullptr;
    size_t *ouputLengths = nullptr;
    char **outputNames = nullptr;

    int result = oclocInvoke(sizeof(argv) / sizeof(argv[0]), argv,
                             1, sources, sourcesLengths, sourcesNames,
                             0, nullptr, nullptr, nullptr,
                             &numOutputs, &outputs, &ouputLengths, &outputNames);

    unsigned char *spirV = nullptr;
    size_t spirVlen = 0;
    const char *log = nullptr;
    size_t logLen = 0;
    for (unsigned int i = 0; i < numOutputs; ++i) {
        std::string spvExtension = ".spv";
        std::string logFileName = "stdout.log";
        auto nameLen = strlen(outputNames[i]);
        if ((nameLen > spvExtension.size()) && (strstr(&outputNames[i][nameLen - spvExtension.size()], spvExtension.c_str()) != nullptr)) {
            spirV = outputs[i];
            spirVlen = ouputLengths[i];
        } else if ((nameLen >= logFileName.size()) && (strstr(outputNames[i], logFileName.c_str()) != nullptr)) {
            log = reinterpret_cast<const char *>(outputs[i]);
            logLen = ouputLengths[i];
            break;
        }
    }

    if ((result != 0) && (logLen == 0)) {
        outCompilerLog = "Unknown error, ocloc returned : " + std::to_string(result) + "\n";
        return ret;
    }

    if (logLen != 0) {
        outCompilerLog = std::string(log, logLen).c_str();
    }

    ret.assign(spirV, spirV + spirVlen);
    oclocFreeOutput(&numOutputs, &outputs, &ouputLengths, &outputNames);
    return ret;
}

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
              << " * vendorId : " << std::hex << deviceProperties.vendorId << std::dec << "\n";

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

    std::string buildLog;
    auto spirV = compileToSpirV(module, "", buildLog);
    if (buildLog.size() > 0) {
        std::cout << "Build log " << buildLog;
    }
    VALIDATECALL((0 == spirV.size()));

    ze_module_handle_t module = nullptr;
    ze_kernel_handle_t kernel = nullptr;

    ze_module_desc_t moduleDesc = {};
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
    }
    VALIDATECALL(zeModuleBuildLogDestroy(buildlog));

    ze_kernel_desc_t kernelDesc = {};
    kernelDesc.pKernelName = "kernel_copy";
    VALIDATECALL(zeKernelCreate(module, &kernelDesc, &kernel));
    ze_kernel_properties_t kernProps;
    VALIDATECALL(zeKernelGetProperties(kernel, &kernProps));
    std::cout << "Kernel : \n"
              << " * name : " << kernelDesc.pKernelName << "\n"
              << " * uuid.mid : " << kernProps.uuid.mid << "\n"
              << " * uuid.kid : " << kernProps.uuid.kid << "\n"
              << " * maxSubgroupSize : " << kernProps.maxSubgroupSize << "\n"
              << " * localMemSize : " << kernProps.localMemSize << "\n"
              << " * spillMemSize : " << kernProps.spillMemSize << "\n"
              << " * privateMemSize : " << kernProps.privateMemSize << "\n"
              << " * maxNumSubgroups : " << kernProps.maxNumSubgroups << "\n"
              << " * numKernelArgs : " << kernProps.numKernelArgs << "\n"
              << " * requiredSubgroupSize : " << kernProps.requiredSubgroupSize << "\n"
              << " * requiredNumSubGroups : " << kernProps.requiredNumSubGroups << "\n"
              << " * requiredGroupSizeX : " << kernProps.requiredGroupSizeX << "\n"
              << " * requiredGroupSizeY : " << kernProps.requiredGroupSizeY << "\n"
              << " * requiredGroupSizeZ : " << kernProps.requiredGroupSizeZ << "\n";

    uint32_t groupSizeX = 32u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    VALIDATECALL(zeKernelSuggestGroupSize(kernel, allocSize, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    VALIDATECALL(zeKernelSetGroupSize(kernel, groupSizeX, groupSizeY, groupSizeZ));

    VALIDATECALL(zeKernelSetArgumentValue(kernel, 1, sizeof(dstBuffer), &dstBuffer));
    VALIDATECALL(zeKernelSetArgumentValue(kernel, 0, sizeof(srcBuffer), &srcBuffer));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = allocSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    VALIDATECALL(zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatchTraits,
                                                 nullptr, 0, nullptr));

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

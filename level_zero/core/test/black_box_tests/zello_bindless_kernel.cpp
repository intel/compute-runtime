/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>

#include "zello_common.h"
#include "zello_compile.h"

#include <array>
#include <cstring>
#include <iostream>
#include <numeric>
#include <sstream>

const char *source = R"===(
__kernel void kernel_copy(__global char *dst, __global char *src){
    uint gid = get_global_id(0);
    dst[gid] = src[gid];
}
)===";

const char *source2 = R"===(
__kernel void kernel_fill(__global char *dst, char value){
    uint gid = get_global_id(0);
    dst[gid] = value;
}
)===";

static std::string kernelName = "kernel_copy";
static std::string kernelName2 = "kernel_fill";

enum class ExecutionMode : uint32_t {
    CommandQueue,
    ImmSyncCmdList
};

void createModule(const char *sourceCode, bool bindless, const ze_context_handle_t context, const ze_device_handle_t device, const std::string &deviceName, ze_module_handle_t &module) {
    std::string buildLog;
    std::string bindlessOptions = "-cl-intel-use-bindless-mode -cl-intel-use-bindless-advanced-mode";
    std::string internalOptions = "";
    if (bindless) {
        internalOptions = bindlessOptions;
    }
    auto bin = compileToNative(sourceCode, deviceName, "", internalOptions, buildLog);
    if (buildLog.size() > 0) {
        std::cout << "Build log " << buildLog;
    }
    SUCCESS_OR_TERMINATE((0 == bin.size()));

    ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = bin.data();
    moduleDesc.inputSize = bin.size();
    moduleDesc.pBuildFlags = "";

    SUCCESS_OR_TERMINATE(zeModuleCreate(context, device, &moduleDesc, &module, nullptr));
}

void createKernel(const ze_module_handle_t module, ze_kernel_handle_t &kernel, const char *kernelName) {

    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = kernelName;
    SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &kernel));
}

void run(const ze_kernel_handle_t &copyKernel, const ze_kernel_handle_t &fillKernel,
         ze_context_handle_t &context, ze_device_handle_t &device, uint32_t id, ExecutionMode mode, bool &outputValidationSuccessful) {

    CommandHandler commandHandler;
    bool isImmediateCmdList = (mode == ExecutionMode::ImmSyncCmdList);

    SUCCESS_OR_TERMINATE(commandHandler.create(context, device, isImmediateCmdList));

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
    constexpr uint8_t val2 = 15;
    uint8_t finalValue = static_cast<uint8_t>(val);
    memset(srcBuffer, val, allocSize);
    memset(dstBuffer, 0, allocSize);

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = allocSize / 32u;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    if (fillKernel != nullptr) {
        finalValue = val2;
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(fillKernel, 0, sizeof(srcBuffer), &srcBuffer));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(fillKernel, 1, sizeof(char), &val2));
        SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(fillKernel, 32U, 1U, 1U));
        SUCCESS_OR_TERMINATE(commandHandler.appendKernel(fillKernel, dispatchTraits));
        SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(commandHandler.cmdList, nullptr, 0, nullptr));
    }

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(dstBuffer), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(srcBuffer), &srcBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, 32U, 1U, 1U));

    SUCCESS_OR_TERMINATE(commandHandler.appendKernel(copyKernel, dispatchTraits));
    SUCCESS_OR_TERMINATE(commandHandler.execute());
    SUCCESS_OR_TERMINATE(commandHandler.synchronize());

    // Validate
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
    } else {
        uint8_t *dstCharBuffer = static_cast<uint8_t *>(dstBuffer);
        if (dstCharBuffer[0] == finalValue) {
            outputValidationSuccessful = true;
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));
}

int main(int argc, char *argv[]) {
    verbose = isVerbose(argc, argv);
    bool outputValidated = false;

    ze_context_handle_t context = nullptr;
    auto devices = zelloInitContextAndGetDevices(context);
    auto device = devices[0];

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    printDeviceProperties(deviceProperties);

    ze_module_handle_t module = nullptr;
    ze_module_handle_t module2 = nullptr;

    std::stringstream ss;
    ss.setf(std::ios::hex, std::ios::basefield);
    ss << "0x" << deviceProperties.deviceId;

    createModule(source, true, context, device, ss.str(), module);
    createModule(source2, false, context, device, ss.str(), module2);

    ExecutionMode executionModes[] = {ExecutionMode::CommandQueue, ExecutionMode::ImmSyncCmdList};
    ze_kernel_handle_t copyKernel = nullptr;
    ze_kernel_handle_t fillKernel = nullptr;
    createKernel(module, copyKernel, kernelName.c_str());
    createKernel(module2, fillKernel, kernelName2.c_str());

    for (auto mode : executionModes) {

        outputValidated = false;

        run(copyKernel, fillKernel, context, device, 0, mode, outputValidated);

        if (!outputValidated) {
            std::cout << "Zello bindless kernel failed\n"
                      << std::endl;
            break;
        }
    }

    SUCCESS_OR_TERMINATE(zeKernelDestroy(copyKernel));
    SUCCESS_OR_TERMINATE(zeKernelDestroy(fillKernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module2));
    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    if (outputValidated) {
        std::cout << "\nZello  bindless kernel PASSED " << std::endl;
    }
    return outputValidated == false ? -1 : 0;
}

/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>

#include "zello_common.h"
#include "zello_compile.h"

#include <fstream>
#include <iostream>
#include <memory>

const char *functionPointersProgram = R"==(
__global char *__builtin_IB_get_function_pointer(__constant char *function_name);
void __builtin_IB_call_function_pointer(__global char *function_pointer,
                                        char *argument_structure);

struct FunctionData {
    __global char *dst;
    const __global char *src;
    unsigned int gid;
};

kernel void memcpy_bytes(__global char *dst, const __global char *src, __global char *pBufferWithFunctionPointer) {
    unsigned int gid = get_global_id(0);
    struct FunctionData functionData;
    functionData.dst = dst;
    functionData.src = src;
    functionData.gid = gid;
    __global char * __global *pBufferWithFunctionPointerChar = (__global char * __global *)pBufferWithFunctionPointer;
    __builtin_IB_call_function_pointer(pBufferWithFunctionPointerChar[0], (char *)&functionData);
}

void copy_helper(char *data) {
    if(data != NULL) {
        struct FunctionData *pFunctionData = (struct FunctionData *)data;
        __global char *dst = pFunctionData->dst;
        const __global char *src = pFunctionData->src;
        unsigned int gid = pFunctionData->gid;
        dst[gid] = src[gid];
    }
}

void other_indirect_f(unsigned int *dimNum) {
    if(dimNum != NULL) {
        if(*dimNum > 2) {
            *dimNum += 2;
        }
    }
}

__kernel void workaround_kernel() {
    __global char *fp = 0;
    switch (get_global_id(0)) {
    case 0:
        fp = __builtin_IB_get_function_pointer("copy_helper");
        break;
    case 1:
        fp = __builtin_IB_get_function_pointer("other_indirect_f");
        break;
    }
    __builtin_IB_call_function_pointer(fp, 0);
}
)==";

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello Function Pointers CL";

    constexpr size_t allocSize = 4096;

    // 1. Setup
    bool outputValidationSuccessful;
    verbose = isVerbose(argc, argv);
    bool aubMode = isAubMode(argc, argv);

    ze_context_handle_t context = nullptr;
    ze_driver_handle_t driverHandle = nullptr;
    auto devices = zelloInitContextAndGetDevices(context, driverHandle);
    auto device = devices[0];

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    printDeviceProperties(deviceProperties);

    std::string buildLog;
    auto spirV = compileToSpirV(functionPointersProgram, "", buildLog);
    if (buildLog.size() > 0) {
        std::cout << "Build log " << buildLog;
    }
    SUCCESS_OR_TERMINATE((0 == spirV.size()));

    ze_module_handle_t module;
    ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    ze_module_build_log_handle_t buildlog;
    moduleDesc.format = ZE_MODULE_FORMAT_IL_SPIRV;
    moduleDesc.pInputModule = spirV.data();
    moduleDesc.inputSize = spirV.size();
    moduleDesc.pBuildFlags = "-cl-take-global-address";
    if (zeModuleCreate(context, device, &moduleDesc, &module, &buildlog) != ZE_RESULT_SUCCESS) {
        size_t szLog = 0;
        zeModuleBuildLogGetString(buildlog, &szLog, nullptr);

        char *strLog = (char *)malloc(szLog);
        zeModuleBuildLogGetString(buildlog, &szLog, strLog);
        std::cout << "Build log:" << strLog << std::endl;

        free(strLog);
        SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));
        std::cout << "\nZello Function Pointers CL Results validation FAILED. Module creation error."
                  << std::endl;
        return 1;
    }
    SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));

    ze_kernel_handle_t kernel;
    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "memcpy_bytes";
    SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &kernel));

    uint32_t groupSizeX = 1u;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(kernel, allocSize, 1U, 1U,
                                                  &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernel, groupSizeX, groupSizeY, groupSizeZ));

    ze_command_queue_handle_t cmdQueue;
    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.ordinal = getCommandQueueOrdinal(device);
    cmdQueueDesc.index = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));

    ze_command_list_handle_t cmdList;
    SUCCESS_OR_TERMINATE(createCommandList(context, device, cmdList));

    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.ordinal = 0;
    deviceDesc.flags = 0;
    deviceDesc.pNext = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};

    void *srcBuffer = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc, allocSize, 1, device, &srcBuffer));

    void *dstBuffer = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc, allocSize, 1, device, &dstBuffer));

    // 2. Initialize memory
    uint8_t initDataSrc[allocSize];
    memset(initDataSrc, 7, sizeof(initDataSrc));
    uint8_t initDataDst[allocSize];
    memset(initDataDst, 3, sizeof(initDataDst));

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, srcBuffer, initDataSrc,
                                                       sizeof(initDataSrc), nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, dstBuffer, initDataDst,
                                                       sizeof(initDataDst), nullptr, 0, nullptr));

    void *copyHelperFunction = nullptr;
    SUCCESS_OR_TERMINATE(zeModuleGetFunctionPointer(module, "copy_helper", &copyHelperFunction));
    if (nullptr == copyHelperFunction) {
        std::cout << "Pointer to function helper not found\n";
        std::terminate();
    }

    void *bufferWithFunctionPointer = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, sizeof(bufferWithFunctionPointer), 1, device, &bufferWithFunctionPointer));

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, bufferWithFunctionPointer, &copyHelperFunction,
                                                       sizeof(void *), nullptr, 0, nullptr));

    // Copying of data must finish before running the user kernel
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));

    // 3. Encode run user kernel
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 0,
                                                  sizeof(dstBuffer), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 1,
                                                  sizeof(srcBuffer), &srcBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 2,
                                                  sizeof(bufferWithFunctionPointer),
                                                  &bufferWithFunctionPointer));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = allocSize / groupSizeX;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    SUCCESS_OR_TERMINATE_BOOL(dispatchTraits.groupCountX * groupSizeX == allocSize);
    SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatchTraits,
                                                         nullptr, 0, nullptr));

    // 4. Encode read back memory
    uint8_t readBackData[allocSize];
    memset(readBackData, 2, sizeof(readBackData));
    // user kernel must finish before we start copying data
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, readBackData, dstBuffer,
                                                       sizeof(readBackData), nullptr, 0, nullptr));

    // 5. Dispatch and wait
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    // 6. Validate
    outputValidationSuccessful = (0 == memcmp(initDataSrc, readBackData, sizeof(readBackData)));
    if (verbose && (false == outputValidationSuccessful)) {
        validate(initDataSrc, readBackData, sizeof(readBackData));
    }
    SUCCESS_OR_WARNING_BOOL(outputValidationSuccessful);

    // 7. Cleanup
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, bufferWithFunctionPointer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));
    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    printResult(aubMode, outputValidationSuccessful, blackBoxName);
    int resultOnFailure = aubMode ? 0 : 1;
    return outputValidationSuccessful ? 0 : resultOnFailure;
}

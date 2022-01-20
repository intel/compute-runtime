/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"
#include "zello_compile.h"

extern bool verbose;
bool verbose = false;

const char *module = R"===(
typedef long16 TYPE;
__attribute__((reqd_work_group_size(32, 1, 1))) // force LWS to 32
__attribute__((intel_reqd_sub_group_size(16)))   // force SIMD to 16
__kernel void
scratch_kernel(__global int *resIdx, global TYPE *src, global TYPE *dst) {
    size_t lid = get_local_id(0);
    size_t gid = get_global_id(0);

    TYPE res1 = src[gid * 3];
    TYPE res2 = src[gid * 3 + 1];
    TYPE res3 = src[gid * 3 + 2];

    __local TYPE locMem[32];
    locMem[lid] = res1;
    barrier(CLK_LOCAL_MEM_FENCE);
    barrier(CLK_GLOBAL_MEM_FENCE);
    TYPE res = (locMem[resIdx[gid]] * res3) * res2 + res1;
    dst[gid] = res;
}
)===";

void executeGpuKernelAndValidate(ze_context_handle_t context, ze_device_handle_t &device, bool &outputValidationSuccessful) {
    ze_command_queue_handle_t cmdQueue;
    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    ze_command_list_handle_t cmdList;

    cmdQueueDesc.ordinal = getCommandQueueOrdinal(device);
    cmdQueueDesc.index = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));
    SUCCESS_OR_TERMINATE(createCommandList(context, device, cmdList));

    // Create two shared buffers
    uint32_t arraySize = 32;
    uint32_t vectorSize = 16;
    uint32_t typeSize = sizeof(uint32_t);
    uint32_t srcAdditionalMul = 3u;

    uint32_t expectedMemorySize = arraySize * vectorSize * typeSize * 2;
    uint32_t srcMemorySize = expectedMemorySize * srcAdditionalMul;
    uint32_t idxMemorySize = arraySize * sizeof(uint32_t);

    ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
    deviceDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED;
    deviceDesc.ordinal = 0;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    hostDesc.flags = ZE_HOST_MEM_ALLOC_FLAG_BIAS_UNCACHED;

    void *srcBuffer = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, srcMemorySize, 1, &srcBuffer));

    void *dstBuffer = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, expectedMemorySize, 1, &dstBuffer));

    void *idxBuffer = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, idxMemorySize, 1, &idxBuffer));

    void *expectedMemory = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, expectedMemorySize, 1, &expectedMemory));

    // Initialize memory
    constexpr uint8_t val = 0;
    memset(srcBuffer, val, srcMemorySize);
    memset(idxBuffer, 0, idxMemorySize);
    memset(dstBuffer, 0, expectedMemorySize);
    memset(expectedMemory, 0, expectedMemorySize);

    auto srcBufferLong = static_cast<uint64_t *>(srcBuffer);
    auto expectedMemoryLong = static_cast<uint64_t *>(expectedMemory);

    for (uint32_t i = 0; i < arraySize; ++i) {
        static_cast<uint32_t *>(idxBuffer)[i] = 2;
        for (uint32_t vecIdx = 0; vecIdx < vectorSize; ++vecIdx) {
            for (uint32_t srcMulIdx = 0; srcMulIdx < srcAdditionalMul; ++srcMulIdx) {
                srcBufferLong[(i * vectorSize * srcAdditionalMul) + srcMulIdx * vectorSize + vecIdx] = 1l;
            }
            expectedMemoryLong[i * vectorSize + vecIdx] = 2l;
        }
    }

    std::string buildLog;
    auto spirV = compileToSpirV(module, "", buildLog);
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
    }
    SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));

    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = "scratch_kernel";
    SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &kernel));

    ze_kernel_properties_t kernelProperties{ZE_STRUCTURE_TYPE_KERNEL_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeKernelGetProperties(kernel, &kernelProperties));
    std::cout << "Scratch size = " << kernelProperties.spillMemSize << "\n";

    uint32_t groupSizeX = arraySize;
    uint32_t groupSizeY = 1u;
    uint32_t groupSizeZ = 1u;
    SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(kernel, groupSizeX, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernel, groupSizeX, groupSizeY, groupSizeZ));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 2, sizeof(dstBuffer), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 1, sizeof(srcBuffer), &srcBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 0, sizeof(idxBuffer), &idxBuffer));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = 1u;
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
    if (memcmp(dstBuffer, expectedMemory, expectedMemorySize)) {
        outputValidationSuccessful = false;
        uint8_t *srcCharBuffer = static_cast<uint8_t *>(expectedMemory);
        uint8_t *dstCharBuffer = static_cast<uint8_t *>(dstBuffer);
        for (size_t i = 0; i < expectedMemorySize; i++) {
            if (srcCharBuffer[i] != dstCharBuffer[i]) {
                std::cout << "srcBuffer[" << i << "] = " << static_cast<unsigned int>(srcCharBuffer[i]) << " not equal to "
                          << "dstBuffer[" << i << "] = " << static_cast<unsigned int>(dstCharBuffer[i]) << "\n";
                break;
            }
        }
    }

    // Cleanup
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, idxBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, expectedMemory));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
}

int main(int argc, char *argv[]) {
    verbose = isVerbose(argc, argv);
    ze_context_handle_t context = nullptr;
    auto devices = zelloInitContextAndGetDevices(context);
    auto device = devices[0];
    bool outputValidationSuccessful;

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    std::cout << "Device : \n"
              << " * name : " << deviceProperties.name << "\n"
              << " * vendorId : " << std::hex << deviceProperties.vendorId << "\n\n";

    executeGpuKernelAndValidate(context, device, outputValidationSuccessful);

    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    std::cout << "\nZello Scratch Results validation " << (outputValidationSuccessful ? "PASSED" : "FAILED") << "\n";

    return 0;
}

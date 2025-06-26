/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/image/image_format_desc_helper.h"
#include <level_zero/ze_api.h>

#include "zello_common.h"
#include "zello_compile.h"

#include <cstring>
#include <iostream>
#include <sstream>

namespace {

const char *source = R"===(

__kernel void kernel_copy(__global char *dst, __global char *src){
    uint gid = get_global_id(0);
    dst[gid] = src[gid];
}

__kernel void kernel_fill(__global char *dst, char value){
    uint gid = get_global_id(0);
    dst[gid] = value;
}

)===";

const char *source2 = R"===(
typedef ulong8 TYPE;
__attribute__((reqd_work_group_size(32, 1, 1)))  // force LWS to 32
__attribute__((intel_reqd_sub_group_size(16)))   // force SIMD to 16
__kernel void kernel_fill(__global char *dst, char value){
    uint gid = get_global_id(0);
    __local TYPE locMem[32];
    {
        size_t lid = get_local_id(0);
        size_t gid = get_global_id(0);

        TYPE res1 = (TYPE)(dst[0]);
        TYPE res2 = (TYPE)(dst[0] + 1);
        TYPE res3 = (TYPE)(dst[0] + 2);

        locMem[lid] = res1;
        barrier(CLK_LOCAL_MEM_FENCE);
        barrier(CLK_GLOBAL_MEM_FENCE);

        TYPE res = (locMem[dst[lid] % 32] * res3) * res2 + res1;
        dst[gid] += (char)res[lid % 16];
    }
    barrier(CLK_GLOBAL_MEM_FENCE);
    dst[gid] = value;
}
)===";

const char *source3 = R"===(
__kernel void image_copy(__global char *dst, image2d_t img){
    int bytesPerRow = 4 * get_image_width(img);
    uint dstOffset = get_global_id(1) * bytesPerRow + get_global_id(0) * 4 ;

    int2 coord = { get_global_id(0), get_global_id(1)};
    int4 data = {0,0,0,0};
    data = read_imagei(img, coord);
    dst[dstOffset ] = data.x;
    dst[dstOffset + 1] = data.y;
    dst[dstOffset + 2] = data.z;
    dst[dstOffset + 3] = data.w;
    
    if(get_global_id(0) == 0){
        printf("gid(0) = %d gid(1) = %d  dstOffset = %d  data.x = %d\n",  get_global_id(0),  get_global_id(1), dstOffset, data.x);
    }
}

)===";

const char *source4 = R"===(
__kernel void image_read_sampler(__global float4 *dst, image2d_t img, sampler_t sampler) {
    float2 coord = {((float)get_global_id(0) + 1.f)/get_global_size(0), ((float)get_global_id(1) + 1.f)/get_global_size(1)};
    size_t dstOffset = get_global_id(1) * get_image_width(img) + get_global_id(0);
    dst[dstOffset] = read_imagef(img, sampler, coord);
}

__kernel void image_read_sampler_oob(__global float4 *dst, image2d_t img, sampler_t sampler) {
    int2 coord = {get_global_id(0) + 1, get_global_id(1) + 1};
    size_t dstOffset = get_global_id(1) * get_image_width(img) + get_global_id(0);
    dst[dstOffset] = read_imagef(img, sampler, coord);
    printf(" gid[ %zu, %zu]  %.2f , %.2f , %.2f , %.2f \n", get_global_id(0), get_global_id(1), dst[dstOffset].x, dst[dstOffset].y, dst[dstOffset].z, dst[dstOffset].w );
}
)===";

const char *source5 = R"===(
#pragma OPENCL EXTENSION cl_khr_mipmap_image : enable
#pragma OPENCL EXTENSION cl_khr_gl_msaa_sharing : enable

kernel void image_query_1d(global int *dst, image1d_t img) {
    dst[0] = get_image_width(img);
    dst[1] = get_image_channel_data_type(img);
    dst[2] = get_image_channel_order(img);
    dst[3] = get_image_num_mip_levels(img);
}
kernel void image_query_1d_array(global int *dst, image1d_array_t img) {
    dst[0] = get_image_width(img);
    dst[1] = get_image_channel_data_type(img);
    dst[2] = get_image_channel_order(img);
    dst[3] = (int)get_image_array_size(img);
    dst[4] = get_image_num_mip_levels(img);
}
kernel void image_query_2d(global int *dst, image2d_t img) {
    dst[0] = get_image_width(img);
    dst[1] = get_image_height(img);
    dst[2] = get_image_channel_data_type(img);
    dst[3] = get_image_channel_order(img);
    int2 dim = get_image_dim(img);
    dst[4] = dim.x;
    dst[5] = dim.y;
    dst[6] = get_image_num_mip_levels(img);
}
kernel void image_query_2d_array(global int *dst, image2d_array_t img) {
    dst[0] = get_image_width(img);
    dst[1] = get_image_height(img);
    dst[2] = get_image_channel_data_type(img);
    dst[3] = get_image_channel_order(img);
    int2 dim = get_image_dim(img);
    dst[4] = dim.x;
    dst[5] = dim.y;
    dst[6] = (int)get_image_array_size(img);
    dst[7] = get_image_num_mip_levels(img);
}
kernel void image_query_2d_depth(global int *dst, image2d_depth_t img) {
    dst[0] = get_image_width(img);
    dst[1] = get_image_height(img);
    dst[2] = get_image_channel_data_type(img);
    dst[3] = get_image_channel_order(img);
    int2 dim = get_image_dim(img);
    dst[4] = dim.x;
    dst[5] = dim.y;
    dst[6] = get_image_num_mip_levels(img);
}
kernel void image_query_2d_array_depth(global int *dst, image2d_array_depth_t img) {
    dst[0] = get_image_width(img);
    dst[1] = get_image_height(img);
    dst[2] = get_image_channel_data_type(img);
    dst[3] = get_image_channel_order(img);
    int2 dim = get_image_dim(img);
    dst[4] = dim.x;
    dst[5] = dim.y;
    dst[6] = (int)get_image_array_size(img);
    dst[7] = get_image_num_mip_levels(img);
}
kernel void image_query_2d_msaa(global int *dst, image2d_msaa_t img) {
    dst[0] = get_image_width(img);
    dst[1] = get_image_height(img);
    dst[2] = get_image_channel_data_type(img);
    dst[3] = get_image_channel_order(img);
    int2 dim = get_image_dim(img);
    dst[4] = dim.x;
    dst[5] = dim.y;
    dst[6] = get_image_num_samples(img);
}
kernel void image_query_3d(global int *dst, image3d_t img) {
    dst[0] = get_image_width(img);
    dst[1] = get_image_height(img);
    dst[2] = get_image_depth(img);
    dst[3] = get_image_channel_data_type(img);
    dst[4] = get_image_channel_order(img);
    int4 dim = get_image_dim(img);
    dst[5] = dim.x;
    dst[6] = dim.y;
    dst[7] = dim.z;
    dst[8] = get_image_num_mip_levels(img);
}
)===";

const char *source1DSampler = R"===(
__kernel void image_read_sampler(__global float4 *dst, __global float4 *dst2,__global float4 *dst3, image1d_t img, image1d_t img2, image1d_t img3,  sampler_t sampler, sampler_t sampler2, sampler_t sampler3) {
    int id = get_global_id(0);
    float coord = (float)(id+1);
    dst[id] = read_imagef(img, sampler, coord);
    printf( "gid[%zu], coord=%.2f, dst.x=%.2f , dst.y=%.2f , dst.z=%.2f , dst.w=%.2f \n", get_global_id(0), coord, dst[id].x, dst[id].y, dst[id].z, dst[id].w);

    float coord2 = (float)(id+1);
    dst2[id] = read_imagef(img2, sampler2, coord2);
    printf( "gid[%zu], coord2=%.2f, dst2.x=%.2f , dst2.y=%.2f , dst2.z=%.2f , dst2.w=%.2f \n", get_global_id(0), coord2, dst2[id].x, dst2[id].y, dst2[id].z, dst2[id].w);

    float coord3 = (float)(id+0.3)/8.0;
    dst3[id] = read_imagef(img3, sampler3, coord3);
    printf( "gid[%zu], coord3=%.2f, dst3.x=%.2f , dst3.y=%.2f , dst3.z=%.2f , dst3.w=%.2f \n", get_global_id(0), coord3, dst3[id].x, dst3[id].y, dst3[id].z, dst3[id].w);
}
)===";

const char *source1DInlineSampler = R"===(
const sampler_t inlineSampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;
const sampler_t inlineSampler2 = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE  | CLK_FILTER_NEAREST;
const sampler_t inlineSampler3 = CLK_NORMALIZED_COORDS_TRUE | CLK_ADDRESS_CLAMP_TO_EDGE  | CLK_FILTER_LINEAR;

__kernel void image_read_inline_sampler(__global float4 *dst,__global float4 *dst2, __global float4 *dst3, image1d_t img, image1d_t img2, image1d_t img3) {
    int id = get_global_id(0);
    float coord = (float)(id+1);
    dst[id] = read_imagef(img, inlineSampler, coord);
    printf( "gid[%zu], coord=%.2f, dst.x=%.2f , dst.y=%.2f , dst.z=%.2f , dst.w=%.2f \n", get_global_id(0), coord, dst[id].x, dst[id].y, dst[id].z, dst[id].w);

    float coord2= (float)(id+1);
    dst2[id] = read_imagef(img2, inlineSampler2, coord2);
    printf( "gid[%zu], coord2=%.2f, dst2.x=%.2f , dst2.y=%.2f , dst2.z=%.2f , dst2.w=%.2f \n", get_global_id(0), coord2, dst2[id].x, dst2[id].y, dst2[id].z, dst2[id].w);

    float coord3= (float)(id+0.3) /8.0;
    dst3[id] = read_imagef(img3, inlineSampler3, coord3);
    printf( "gid[%zu], coord3=%.2f, dst3.x=%.2f , dst3.y=%.2f , dst3.z=%.2f , dst3.w=%.2f \n", get_global_id(0), coord3, dst3[id].x, dst3[id].y, dst3[id].z, dst3[id].w);

}
)===";

static std::string kernelName = "kernel_copy";
static std::string kernelName2 = "kernel_fill";
static std::string kernelName3 = "image_copy";
static std::string kernelName4 = "image_read_sampler";
static std::string kernelName4a = "image_read_sampler_oob";
static std::string kernelName1DSampler = "image_read_sampler";
static std::string kernelName1DInlineSampler = "image_read_inline_sampler";

enum class ExecutionMode : uint32_t {
    commandQueue,
    immSyncCmdList
};

enum class AddressingMode : uint32_t {
    defaultMode,
    bindless,
    bindlessImages,
    bindful
};

typedef ze_result_t(ZE_APICALL *zeImageGetDeviceOffsetExp_pfn)(
    ze_image_handle_t hImage,
    uint64_t *pDeviceOffset);

zeImageGetDeviceOffsetExp_pfn zeImageGetDeviceOffsetExpFunctionPtr = nullptr;

typedef ze_result_t(ZE_APICALL *zeMemGetPitchFor2dImage_pfn)(
    ze_context_handle_t hContext,
    ze_device_handle_t hDevice,
    size_t imageWidth,
    size_t imageHeight,
    unsigned int elementSizeInBytes,
    size_t *rowPitch);

zeMemGetPitchFor2dImage_pfn zeMemGetPitchFor2dImageFunctionPtr = nullptr;

void createModule(const char *sourceCode, AddressingMode addressing, const ze_context_handle_t context, const ze_device_handle_t device,
                  const std::string &deviceName, const std::string &revisionId, ze_module_handle_t &module, const std::string &internalOption, bool forceScratch) {
    std::string buildLog;
    std::string bindlessOptions = "-cl-intel-use-bindless-mode -cl-intel-use-bindless-advanced-mode";
    std::string bindlessImagesOptions = "-cl-intel-use-bindless-images -cl-intel-use-bindless-advanced-mode";
    std::string internalOptions = internalOption;
    std::string statefulMode;
    if (addressing == AddressingMode::bindless) {
        statefulMode = "bindless";
    }
    if (addressing == AddressingMode::bindful) {
        statefulMode = "bindful";
    }
    if (addressing == AddressingMode::bindlessImages) {
        internalOptions += " " + bindlessImagesOptions;
    }

    std::string buildFlags = "-cl-std=CL3.0";
    if (forceScratch) {
        std::string scratchOpts(LevelZeroBlackBoxTests::scratchKernelBuildOptions);
        buildFlags += " " + scratchOpts;
    }
    auto bin = LevelZeroBlackBoxTests::compileToNative(sourceCode, deviceName, revisionId, buildFlags, internalOptions, statefulMode, buildLog);
    LevelZeroBlackBoxTests::printBuildLog(buildLog);
    SUCCESS_OR_TERMINATE((0 == bin.size()));

    ze_module_desc_t moduleDesc = {ZE_STRUCTURE_TYPE_MODULE_DESC};
    moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
    moduleDesc.pInputModule = bin.data();
    moduleDesc.inputSize = bin.size();
    moduleDesc.pBuildFlags = buildFlags.c_str();

    SUCCESS_OR_TERMINATE(zeModuleCreate(context, device, &moduleDesc, &module, nullptr));
}

void createKernel(const ze_module_handle_t module, ze_kernel_handle_t &kernel, const char *kernelName) {

    ze_kernel_desc_t kernelDesc = {ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernelDesc.pKernelName = kernelName;
    SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &kernel));
}

void run(const ze_kernel_handle_t &copyKernel, const ze_kernel_handle_t &fillKernel,
         ze_context_handle_t &context, ze_device_handle_t &device, uint32_t id, ExecutionMode mode, bool &outputValidationSuccessful) {

    LevelZeroBlackBoxTests::CommandHandler commandHandler;
    bool isImmediateCmdList = (mode == ExecutionMode::immSyncCmdList);

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
        } else {
            std::cout << "dstCharBuffer[0] != finalValue\n"
                      << "dstCharBuffer[0]==" << static_cast<uint32_t>(dstCharBuffer[0]) << " finalValue==" << static_cast<uint32_t>(finalValue) << std::endl;
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));
}

bool testBindlessBufferCopy(ze_context_handle_t context, ze_device_handle_t device, const std::string &deviceId, const std::string &revisionId) {
    bool outputValidated = false;

    ze_module_handle_t module = nullptr;
    ze_module_handle_t module2 = nullptr;
    createModule(source, AddressingMode::bindless, context, device, deviceId, revisionId, module, "", false);
    createModule(source, AddressingMode::bindful, context, device, deviceId, revisionId, module2, "", false);

    ExecutionMode executionModes[] = {ExecutionMode::immSyncCmdList, ExecutionMode::commandQueue};
    ze_kernel_handle_t copyKernel = nullptr;
    ze_kernel_handle_t fillKernel = nullptr;
    createKernel(module, copyKernel, kernelName.c_str());
    createKernel(module2, fillKernel, kernelName2.c_str());

    for (auto mode : executionModes) {

        run(copyKernel, fillKernel, context, device, 0, mode, outputValidated);

        if (!outputValidated) {
            std::cout << "testBindlessBufferCopy with mode " << static_cast<uint32_t>(mode) << " failed.\n"
                      << std::endl;
            break;
        }
    }

    SUCCESS_OR_TERMINATE(zeKernelDestroy(copyKernel));
    SUCCESS_OR_TERMINATE(zeKernelDestroy(fillKernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module2));

    return outputValidated;
}

void runBindlessBindful(const ze_kernel_handle_t &firstKernel, const ze_kernel_handle_t &secondKernel,
                        ze_context_handle_t &context, ze_device_handle_t &device, ExecutionMode mode, bool &outputValidationSuccessful) {

    LevelZeroBlackBoxTests::CommandHandler commandHandler;
    bool isImmediateCmdList = (mode == ExecutionMode::immSyncCmdList);

    SUCCESS_OR_TERMINATE(commandHandler.create(context, device, isImmediateCmdList));

    constexpr size_t allocSize = 32;
    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    hostDesc.flags = ZE_HOST_MEM_ALLOC_FLAG_BIAS_CACHED;

    void *buffer1 = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, 1, &buffer1));

    void *buffer2 = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, 1, &buffer2));

    void *buffer3 = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, 1, &buffer3));

    // Initialize memory
    constexpr uint8_t val1 = 55;
    constexpr uint8_t val2 = 22;
    constexpr uint8_t val3 = 77;

    memset(buffer1, 0, allocSize);
    memset(buffer2, 0, allocSize);
    memset(buffer3, 0, allocSize);

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = 1u;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(firstKernel, 0, sizeof(buffer1), &buffer1));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(firstKernel, 1, sizeof(char), &val1));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(firstKernel, 32U, 1U, 1U));
    SUCCESS_OR_TERMINATE(commandHandler.appendKernel(firstKernel, dispatchTraits));
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(commandHandler.cmdList, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(secondKernel, 0, sizeof(buffer2), &buffer2));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(secondKernel, 1, sizeof(val2), &val2));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(secondKernel, 32U, 1U, 1U));
    SUCCESS_OR_TERMINATE(commandHandler.appendKernel(secondKernel, dispatchTraits));
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(commandHandler.cmdList, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(firstKernel, 0, sizeof(buffer1), &buffer3));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(firstKernel, 1, sizeof(char), &val3));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(firstKernel, 32U, 1U, 1U));
    SUCCESS_OR_TERMINATE(commandHandler.appendKernel(firstKernel, dispatchTraits));

    SUCCESS_OR_TERMINATE(commandHandler.execute());
    SUCCESS_OR_TERMINATE(commandHandler.synchronize());

    outputValidationSuccessful = true;
    uint8_t *buffer1Uint = static_cast<uint8_t *>(buffer1);
    uint8_t *buffer2Uint = static_cast<uint8_t *>(buffer2);
    uint8_t *buffer3Uint = static_cast<uint8_t *>(buffer3);
    for (size_t i = 0; i < allocSize; i++) {
        if (buffer1Uint[i] != val1) {
            std::cout << "buffer1Uint[" << i << "] = " << std::dec << static_cast<unsigned int>(buffer1Uint[i]) << " not equal to "
                      << "val1 = " << std::dec << static_cast<unsigned int>(val1) << "\n";
            outputValidationSuccessful = false;
            break;
        }
        if (buffer2Uint[i] != val2) {
            std::cout << "buffer2Uint[" << i << "] = " << std::dec << static_cast<unsigned int>(buffer2Uint[i]) << " not equal to "
                      << "val2 = " << std::dec << static_cast<unsigned int>(val2) << "\n";
            outputValidationSuccessful = false;
            break;
        }
        if (buffer3Uint[i] != val3) {
            std::cout << "buffer3Uint[" << i << "] = " << std::dec << static_cast<unsigned int>(buffer3Uint[i]) << " not equal to "
                      << "val3 = " << std::dec << static_cast<unsigned int>(val3) << "\n";
            outputValidationSuccessful = false;
            break;
        }
    }

    if (!outputValidationSuccessful) {
        std::cout << " TEST FAILED\n"
                  << std::endl;
    }

    SUCCESS_OR_TERMINATE(zeMemFree(context, buffer1));
    SUCCESS_OR_TERMINATE(zeMemFree(context, buffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(context, buffer3));
}

bool testBindlessBindfulKernel(ze_context_handle_t context, ze_device_handle_t device, const std::string &deviceId, const std::string &revisionId) {
    bool outputValidated = false;

    ze_module_handle_t module = nullptr;
    ze_module_handle_t module2 = nullptr;
    createModule(source2, AddressingMode::bindless, context, device, deviceId, revisionId, module, "", true);
    createModule(source2, AddressingMode::bindful, context, device, deviceId, revisionId, module2, "", true);

    ExecutionMode executionModes[] = {ExecutionMode::commandQueue, ExecutionMode::immSyncCmdList};
    ze_kernel_handle_t bindlessKernel = nullptr;
    ze_kernel_handle_t bindfulKernel = nullptr;
    createKernel(module, bindlessKernel, kernelName2.c_str());
    createKernel(module2, bindfulKernel, kernelName2.c_str());

    std::tuple<ze_kernel_handle_t, ze_kernel_handle_t, std::string> kernelOrder[2] = {{bindlessKernel, bindfulKernel, "First Bindless Then Bindful"},
                                                                                      {bindfulKernel, bindlessKernel, "First Bindful Then Bindless"}};

    for (auto kernel : kernelOrder) {

        for (auto mode : executionModes) {

            runBindlessBindful(std::get<0>(kernel), std::get<1>(kernel), context, device, mode, outputValidated);

            if (!outputValidated) {
                std::cout << "testBindlessBindfulKernel with mode " << static_cast<uint32_t>(mode) << " failed.\n"
                          << std::endl;
                break;
            } else {
                std::cout << "testBindlessBindfulKernel with mode " << static_cast<uint32_t>(mode) << " " << std::get<2>(kernel) << " PASSED.\n"
                          << std::endl;
            }
        }
    }

    SUCCESS_OR_TERMINATE(zeKernelDestroy(bindlessKernel));
    SUCCESS_OR_TERMINATE(zeKernelDestroy(bindfulKernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module2));

    return outputValidated;
}

bool testBindlessImages(ze_context_handle_t context, ze_device_handle_t device, const std::string &deviceId, const std::string &revisionId,
                        int imageCount, AddressingMode mode, ExecutionMode execMode) {
    bool outputValidated = false;

    ze_module_handle_t module = nullptr;
    ze_kernel_handle_t copyKernel = nullptr;

    createModule(source3, mode, context, device, deviceId, revisionId, module, "", false);
    createKernel(module, copyKernel, kernelName3.c_str());

    LevelZeroBlackBoxTests::CommandHandler commandHandler;
    bool isImmediateCmdList = execMode == ExecutionMode::immSyncCmdList ? true : false;

    SUCCESS_OR_TERMINATE(commandHandler.create(context, device, isImmediateCmdList));

    constexpr size_t allocSize = 4096;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    hostDesc.flags = ZE_HOST_MEM_ALLOC_FLAG_BIAS_UNCACHED;

    void *dstBuffer = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, 1, &dstBuffer));

    // Initialize memory
    constexpr uint8_t val = 55;
    memset(dstBuffer, 0, allocSize);

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  nullptr,
                                  0,
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_SINT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  4,
                                  4,
                                  1,
                                  0,
                                  0};

    std::vector<ze_image_handle_t> tempImages(imageCount);
    ze_image_region_t srcRegion = {0, 0, 0, (uint32_t)srcImgDesc.width, (uint32_t)srcImgDesc.height, (uint32_t)srcImgDesc.depth};

    for (int i = 0; i < imageCount; i++) {
        SUCCESS_OR_TERMINATE(zeImageCreate(context, device, &srcImgDesc, &tempImages[i]));
    }

    std::vector<int> data;
    data.assign(srcImgDesc.width * srcImgDesc.height * 4, val);

    for (int i = 0; i < imageCount; i++) {
        SUCCESS_OR_TERMINATE(zeCommandListAppendImageCopyFromMemory(commandHandler.cmdList, tempImages[i], data.data(),
                                                                    &srcRegion, nullptr, 0, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(commandHandler.cmdList, nullptr, 0, nullptr));
        SUCCESS_OR_TERMINATE(commandHandler.execute());
        SUCCESS_OR_TERMINATE(commandHandler.synchronize());
        SUCCESS_OR_TERMINATE(zeCommandListReset(commandHandler.cmdList));
    }

    for (int i = 0; i < imageCount / 2; i++) {
        SUCCESS_OR_TERMINATE(zeImageDestroy(tempImages[i]));
    }

    ze_image_handle_t srcImg;
    srcImgDesc.width = 32;
    srcImgDesc.height = 4;
    srcImgDesc.format = {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_SINT,
                         ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                         ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A};

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = 1;
    dispatchTraits.groupCountY = 4u;
    dispatchTraits.groupCountZ = 1u;

    SUCCESS_OR_TERMINATE(zeImageCreate(context, device, &srcImgDesc, &srcImg));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(srcImg), &srcImg));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(dstBuffer), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, 32U, 1U, 1U));

    std::vector<uint8_t> data2;
    data2.assign(srcImgDesc.width * srcImgDesc.height * 4, 2);
    srcRegion = {0, 0, 0, (uint32_t)srcImgDesc.width, (uint32_t)srcImgDesc.height, (uint32_t)srcImgDesc.depth};

    SUCCESS_OR_TERMINATE(zeCommandListAppendImageCopyFromMemory(commandHandler.cmdList, srcImg, data2.data(),
                                                                &srcRegion, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(commandHandler.cmdList, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(commandHandler.appendKernel(copyKernel, dispatchTraits));
    SUCCESS_OR_TERMINATE(commandHandler.execute());
    SUCCESS_OR_TERMINATE(commandHandler.synchronize());

    // Validate
    if (memcmp(dstBuffer, data2.data(), data2.size())) {
        uint8_t *dstCharBuffer = static_cast<uint8_t *>(dstBuffer);
        for (size_t i = 0; i < allocSize; i++) {
            if (data2[i] != dstCharBuffer[i]) {
                std::cout << "error: data2[" << i << "] = " << std::dec << static_cast<unsigned int>(data2[i]) << " not equal to "
                          << "dstBuffer[" << i << "] = " << std::dec << static_cast<unsigned int>(dstCharBuffer[i]) << "\n";
                break;
            }
        }
    } else {
        outputValidated = true;
    }

    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));

    for (int i = imageCount / 2; i < imageCount; i++) {
        SUCCESS_OR_TERMINATE(zeImageDestroy(tempImages[i]));
    }

    SUCCESS_OR_TERMINATE(zeImageDestroy(srcImg));
    SUCCESS_OR_TERMINATE(zeKernelDestroy(copyKernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));

    return outputValidated;
}

bool testBindlessBindfulImageKernel(ze_context_handle_t context, ze_device_handle_t device, const std::string &deviceId, const std::string &revisionId) {

    ExecutionMode executionModes[] = {ExecutionMode::commandQueue, ExecutionMode::immSyncCmdList};

    std::tuple<AddressingMode, AddressingMode, std::string> kernelOrder[2] = {{AddressingMode::bindless, AddressingMode::bindful, "First Bindless Then Bindful"},
                                                                              {AddressingMode::bindful, AddressingMode::bindless, "First Bindful Then Bindless"}};

    for (auto kernelMode : kernelOrder) {

        for (auto mode : executionModes) {

            bool result1 = testBindlessImages(context, device, deviceId, revisionId, 0, std::get<0>(kernelMode), mode);
            bool result2 = testBindlessImages(context, device, deviceId, revisionId, 0, std::get<1>(kernelMode), mode);

            if (!result1 || !result2) {
                std::cout << "testBindlessBindfulImageKernel with mode " << static_cast<uint32_t>(mode) << " " << std::get<2>(kernelMode) << " FAILED.\n"
                          << std::endl;
                return false;
            } else {
                std::cout << "testBindlessBindfulKernel with mode " << static_cast<uint32_t>(mode) << " " << std::get<2>(kernelMode) << " PASSED.\n"
                          << std::endl;
            }
        }
    }
    return true;
}

bool testBindlessImageSampled(ze_context_handle_t context, ze_device_handle_t device, const std::string &deviceId,
                              const std::string &revisionId, AddressingMode mode) {
    bool outputValidated = true;

    ze_module_handle_t module = nullptr;
    ze_kernel_handle_t kernel = nullptr;

    createModule(source4, mode, context, device, deviceId, revisionId, module, "", false);
    createKernel(module, kernel, kernelName4.c_str());

    LevelZeroBlackBoxTests::CommandHandler commandHandler;
    bool isImmediateCmdList = false;

    SUCCESS_OR_TERMINATE(commandHandler.create(context, device, isImmediateCmdList));

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    hostDesc.flags = ZE_HOST_MEM_ALLOC_FLAG_BIAS_UNCACHED;

    ze_sampler_desc_t samplerDesc = {ZE_STRUCTURE_TYPE_SAMPLER_DESC,
                                     nullptr,
                                     ZE_SAMPLER_ADDRESS_MODE_CLAMP,
                                     ZE_SAMPLER_FILTER_MODE_LINEAR,
                                     true};
    ze_sampler_handle_t sampler;
    SUCCESS_OR_TERMINATE(zeSamplerCreate(context, device, &samplerDesc, &sampler));

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  nullptr,
                                  0,
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_FLOAT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  32,
                                  4,
                                  1,
                                  0,
                                  0};

    constexpr size_t bytesPerPixel = sizeof(float) * 4;
    size_t bytesPerRow = srcImgDesc.width * bytesPerPixel;
    size_t allocSize = bytesPerRow * srcImgDesc.height;

    // Create and initialize host memory
    void *dstBuffer;
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, 1, &dstBuffer));
    for (uint32_t i = 0; i < srcImgDesc.height; ++i) {
        float *dstRow = reinterpret_cast<float *>(dstBuffer) + srcImgDesc.width * 4 * i;
        for (size_t j = 0; j < srcImgDesc.width; ++j) {
            for (size_t k = 0; k < 4; ++k) {
                dstRow[j * 4 + k] = static_cast<float>(i * 10);
            }
        }
    }

    ze_image_handle_t srcImg;
    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = 1u;
    dispatchTraits.groupCountY = 4u;
    dispatchTraits.groupCountZ = 1u;

    SUCCESS_OR_TERMINATE(zeImageCreate(context, device, &srcImgDesc, &srcImg));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 0, sizeof(dstBuffer), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 1, sizeof(srcImg), &srcImg));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 2, sizeof(sampler), &sampler));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernel, 32U, 1U, 1U));

    ze_image_region_t srcRegion = {0, 0, 0, (uint32_t)srcImgDesc.width, (uint32_t)srcImgDesc.height, (uint32_t)srcImgDesc.depth};

    std::vector<float> data(srcImgDesc.width * srcImgDesc.height * 4);
    memcpy(data.data(), dstBuffer, allocSize);

    SUCCESS_OR_TERMINATE(zeCommandListAppendImageCopyFromMemory(commandHandler.cmdList, srcImg, data.data(),
                                                                &srcRegion, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(commandHandler.cmdList, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(commandHandler.appendKernel(kernel, dispatchTraits));
    SUCCESS_OR_TERMINATE(commandHandler.execute());
    SUCCESS_OR_TERMINATE(commandHandler.synchronize());

    // Validate
    float *dst = reinterpret_cast<float *>(dstBuffer);
    std::vector<float> reference = {5.f, 15.f, 25.f, 30.f};
    for (size_t i = 0; (i < srcImgDesc.height) && outputValidated; ++i) {
        for (size_t j = 0; j < (srcImgDesc.width * 4); ++j, ++dst) {
            if (*dst != reference[i]) {
                std::cerr << "error: dstBuffer[" << i << "][" << j << "] = " << *dst << " is not equal to " << reference[i] << "\n";
                outputValidated = false;
                break;
            }
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeSamplerDestroy(sampler));
    SUCCESS_OR_TERMINATE(zeImageDestroy(srcImg));
    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));

    if (!outputValidated) {
        std::cout << "\nTest FAILED" << std::endl;
    } else {
        std::cout << "\nTest PASSED" << std::endl;
    }

    return outputValidated;
}

bool testBindlessImageSampledBorderColor(ze_context_handle_t context, ze_device_handle_t device, const std::string &deviceId,
                                         const std::string &revisionId, AddressingMode mode) {
    bool outputValidated = true;

    ze_module_handle_t module = nullptr;
    ze_kernel_handle_t kernel = nullptr;

    createModule(source4, mode, context, device, deviceId, revisionId, module, "", false);
    createKernel(module, kernel, kernelName4a.c_str());

    LevelZeroBlackBoxTests::CommandHandler commandHandler;
    SUCCESS_OR_TERMINATE(commandHandler.create(context, device, true));

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    hostDesc.flags = ZE_HOST_MEM_ALLOC_FLAG_BIAS_UNCACHED;

    ze_sampler_desc_t samplerDesc = {ZE_STRUCTURE_TYPE_SAMPLER_DESC,
                                     nullptr,
                                     ZE_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                     ZE_SAMPLER_FILTER_MODE_NEAREST,
                                     false};
    ze_sampler_handle_t sampler;
    SUCCESS_OR_TERMINATE(zeSamplerCreate(context, device, &samplerDesc, &sampler));

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  nullptr,
                                  0,
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_FLOAT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G},
                                  2,
                                  2,
                                  1,
                                  0,
                                  0};

    constexpr size_t bytesPerPixel = sizeof(float) * 4;
    size_t bytesPerRow = srcImgDesc.width * bytesPerPixel;
    size_t allocSize = bytesPerRow * srcImgDesc.height;

    // Create and initialize host memory
    void *dstBuffer;
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, 1, &dstBuffer));
    for (uint32_t i = 0; i < srcImgDesc.height; ++i) {
        float *dstRow = reinterpret_cast<float *>(dstBuffer) + srcImgDesc.width * 4 * i;
        for (size_t j = 0; j < srcImgDesc.width; ++j) {
            for (size_t k = 0; k < 4; ++k) {
                dstRow[j * 4 + k] = 4.f;
            }
        }
    }

    ze_image_handle_t srcImg;
    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = 1u;
    dispatchTraits.groupCountY = 2u;
    dispatchTraits.groupCountZ = 1u;

    SUCCESS_OR_TERMINATE(zeImageCreate(context, device, &srcImgDesc, &srcImg));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 0, sizeof(dstBuffer), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 1, sizeof(srcImg), &srcImg));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 2, sizeof(sampler), &sampler));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernel, 2U, 1U, 1U));

    ze_image_region_t srcRegion = {0, 0, 0, (uint32_t)srcImgDesc.width, (uint32_t)srcImgDesc.height, (uint32_t)srcImgDesc.depth};

    std::vector<float> data(srcImgDesc.width * srcImgDesc.height * 4);
    memcpy(data.data(), dstBuffer, allocSize);

    SUCCESS_OR_TERMINATE(zeCommandListAppendImageCopyFromMemory(commandHandler.cmdList, srcImg, data.data(),
                                                                &srcRegion, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(commandHandler.cmdList, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(commandHandler.appendKernel(kernel, dispatchTraits));
    SUCCESS_OR_TERMINATE(commandHandler.execute());
    SUCCESS_OR_TERMINATE(commandHandler.synchronize());

    // Validate
    float *dst = reinterpret_cast<float *>(dstBuffer);
    std::vector<float> reference = {0.f, 0.f, 0.f, 0.f};

    for (size_t i = 0; (i < srcImgDesc.height); ++i) {
        for (size_t j = 0; j < srcImgDesc.width; ++j) {
            if (i == 0 && j == 0) {
                for (size_t k = 0; k < 4; k++) {
                    if (dst[k] != 4.0f) {
                        std::cerr << "error: dstBuffer[" << i << "][" << j << "] = " << dst[k] << " is not equal to " << 4.0f << "\n";
                        outputValidated = false;
                        break;
                    }
                }
            } else {
                for (size_t k = 0; k < 4; k++) {
                    if (dst[k] != reference[k]) {
                        std::cerr << "error: dstBuffer[" << i << "][" << j << "] = " << dst[k] << " is not equal to " << reference[k] << "\n";
                        outputValidated = false;
                        break;
                    }
                }
            }
            dst += 4;
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeSamplerDestroy(sampler));
    SUCCESS_OR_TERMINATE(zeImageDestroy(srcImg));
    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));

    if (!outputValidated) {
        std::cout << "\nTest FAILED" << std::endl;
    } else {
        std::cout << "\nTest PASSED" << std::endl;
    }

    return outputValidated;
}

bool testBindlessImage1DSampled(ze_context_handle_t context, ze_device_handle_t device, const std::string &deviceId,
                                const std::string &revisionId, AddressingMode mode, bool useInlineSampler) {
    bool outputValidated = true;

    ze_module_handle_t module = nullptr;
    ze_kernel_handle_t kernel = nullptr;

    createModule(useInlineSampler ? source1DInlineSampler : source1DSampler, mode, context, device, deviceId, revisionId, module, "", false);
    createKernel(module, kernel, useInlineSampler ? kernelName1DInlineSampler.c_str() : kernelName1DSampler.c_str());

    LevelZeroBlackBoxTests::CommandHandler commandHandler;
    bool isImmediateCmdList = false;

    SUCCESS_OR_TERMINATE(commandHandler.create(context, device, isImmediateCmdList));

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    hostDesc.flags = ZE_HOST_MEM_ALLOC_FLAG_BIAS_UNCACHED;

    ze_sampler_handle_t sampler = nullptr;
    ze_sampler_handle_t sampler2 = nullptr;
    ze_sampler_handle_t sampler3 = nullptr;

    if (!useInlineSampler) {
        bool normalized = false;
        // ZE_SAMPLER_ADDRESS_MODE_CLAMP should return edge values for out of bound access
        ze_sampler_desc_t samplerDesc = {ZE_STRUCTURE_TYPE_SAMPLER_DESC,
                                         nullptr,
                                         ZE_SAMPLER_ADDRESS_MODE_CLAMP,
                                         ZE_SAMPLER_FILTER_MODE_NEAREST,
                                         normalized};
        SUCCESS_OR_TERMINATE(zeSamplerCreate(context, device, &samplerDesc, &sampler));

        ze_sampler_desc_t samplerDesc2 = {ZE_STRUCTURE_TYPE_SAMPLER_DESC,
                                          nullptr,
                                          ZE_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                          ZE_SAMPLER_FILTER_MODE_NEAREST,
                                          normalized};
        SUCCESS_OR_TERMINATE(zeSamplerCreate(context, device, &samplerDesc2, &sampler2));

        normalized = true;
        ze_sampler_desc_t samplerDesc3 = {ZE_STRUCTURE_TYPE_SAMPLER_DESC,
                                          nullptr,
                                          ZE_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                                          ZE_SAMPLER_FILTER_MODE_LINEAR,
                                          normalized};
        SUCCESS_OR_TERMINATE(zeSamplerCreate(context, device, &samplerDesc3, &sampler3));
    }

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  nullptr,
                                  0,
                                  ZE_IMAGE_TYPE_1D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_FLOAT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  8,
                                  1,
                                  1,
                                  0,
                                  0};

    ze_image_desc_t srcImgDesc2 = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                   nullptr,
                                   0,
                                   ZE_IMAGE_TYPE_1D,
                                   {ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_FLOAT,
                                    ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                    ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                   8,
                                   1,
                                   1,
                                   0,
                                   0};

    ze_image_desc_t srcImgDesc3 = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                   nullptr,
                                   0,
                                   ZE_IMAGE_TYPE_1D,
                                   {ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_FLOAT,
                                    ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                    ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                   8,
                                   1,
                                   1,
                                   0,
                                   0};
    constexpr auto nChannels = 4u;
    constexpr auto bytesPerChannel = sizeof(float);
    constexpr auto bytesPerPixel = bytesPerChannel * nChannels;
    uint32_t xDim = static_cast<uint32_t>(srcImgDesc.width);
    uint32_t yDim = static_cast<uint32_t>(srcImgDesc.height);
    uint32_t zDim = static_cast<uint32_t>(srcImgDesc.depth);
    uint32_t nPixels = xDim * yDim * zDim;
    size_t allocSize = nPixels * bytesPerPixel;

    // Create and initialize host memory
    void *dstBuffer = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, 1, &dstBuffer));

    void *dstBuffer2 = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, 1, &dstBuffer2));

    void *dstBuffer3 = nullptr;
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, 1, &dstBuffer3));

    float *dst = reinterpret_cast<float *>(dstBuffer);
    float *dst2 = reinterpret_cast<float *>(dstBuffer2);
    float *dst3 = reinterpret_cast<float *>(dstBuffer3);

    for (auto iPixel = 0u; iPixel < srcImgDesc.width; ++iPixel) {
        for (auto channel = 0u; channel < 4; ++channel) {
            dst[iPixel * bytesPerChannel + channel] = static_cast<float>(iPixel * 10);
            dst2[iPixel * bytesPerChannel + channel] = static_cast<float>(iPixel * 10);
            dst3[iPixel * bytesPerChannel + channel] = static_cast<float>(iPixel * 10);
        }
    }

    ze_image_handle_t srcImg;
    ze_image_handle_t srcImg2;
    ze_image_handle_t srcImg3;

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = 1u;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    SUCCESS_OR_TERMINATE(zeImageCreate(context, device, &srcImgDesc, &srcImg));
    SUCCESS_OR_TERMINATE(zeImageCreate(context, device, &srcImgDesc2, &srcImg2));
    SUCCESS_OR_TERMINATE(zeImageCreate(context, device, &srcImgDesc3, &srcImg3));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 0, sizeof(dstBuffer), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 1, sizeof(dstBuffer2), &dstBuffer2));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 2, sizeof(dstBuffer3), &dstBuffer3));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 3, sizeof(srcImg), &srcImg));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 4, sizeof(srcImg2), &srcImg2));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 5, sizeof(srcImg3), &srcImg3));

    if (!useInlineSampler) {
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 6, sizeof(sampler), &sampler));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 7, sizeof(sampler2), &sampler2));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 8, sizeof(sampler3), &sampler3));
    }
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernel, xDim, 1u, 1u));

    ze_image_region_t srcRegion = {0, 0, 0, (uint32_t)srcImgDesc.width, (uint32_t)srcImgDesc.height, (uint32_t)srcImgDesc.depth};
    ze_image_region_t srcRegion2 = {0, 0, 0, (uint32_t)srcImgDesc.width, (uint32_t)srcImgDesc.height, (uint32_t)srcImgDesc.depth};
    ze_image_region_t srcRegion3 = {0, 0, 0, (uint32_t)srcImgDesc.width, (uint32_t)srcImgDesc.height, (uint32_t)srcImgDesc.depth};

    std::vector<float> data(nPixels * nChannels);
    std::vector<float> data2(nPixels * nChannels);
    std::vector<float> data3(nPixels * nChannels);

    memcpy(data.data(), dstBuffer, allocSize);
    memcpy(data2.data(), dstBuffer2, allocSize);
    memcpy(data3.data(), dstBuffer3, allocSize);

    SUCCESS_OR_TERMINATE(zeCommandListAppendImageCopyFromMemory(commandHandler.cmdList, srcImg, data.data(),
                                                                &srcRegion, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendImageCopyFromMemory(commandHandler.cmdList, srcImg2, data2.data(),
                                                                &srcRegion2, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendImageCopyFromMemory(commandHandler.cmdList, srcImg3, data3.data(),
                                                                &srcRegion3, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(commandHandler.cmdList, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(commandHandler.appendKernel(kernel, dispatchTraits));
    SUCCESS_OR_TERMINATE(commandHandler.execute());
    SUCCESS_OR_TERMINATE(commandHandler.synchronize());

    // Validate
    float *output = reinterpret_cast<float *>(dstBuffer);
    float *output2 = reinterpret_cast<float *>(dstBuffer2);
    float *output3 = reinterpret_cast<float *>(dstBuffer3);

    // nearest filtering
    std::vector<float> expectedOutput = {10.f, 20.f, 30.f, 40.f, 50.f, 60.f, 70.f, 70.f};
    std::vector<float> expectedOutput2 = {10.f, 20.f, 30.f, 40.f, 50.f, 60.f, 70.f, 0.f};

    // linear filtering
    std::vector<float> expectedOutput3 = {0.f, 8.0f, 18.f, 28.f, 38.f, 48.f, 58.f, 68.f};

    if (useInlineSampler) {
        // inline sampler1 use CLK_ADDRESS_CLAMP - which will return border color (0,0,0,0) or (0,0,0,1)
        expectedOutput = {10.f, 20.f, 30.f, 40.f, 50.f, 60.f, 70.f, 0.f};
        // inline sampler2 use CLK_ADDRESS_CLAMP_TO_EDGE - which will return edge values for out of bounds access
        expectedOutput2 = {10.f, 20.f, 30.f, 40.f, 50.f, 60.f, 70.f, 70.f};
    }

    for (auto i = 0u; i < nPixels; ++i) {
        for (auto j = 0u; j < nChannels; ++j) {

            if (output[i * nChannels + j] != expectedOutput[i]) {
                std::cerr << "error: dstBuffer[" << i << "] channel[" << j << "] = " << output[i * nChannels + j] << " is not equal to " << expectedOutput[i] << "\n ";
                outputValidated = false;
                break;
            }
            if (output2[i * nChannels + j] != expectedOutput2[i]) {
                std::cerr << "error: dstBuffer2[" << i << "] channel[" << j << "] = " << output2[i * nChannels + j] << " is not equal to " << expectedOutput2[i] << "\n ";
                outputValidated = false;
                break;
            }
            int outputFloor3 = static_cast<int>(output3[i * nChannels + j]);
            int expectedFloor3 = static_cast<int>(expectedOutput3[i]);
            if (outputFloor3 != expectedFloor3) {
                std::cerr << "error: dstBuffer3[" << i << "] channel[" << j << "] = " << output3[i * nChannels + j] << " is not equal to " << expectedOutput3[i] << "\n ";
                outputValidated = false;
                break;
            }
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer2));
    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer3));

    if (!useInlineSampler) {
        SUCCESS_OR_TERMINATE(zeSamplerDestroy(sampler));
        SUCCESS_OR_TERMINATE(zeSamplerDestroy(sampler2));
        SUCCESS_OR_TERMINATE(zeSamplerDestroy(sampler3));
    }
    SUCCESS_OR_TERMINATE(zeImageDestroy(srcImg));
    SUCCESS_OR_TERMINATE(zeImageDestroy(srcImg2));
    SUCCESS_OR_TERMINATE(zeImageDestroy(srcImg3));

    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));

    if (outputValidated) {
        std::cout << "\nTest PASSED" << std::endl;
    } else {
        std::cout << "\nTest FAILED" << std::endl;
    }

    return outputValidated;
}

bool runImageQuery(ze_context_handle_t context, ze_device_handle_t device, ze_module_handle_t module,
                   const char *kernelName, ze_image_desc_t &imgDesc, std::vector<uint32_t> &reference, bool imgIsSupported) {
    if (!imgIsSupported) {
        std::cerr << "Kernel " << kernelName << " is not supported and skipped.\n";
        return true;
    }

    ze_kernel_handle_t kernel = nullptr;
    createKernel(module, kernel, kernelName);

    LevelZeroBlackBoxTests::CommandHandler commandHandler;
    bool isImmediateCmdList = false;

    SUCCESS_OR_TERMINATE(commandHandler.create(context, device, isImmediateCmdList));

    ze_group_count_t dispatchTraits;
    dispatchTraits.groupCountX = 1u;
    dispatchTraits.groupCountY = 1u;
    dispatchTraits.groupCountZ = 1u;

    ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
    hostDesc.flags = ZE_HOST_MEM_ALLOC_FLAG_BIAS_UNCACHED;
    void *dstBuffer;
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, reference.size() * sizeof(int), sizeof(int), &dstBuffer));

    ze_image_handle_t img;
    SUCCESS_OR_TERMINATE(zeImageCreate(context, device, &imgDesc, &img));

    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 0, sizeof(dstBuffer), &dstBuffer));
    SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 1, sizeof(img), &img));
    SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernel, 1U, 1U, 1U));

    SUCCESS_OR_TERMINATE(commandHandler.appendKernel(kernel, dispatchTraits));
    SUCCESS_OR_TERMINATE(commandHandler.execute());
    SUCCESS_OR_TERMINATE(commandHandler.synchronize());

    // Validate
    bool outputValidated = true;
    auto *dst = reinterpret_cast<int *>(dstBuffer);
    for (size_t i = 0; i < reference.size(); ++i) {
        if (dst[i] != static_cast<int>(reference[i])) {
            std::cerr << "Kernel " << kernelName << " error: dstBuffer[" << i << "] = " << dst[i] << " is not equal to reference[" << i << "] = " << reference[i] << "\n";
            outputValidated = false;
            break;
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeImageDestroy(img));
    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));

    if (outputValidated) {
        std::cout << "Kernel " << kernelName << " is successful.\n";
    }

    return outputValidated;
}

bool testBindlessImageQuery(ze_context_handle_t context, ze_device_handle_t device, const std::string &deviceId,
                            const std::string &revisionId, AddressingMode mode,
                            bool is1dImageSupported, bool is2dImageSupported, bool is3dImageSupported, bool isImageArraySupported) {
    ze_module_handle_t module = nullptr;
    createModule(source5, mode, context, device, deviceId, revisionId, module, "-cl-ext=+cl_khr_gl_msaa_sharing", false);

    ze_image_desc_t desc1D = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                              nullptr,
                              0,
                              ZE_IMAGE_TYPE_1D,
                              {ZE_IMAGE_FORMAT_LAYOUT_32, ZE_IMAGE_FORMAT_TYPE_UINT,
                               ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_X,
                               ZE_IMAGE_FORMAT_SWIZZLE_X, ZE_IMAGE_FORMAT_SWIZZLE_X},
                              8,
                              1,
                              1,
                              0,
                              0};

    std::vector<uint32_t> reference1D = {static_cast<uint32_t>(desc1D.width),
                                         L0::getClChannelDataType(desc1D.format), L0::getClChannelOrder(desc1D.format),
                                         desc1D.miplevels};

    ze_image_desc_t desc1DArray = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                   nullptr,
                                   0,
                                   ZE_IMAGE_TYPE_1DARRAY,
                                   {ZE_IMAGE_FORMAT_LAYOUT_32_32, ZE_IMAGE_FORMAT_TYPE_SINT,
                                    ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                    ZE_IMAGE_FORMAT_SWIZZLE_X, ZE_IMAGE_FORMAT_SWIZZLE_X},
                                   8,
                                   1,
                                   1,
                                   3,
                                   0};

    std::vector<uint32_t> reference1DArray = {static_cast<uint32_t>(desc1DArray.width),
                                              L0::getClChannelDataType(desc1DArray.format), L0::getClChannelOrder(desc1DArray.format),
                                              desc1DArray.arraylevels, desc1DArray.miplevels};

    ze_image_desc_t desc2D = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                              nullptr,
                              0,
                              ZE_IMAGE_TYPE_2D,
                              {ZE_IMAGE_FORMAT_LAYOUT_16_16, ZE_IMAGE_FORMAT_TYPE_UNORM,
                               ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                               ZE_IMAGE_FORMAT_SWIZZLE_X, ZE_IMAGE_FORMAT_SWIZZLE_X},
                              16,
                              2,
                              1,
                              0,
                              0};

    std::vector<uint32_t> reference2D = {static_cast<uint32_t>(desc2D.width), desc2D.height,
                                         L0::getClChannelDataType(desc2D.format), L0::getClChannelOrder(desc2D.format),
                                         static_cast<uint32_t>(desc2D.width), desc2D.height, desc2D.miplevels};

    ze_image_desc_t desc2DArray = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                   nullptr,
                                   0,
                                   ZE_IMAGE_TYPE_2DARRAY,
                                   {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_SNORM,
                                    ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                    ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_X},
                                   16,
                                   2,
                                   1,
                                   5,
                                   0};

    std::vector<uint32_t> reference2DArray = {static_cast<uint32_t>(desc2DArray.width), desc2DArray.height,
                                              L0::getClChannelDataType(desc2DArray.format), L0::getClChannelOrder(desc2DArray.format),
                                              static_cast<uint32_t>(desc2DArray.width), desc2DArray.height, desc2DArray.arraylevels, desc2DArray.miplevels};

    ze_image_desc_t desc2DDepth = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                   nullptr,
                                   0,
                                   ZE_IMAGE_TYPE_2D,
                                   {ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16, ZE_IMAGE_FORMAT_TYPE_UINT,
                                    ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                    ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_X},
                                   16,
                                   2,
                                   1,
                                   0,
                                   0};

    std::vector<uint32_t> reference2DDepth = {static_cast<uint32_t>(desc2DDepth.width), desc2DDepth.height,
                                              L0::getClChannelDataType(desc2DDepth.format), L0::getClChannelOrder(desc2DDepth.format),
                                              static_cast<uint32_t>(desc2DDepth.width), desc2DDepth.height, desc2DDepth.miplevels};

    ze_image_desc_t desc2DArrayDepth = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                        nullptr,
                                        0,
                                        ZE_IMAGE_TYPE_2DARRAY,
                                        {ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16, ZE_IMAGE_FORMAT_TYPE_FLOAT,
                                         ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                         ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_X},
                                        16,
                                        2,
                                        1,
                                        3,
                                        0};

    std::vector<uint32_t> reference2DArrayDepth = {static_cast<uint32_t>(desc2DArrayDepth.width), desc2DArrayDepth.height,
                                                   L0::getClChannelDataType(desc2DArrayDepth.format), L0::getClChannelOrder(desc2DArrayDepth.format),
                                                   static_cast<uint32_t>(desc2DArrayDepth.width), desc2DArrayDepth.height,
                                                   desc2DArrayDepth.arraylevels, desc2DArrayDepth.miplevels};

    ze_image_desc_t desc2DMsaa = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  nullptr,
                                  0,
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16, ZE_IMAGE_FORMAT_TYPE_UNORM,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  16,
                                  2,
                                  1,
                                  0,
                                  0};

    std::vector<uint32_t> reference2DMsaa = {static_cast<uint32_t>(desc2DMsaa.width), desc2DMsaa.height,
                                             L0::getClChannelDataType(desc2DMsaa.format), L0::getClChannelOrder(desc2DMsaa.format),
                                             static_cast<uint32_t>(desc2DMsaa.width), desc2DMsaa.height, desc2DMsaa.miplevels};

    ze_image_desc_t desc3D = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                              nullptr,
                              0,
                              ZE_IMAGE_TYPE_3D,
                              {ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_FLOAT,
                               ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                               ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                              32,
                              4,
                              3,
                              0,
                              0};

    std::vector<uint32_t> reference3D = {static_cast<uint32_t>(desc3D.width), desc3D.height, desc3D.depth,
                                         L0::getClChannelDataType(desc3D.format), L0::getClChannelOrder(desc3D.format),
                                         static_cast<uint32_t>(desc3D.width), desc3D.height, desc3D.depth, desc3D.miplevels};

    bool res = true;
    res &= runImageQuery(context, device, module, "image_query_1d", desc1D, reference1D, is1dImageSupported);
    res &= runImageQuery(context, device, module, "image_query_1d_array", desc1DArray, reference1DArray, is1dImageSupported && isImageArraySupported);
    res &= runImageQuery(context, device, module, "image_query_2d", desc2D, reference2D, is2dImageSupported);
    res &= runImageQuery(context, device, module, "image_query_2d_array", desc2DArray, reference2DArray, is2dImageSupported && isImageArraySupported);
    res &= runImageQuery(context, device, module, "image_query_2d_depth", desc2DDepth, reference2DDepth, is2dImageSupported);
    res &= runImageQuery(context, device, module, "image_query_2d_array_depth", desc2DArrayDepth, reference2DArrayDepth, is2dImageSupported && isImageArraySupported);
    res &= runImageQuery(context, device, module, "image_query_2d_msaa", desc2DMsaa, reference2DMsaa, is2dImageSupported);
    res &= runImageQuery(context, device, module, "image_query_3d", desc3D, reference3D, is3dImageSupported);

    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));
    return res;
}

bool testZeExperimentalBindlessImages(ze_context_handle_t context, ze_device_handle_t device, const std::string &deviceId, const std::string &revisionId, AddressingMode mode) {
    bool outputValidated = false;

    ze_module_handle_t module = nullptr;
    createModule(source3, mode, context, device, deviceId, revisionId, module, "", false);

    ze_device_compute_properties_t computeProperties = {};
    zeDeviceGetComputeProperties(device, &computeProperties);

    ze_device_pitched_alloc_exp_properties_t extendedProperties = {};
    extendedProperties.stype = ZE_STRUCTURE_TYPE_PITCHED_ALLOC_DEVICE_EXP_PROPERTIES;

    ze_device_image_properties_t imageProperties;
    imageProperties.pNext = &extendedProperties;

    SUCCESS_OR_TERMINATE(zeDeviceGetImageProperties(device, &imageProperties));

    size_t allWidths[] = {
        13,
        extendedProperties.maxImageLinearWidth};

    bool useImageView[] = {false, true};

    for (auto useView : useImageView) {
        for (auto width : allWidths) {

            ze_kernel_handle_t copyKernel = nullptr;
            createKernel(module, copyKernel, kernelName3.c_str());

            size_t rowPitch = 0;

            size_t imageWidth = width;
            size_t imageHeight = 4;
            size_t imageDepth = 1;
            uint32_t elementSizeInBytes = 4;

            uint32_t groupCountX = 1;
            uint32_t groupSizeX = static_cast<uint32_t>(width);
            if (groupSizeX > computeProperties.maxGroupSizeX) {
                groupCountX = groupSizeX / 32;
                groupSizeX = 32;
                imageHeight = 1;
            }

            if (useView) {
                std::cout << "\nTesting imageView: ";

            } else {
                std::cout << "\nTesting image: ";
            }
            std::cout << "width, height, depth, elementSizeInBytes = " << imageWidth << ", " << imageHeight << ", " << imageDepth << ", " << elementSizeInBytes << " ... " << std::endl;

            LevelZeroBlackBoxTests::CommandHandler commandHandler;
            bool isImmediateCmdList = false;
            SUCCESS_OR_TERMINATE(commandHandler.create(context, device, isImmediateCmdList));
            SUCCESS_OR_TERMINATE(zeMemGetPitchFor2dImageFunctionPtr(context, device, imageWidth, imageHeight, elementSizeInBytes, &rowPitch));

            if (LevelZeroBlackBoxTests::verbose) {
                std::cout << "zeMemGetPitchFor2dImageFunctionPtr()  with " << std::dec << // image dimesions
                    "\n\t imageWidth = " << imageWidth <<                                 //
                    "\n\t imageHeight = " << imageHeight <<                               //
                    "\n\t elementSizeInBytes = " << elementSizeInBytes <<                 //
                    "\n returned rowPitch = " << rowPitch << std::endl;

                std::cout << " imageWidth * elementSizeInBytes = " << imageWidth * elementSizeInBytes << std::endl;
            }

            ze_device_mem_alloc_desc_t deviceDesc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC};
            deviceDesc.flags = ZE_DEVICE_MEM_ALLOC_FLAG_BIAS_UNCACHED;
            deviceDesc.ordinal = 0;

            ze_host_mem_alloc_desc_t hostDesc = {ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC};
            hostDesc.flags = ZE_HOST_MEM_ALLOC_FLAG_BIAS_UNCACHED;

            size_t allocSize = rowPitch * imageHeight;

            void *pitchedBuffer = nullptr;
            void *pitchedBufferDevice = nullptr;
            SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, 1, &pitchedBuffer));
            SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, allocSize, 1, device, &pitchedBufferDevice));

            void *dstBuffer = nullptr;
            SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, allocSize, 1, &dstBuffer));

            // Initialize memory
            constexpr uint8_t val = 55;
            memset(pitchedBuffer, 2, allocSize);
            {
                uint8_t *srcCharBuffer = static_cast<uint8_t *>(pitchedBuffer);
                for (size_t i = 0; i < imageHeight; i++) {
                    memset(srcCharBuffer, val, imageWidth * elementSizeInBytes);
                    srcCharBuffer += rowPitch;
                }
            }
            // Copy from heap to device-allocated memory
            SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(commandHandler.cmdList, pitchedBufferDevice, pitchedBuffer, allocSize,
                                                               nullptr, 0, nullptr));
            SUCCESS_OR_TERMINATE(commandHandler.execute());
            SUCCESS_OR_TERMINATE(commandHandler.synchronize());

            memset(dstBuffer, 0, allocSize);

            ze_image_pitched_exp_desc_t pitchedDesc = {};
            pitchedDesc.stype = ZE_STRUCTURE_TYPE_PITCHED_IMAGE_EXP_DESC;
            pitchedDesc.ptr = pitchedBufferDevice;

            ze_image_bindless_exp_desc_t bindlessExtDesc = {};
            bindlessExtDesc.stype = ZE_STRUCTURE_TYPE_BINDLESS_IMAGE_EXP_DESC;
            bindlessExtDesc.pNext = &pitchedDesc;
            bindlessExtDesc.flags = ZE_IMAGE_BINDLESS_EXP_FLAG_BINDLESS;

            ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                          &bindlessExtDesc,
                                          0,
                                          ZE_IMAGE_TYPE_2D,
                                          {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_SINT,
                                           ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                           ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                          imageWidth,
                                          static_cast<uint32_t>(imageHeight),
                                          static_cast<uint32_t>(imageDepth),
                                          0,
                                          0};

            ze_image_handle_t srcImg = nullptr;
            ze_image_handle_t imgView = nullptr;

            ze_group_count_t dispatchTraits = {};
            dispatchTraits.groupCountX = groupCountX;
            dispatchTraits.groupCountY = static_cast<uint32_t>(imageHeight);
            dispatchTraits.groupCountZ = 1u;

            SUCCESS_OR_TERMINATE(zeImageCreate(context, device, &srcImgDesc, &srcImg));

            if (useView) {
                bindlessExtDesc.pNext = nullptr;
                SUCCESS_OR_TERMINATE(zeImageViewCreateExt(context, device, &srcImgDesc, srcImg, &imgView));
            }

            uint64_t bindlessOffset;
            SUCCESS_OR_TERMINATE(zeImageGetDeviceOffsetExpFunctionPtr(srcImg, &bindlessOffset));
            if (LevelZeroBlackBoxTests::verbose) {
                std::cout << "zeImageGetDeviceOffsetExp(srcImg, &bindlessOffset) " << std::dec << "\n\t bindlessOffset = " << bindlessOffset << std::endl;
            }

            SUCCESS_OR_TERMINATE(zeContextMakeMemoryResident(context, device, pitchedBufferDevice, allocSize));

            if (useView) {
                SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(imgView), &imgView));
            } else {
                SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 1, sizeof(srcImg), &srcImg));
            }
            SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(copyKernel, 0, sizeof(dstBuffer), &dstBuffer));
            SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(copyKernel, groupSizeX, 1U, 1U));

            SUCCESS_OR_TERMINATE(commandHandler.appendKernel(copyKernel, dispatchTraits));
            SUCCESS_OR_TERMINATE(commandHandler.execute());
            SUCCESS_OR_TERMINATE(commandHandler.synchronize());

            SUCCESS_OR_TERMINATE(zeContextEvictMemory(context, device, pitchedBufferDevice, allocSize));

            // Validate
            uint8_t *srcCharBuffer = static_cast<uint8_t *>(pitchedBuffer);
            uint8_t *dstCharBuffer = static_cast<uint8_t *>(dstBuffer);

            outputValidated = true;
            for (size_t i = 0; i < imageHeight; i++) {

                if (memcmp(dstCharBuffer, srcCharBuffer, imageWidth * elementSizeInBytes)) {
                    std::cout << "error: dstCharBuffer for row " << i << " not equal to "
                              << "srcCharBuffer at row " << i << "\n";

                    for (size_t x = 0; x < imageWidth * elementSizeInBytes; x++) {
                        std::cout << "error: dstCharBuffer[" << x << "] = " << std::dec << static_cast<unsigned int>(dstCharBuffer[x]) << " not equal to "
                                  << "srcCharBuffer[" << x << "] = " << std::dec << static_cast<unsigned int>(srcCharBuffer[x]) << "\n";
                    }
                    outputValidated = false;
                    break;
                }
                srcCharBuffer += rowPitch;
                dstCharBuffer += imageWidth * elementSizeInBytes;
            }

            if (imgView != nullptr) {
                SUCCESS_OR_TERMINATE(zeImageDestroy(imgView));
            }
            SUCCESS_OR_TERMINATE(zeImageDestroy(srcImg));
            SUCCESS_OR_TERMINATE(zeMemFree(context, pitchedBufferDevice));
            SUCCESS_OR_TERMINATE(zeMemFree(context, pitchedBuffer));
            SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
            if (!outputValidated) {
                std::cout << "\nTest case FAILED" << std::endl;

                break;
            }
            std::cout << "\nTest case PASSED" << std::endl;
            SUCCESS_OR_TERMINATE(zeKernelDestroy(copyKernel));
        }
    }

    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));

    return outputValidated;
}

} // namespace

int main(int argc, char *argv[]) {
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool outputValidated = true;

    ze_driver_handle_t driverHandle = nullptr;
    ze_context_handle_t context = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context, driverHandle);
    auto device = devices[0];

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    LevelZeroBlackBoxTests::printDeviceProperties(deviceProperties);

    std::stringstream ss;
    ss.setf(std::ios::hex, std::ios::basefield);
    ss << "0x" << deviceProperties.deviceId;

    ze_device_uuid_t uuid = deviceProperties.uuid;
    std::string revisionId = std::to_string(reinterpret_cast<uint16_t *>(uuid.id)[2]);

    int numTests = 10;
    int testCase = -1;
    testCase = LevelZeroBlackBoxTests::getParamValue(argc, argv, "", "--test-case", -1);
    if (testCase < -1 || testCase >= numTests) {
        std::cout << "\nInvalid test-case. Valid values are between [-1, " << (numTests - 1) << "]" << std::endl;
        return -1;
    }

    auto bindlessImages = LevelZeroBlackBoxTests::isParamEnabled(argc, argv, "", "--bindless-images");
    AddressingMode mode = bindlessImages ? AddressingMode::bindlessImages : AddressingMode::bindless;

    bool is1dImageSupported = LevelZeroBlackBoxTests::checkImageSupport(device, true, false, false, false);
    bool is2dImageSupported = LevelZeroBlackBoxTests::checkImageSupport(device, false, true, false, false);
    bool is3dImageSupported = LevelZeroBlackBoxTests::checkImageSupport(device, false, false, true, false);
    bool isImageArraySupported = LevelZeroBlackBoxTests::checkImageSupport(device, false, false, false, true);

    for (int i = 0; i < numTests; i++) {
        if (testCase != -1) {
            i = testCase;
        }

        switch (i) {
        default:
        case 0:
            std::cout << "\ntest case: testBindlessBufferCopy\n"
                      << std::endl;
            outputValidated &= testBindlessBufferCopy(context, device, ss.str(), revisionId);
            break;
        case 1:
            std::cout << "\ntest case: testBindlessImages\n"
                      << std::endl;
            if (is2dImageSupported) {
                int defaultImageCount = testCase == 1 ? 4 * 4096 + 8 : 4;
                auto imageCount = LevelZeroBlackBoxTests::getParamValue(argc, argv, "", "--image-count", defaultImageCount);

                std::cout << "--image-count: " << imageCount << std::endl;

                if (bindlessImages) {
                    std::cout << "--bindless-images " << std::endl;
                }

                outputValidated &= testBindlessImages(context, device, ss.str(), revisionId, imageCount, mode, ExecutionMode::commandQueue);
            } else {
                std::cout << "Skipped. testBindlessImages case not supported\n";
            }
            break;
        case 2:
            std::cout << "\ntest case: testBindlessImageSampled\n"
                      << std::endl;
            if (is2dImageSupported) {
                if (bindlessImages) {
                    std::cout << "--bindless-images " << std::endl;
                }
                outputValidated &= testBindlessImageSampled(context, device, ss.str(), revisionId, mode);
            } else {
                std::cout << "Skipped. testBindlessImageSampled not supported\n";
            }
            break;
        case 3:
            std::cout << "\ntest case: testBindlessImageQuery\n"
                      << std::endl;
            if (is1dImageSupported || is2dImageSupported || is3dImageSupported) {
                if (bindlessImages) {
                    std::cout << "--bindless-images " << std::endl;
                }
                outputValidated &= testBindlessImageQuery(context, device, ss.str(), revisionId, mode,
                                                          is1dImageSupported, is2dImageSupported, is3dImageSupported, isImageArraySupported);
            } else {
                std::cout << "Skipped. testBindlessImageQuery not supported\n";
            }
            break;
        case 4: {
            std::cout << "\ntest case: testZeExperimentalBindlessImages\n"
                      << std::endl;

            if (!is2dImageSupported) {
                std::cout << "Skipped. testZeExperimentalBindlessImages not supported\n";
                break;
            }

            std::string extensionName = "ZE_experimental_bindless_image";
            ze_driver_extension_properties_t extensionProperties{};

            strncpy(extensionProperties.name, extensionName.c_str(), extensionName.size());
            extensionProperties.version = ZE_BINDLESS_IMAGE_EXP_VERSION_CURRENT;

            std::vector<ze_driver_extension_properties_t> extensionsToCheck;
            extensionsToCheck.push_back(extensionProperties);

            bool extensionFound = LevelZeroBlackBoxTests::checkExtensionIsPresent(driverHandle, extensionsToCheck);
            if (extensionFound == false) {
                std::cout << extensionName << " extension not found on this driver\n";
            }

            if (extensionFound) {
                auto extensionResult = zeDriverGetExtensionFunctionAddress(driverHandle, "zeMemGetPitchFor2dImage", (void **)&zeMemGetPitchFor2dImageFunctionPtr);
                std::cout << "zeDriverGetExtensionFunctionAddress(\"zeMemGetPitchFor2dImage\") returned: " << extensionResult << std::endl;
                auto extensionResult2 = zeDriverGetExtensionFunctionAddress(driverHandle, "zeImageGetDeviceOffsetExp", (void **)&zeImageGetDeviceOffsetExpFunctionPtr);
                std::cout << "zeDriverGetExtensionFunctionAddress(\"zeImageGetDeviceOffsetExp\") returned: " << extensionResult2 << std::endl;

                if (zeMemGetPitchFor2dImageFunctionPtr == nullptr || zeImageGetDeviceOffsetExpFunctionPtr == nullptr ||
                    extensionResult != ZE_RESULT_SUCCESS || extensionResult2 != ZE_RESULT_SUCCESS) {
                    std::cout << "Failed getting " << extensionName << " extension function address" << std::endl;
                    extensionFound = false;
                }
            }

            if (!extensionFound) {
                std::cout << "Skipped. testZeExperimentalBindlessImages case not supported\n";
                break;
            }

            if (bindlessImages) {
                std::cout << "--bindless-images " << std::endl;
            }
            outputValidated &= testZeExperimentalBindlessImages(context, device, ss.str(), revisionId, mode);
        } break;
        case 5:
            std::cout << "\ntest case: testBindlessImageSampledBorderColor\n"
                      << std::endl;

            if (is2dImageSupported) {
                if (bindlessImages) {
                    std::cout << "--bindless-images " << std::endl;
                }
                outputValidated &= testBindlessImageSampledBorderColor(context, device, ss.str(), revisionId, mode);
            } else {
                std::cout << "Skipped. testBindlessImageSampledBorderColor not supported\n";
            }
            break;
        case 6:
            std::cout << "\ntest case: testBindlessBindfulKernel\n"
                      << std::endl;

            outputValidated &= testBindlessBindfulKernel(context, device, ss.str(), revisionId);
            break;
        case 7:

            if (is2dImageSupported) {
                std::cout << "\ntest case: testBindlessBindfulImageKernel\n"
                          << std::endl;

                outputValidated &= testBindlessBindfulImageKernel(context, device, ss.str(), revisionId);
            } else {
                std::cout << "Skipped. testBindlessBindfulImageKernel case not supported\n";
            }

            break;

        case 8:
            if (is1dImageSupported) {
                bool useInlineSampler = false;
                std::cout << "\ntest case: testBindlessImage1DSampled\n"
                          << std::endl;
                outputValidated &= testBindlessImage1DSampled(context, device, ss.str(), revisionId, mode, useInlineSampler);
            } else {
                std::cout << "Skipped. testBindlessImage1DSampled case not supported\n";
            }
            break;
        case 9:
            if (is1dImageSupported) {
                bool useInlineSampler = true;
                std::cout << "\ntest case: testBindlessImage1DSampled with inline sampler\n"
                          << std::endl;
                outputValidated &= testBindlessImage1DSampled(context, device, ss.str(), revisionId, mode, useInlineSampler);
            } else {
                std::cout << "Skipped. testBindlessImage1DSampled case with inline sampler not supported\n";
            }
            break;
        }

        if (testCase != -1) {
            break;
        }
        if (!outputValidated) {
            break;
        }
    }

    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    if (outputValidated) {
        std::cout << "\nZello  bindless kernel PASSED " << std::endl;
    } else {
        std::cout << "Zello bindless kernel failed\n"
                  << std::endl;
    }
    return outputValidated == false ? -1 : 0;
}

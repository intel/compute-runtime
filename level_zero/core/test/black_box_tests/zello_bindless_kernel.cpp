/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/driver_experimental/public/ze_bindless_image_exp.h"
#include <level_zero/ze_api.h>

#include "zello_common.h"
#include "zello_compile.h"

#include <array>
#include <cstring>
#include <iostream>
#include <numeric>
#include <sstream>

const char *source = R"===(
typedef ulong16 TYPE;
__attribute__((reqd_work_group_size(32, 1, 1)))  // force LWS to 32
__attribute__((intel_reqd_sub_group_size(16)))   // force SIMD to 16
__kernel void kernel_copy(__global char *dst, __global char *src){
    uint gid = get_global_id(0);
    dst[gid] = src[gid];

    __local TYPE locMem[32];
    {
        size_t lid = get_local_id(0);
        size_t gid = get_global_id(0);

        TYPE res1 = (TYPE)(src[gid * 3]);
        TYPE res2 = (TYPE)(src[gid * 3 + 1]);
        TYPE res3 = (TYPE)(src[gid * 3 + 2]);

        locMem[lid] = res1;
        barrier(CLK_LOCAL_MEM_FENCE);
        barrier(CLK_GLOBAL_MEM_FENCE);
    
        TYPE res = (locMem[src[gid]] * res3) * res2 + res1;
        src[0] += (char)res[lid];
        
    }
    barrier(CLK_GLOBAL_MEM_FENCE);
    src[0] = dst[0];
}
)===";

const char *source2 = R"===(
__kernel void kernel_fill(__global char *dst, char value){
    uint gid = get_global_id(0);
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
)===";

static std::string kernelName = "kernel_copy";
static std::string kernelName2 = "kernel_fill";
static std::string kernelName3 = "image_copy";
static std::string kernelName4 = "image_read_sampler";

enum class ExecutionMode : uint32_t {
    commandQueue,
    immSyncCmdList
};

enum class AddressingMode : uint32_t {
    defaultMode,
    bindless,
    bindlessImages
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

void createModule(const char *sourceCode, AddressingMode addressing, const ze_context_handle_t context, const ze_device_handle_t device, const std::string &deviceName, const std::string &revisionId, ze_module_handle_t &module) {
    std::string buildLog;
    std::string bindlessOptions = "-cl-intel-use-bindless-mode -cl-intel-use-bindless-advanced-mode";
    std::string bindlessImagesOptions = "-cl-intel-use-bindless-images -cl-intel-use-bindless-advanced-mode";
    std::string internalOptions = "";
    if (addressing == AddressingMode::bindless) {
        internalOptions = bindlessOptions;
    }
    if (addressing == AddressingMode::bindlessImages) {
        internalOptions = bindlessImagesOptions;
    }
    auto bin = LevelZeroBlackBoxTests::compileToNative(sourceCode, deviceName, revisionId, "", internalOptions, buildLog);
    LevelZeroBlackBoxTests::printBuildLog(buildLog);
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
        }
    }

    SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
    SUCCESS_OR_TERMINATE(zeMemFree(context, srcBuffer));
}

bool testBindlessBufferCopy(ze_context_handle_t context, ze_device_handle_t device, const std::string &deviceId, const std::string &revisionId) {
    bool outputValidated = false;

    ze_module_handle_t module = nullptr;
    ze_module_handle_t module2 = nullptr;
    createModule(source, AddressingMode::bindless, context, device, deviceId, revisionId, module);
    createModule(source2, AddressingMode::defaultMode, context, device, deviceId, revisionId, module2);

    ExecutionMode executionModes[] = {ExecutionMode::commandQueue, ExecutionMode::immSyncCmdList};
    ze_kernel_handle_t copyKernel = nullptr;
    ze_kernel_handle_t fillKernel = nullptr;
    createKernel(module, copyKernel, kernelName.c_str());
    createKernel(module2, fillKernel, kernelName2.c_str());

    for (auto mode : executionModes) {

        run(copyKernel, fillKernel, context, device, 0, mode, outputValidated);

        if (!outputValidated) {
            break;
        }
    }

    SUCCESS_OR_TERMINATE(zeKernelDestroy(copyKernel));
    SUCCESS_OR_TERMINATE(zeKernelDestroy(fillKernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module2));

    return outputValidated;
}

bool testBindlessImages(ze_context_handle_t context, ze_device_handle_t device, const std::string &deviceId, const std::string &revisionId,
                        int imageCount, AddressingMode mode) {
    bool outputValidated = false;

    ze_module_handle_t module = nullptr;
    ze_kernel_handle_t copyKernel = nullptr;

    createModule(source3, mode, context, device, deviceId, revisionId, module);
    createKernel(module, copyKernel, kernelName3.c_str());

    LevelZeroBlackBoxTests::CommandHandler commandHandler;
    bool isImmediateCmdList = false;

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

bool testBindlessImageSampled(ze_context_handle_t context, ze_device_handle_t device, const std::string &deviceId,
                              const std::string &revisionId, AddressingMode mode) {
    bool outputValidated = true;

    ze_module_handle_t module = nullptr;
    ze_kernel_handle_t kernel = nullptr;

    createModule(source4, mode, context, device, deviceId, revisionId, module);
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
    dispatchTraits.groupCountX = 1;
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
    std::vector<float> groundTruth = {5.f, 15.f, 25.f, 30.f};
    for (size_t i = 0; (i < srcImgDesc.height) && outputValidated; ++i) {
        for (size_t j = 0; j < (srcImgDesc.width * 4); ++j, ++dst) {
            if (*dst != groundTruth[i]) {
                std::cerr << "error: dstBuffer[" << i << "][" << j << "] = " << *dst << " is not equal to " << groundTruth[i] << "\n";
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

    return outputValidated;
}

bool testZeExperimentalBindlessImages(ze_context_handle_t context, ze_device_handle_t device, const std::string &deviceId, const std::string &revisionId, AddressingMode mode) {
    bool outputValidated = false;

    ze_module_handle_t module = nullptr;
    ze_kernel_handle_t copyKernel = nullptr;

    createModule(source3, mode, context, device, deviceId, revisionId, module);
    createKernel(module, copyKernel, kernelName3.c_str());

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
            SUCCESS_OR_TERMINATE(zeMemFree(context, pitchedBuffer));
            SUCCESS_OR_TERMINATE(zeMemFree(context, dstBuffer));
            if (!outputValidated) {
                std::cout << "\nTest case FAILED" << std::endl;

                break;
            }
            std::cout << "\nTest case PASSED" << std::endl;
        }
    }

    SUCCESS_OR_TERMINATE(zeKernelDestroy(copyKernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));

    return outputValidated;
}

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

    int numTests = 4;
    int testCase = -1;
    testCase = LevelZeroBlackBoxTests::getParamValue(argc, argv, "", "--test-case", -1);
    if (testCase < -1 || testCase >= numTests) {
        std::cout << "\nInvalid test-case. Valid values are between [-1, " << (numTests - 1) << "]" << std::endl;
        return -1;
    }

    bool isIntegratedGPU = (deviceProperties.flags & ZE_DEVICE_PROPERTY_FLAG_INTEGRATED);

    auto bindlessImages = LevelZeroBlackBoxTests::isParamEnabled(argc, argv, "", "--bindless-images");
    AddressingMode mode = bindlessImages ? AddressingMode::bindlessImages : AddressingMode::bindless;

    bool is2dImageSupported = LevelZeroBlackBoxTests::checkImageSupport(device, false, true, false);

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
            if (!isIntegratedGPU && is2dImageSupported) {
                int defaultImageCount = testCase == 1 ? 4 * 4096 + 8 : 4;
                auto imageCount = LevelZeroBlackBoxTests::getParamValue(argc, argv, "", "--image-count", defaultImageCount);

                std::cout << "--image-count: " << imageCount << std::endl;

                if (bindlessImages) {
                    std::cout << "--bindless-images " << std::endl;
                }

                outputValidated &= testBindlessImages(context, device, ss.str(), revisionId, imageCount, mode);
            } else {
                std::cout << "Skipped. testBindlessImages case not supported\n";
            }
            break;
        case 2:
            std::cout << "\ntest case: testBindlessImageSampled\n"
                      << std::endl;
            if (!isIntegratedGPU && is2dImageSupported) {
                if (bindlessImages) {
                    std::cout << "--bindless-images " << std::endl;
                }
                outputValidated &= testBindlessImageSampled(context, device, ss.str(), revisionId, mode);
            } else {
                std::cout << "Skipped. testBindlessImageSampled not supported\n";
            }
            break;
        case 3:
            std::cout << "\ntest case: testZeExperimentalBindlessImages\n"
                      << std::endl;

            if (isIntegratedGPU || !is2dImageSupported) {
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

            break;
        }

        if (testCase != -1) {
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

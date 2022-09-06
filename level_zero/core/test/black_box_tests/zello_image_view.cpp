/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zello_common.h"
#include "zello_compile.h"

const char *readNV12Module = R"===(
    __kernel void
    ReadNV12Kernel(
        read_only image2d_t nv12Img,
        uint width,
        uint height,
        __global uchar *pDest) {
    int tid_x = get_global_id(0);
    int tid_y = get_global_id(1);
    float4 colorY;
    int2 coord;
    const sampler_t samplerA = CLK_NORMALIZED_COORDS_FALSE |
                               CLK_ADDRESS_NONE |
                               CLK_FILTER_NEAREST;
    if (tid_x < width && tid_y < height) {
        coord = (int2)(tid_x, tid_y);
        if (((tid_y * width) + tid_x) < (width * height)) {
            colorY = read_imagef(nv12Img, samplerA, coord);
            pDest[(tid_y * width) + tid_x] = (uchar)(255.0f * colorY.y);
            if ((tid_x % 2 == 0) && (tid_y % 2 == 0)) {
                pDest[(width * height) + (tid_y / 2 * width) + (tid_x)] = (uchar)(255.0f * colorY.z);
                pDest[(width * height) + (tid_y / 2 * width) + (tid_x) + 1] = (uchar)(255.0f * colorY.x);
            }
        }
    }
}
)===";

void testAppendImageViewNV12Copy(ze_context_handle_t &context, ze_device_handle_t &device, bool &validRet) {
    std::string buildLog;
    auto spirV = compileToSpirV(readNV12Module, "", buildLog);
    if (buildLog.size() > 0) {
        std::cout << "Build log " << buildLog;
    }
    SUCCESS_OR_TERMINATE((0 == spirV.size()));

    const size_t width = 32;
    const size_t height = 32;
    const size_t depth = 1;
    const size_t size = 4 * width * height * depth; /* 4 channels per pixel */

    ze_command_queue_handle_t cmdQueue = nullptr;
    ze_command_list_handle_t cmdList = nullptr;

    ze_command_queue_desc_t cmdQueueDesc = {};
    cmdQueueDesc.pNext = nullptr;
    cmdQueueDesc.flags = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    cmdQueueDesc.ordinal = 0;
    cmdQueueDesc.index = 0;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));

    ze_command_list_desc_t cmdListDesc = {};
    cmdListDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
    cmdListDesc.pNext = nullptr;
    cmdListDesc.flags = 0;
    SUCCESS_OR_TERMINATE(zeCommandListCreate(context, device, &cmdListDesc, &cmdList));

    // create NV12 image
    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  nullptr,
                                  (ZE_IMAGE_FLAG_BIAS_UNCACHED),
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_NV12, ZE_IMAGE_FORMAT_TYPE_UNORM,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  width,
                                  height,
                                  depth,
                                  0,
                                  0};

    ze_image_handle_t srcImg;

    SUCCESS_OR_TERMINATE(
        zeImageCreate(context, device, &srcImgDesc, &srcImg));

    // create image_veiw for Y plane
    ze_image_view_planar_exp_desc_t planeYdesc = {};
    planeYdesc.stype = ZE_STRUCTURE_TYPE_IMAGE_VIEW_PLANAR_EXP_DESC;
    planeYdesc.planeIndex = 0u; // Y plane

    ze_image_desc_t imageViewDescPlaneY = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                           &planeYdesc,
                                           (ZE_IMAGE_FLAG_KERNEL_WRITE | ZE_IMAGE_FLAG_BIAS_UNCACHED),
                                           ZE_IMAGE_TYPE_2D,
                                           {ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                                            ZE_IMAGE_FORMAT_SWIZZLE_A, ZE_IMAGE_FORMAT_SWIZZLE_B,
                                            ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_R},
                                           width,
                                           height,
                                           depth,
                                           0,
                                           0};
    ze_image_handle_t planeYImageView;

    SUCCESS_OR_TERMINATE(
        zeImageViewCreateExp(context, device, &imageViewDescPlaneY, srcImg, &planeYImageView));

    // create image_view for UV plane
    ze_image_view_planar_exp_desc_t planeUVdesc = {};
    planeUVdesc.stype = ZE_STRUCTURE_TYPE_IMAGE_VIEW_PLANAR_EXP_DESC;
    planeUVdesc.planeIndex = 1u; // UV plane

    ze_image_desc_t imageViewDescPlaneUV = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                            &planeUVdesc,
                                            (ZE_IMAGE_FLAG_KERNEL_WRITE | ZE_IMAGE_FLAG_BIAS_UNCACHED),
                                            ZE_IMAGE_TYPE_2D,
                                            {ZE_IMAGE_FORMAT_LAYOUT_8_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                                             ZE_IMAGE_FORMAT_SWIZZLE_A, ZE_IMAGE_FORMAT_SWIZZLE_B,
                                             ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_R},
                                            width / 2,
                                            height / 2,
                                            depth,
                                            0,
                                            0};
    ze_image_handle_t planeUVImageView;

    SUCCESS_OR_TERMINATE(
        zeImageViewCreateExp(context, device, &imageViewDescPlaneUV, srcImg, &planeUVImageView));

    // prepare input data
    std::vector<uint8_t> srcVecY;
    srcVecY.resize(width * height);

    for (size_t i = 0; i < width * height; ++i) {
        srcVecY[i] = static_cast<uint8_t>(i);
    }

    std::vector<uint8_t> srcVecUV;
    srcVecUV.resize((width / 2) * (height));

    for (size_t i = 0; i < (width / 2) * (height); ++i) {
        if (i % 2 == 0) {
            srcVecUV[i] = static_cast<uint8_t>(0x33);
        } else {
            srcVecUV[i] = static_cast<uint8_t>(0x55);
        }
    }

    // prepare destination buffer
    uint8_t *dstMem;

    ze_host_mem_alloc_desc_t hostDesc = {};
    hostDesc.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
    hostDesc.pNext = nullptr;
    hostDesc.flags = 0;
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, size, 1, (void **)(&dstMem)));

    for (size_t i = 0; i < size; ++i) {
        dstMem[i] = 0xff;
    }

    ze_image_region_t srcYRegion = {0, 0, 0, width, height, depth};
    ze_image_region_t srcUVRegion = {0, 0, 0, width / 2, height / 2, depth};

    SUCCESS_OR_TERMINATE(zeCommandListAppendImageCopyFromMemory(cmdList, planeYImageView, srcVecY.data(),
                                                                &srcYRegion, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendImageCopyFromMemory(cmdList, planeUVImageView, srcVecUV.data(),
                                                                &srcUVRegion, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));

    // create kernel which reads NV12 surface
    ze_module_handle_t module = nullptr;
    ze_kernel_handle_t kernel = nullptr;
    {
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
            SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));
            std::cout << "\nZello Image View Results validation FAILED. Module creation error."
                      << std::endl;
            SUCCESS_OR_TERMINATE_BOOL(false);
        }
        SUCCESS_OR_TERMINATE(zeModuleBuildLogDestroy(buildlog));

        ze_kernel_desc_t kernelDesc = {};
        kernelDesc.pKernelName = "ReadNV12Kernel";
        SUCCESS_OR_TERMINATE(zeKernelCreate(module, &kernelDesc, &kernel));

        uint32_t groupSizeX = width;
        uint32_t groupSizeY = height;
        uint32_t groupSizeZ = 1u;
        SUCCESS_OR_TERMINATE(zeKernelSuggestGroupSize(kernel, groupSizeX, 1U, 1U, &groupSizeX, &groupSizeY, &groupSizeZ));
        SUCCESS_OR_TERMINATE(zeKernelSetGroupSize(kernel, groupSizeX, groupSizeY, groupSizeZ));

        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 0, sizeof(srcImg), &srcImg));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 1, sizeof(int), &width));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 2, sizeof(int), &height));
        SUCCESS_OR_TERMINATE(zeKernelSetArgumentValue(kernel, 3, sizeof(void *), &dstMem));

        ze_group_count_t dispatchTraits;
        dispatchTraits.groupCountX = width / groupSizeX;
        dispatchTraits.groupCountY = height / groupSizeY;
        dispatchTraits.groupCountZ = 1u;

        SUCCESS_OR_TERMINATE(zeCommandListAppendLaunchKernel(cmdList, kernel, &dispatchTraits,
                                                             nullptr, 0, nullptr));

        SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));
    }

    // destination buffer for Y plane
    std::vector<uint8_t> dstVecY;
    dstVecY.resize(width * height);
    // destination buffer for UV plane
    std::vector<uint8_t> dstVecUV;
    dstVecUV.resize((width / 2) * (height));

    // read Y plane data
    SUCCESS_OR_TERMINATE(zeCommandListAppendImageCopyToMemory(cmdList, dstVecY.data(), planeYImageView,
                                                              &srcYRegion, nullptr, 0, nullptr));

    // read UV plane data
    SUCCESS_OR_TERMINATE(zeCommandListAppendImageCopyToMemory(cmdList, dstVecUV.data(), planeUVImageView,
                                                              &srcUVRegion, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    // validate Y plane data
    auto result = memcmp(srcVecY.data(), dstVecY.data(), width * height);

    validRet = true;
    if (result != 0) {
        std::cout << "Failed to validate data read for plane Y from Y-plane view" << std::endl;
        validRet = false;
    }

    result = memcmp(dstVecY.data(), dstMem, width * height);

    if (result != 0 && validRet) {
        std::cout << "Failed to validate data read for plane Y from nv12 surface" << std::endl;
        validRet = false;
    }

    // validate UV plane data
    result = memcmp(srcVecUV.data(), dstVecUV.data(), (width / 2) * (height));

    if (result != 0 && validRet) {
        std::cout << "Failed to validate data read for plane Y from Y-plane view" << std::endl;
        validRet = false;
    }

    result = memcmp(dstVecUV.data(), (dstMem + (width * height)), (width / 2) * (height));

    if (result != 0 && validRet) {
        std::cout << "Failed to validate data read for plane UV from nv12 surface" << std::endl;
        validRet = false;
    }

    // cleanup
    SUCCESS_OR_TERMINATE(zeImageDestroy(srcImg));
    SUCCESS_OR_TERMINATE(zeImageDestroy(planeYImageView));
    SUCCESS_OR_TERMINATE(zeImageDestroy(planeUVImageView));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    SUCCESS_OR_TERMINATE(zeKernelDestroy(kernel));
    SUCCESS_OR_TERMINATE(zeModuleDestroy(module));
}

void testAppendImageViewRGBPCopy(ze_context_handle_t &context, ze_device_handle_t &device, bool &validRet) {
    const size_t width = 32;
    const size_t height = 32;
    const size_t depth = 1;
    const size_t size = 3 * width * height * depth; /* 3 channels per pixel */

    ze_command_queue_handle_t cmdQueue = nullptr;
    ze_command_list_handle_t cmdList = nullptr;

    ze_command_queue_desc_t cmdQueueDesc = {};
    cmdQueueDesc.pNext = nullptr;
    cmdQueueDesc.flags = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    cmdQueueDesc.ordinal = 0;
    cmdQueueDesc.index = 0;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));

    ze_command_list_desc_t cmdListDesc = {};
    cmdListDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
    cmdListDesc.pNext = nullptr;
    cmdListDesc.flags = 0;
    SUCCESS_OR_TERMINATE(zeCommandListCreate(context, device, &cmdListDesc, &cmdList));

    // create RGBP image
    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  nullptr,
                                  (ZE_IMAGE_FLAG_BIAS_UNCACHED),
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_RGBP, ZE_IMAGE_FORMAT_TYPE_UINT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  width,
                                  height,
                                  depth,
                                  0,
                                  0};

    ze_image_handle_t srcImg;

    SUCCESS_OR_TERMINATE(
        zeImageCreate(context, device, &srcImgDesc, &srcImg));

    // create image_veiw for Y plane
    ze_image_view_planar_exp_desc_t planeYdesc = {};
    planeYdesc.stype = ZE_STRUCTURE_TYPE_IMAGE_VIEW_PLANAR_EXP_DESC;
    planeYdesc.planeIndex = 0u; // Y plane

    ze_image_desc_t imageViewDescPlaneY = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                           &planeYdesc,
                                           (ZE_IMAGE_FLAG_KERNEL_WRITE | ZE_IMAGE_FLAG_BIAS_UNCACHED),
                                           ZE_IMAGE_TYPE_2D,
                                           {ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                                            ZE_IMAGE_FORMAT_SWIZZLE_A, ZE_IMAGE_FORMAT_SWIZZLE_B,
                                            ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_R},
                                           width,
                                           height,
                                           depth,
                                           0,
                                           0};
    ze_image_handle_t planeYImageView;

    SUCCESS_OR_TERMINATE(
        zeImageViewCreateExp(context, device, &imageViewDescPlaneY, srcImg, &planeYImageView));

    // create image_view for U plane
    ze_image_view_planar_exp_desc_t planeUdesc = {};
    planeUdesc.stype = ZE_STRUCTURE_TYPE_IMAGE_VIEW_PLANAR_EXP_DESC;
    planeUdesc.planeIndex = 1u; // U plane

    ze_image_desc_t imageViewDescPlaneU = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                           &planeUdesc,
                                           (ZE_IMAGE_FLAG_KERNEL_WRITE | ZE_IMAGE_FLAG_BIAS_UNCACHED),
                                           ZE_IMAGE_TYPE_2D,
                                           {ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                                            ZE_IMAGE_FORMAT_SWIZZLE_A, ZE_IMAGE_FORMAT_SWIZZLE_B,
                                            ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_R},
                                           width,
                                           height,
                                           depth,
                                           0,
                                           0};
    ze_image_handle_t planeUImageView;

    SUCCESS_OR_TERMINATE(
        zeImageViewCreateExp(context, device, &imageViewDescPlaneU, srcImg, &planeUImageView));

    // create image_view for V plane
    ze_image_view_planar_exp_desc_t planeVdesc = {};
    planeVdesc.stype = ZE_STRUCTURE_TYPE_IMAGE_VIEW_PLANAR_EXP_DESC;
    planeVdesc.planeIndex = 2u; // V plane

    ze_image_desc_t imageViewDescPlaneV = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                           &planeVdesc,
                                           (ZE_IMAGE_FLAG_KERNEL_WRITE | ZE_IMAGE_FLAG_BIAS_UNCACHED),
                                           ZE_IMAGE_TYPE_2D,
                                           {ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                                            ZE_IMAGE_FORMAT_SWIZZLE_A, ZE_IMAGE_FORMAT_SWIZZLE_B,
                                            ZE_IMAGE_FORMAT_SWIZZLE_G, ZE_IMAGE_FORMAT_SWIZZLE_R},
                                           width,
                                           height,
                                           depth,
                                           0,
                                           0};
    ze_image_handle_t planeVImageView;

    SUCCESS_OR_TERMINATE(
        zeImageViewCreateExp(context, device, &imageViewDescPlaneV, srcImg, &planeVImageView));

    // prepare input data
    std::vector<uint8_t> srcVecY;
    srcVecY.resize(width * height);

    for (size_t i = 0; i < width * height; ++i) {
        srcVecY[i] = static_cast<uint8_t>(i);
    }

    std::vector<uint8_t> srcVecU;
    srcVecU.resize(width * height);

    for (size_t i = 0; i < width * height; ++i) {
        if (i % 2 == 0) {
            srcVecU[i] = static_cast<uint8_t>(0x33);
        } else {
            srcVecU[i] = static_cast<uint8_t>(0x55);
        }
    }

    std::vector<uint8_t> srcVecV;
    srcVecV.resize(width * height);

    for (size_t i = 0; i < width * height; ++i) {
        if (i % 2 == 0) {
            srcVecV[i] = static_cast<uint8_t>(0x44);
        } else {
            srcVecV[i] = static_cast<uint8_t>(0x66);
        }
    }

    // prepare destination buffer
    uint8_t *dstMem;

    ze_host_mem_alloc_desc_t hostDesc = {};
    hostDesc.stype = ZE_STRUCTURE_TYPE_HOST_MEM_ALLOC_DESC;
    hostDesc.pNext = nullptr;
    hostDesc.flags = 0;
    SUCCESS_OR_TERMINATE(zeMemAllocHost(context, &hostDesc, size, 1, (void **)(&dstMem)));

    for (size_t i = 0; i < size; ++i) {
        dstMem[i] = 0xff;
    }

    ze_image_region_t srcYRegion = {0, 0, 0, width, height, depth};
    ze_image_region_t srcURegion = {0, 0, 0, width, height, depth};
    ze_image_region_t srcVRegion = {0, 0, 0, width, height, depth};

    SUCCESS_OR_TERMINATE(zeCommandListAppendImageCopyFromMemory(cmdList, planeYImageView, srcVecY.data(),
                                                                &srcYRegion, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendImageCopyFromMemory(cmdList, planeUImageView, srcVecU.data(),
                                                                &srcURegion, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendImageCopyFromMemory(cmdList, planeVImageView, srcVecV.data(),
                                                                &srcVRegion, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));

    // destination buffer for Y plane
    std::vector<uint8_t> dstVecY;
    dstVecY.resize(width * height);
    // destination buffer for U plane
    std::vector<uint8_t> dstVecU;
    dstVecU.resize(width * height);
    // destination buffer for V plane
    std::vector<uint8_t> dstVecV;
    dstVecV.resize(width * height);

    // read Y plane data
    SUCCESS_OR_TERMINATE(zeCommandListAppendImageCopyToMemory(cmdList, dstVecY.data(), planeYImageView,
                                                              &srcYRegion, nullptr, 0, nullptr));

    // read U plane data
    SUCCESS_OR_TERMINATE(zeCommandListAppendImageCopyToMemory(cmdList, dstVecU.data(), planeUImageView,
                                                              &srcURegion, nullptr, 0, nullptr));

    // read V plane data
    SUCCESS_OR_TERMINATE(zeCommandListAppendImageCopyToMemory(cmdList, dstVecV.data(), planeVImageView,
                                                              &srcVRegion, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    // validate Y plane data
    auto result = memcmp(srcVecY.data(), dstVecY.data(), width * height);

    if (result != 0) {
        std::cout << "Failed to validate data read for plane Y from Y-plane view" << std::endl;
        validRet = false;
    }

    // validate U plane data
    result = memcmp(srcVecU.data(), dstVecU.data(), width * height);

    if (result != 0 && validRet) {
        std::cout << "Failed to validate data read for plane U from U-plane view" << std::endl;
        validRet = false;
    }

    // validate V plane data
    result = memcmp(srcVecV.data(), dstVecV.data(), width * height);

    if (result != 0 && validRet) {
        std::cout << "Failed to validate data read for plane V from V-plane view" << std::endl;
        validRet = false;
    }

    // cleanup
    SUCCESS_OR_TERMINATE(zeImageDestroy(srcImg));
    SUCCESS_OR_TERMINATE(zeImageDestroy(planeYImageView));
    SUCCESS_OR_TERMINATE(zeImageDestroy(planeUImageView));
    SUCCESS_OR_TERMINATE(zeImageDestroy(planeVImageView));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello Image View";
    verbose = isVerbose(argc, argv);
    bool aubMode = isAubMode(argc, argv);

    ze_context_handle_t context = nullptr;
    auto devices = zelloInitContextAndGetDevices(context);
    auto device = devices[0];
    bool outputValidationSuccessful = false;

    ze_device_properties_t deviceProperties = {};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    printDeviceProperties(deviceProperties);

    testAppendImageViewNV12Copy(context, device, outputValidationSuccessful);
    if (outputValidationSuccessful || aubMode) {
        testAppendImageViewRGBPCopy(context, device, outputValidationSuccessful);
    }

    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    printResult(aubMode, outputValidationSuccessful, blackBoxName);
    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;
    return (outputValidationSuccessful ? 0 : 1);
}

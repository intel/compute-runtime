/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>

#include "zello_common.h"

#include <cstring>
#include <iostream>

bool testAppendImageCopy(ze_context_handle_t &context, ze_device_handle_t &device) {
    const size_t width = 32;
    const size_t height = 24;
    const size_t depth = 1;
    const size_t size = 4 * width * height * depth; /* 4 channels per pixel */

    ze_command_queue_handle_t cmdQueue = nullptr;
    ze_command_list_handle_t cmdList = nullptr;

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
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

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  nullptr,
                                  0,
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  width,
                                  height,
                                  depth,
                                  0,
                                  0};

    ze_image_handle_t srcImg;
    ze_image_region_t srcRegion = {0, 0, 0, width, height, depth};

    SUCCESS_OR_TERMINATE(
        zeImageCreate(context, device, &srcImgDesc, &srcImg));

    ze_image_desc_t dstImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  nullptr,
                                  ZE_IMAGE_FLAG_KERNEL_WRITE,
                                  ZE_IMAGE_TYPE_2D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  width,
                                  height,
                                  depth,
                                  0,
                                  0};
    ze_image_handle_t dstImg;
    ze_image_region_t dstRegion = {0, 0, 0, width, height, depth};

    SUCCESS_OR_TERMINATE(
        zeImageCreate(context, device, &dstImgDesc, &dstImg));

    uint8_t *srcBuffer = new uint8_t[size];
    uint8_t *dstBuffer = new uint8_t[size];
    for (size_t i = 0; i < size; ++i) {
        srcBuffer[i] = static_cast<uint8_t>(i);
        dstBuffer[i] = 0xff;
    }

    // Copy from srcBuffer->srcImg->dstImg->dstBuffer, so at the end dstBuffer = srcBuffer
    SUCCESS_OR_TERMINATE(zeCommandListAppendImageCopyFromMemory(cmdList, srcImg, srcBuffer,
                                                                &srcRegion, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(
        zeCommandListAppendImageCopy(cmdList, dstImg, srcImg, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendImageCopyToMemory(cmdList, dstBuffer, dstImg,
                                                              &dstRegion, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    bool validRet = LevelZeroBlackBoxTests::validate(srcBuffer, dstBuffer, size);

    delete[] srcBuffer;
    delete[] dstBuffer;
    SUCCESS_OR_TERMINATE(zeImageDestroy(dstImg));
    SUCCESS_OR_TERMINATE(zeImageDestroy(srcImg));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));

    return validRet;
}

bool testAppendImageCopyExt(ze_context_handle_t &context, ze_device_handle_t &device) {
    constexpr size_t width = 32;
    constexpr size_t height = 24;
    constexpr size_t depth = 1;
    constexpr size_t bytesPerPixel = 4;

    ze_command_queue_handle_t cmdQueue = nullptr;
    ze_command_list_handle_t cmdList = nullptr;

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
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

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  nullptr,
                                  0,
                                  ZE_IMAGE_TYPE_3D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  width,
                                  height,
                                  depth,
                                  0,
                                  0};

    ze_image_handle_t srcImg;
    ze_image_region_t srcRegion = {0, 0, 0, width, height, depth};

    SUCCESS_OR_TERMINATE(
        zeImageCreate(context, device, &srcImgDesc, &srcImg));

    ze_image_desc_t dstImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  nullptr,
                                  ZE_IMAGE_FLAG_KERNEL_WRITE,
                                  ZE_IMAGE_TYPE_3D,
                                  {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  width,
                                  height,
                                  depth,
                                  0,
                                  0};
    ze_image_handle_t dstImg;
    ze_image_region_t dstRegion = {0, 0, 0, width, height, depth};

    SUCCESS_OR_TERMINATE(
        zeImageCreate(context, device, &dstImgDesc, &dstImg));

    constexpr uint32_t srcRowPitch = bytesPerPixel * width * 2;
    constexpr uint32_t srcSlicePitch = srcRowPitch * height;
    constexpr size_t srcBufferSize = srcSlicePitch * depth;
    uint8_t *srcBuffer = new uint8_t[srcBufferSize];
    for (size_t i = 0; i < (srcBufferSize / bytesPerPixel); ++i) {
        auto v = static_cast<uint8_t>(i % 256);
        srcBuffer[i * bytesPerPixel] = v;
        srcBuffer[i * bytesPerPixel + 1] = v;
        srcBuffer[i * bytesPerPixel + 2] = v;
        srcBuffer[i * bytesPerPixel + 3] = v;
    }

    constexpr uint32_t dstRowPitch = bytesPerPixel * width * 4;
    constexpr uint32_t dstSlicePitch = dstRowPitch * height;
    constexpr size_t dstBufferSize = dstSlicePitch * depth;
    uint8_t *dstBuffer = new uint8_t[dstBufferSize];
    std::fill(dstBuffer, dstBuffer + dstBufferSize, 0xff);

    // Copy from srcBuffer->srcImg->dstImg->dstBuffer, so at the end dstRegion of dstBuffer = srcRegion of srcBuffer
    SUCCESS_OR_TERMINATE(zeCommandListAppendImageCopyFromMemoryExt(cmdList, srcImg, srcBuffer,
                                                                   &srcRegion, srcRowPitch, srcSlicePitch, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(
        zeCommandListAppendImageCopy(cmdList, dstImg, srcImg, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendImageCopyToMemoryExt(cmdList, dstBuffer, dstImg,
                                                                 &dstRegion, dstRowPitch, dstSlicePitch, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    bool validRet = true;
    for (size_t d = 0; (d < depth) && validRet; ++d) {
        uint8_t *srcSlice = srcBuffer + srcSlicePitch * d;
        uint8_t *dstSlice = dstBuffer + dstSlicePitch * d;
        for (size_t h = 0; h < height; ++h) {
            uint8_t *srcRow = srcSlice + srcRowPitch * h;
            uint8_t *dstRow = dstSlice + dstRowPitch * h;
            if (std::memcmp(srcRow, dstRow, bytesPerPixel * width)) {
                validRet = false;
                break;
            }
        }
    }

    delete[] srcBuffer;
    delete[] dstBuffer;
    SUCCESS_OR_TERMINATE(zeImageDestroy(dstImg));
    SUCCESS_OR_TERMINATE(zeImageDestroy(srcImg));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));

    return validRet;
}

bool testAppendImageCopyArray(ze_context_handle_t &context, ze_device_handle_t &device, bool useCopyEngine, const ze_image_type_t type, const uint32_t width, const uint32_t height, const uint32_t arraylevels, bool testImageCopyRegion) {
    constexpr uint32_t depth = 1;
    constexpr size_t bytesPerPixel = 4;

    uint32_t groupOrdinal = 0;
    if (useCopyEngine) {
        groupOrdinal = LevelZeroBlackBoxTests::getCopyOnlyCommandQueueOrdinal(device);
        if (groupOrdinal == std::numeric_limits<uint32_t>::max()) {
            std::cout << "No Copy queue group found. Skipping testAppendImageCopyExtArray run\n"; // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
            return true;
        }
    }

    ze_command_queue_handle_t cmdQueue = nullptr;
    ze_command_list_handle_t cmdList = nullptr;

    ze_command_queue_desc_t cmdQueueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    cmdQueueDesc.pNext = nullptr;
    cmdQueueDesc.flags = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    cmdQueueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    cmdQueueDesc.ordinal = groupOrdinal;
    cmdQueueDesc.index = 0;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));

    ze_command_list_desc_t cmdListDesc = {};
    cmdListDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
    cmdListDesc.pNext = nullptr;
    cmdListDesc.commandQueueGroupOrdinal = groupOrdinal;
    cmdListDesc.flags = 0;
    SUCCESS_OR_TERMINATE(zeCommandListCreate(context, device, &cmdListDesc, &cmdList));

    ze_image_desc_t imgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                               nullptr,
                               0,
                               type,
                               {ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8, ZE_IMAGE_FORMAT_TYPE_UINT,
                                ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                               width,
                               height,
                               depth,
                               arraylevels,
                               0};
    ze_image_handle_t img;
    SUCCESS_OR_TERMINATE(
        zeImageCreate(context, device, &imgDesc, &img));

    ze_image_region_t region = {0, 0, 0, width, height, depth};
    if (type == ZE_IMAGE_TYPE_1DARRAY) {
        region.height = arraylevels;
    } else if (type == ZE_IMAGE_TYPE_2DARRAY) {
        region.depth = arraylevels;
    }

    ze_image_handle_t img2 = nullptr;
    ze_image_region_t region2 = region;
    constexpr uint32_t offset = 4;
    if (testImageCopyRegion) {
        region2.originX += offset;
        imgDesc.width += 2 * offset;
        if (type == ZE_IMAGE_TYPE_2DARRAY) {
            region2.originY += offset;
            imgDesc.height += 2 * offset;
        }
        SUCCESS_OR_TERMINATE(zeImageCreate(context, device, &imgDesc, &img2));
    }

    const uint32_t rowPitch = bytesPerPixel * width;
    const size_t imgSize = rowPitch * height * arraylevels;
    uint8_t *srcBuffer = new uint8_t[imgSize];
    uint8_t *dstBuffer = new uint8_t[imgSize];
    for (size_t i = 0; i < imgSize; ++i) {
        auto v = static_cast<uint8_t>(i % 256);
        srcBuffer[i] = v;
        dstBuffer[i] = 0xff;
    }

    // Copy from srcBuffer->img[->img2]->dstBuffer, so at the end dstBuffer = srcBuffer
    SUCCESS_OR_TERMINATE(zeCommandListAppendImageCopyFromMemoryExt(cmdList, img, srcBuffer,
                                                                   &region, 0u, 0u, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));
    if (testImageCopyRegion) {
        SUCCESS_OR_TERMINATE(zeCommandListAppendImageCopyRegion(cmdList, img2, img,
                                                                &region2, &region, nullptr, 0, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));
        SUCCESS_OR_TERMINATE(zeCommandListAppendImageCopyToMemoryExt(cmdList, dstBuffer, img2,
                                                                     &region2, 0u, 0u, nullptr, 0, nullptr));
    } else {
        SUCCESS_OR_TERMINATE(zeCommandListAppendImageCopyToMemoryExt(cmdList, dstBuffer, img,
                                                                     &region, 0u, 0u, nullptr, 0, nullptr));
    }
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    bool validRet = LevelZeroBlackBoxTests::validate(srcBuffer, dstBuffer, imgSize);

    delete[] srcBuffer;
    delete[] dstBuffer;
    SUCCESS_OR_TERMINATE(zeImageDestroy(img));
    if (testImageCopyRegion) {
        SUCCESS_OR_TERMINATE(zeImageDestroy(img2));
    }
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));

    return validRet;
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello Copy Image";
    const std::string blackBoxNameExt = "Zello Copy Image Ext";
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);

    ze_context_handle_t context = nullptr;
    auto devices = LevelZeroBlackBoxTests::zelloInitContextAndGetDevices(context);
    auto device = devices[0];

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    LevelZeroBlackBoxTests::printDeviceProperties(deviceProperties);

    if (!LevelZeroBlackBoxTests::checkImageSupport(device, false, true, false, false)) {
        return 0;
    }

    bool outputValidationSuccessful = testAppendImageCopy(context, device);
    bool result = outputValidationSuccessful;
    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxName);
    outputValidationSuccessful = testAppendImageCopyExt(context, device);
    result &= outputValidationSuccessful;
    LevelZeroBlackBoxTests::printResult(aubMode, outputValidationSuccessful, blackBoxNameExt);

    bool result1DArray = true;
    for (uint32_t width = 1; width <= 128; width <<= 1) {
        for (uint32_t arraylevels = 1; arraylevels <= 5; ++arraylevels) {
            result1DArray &= testAppendImageCopyArray(context, device, true, ZE_IMAGE_TYPE_1DARRAY, width, 1u, arraylevels, false);
            result1DArray &= testAppendImageCopyArray(context, device, true, ZE_IMAGE_TYPE_1DARRAY, width, 1u, arraylevels, true);
        }
    }
    result &= result1DArray;
    LevelZeroBlackBoxTests::printResult(aubMode, result1DArray, blackBoxNameExt + " 1DARRAY");

    bool result2DArray = true;
    for (uint32_t width = 1; width <= 128; width <<= 1) {
        for (uint32_t height = 1; height < 9; ++height) {
            for (uint32_t arraylevels = 1; arraylevels <= 5; ++arraylevels) {
                result2DArray &= testAppendImageCopyArray(context, device, true, ZE_IMAGE_TYPE_2DARRAY, width, height, arraylevels, false);
                result2DArray &= testAppendImageCopyArray(context, device, true, ZE_IMAGE_TYPE_2DARRAY, width, height, arraylevels, true);
            }
        }
    }
    result &= result2DArray;
    LevelZeroBlackBoxTests::printResult(aubMode, result1DArray, blackBoxNameExt + " 2DARRAY");

    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    result = aubMode ? true : result;
    return (result ? 0 : 1);
}

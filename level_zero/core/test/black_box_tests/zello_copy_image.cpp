/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>

#include "zello_common.h"

#include <fstream>
#include <iostream>
#include <memory>

void testAppendImageCopy(ze_context_handle_t &context, ze_device_handle_t &device, bool &validRet) {
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

    validRet = (0 == memcmp(srcBuffer, dstBuffer, size));

    delete[] srcBuffer;
    delete[] dstBuffer;
    SUCCESS_OR_TERMINATE(zeImageDestroy(dstImg));
    SUCCESS_OR_TERMINATE(zeImageDestroy(srcImg));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello Copy Image";
    verbose = isVerbose(argc, argv);
    bool aubMode = isAubMode(argc, argv);

    ze_context_handle_t context = nullptr;
    auto devices = zelloInitContextAndGetDevices(context);
    auto device = devices[0];
    bool outputValidationSuccessful;

    ze_device_properties_t deviceProperties = {ZE_STRUCTURE_TYPE_DEVICE_PROPERTIES};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));
    printDeviceProperties(deviceProperties);

    testAppendImageCopy(context, device, outputValidationSuccessful);
    SUCCESS_OR_TERMINATE(zeContextDestroy(context));

    printResult(aubMode, outputValidationSuccessful, blackBoxName);

    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;
    return (outputValidationSuccessful ? 0 : 1);
}

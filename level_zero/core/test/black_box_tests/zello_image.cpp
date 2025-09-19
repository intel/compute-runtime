/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>

#include "zello_common.h"

#include <algorithm>
#include <iostream>

#define imageIndex(buf, x, y, z, chan) \
    (buf)[(chan) +                     \
          (x)*channels +               \
          (y)*channels * hostWidth +   \
          (z)*channels * hostWidth * hostHeight]

void testAppendImageFunction(ze_context_handle_t &context,
                             ze_device_handle_t &device,
                             ze_command_queue_handle_t &cmdQueue,
                             uint32_t cmdQueueOrdinal,
                             bool &validRet,
                             ze_image_type_t imageType) {

    const size_t channels = 4;

    uint32_t hostWidth = LevelZeroBlackBoxTests::verbose ? 5 : 131;
    uint32_t hostHeight = LevelZeroBlackBoxTests::verbose ? 4 : 89;
    uint32_t hostDepth = LevelZeroBlackBoxTests::verbose ? 3 : 10;

    // Apply a few pixels of offset to copy-in and copy-out
    uint32_t inOffsetX = 1;
    uint32_t inOffsetY = 2;
    uint32_t inOffsetZ = 3;

    uint32_t outOffsetX = 2;
    uint32_t outOffsetY = 3;
    uint32_t outOffsetZ = 1;

    if (imageType != ZE_IMAGE_TYPE_3D) {
        hostDepth = 1;
        inOffsetZ = 0;
        outOffsetZ = 0;

        if (imageType != ZE_IMAGE_TYPE_2D) {
            hostHeight = 1;
            inOffsetY = 0;
            outOffsetY = 0;
        }
    }

    const size_t hostSize = hostWidth * hostHeight * hostDepth * channels;

    // GPU image should be big enough to accommodate host image plus offsets
    const uint32_t gpuWidth = hostWidth + std::max(inOffsetX, outOffsetX);
    const uint32_t gpuHeight = hostHeight + std::max(inOffsetY, outOffsetY);
    const uint32_t gpuDepth = hostDepth + std::max(inOffsetZ, outOffsetZ);

    ze_command_list_handle_t cmdList;

    ze_command_list_desc_t cmdListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    cmdListDesc.commandQueueGroupOrdinal = cmdQueueOrdinal;
    SUCCESS_OR_TERMINATE(zeCommandListCreate(context, device, &cmdListDesc, &cmdList));

    ze_image_desc_t srcImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  nullptr,
                                  0,
                                  imageType,
                                  {ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_UINT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  gpuWidth,
                                  gpuHeight,
                                  gpuDepth,
                                  0,
                                  0};
    ze_image_handle_t srcImg;
    ze_image_region_t srcRegion = {inOffsetX, inOffsetY, inOffsetZ, hostWidth, hostHeight, hostDepth};

    SUCCESS_OR_TERMINATE(
        zeImageCreate(context, device, &srcImgDesc, &srcImg));

    ze_image_desc_t dstImgDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC,
                                  nullptr,
                                  ZE_IMAGE_FLAG_KERNEL_WRITE,
                                  imageType,
                                  {ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32, ZE_IMAGE_FORMAT_TYPE_UINT,
                                   ZE_IMAGE_FORMAT_SWIZZLE_R, ZE_IMAGE_FORMAT_SWIZZLE_G,
                                   ZE_IMAGE_FORMAT_SWIZZLE_B, ZE_IMAGE_FORMAT_SWIZZLE_A},
                                  gpuWidth,
                                  gpuHeight,
                                  gpuDepth,
                                  0,
                                  0};

    ze_image_handle_t dstImg;
    ze_image_region_t dstRegion = {outOffsetX, outOffsetY, outOffsetZ, hostWidth, hostHeight, hostDepth};

    SUCCESS_OR_TERMINATE(
        zeImageCreate(context, device, &dstImgDesc, &dstImg));

    uint32_t *srcBuffer = new uint32_t[hostSize];
    uint32_t *dstBuffer = new uint32_t[hostSize];
    for (size_t i = 0; i < hostSize; ++i) {
        srcBuffer[i] = static_cast<uint32_t>((i + 1) & 0xffffffff);
        dstBuffer[i] = 0xff;
    }

    // Copy from srcBuffer to srcImg to dstImg to dstBuffer
    // At the end dstBuffer should be the same srcBuffer with some offset
    SUCCESS_OR_TERMINATE(zeCommandListAppendImageCopyFromMemory(cmdList, srcImg, srcBuffer,
                                                                &srcRegion, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendImageCopy(cmdList, dstImg, srcImg, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListAppendImageCopyToMemory(cmdList, dstBuffer, dstImg,
                                                              &dstRegion, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    if (LevelZeroBlackBoxTests::verbose) {
        size_t pixelWidth = channels;
        size_t rowWidth = pixelWidth * hostWidth;
        size_t sliceWidth = rowWidth * hostHeight;
        size_t totalWidth = sliceWidth * hostDepth;

        std::cout << "source" << std::endl;
        for (size_t i = 0; i < hostSize; ++i) {
            std::cout << static_cast<uint32_t>(srcBuffer[i]) << " ";
            if (i % sliceWidth == sliceWidth - 1 && i != totalWidth - 1)
                std::cout << std::endl
                          << "---" << std::endl;
            else if (i % rowWidth == rowWidth - 1)
                std::cout << std::endl;
            else if (i % pixelWidth == pixelWidth - 1)
                std::cout << "| ";
        }
        std::cout << std::endl;

        std::cout << "destination" << std::endl;
        for (size_t i = 0; i < hostSize; ++i) {
            std::cout << static_cast<uint32_t>(dstBuffer[i]) << " ";
            if (i % sliceWidth == sliceWidth - 1 && i != totalWidth - 1)
                std::cout << std::endl
                          << "---" << std::endl;
            else if (i % rowWidth == rowWidth - 1)
                std::cout << std::endl;
            else if (i % pixelWidth == pixelWidth - 1)
                std::cout << "| ";
        }
        std::cout << std::endl;
    }

    validRet = true;

    int errorPrintLimit = 30;
    for (uint32_t xi = 0; xi < hostWidth; xi++) {
        for (uint32_t yi = 0; yi < hostHeight; yi++) {
            for (uint32_t zi = 0; zi < hostDepth; zi++) {
                int xo = xi + inOffsetX - outOffsetX;
                int yo = yi + inOffsetY - outOffsetY;
                int zo = zi + inOffsetZ - outOffsetZ;

                // pixels in the margins will have junk if offsets don't match; ignore those
                if (xo >= 0 && yo >= 0 && zo >= 0 &&
                    xo < static_cast<int>(hostWidth) &&
                    yo < static_cast<int>(hostHeight) &&
                    zo < static_cast<int>(hostDepth)) {
                    for (size_t chan = 0; chan < channels; chan++) {
                        auto input = imageIndex(srcBuffer, xi, yi, zi, chan);
                        auto output = imageIndex(dstBuffer, xo, yo, zo, chan);
                        if (input != output) {
                            validRet = false;
                            if (errorPrintLimit > 0) {
                                std::cout << "error: " << xi << "," << yi << "," << zi
                                          << " (" << input << ") does not match "
                                          << xo << "," << yo << "," << zo
                                          << " (" << output << ")" << std::endl;
                                errorPrintLimit--;
                            }
                        }
                    }
                }
            }
        }
    }

    delete[] srcBuffer;
    delete[] dstBuffer;
    SUCCESS_OR_TERMINATE(zeImageDestroy(srcImg));
    SUCCESS_OR_TERMINATE(zeImageDestroy(dstImg));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
}

int main(int argc, char *argv[]) {
    const std::string blackBoxName = "Zello Image";
    LevelZeroBlackBoxTests::verbose = LevelZeroBlackBoxTests::isVerbose(argc, argv);
    bool aubMode = LevelZeroBlackBoxTests::isAubMode(argc, argv);

    bool do1D = LevelZeroBlackBoxTests::isParamEnabled(argc, argv, "-1", "--1D");
    bool do2D = LevelZeroBlackBoxTests::isParamEnabled(argc, argv, "-2", "--2D");
    bool do3D = LevelZeroBlackBoxTests::isParamEnabled(argc, argv, "-3", "--3D");

    // by default, do all tests
    if (!do1D && !do2D && !do3D) {
        do1D = do2D = do3D = true;
    }

    ze_driver_handle_t driver;
    ze_context_handle_t context;
    ze_device_handle_t device;
    ze_command_queue_handle_t cmdQueue;
    uint32_t cmdQueueOrdinal;
    LevelZeroBlackBoxTests::initialize(driver, context, device, cmdQueue, cmdQueueOrdinal);

    if (!LevelZeroBlackBoxTests::checkImageSupport(device, do1D, do2D, do3D, false)) {
        return 0;
    }

    bool success1D = false;
    bool success2D = false;
    bool success3D = false;

    std::string caseName;
    if (do1D) {
        caseName = "1D";
        testAppendImageFunction(context, device, cmdQueue, cmdQueueOrdinal, success1D, ZE_IMAGE_TYPE_1D);
        LevelZeroBlackBoxTests::printResult(aubMode, success1D, blackBoxName, caseName);
    }
    if (do2D) {
        caseName = "2D";
        testAppendImageFunction(context, device, cmdQueue, cmdQueueOrdinal, success2D, ZE_IMAGE_TYPE_2D);
        LevelZeroBlackBoxTests::printResult(aubMode, success1D, blackBoxName, caseName);
    }
    if (do3D) {
        caseName = "3D";
        testAppendImageFunction(context, device, cmdQueue, cmdQueueOrdinal, success3D, ZE_IMAGE_TYPE_3D);
        LevelZeroBlackBoxTests::printResult(aubMode, success1D, blackBoxName, caseName);
    }
    LevelZeroBlackBoxTests::teardown(cmdQueue);

    bool outputValidationSuccessful = !((do1D && !success1D) || (do2D && !success2D) || (do3D && !success3D));
    outputValidationSuccessful = aubMode ? true : outputValidationSuccessful;
    return outputValidationSuccessful ? 0 : 1;
}

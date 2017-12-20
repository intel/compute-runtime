/*
* Copyright (c) 2017, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#include "runtime/command_queue/dispatch_walker.h"
#include "gtest/gtest.h"

using namespace OCLRT;

TEST(localWorkSizeTest, given1DimWorkGroupAndSimdEqual8WhenComputeCalledThenLocalGroupComputed) {
    //wsInfo maxWorkGroupSize, hasBariers, simdSize, slmTotalSize, coreFamily, numThreadsPerSlice, localMemorySize, imgUsed, yTiledSurface
    WorkSizeInfo wsInfo(256, 0u, 8, 0u, IGFX_GEN9_CORE, 32u, 0u, false, false);
    uint32_t workDim = 1;
    size_t workGroup[3] = {6144, 1, 1};
    size_t workGroupSize[3];

    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 64u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 48;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 16u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 1536;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 64u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 333;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 9u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 128;
    wsInfo.simdSize = 0;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 1u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, given1DimWorkGroupAndSimdEqual32WhenComputeCalledThenLocalGroupComputed) {
    WorkSizeInfo wsInfo(256, 0u, 32, 0u, IGFX_GEN9_CORE, 32u, 0u, false, false);
    uint32_t workDim = 1;
    size_t workGroup[3] = {6144, 1, 1};
    size_t workGroupSize[3];

    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 48;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 48u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 512;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, given2DimWorkGroupAndSimdEqual8WhenComputeCalledThenLocalGroupComputed) {
    WorkSizeInfo wsInfo(256, 0u, 8, 0u, IGFX_GEN9_CORE, 32u, 0u, false, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {384, 96, 1};
    size_t workGroupSize[3];

    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 64u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 48;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 16u);
    ;
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 12;
    workGroup[1] = 512;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 4u);
    EXPECT_EQ(workGroupSize[1], 16u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, given2DimWorkGroupAndSimdEqual32WhenComputeCalledThenLocalGroupComputed) {
    WorkSizeInfo wsInfo(256, 0u, 32, 0u, IGFX_GEN9_CORE, 32u, 0u, false, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {384, 96, 1};
    size_t workGroupSize[3];

    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 128u);
    EXPECT_EQ(workGroupSize[1], 2u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 48;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 16u);
    EXPECT_EQ(workGroupSize[1], 16u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 12;
    workGroup[1] = 512;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 4u);
    EXPECT_EQ(workGroupSize[1], 64u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 1;
    workGroup[1] = 384;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 1u);
    EXPECT_EQ(workGroupSize[1], 128u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, given3DimWorkGroupAndSimdEqual8WhenComputeCalledThenLocalGroupComputed) {
    WorkSizeInfo wsInfo(256, 0u, 8, 0u, IGFX_GEN9_CORE, 32u, 0u, false, false);
    uint32_t workDim = 3;
    size_t workGroup[3] = {384, 384, 384};
    size_t workGroupSize[3];

    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 64u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 96;
    workGroup[1] = 4;
    workGroup[2] = 4;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 32u);
    EXPECT_EQ(workGroupSize[1], 2u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 12;
    workGroup[1] = 512;
    workGroup[2] = 48;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 4u);
    EXPECT_EQ(workGroupSize[1], 16u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 2;
    workGroup[1] = 2;
    workGroup[2] = 3;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 2u);
    EXPECT_EQ(workGroupSize[1], 2u);
    EXPECT_EQ(workGroupSize[2], 3u);
}

TEST(localWorkSizeTest, given3DimWorkGroupAndSimdEqual32WhenComputeCalledThenLocalGroupComputed) {
    OCLRT::WorkSizeInfo wsInfo(256, 0u, 32, 0u, IGFX_GEN9_CORE, 32u, 0u, false, false);
    uint32_t workDim = 3;
    size_t workGroup[3] = {384, 384, 384};
    size_t workGroupSize[3];
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 128u);
    EXPECT_EQ(workGroupSize[1], 2u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 96;
    workGroup[1] = 6;
    workGroup[2] = 4;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 32u);
    EXPECT_EQ(workGroupSize[1], 2u);
    EXPECT_EQ(workGroupSize[2], 4u);

    workGroup[0] = 12;
    workGroup[1] = 512;
    workGroup[2] = 48;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 4u);
    EXPECT_EQ(workGroupSize[1], 64u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 6;
    workGroup[1] = 4;
    workGroup[2] = 64;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 2u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 32u);

    workGroup[0] = 113;
    workGroup[1] = 113;
    workGroup[2] = 113;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 113u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, given2DimWorkGroupAndSimdEqual256WhenComputeCalledThenLocalGroupComputed) {
    WorkSizeInfo wsInfo(256, 0u, 256, 0u, IGFX_GEN9_CORE, 32u, 0u, false, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {384, 96, 1};
    size_t workGroupSize[3];

    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 128u);
    EXPECT_EQ(workGroupSize[1], 2u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, basemarkKernelAddConstantMultiply) {
    WorkSizeInfo wsInfo(256, 0u, 32, 0u, IGFX_GEN9_CORE, 32u, 0u, true, true);
    uint32_t workDim = 2;
    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 2048;
    workGroup[1] = 2048;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 64u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 1920;
    workGroup[1] = 1080;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 64u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, basemarkKernelAddDrop) {
    WorkSizeInfo wsInfo(256, 0u, 32, 0u, IGFX_GEN9_CORE, 32u, 0u, false, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 1024;
    workGroup[1] = 1024;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, basemarkKernelAddForcesAndDensity) {
    WorkSizeInfo wsInfo(256, 0u, 32, 0u, IGFX_GEN9_CORE, 32u, 0u, false, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 1024;
    workGroup[1] = 1024;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}
TEST(localWorkSizeTest, basemarkKernelAdvect) {
    WorkSizeInfo wsInfo(256, 0u, 32, 0u, IGFX_GEN9_CORE, 32u, 0u, false, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 1024;
    workGroup[1] = 1024;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, basemarkKernelAlignedBuff2Img) {
    WorkSizeInfo wsInfo(256, 0u, 32, 0u, IGFX_GEN9_CORE, 32u, 0u, true, true);
    uint32_t workDim = 2;
    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 256;
    workGroup[1] = 1024;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 64u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 512;
    workGroup[1] = 2048;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 64u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 480;
    workGroup[1] = 1080;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 32u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 196;
    workGroup[1] = 30;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 49u);
    EXPECT_EQ(workGroupSize[1], 5u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, basemarkKernelAlignedCopyBuffer) {
    WorkSizeInfo wsInfo(256, 0u, 32, 0u, IGFX_GEN9_CORE, 32u, 0u, false, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 65536;
    workGroup[1] = 1;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 524288;
    workGroup[1] = 1;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 256u);
    EXPECT_EQ(workGroupSize[1], 1u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, basemarkKernelAlignedCopyBuff2D) {
    WorkSizeInfo wsInfo(256, 0u, 32, 0u, IGFX_GEN9_CORE, 32u, 0u, true, true);
    uint32_t workDim = 2;
    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 256;
    workGroup[1] = 1024;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 64u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 512;
    workGroup[1] = 2048;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 64u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 208;
    workGroup[1] = 2;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 16u);
    EXPECT_EQ(workGroupSize[1], 2u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 6;
    workGroup[1] = 128;
    wsInfo.simdSize = 8;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 2u);
    EXPECT_EQ(workGroupSize[1], 4u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 3;
    workGroup[1] = 128;
    wsInfo.simdSize = 8;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 1u);
    EXPECT_EQ(workGroupSize[1], 64u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, basemarkKernelGenerateHistogram) {
    WorkSizeInfo wsInfo(256u, 1u, 32, 4096u, IGFX_GEN9_CORE, 56u, 65536u, true, true);
    uint32_t workDim = 2;
    size_t workGroup[3] = {1, 1, 1};
    size_t workGroupSize[3];

    workGroup[0] = 2048;
    workGroup[1] = 2048;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 16u);
    EXPECT_EQ(workGroupSize[1], 16u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 1920;
    workGroup[1] = 1080;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 32u);
    EXPECT_EQ(workGroupSize[1], 8u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, useStrictRatio) {
    WorkSizeInfo wsInfo(256u, 0u, 32u, 0u, IGFX_GEN9_CORE, 0u, 0u, true, true);
    uint32_t workDim = 2;
    size_t workGroup[3] = {194, 234, 1};
    size_t workGroupSize[3];

    workGroup[0] = 194;
    workGroup[1] = 234;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 2u);
    EXPECT_EQ(workGroupSize[1], 117u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 100;
    workGroup[1] = 100;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 20u);
    EXPECT_EQ(workGroupSize[1], 5u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 54;
    workGroup[1] = 154;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 27u);
    EXPECT_EQ(workGroupSize[1], 7u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, useBarriers) {
    WorkSizeInfo wsInfo(256u, 1u, 32u, 0u, IGFX_GEN9_CORE, 56u, 0u, true, true);
    uint32_t workDim = 2;
    size_t workGroup[3] = {194, 234, 1};
    size_t workGroupSize[3];

    workGroup[0] = 194;
    workGroup[1] = 234;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 97u);
    EXPECT_EQ(workGroupSize[1], 2u);
    EXPECT_EQ(workGroupSize[2], 1u);

    wsInfo.yTiledSurfaces = false;
    wsInfo.imgUsed = false;
    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 2u);
    EXPECT_EQ(workGroupSize[1], 78u);
    EXPECT_EQ(workGroupSize[2], 1u);

    OCLRT::computeWorkgroupSizeND(wsInfo, workGroupSize, workGroup, workDim);
    EXPECT_EQ(workGroupSize[0], 2u);
    EXPECT_EQ(workGroupSize[1], 78u);
    EXPECT_EQ(workGroupSize[2], 1u);

    wsInfo.coreFamily = IGFX_GEN8_CORE;
    wsInfo.setMinWorkGroupSize();
}

TEST(localWorkSizeTest, given2DimWorkWhenComputeSquaredCalledThenLocalGroupComputed) {
    WorkSizeInfo wsInfo(256, 0u, 16, 0u, IGFX_GEN9_CORE, 6u, 0u, false, false);
    uint32_t workDim = 2;
    size_t workGroup[3] = {2048, 272, 1};
    size_t workGroupSize[3];

    OCLRT::computeWorkgroupSizeSquared(wsInfo.maxWorkGroupSize, workGroupSize, workGroup, wsInfo.simdSize, workDim);
    EXPECT_EQ(workGroupSize[0], 16u);
    EXPECT_EQ(workGroupSize[1], 16u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 1024;
    workGroup[1] = 1024;
    OCLRT::computeWorkgroupSizeSquared(wsInfo.maxWorkGroupSize, workGroupSize, workGroup, wsInfo.simdSize, workDim);
    EXPECT_EQ(workGroupSize[0], 16u);
    EXPECT_EQ(workGroupSize[1], 16u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 512;
    workGroup[1] = 104;
    OCLRT::computeWorkgroupSizeSquared(wsInfo.maxWorkGroupSize, workGroupSize, workGroup, wsInfo.simdSize, workDim);
    EXPECT_EQ(workGroupSize[0], 32u);
    EXPECT_EQ(workGroupSize[1], 8u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 104;
    workGroup[1] = 512;
    OCLRT::computeWorkgroupSizeSquared(wsInfo.maxWorkGroupSize, workGroupSize, workGroup, wsInfo.simdSize, workDim);
    EXPECT_EQ(workGroupSize[0], 8u);
    EXPECT_EQ(workGroupSize[1], 32u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 184;
    workGroup[1] = 368;
    OCLRT::computeWorkgroupSizeSquared(wsInfo.maxWorkGroupSize, workGroupSize, workGroup, wsInfo.simdSize, workDim);
    EXPECT_EQ(workGroupSize[0], 8u);
    EXPECT_EQ(workGroupSize[1], 16u);
    EXPECT_EQ(workGroupSize[2], 1u);

    workGroup[0] = 113;
    workGroup[1] = 2;
    OCLRT::computeWorkgroupSizeSquared(wsInfo.maxWorkGroupSize, workGroupSize, workGroup, wsInfo.simdSize, workDim);
    EXPECT_EQ(workGroupSize[0], 113u);
    EXPECT_EQ(workGroupSize[1], 2u);
    EXPECT_EQ(workGroupSize[2], 1u);
}

TEST(localWorkSizeTest, givenDebugVariableEnableComputeWorkSizeNDWhenCheckValueExpectTrue) {
    bool isEnabled = DebugManager.flags.EnableComputeWorkSizeND.get();
    EXPECT_TRUE(isEnabled == false);
}
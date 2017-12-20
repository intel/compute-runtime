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

#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/program/block_kernel_manager.h"

#include "unit_tests/mocks/mock_block_kernel_manager.h"

#include "gtest/gtest.h"

using namespace OCLRT;

TEST(BlockKernelManagerTest, pushPrivateSurfaceResizesArray) {
    GraphicsAllocation allocation(0, 0);
    KernelInfo *blockInfo = new KernelInfo;
    MockBlockKernelManager blockManager;

    blockManager.addBlockKernelInfo(blockInfo);

    EXPECT_EQ(0u, blockManager.blockPrivateSurfaceArray.size());

    blockManager.pushPrivateSurface(&allocation, 0);

    EXPECT_EQ(1u, blockManager.blockPrivateSurfaceArray.size());
}

TEST(BlockKernelManagerTest, pushPrivateSurfacePlacesAllocationInCorrectPosition) {
    GraphicsAllocation allocation1(0, 0);
    GraphicsAllocation allocation2(0, 0);
    KernelInfo *blockInfo = new KernelInfo;
    KernelInfo *blockInfo2 = new KernelInfo;
    MockBlockKernelManager blockManager;

    blockManager.addBlockKernelInfo(blockInfo);
    blockManager.addBlockKernelInfo(blockInfo2);

    blockManager.pushPrivateSurface(&allocation1, 0);
    blockManager.pushPrivateSurface(&allocation2, 1);

    EXPECT_EQ(2u, blockManager.blockPrivateSurfaceArray.size());

    EXPECT_EQ(&allocation1, blockManager.blockPrivateSurfaceArray[0]);
    EXPECT_EQ(&allocation2, blockManager.blockPrivateSurfaceArray[1]);
}

TEST(BlockKernelManagerTest, pushPrivateSurfaceSetsPrivateSurfaceArrayToNullptrOnFirstCall) {
    GraphicsAllocation allocation(0, 0);
    KernelInfo *blockInfo = new KernelInfo;
    KernelInfo *blockInfo2 = new KernelInfo;
    KernelInfo *blockInfo3 = new KernelInfo;
    MockBlockKernelManager blockManager;

    blockManager.addBlockKernelInfo(blockInfo);
    blockManager.addBlockKernelInfo(blockInfo2);
    blockManager.addBlockKernelInfo(blockInfo3);

    blockManager.pushPrivateSurface(&allocation, 1);

    EXPECT_EQ(3u, blockManager.blockPrivateSurfaceArray.size());

    EXPECT_EQ(nullptr, blockManager.blockPrivateSurfaceArray[0]);
    EXPECT_EQ(nullptr, blockManager.blockPrivateSurfaceArray[2]);
}

TEST(BlockKernelManagerTest, getPrivateSurface) {
    GraphicsAllocation allocation(0, 0);
    KernelInfo *blockInfo = new KernelInfo;

    MockBlockKernelManager blockManager;

    blockManager.addBlockKernelInfo(blockInfo);

    blockManager.pushPrivateSurface(&allocation, 0);

    EXPECT_EQ(1u, blockManager.blockPrivateSurfaceArray.size());

    EXPECT_EQ(&allocation, blockManager.getPrivateSurface(0));
}

TEST(BlockKernelManagerTest, getPrivateSurfaceWithOutOfBoundOrdinalRetunrsNullptr) {
    MockBlockKernelManager blockManager;
    EXPECT_EQ(nullptr, blockManager.getPrivateSurface(0));
    EXPECT_EQ(nullptr, blockManager.getPrivateSurface(10));
}
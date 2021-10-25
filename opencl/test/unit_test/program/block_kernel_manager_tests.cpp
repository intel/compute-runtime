/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/program/kernel_info.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "opencl/test/unit_test/mocks/mock_block_kernel_manager.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(BlockKernelManagerTest, WhenPushingPrivateSurfaceThenPrivateSurfaceArrayIsResized) {
    MockGraphicsAllocation allocation(0, 0);
    KernelInfo *blockInfo = new KernelInfo;
    MockBlockKernelManager blockManager;

    blockManager.addBlockKernelInfo(blockInfo);

    EXPECT_EQ(0u, blockManager.blockPrivateSurfaceArray.size());

    blockManager.pushPrivateSurface(&allocation, 0);

    EXPECT_EQ(1u, blockManager.blockPrivateSurfaceArray.size());
}

TEST(BlockKernelManagerTest, WhenPushingPrivateSurfaceThenAllocationsArePlacedInCorrectPosition) {
    MockGraphicsAllocation allocation1(0, 0);
    MockGraphicsAllocation allocation2(0, 0);
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

TEST(BlockKernelManagerTest, WhenPushingPrivateSurfaceThenPrivateSurfaceArrayIsSetToNullptrOnFirstCall) {
    MockGraphicsAllocation allocation(0, 0);
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

TEST(BlockKernelManagerTest, WhenGettingPrivateSurfaceThenValidAllocationIsReturned) {
    MockGraphicsAllocation allocation(0, 0);
    KernelInfo *blockInfo = new KernelInfo;

    MockBlockKernelManager blockManager;

    blockManager.addBlockKernelInfo(blockInfo);

    blockManager.pushPrivateSurface(&allocation, 0);

    EXPECT_EQ(1u, blockManager.blockPrivateSurfaceArray.size());

    EXPECT_EQ(&allocation, blockManager.getPrivateSurface(0));
}

TEST(BlockKernelManagerTest, GivenOutOfBoundIndexWhenGettingPrivateSurfaceThenNullptrIsReturned) {
    MockBlockKernelManager blockManager;
    EXPECT_EQ(nullptr, blockManager.getPrivateSurface(0));
    EXPECT_EQ(nullptr, blockManager.getPrivateSurface(10));
}

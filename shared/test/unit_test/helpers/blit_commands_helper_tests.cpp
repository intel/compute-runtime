/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/blit_commands_helper.h"

#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(BlitCommandsHelperTest, GivenBufferParamsWhenConstructingPropertiesForBufferRegionsThenPropertiesCreatedCorrectly) {
    uint32_t src[] = {1, 2, 3, 4};
    uint32_t dst[] = {4, 3, 2, 1};
    uint64_t srcGpuAddr = 0x12345;
    uint64_t dstGpuAddr = 0x54321;
    std::unique_ptr<MockGraphicsAllocation> srcAlloc(new MockGraphicsAllocation(src, srcGpuAddr, sizeof(src)));
    std::unique_ptr<MockGraphicsAllocation> dstAlloc(new MockGraphicsAllocation(dst, dstGpuAddr, sizeof(dst)));
    Vec3<size_t> srcOffsets{1, 2, 3};
    Vec3<size_t> dstOffsets{3, 2, 1};
    Vec3<size_t> copySize{2, 2, 2};

    size_t srcRowPitch = 2;
    size_t srcSlicePitch = 3;

    size_t dstRowPitch = 2;
    size_t dstSlicePitch = 3;

    auto blitProperties = NEO::BlitProperties::constructPropertiesForCopyBuffer(dstAlloc.get(), srcAlloc.get(),
                                                                                dstOffsets, srcOffsets, copySize, srcRowPitch, srcSlicePitch,
                                                                                dstRowPitch, dstSlicePitch);

    EXPECT_EQ(blitProperties.blitDirection, BlitterConstants::BlitDirection::BufferToBuffer);
    EXPECT_EQ(blitProperties.dstAllocation, dstAlloc.get());
    EXPECT_EQ(blitProperties.srcAllocation, srcAlloc.get());
    EXPECT_EQ(blitProperties.dstGpuAddress, dstGpuAddr);
    EXPECT_EQ(blitProperties.srcGpuAddress, srcGpuAddr);
    EXPECT_EQ(blitProperties.copySize, copySize);
    EXPECT_EQ(blitProperties.dstOffset, dstOffsets);
    EXPECT_EQ(blitProperties.srcOffset, srcOffsets);
    EXPECT_EQ(blitProperties.dstRowPitch, dstRowPitch);
    EXPECT_EQ(blitProperties.dstSlicePitch, dstSlicePitch);
    EXPECT_EQ(blitProperties.srcRowPitch, srcRowPitch);
    EXPECT_EQ(blitProperties.srcSlicePitch, srcSlicePitch);
}

TEST(BlitCommandsHelperTest, GivenCopySizeYAndZEqual0WhenConstructingPropertiesForBufferRegionsThenCopyZAndZEqual1) {
    uint32_t src[] = {1, 2, 3, 4};
    uint32_t dst[] = {4, 3, 2, 1};
    uint64_t srcGpuAddr = 0x12345;
    uint64_t dstGpuAddr = 0x54321;
    std::unique_ptr<MockGraphicsAllocation> srcAlloc(new MockGraphicsAllocation(src, srcGpuAddr, sizeof(src)));
    std::unique_ptr<MockGraphicsAllocation> dstAlloc(new MockGraphicsAllocation(dst, dstGpuAddr, sizeof(dst)));
    Vec3<size_t> srcOffsets{1, 2, 3};
    Vec3<size_t> dstOffsets{3, 2, 1};
    Vec3<size_t> copySize{2, 0, 0};

    size_t srcRowPitch = 2;
    size_t srcSlicePitch = 3;

    size_t dstRowPitch = 2;
    size_t dstSlicePitch = 3;

    auto blitProperties = NEO::BlitProperties::constructPropertiesForCopyBuffer(dstAlloc.get(), srcAlloc.get(),
                                                                                dstOffsets, srcOffsets, copySize, srcRowPitch, srcSlicePitch,
                                                                                dstRowPitch, dstSlicePitch);
    Vec3<size_t> expectedSize{copySize.x, 1, 1};
    EXPECT_EQ(blitProperties.copySize, expectedSize);
}
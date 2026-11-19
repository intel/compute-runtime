/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/program/kernel_info.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(KernelInfoTests, givenKernelWithSurfaceStateHeapInBinaryWhenGettingSurfaceStateHeapSizeThenBinaryHeapSizeIsReturned) {
    KernelInfo kernelInfo;
    kernelInfo.heapInfo.surfaceStateHeapSize = 0x88;
    kernelInfo.kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::BindlessAndStateless;
    kernelInfo.kernelDescriptor.kernelAttributes.imageAddressingMode = KernelDescriptor::Bindless;
    kernelInfo.kernelDescriptor.kernelAttributes.numArgsStateful = 4;
    kernelInfo.kernelDescriptor.kernelAttributes.numBindlessImages = 2;

    EXPECT_EQ(0x88u, kernelInfo.getSurfaceStateHeapSize(64u));
}

TEST(KernelInfoTests, givenBindlessKernelWithBindlessImagesWhenGettingSurfaceStateHeapSizeThenStatefulArgsAndBindlessImagesAreCounted) {
    KernelInfo kernelInfo;
    kernelInfo.kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::BindlessAndStateless;
    kernelInfo.kernelDescriptor.kernelAttributes.imageAddressingMode = KernelDescriptor::Bindless;
    kernelInfo.kernelDescriptor.kernelAttributes.numArgsStateful = 2;
    kernelInfo.kernelDescriptor.kernelAttributes.numBindlessImages = 1;

    EXPECT_EQ(3u * 64u, kernelInfo.getSurfaceStateHeapSize(64u));
    EXPECT_EQ(3u * 32u, kernelInfo.getSurfaceStateHeapSize(32u));
}

TEST(KernelInfoTests, givenBindfulKernelWithoutSurfaceStateHeapInBinaryWhenGettingSurfaceStateHeapSizeThenZeroIsReturned) {
    KernelInfo kernelInfo;
    kernelInfo.kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::BindfulAndStateless;
    kernelInfo.kernelDescriptor.kernelAttributes.imageAddressingMode = KernelDescriptor::Bindful;
    kernelInfo.kernelDescriptor.kernelAttributes.numArgsStateful = 2;
    kernelInfo.kernelDescriptor.kernelAttributes.numBindlessImages = 1;

    EXPECT_EQ(0u, kernelInfo.getSurfaceStateHeapSize(64u));
}

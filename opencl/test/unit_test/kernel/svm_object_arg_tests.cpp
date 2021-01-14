/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/multi_graphics_allocation.h"

#include "opencl/source/kernel/svm_object_arg.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(SvmObjectArgTest, givenSingleGraphicsAllocationWhenCreatingSvmObjectArgThenProperPropertiesAreStored) {
    GraphicsAllocation graphicsAllocation{0u, GraphicsAllocation::AllocationType::BUFFER, nullptr, 0u, 0, MemoryPool::MemoryNull, 1u};

    SvmObjectArg svmObjectArg(&graphicsAllocation);

    EXPECT_EQ(&graphicsAllocation, svmObjectArg.getGraphicsAllocation(0u));
    EXPECT_EQ(graphicsAllocation.isCoherent(), svmObjectArg.isCoherent());
    EXPECT_EQ(graphicsAllocation.getAllocationType(), svmObjectArg.getAllocationType());
    EXPECT_EQ(nullptr, svmObjectArg.getMultiDeviceSvmAlloc());
}
TEST(SvmObjectArgTest, givenMultiGraphicsAllocationWhenCreatingSvmObjectArgThenProperPropertiesAreStored) {
    GraphicsAllocation graphicsAllocation0{0u, GraphicsAllocation::AllocationType::BUFFER, nullptr, 0u, 0, MemoryPool::MemoryNull, 1u};
    GraphicsAllocation graphicsAllocation1{1u, GraphicsAllocation::AllocationType::BUFFER, nullptr, 0u, 0, MemoryPool::MemoryNull, 1u};

    MultiGraphicsAllocation multiGraphicsAllocation(1);
    multiGraphicsAllocation.addAllocation(&graphicsAllocation0);
    multiGraphicsAllocation.addAllocation(&graphicsAllocation1);

    SvmObjectArg svmObjectArg(&multiGraphicsAllocation);

    EXPECT_EQ(&graphicsAllocation0, svmObjectArg.getGraphicsAllocation(0u));
    EXPECT_EQ(&graphicsAllocation1, svmObjectArg.getGraphicsAllocation(1u));
    EXPECT_EQ(multiGraphicsAllocation.isCoherent(), svmObjectArg.isCoherent());
    EXPECT_EQ(multiGraphicsAllocation.getAllocationType(), svmObjectArg.getAllocationType());
    EXPECT_EQ(&multiGraphicsAllocation, svmObjectArg.getMultiDeviceSvmAlloc());
}

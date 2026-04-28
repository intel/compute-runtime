/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/app_resource_classification.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

TEST(AppResourceClassificationTest, givenApplicationAllocationTypesWhenCheckingIsApplicationResourceThenReturnTrue) {
    EXPECT_TRUE(AppResourceClassification::isApplicationResource(AllocationType::buffer));
    EXPECT_TRUE(AppResourceClassification::isApplicationResource(AllocationType::bufferHostMemory));
    EXPECT_TRUE(AppResourceClassification::isApplicationResource(AllocationType::image));
    EXPECT_TRUE(AppResourceClassification::isApplicationResource(AllocationType::sharedBuffer));
    EXPECT_TRUE(AppResourceClassification::isApplicationResource(AllocationType::sharedImage));
    EXPECT_TRUE(AppResourceClassification::isApplicationResource(AllocationType::sharedResourceCopy));
    EXPECT_TRUE(AppResourceClassification::isApplicationResource(AllocationType::unifiedSharedMemory));
    EXPECT_TRUE(AppResourceClassification::isApplicationResource(AllocationType::svmCpu));
    EXPECT_TRUE(AppResourceClassification::isApplicationResource(AllocationType::svmGpu));
    EXPECT_TRUE(AppResourceClassification::isApplicationResource(AllocationType::svmZeroCopy));
    EXPECT_TRUE(AppResourceClassification::isApplicationResource(AllocationType::mapAllocation));
    EXPECT_TRUE(AppResourceClassification::isApplicationResource(AllocationType::externalHostPtr));
    EXPECT_TRUE(AppResourceClassification::isApplicationResource(AllocationType::writeCombined));
    EXPECT_TRUE(AppResourceClassification::isApplicationResource(AllocationType::fillPattern));
    EXPECT_TRUE(AppResourceClassification::isApplicationResource(AllocationType::globalSurface));
    EXPECT_TRUE(AppResourceClassification::isApplicationResource(AllocationType::constantSurface));
    EXPECT_TRUE(AppResourceClassification::isApplicationResource(AllocationType::kernelIsa));
    EXPECT_TRUE(AppResourceClassification::isApplicationResource(AllocationType::printfSurface));
    EXPECT_TRUE(AppResourceClassification::isApplicationResource(AllocationType::assertBuffer));
    EXPECT_TRUE(AppResourceClassification::isApplicationResource(AllocationType::privateSurface));
}

TEST(AppResourceClassificationTest, givenInternalAllocationTypesWhenCheckingIsApplicationResourceThenReturnFalse) {
    EXPECT_FALSE(AppResourceClassification::isApplicationResource(AllocationType::unknown));
    EXPECT_FALSE(AppResourceClassification::isApplicationResource(AllocationType::commandBuffer));
    EXPECT_FALSE(AppResourceClassification::isApplicationResource(AllocationType::indirectObjectHeap));
    EXPECT_FALSE(AppResourceClassification::isApplicationResource(AllocationType::instructionHeap));
    EXPECT_FALSE(AppResourceClassification::isApplicationResource(AllocationType::internalHeap));
    EXPECT_FALSE(AppResourceClassification::isApplicationResource(AllocationType::internalHostMemory));
    EXPECT_FALSE(AppResourceClassification::isApplicationResource(AllocationType::kernelArgsBuffer));
    EXPECT_FALSE(AppResourceClassification::isApplicationResource(AllocationType::kernelIsaInternal));
    EXPECT_FALSE(AppResourceClassification::isApplicationResource(AllocationType::linearStream));
    EXPECT_FALSE(AppResourceClassification::isApplicationResource(AllocationType::mcs));
    EXPECT_FALSE(AppResourceClassification::isApplicationResource(AllocationType::preemption));
    EXPECT_FALSE(AppResourceClassification::isApplicationResource(AllocationType::profilingTagBuffer));
    EXPECT_FALSE(AppResourceClassification::isApplicationResource(AllocationType::scratchSurface));
    EXPECT_FALSE(AppResourceClassification::isApplicationResource(AllocationType::surfaceStateHeap));
    EXPECT_FALSE(AppResourceClassification::isApplicationResource(AllocationType::syncBuffer));
    EXPECT_FALSE(AppResourceClassification::isApplicationResource(AllocationType::tagBuffer));
    EXPECT_FALSE(AppResourceClassification::isApplicationResource(AllocationType::globalFence));
    EXPECT_FALSE(AppResourceClassification::isApplicationResource(AllocationType::timestampPacketTagBuffer));
    EXPECT_FALSE(AppResourceClassification::isApplicationResource(AllocationType::ringBuffer));
    EXPECT_FALSE(AppResourceClassification::isApplicationResource(AllocationType::semaphoreBuffer));
    EXPECT_FALSE(AppResourceClassification::isApplicationResource(AllocationType::debugContextSaveArea));
    EXPECT_FALSE(AppResourceClassification::isApplicationResource(AllocationType::debugSbaTrackingBuffer));
    EXPECT_FALSE(AppResourceClassification::isApplicationResource(AllocationType::debugModuleArea));
    EXPECT_FALSE(AppResourceClassification::isApplicationResource(AllocationType::workPartitionSurface));
    EXPECT_FALSE(AppResourceClassification::isApplicationResource(AllocationType::gpuTimestampDeviceBuffer));
    EXPECT_FALSE(AppResourceClassification::isApplicationResource(AllocationType::swTagBuffer));
    EXPECT_FALSE(AppResourceClassification::isApplicationResource(AllocationType::deferredTasksList));
    EXPECT_FALSE(AppResourceClassification::isApplicationResource(AllocationType::syncDispatchToken));
    EXPECT_FALSE(AppResourceClassification::isApplicationResource(AllocationType::hostFunction));
}

TEST(AppResourceClassificationTest, givenAllAllocationTypesWhenCheckingIsApplicationResourceThenAllTypesAreCovered) {
    constexpr int expectedApplicationTypes = 20;
    constexpr int expectedDriverTypes = 29;

    int applicationTypesCount = 0;
    int driverTypesCount = 0;

    for (int i = 0; i < static_cast<int>(AllocationType::count); ++i) {
        auto type = static_cast<AllocationType>(i);
        if (AppResourceClassification::isApplicationResource(type)) {
            applicationTypesCount++;
        } else {
            driverTypesCount++;
        }
    }

    EXPECT_EQ(expectedApplicationTypes, applicationTypesCount);
    EXPECT_EQ(expectedDriverTypes, driverTypesCount);
    EXPECT_EQ(static_cast<int>(AllocationType::count), applicationTypesCount + driverTypesCount);
}

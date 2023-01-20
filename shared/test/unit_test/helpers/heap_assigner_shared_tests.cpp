/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/heap_assigner.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

TEST(HeapAssigner, givenInternalHeapIndexWhenMappingToInternalFrontWindowThenInternalFrontWindowReturned) {
    EXPECT_EQ(HeapIndex::HEAP_INTERNAL_FRONT_WINDOW, HeapAssigner::mapInternalWindowIndex(HeapIndex::HEAP_INTERNAL));
}

TEST(HeapAssigner, givenInternalDeviceHeapIndexWhenMappingToInternalFrontWindowThenInternalDeviceFrontWindowReturned) {
    EXPECT_EQ(HeapIndex::HEAP_INTERNAL_DEVICE_FRONT_WINDOW, HeapAssigner::mapInternalWindowIndex(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY));
}

TEST(HeapAssigner, givenOtherThanInternalHeapIndexWhenMappingToInternalFrontWindowThenAbortIsThrown) {
    EXPECT_THROW(HeapAssigner::mapInternalWindowIndex(HeapIndex::HEAP_STANDARD), std::exception);
}

TEST(HeapAssigner, givenInternalHeapIndexWhenCheckingIsInternalHeapThenTrueIsReturned) {
    EXPECT_TRUE(HeapAssigner::isInternalHeap(HeapIndex::HEAP_INTERNAL));
    EXPECT_TRUE(HeapAssigner::isInternalHeap(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY));
}

TEST(HeapAssigner, givenNonInternalHeapIndexWhenCheckingIsInternalHeapThenFalseIsReturned) {
    EXPECT_FALSE(HeapAssigner::isInternalHeap(HeapIndex::HEAP_EXTERNAL));
    EXPECT_FALSE(HeapAssigner::isInternalHeap(HeapIndex::HEAP_EXTERNAL_DEVICE_MEMORY));
    EXPECT_FALSE(HeapAssigner::isInternalHeap(HeapIndex::HEAP_EXTERNAL_FRONT_WINDOW));
    EXPECT_FALSE(HeapAssigner::isInternalHeap(HeapIndex::HEAP_EXTERNAL_DEVICE_FRONT_WINDOW));
    EXPECT_FALSE(HeapAssigner::isInternalHeap(HeapIndex::HEAP_INTERNAL_FRONT_WINDOW));
    EXPECT_FALSE(HeapAssigner::isInternalHeap(HeapIndex::HEAP_INTERNAL_DEVICE_FRONT_WINDOW));
    EXPECT_FALSE(HeapAssigner::isInternalHeap(HeapIndex::HEAP_STANDARD));
    EXPECT_FALSE(HeapAssigner::isInternalHeap(HeapIndex::HEAP_STANDARD64KB));
    EXPECT_FALSE(HeapAssigner::isInternalHeap(HeapIndex::HEAP_SVM));
    EXPECT_FALSE(HeapAssigner::isInternalHeap(HeapIndex::HEAP_EXTENDED));
}
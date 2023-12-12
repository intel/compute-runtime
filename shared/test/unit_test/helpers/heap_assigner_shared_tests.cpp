/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/heap_assigner.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

TEST(HeapAssigner, givenInternalHeapIndexWhenMappingToInternalFrontWindowThenInternalFrontWindowReturned) {
    EXPECT_EQ(HeapIndex::heapInternalFrontWindow, HeapAssigner::mapInternalWindowIndex(HeapIndex::heapInternal));
}

TEST(HeapAssigner, givenInternalDeviceHeapIndexWhenMappingToInternalFrontWindowThenInternalDeviceFrontWindowReturned) {
    EXPECT_EQ(HeapIndex::heapInternalDeviceFrontWindow, HeapAssigner::mapInternalWindowIndex(HeapIndex::heapInternalDeviceMemory));
}

TEST(HeapAssigner, givenOtherThanInternalHeapIndexWhenMappingToInternalFrontWindowThenAbortIsThrown) {
    EXPECT_THROW(HeapAssigner::mapInternalWindowIndex(HeapIndex::heapStandard), std::exception);
}

TEST(HeapAssigner, givenInternalHeapIndexWhenCheckingIsInternalHeapThenTrueIsReturned) {
    EXPECT_TRUE(HeapAssigner::isInternalHeap(HeapIndex::heapInternal));
    EXPECT_TRUE(HeapAssigner::isInternalHeap(HeapIndex::heapInternalDeviceMemory));
}

TEST(HeapAssigner, givenNonInternalHeapIndexWhenCheckingIsInternalHeapThenFalseIsReturned) {
    EXPECT_FALSE(HeapAssigner::isInternalHeap(HeapIndex::heapExternal));
    EXPECT_FALSE(HeapAssigner::isInternalHeap(HeapIndex::heapExternalDeviceMemory));
    EXPECT_FALSE(HeapAssigner::isInternalHeap(HeapIndex::heapExternalFrontWindow));
    EXPECT_FALSE(HeapAssigner::isInternalHeap(HeapIndex::heapExternalDeviceFrontWindow));
    EXPECT_FALSE(HeapAssigner::isInternalHeap(HeapIndex::heapInternalFrontWindow));
    EXPECT_FALSE(HeapAssigner::isInternalHeap(HeapIndex::heapInternalDeviceFrontWindow));
    EXPECT_FALSE(HeapAssigner::isInternalHeap(HeapIndex::heapStandard));
    EXPECT_FALSE(HeapAssigner::isInternalHeap(HeapIndex::heapStandard64KB));
    EXPECT_FALSE(HeapAssigner::isInternalHeap(HeapIndex::heapSvm));
    EXPECT_FALSE(HeapAssigner::isInternalHeap(HeapIndex::heapExtended));
}
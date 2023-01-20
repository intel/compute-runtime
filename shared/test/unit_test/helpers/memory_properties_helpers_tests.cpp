/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/memory_properties_helpers.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(MemoryPropertiesHelperTests, whenQueryingUseSystemMemoryForCrossRootDeviceAccessThenReturnTrueForMultiRootDeviceContexts) {
    for (auto multiRootDevice : {false, true}) {
        EXPECT_EQ(multiRootDevice, MemoryPropertiesHelper::useSystemMemoryForCrossRootDeviceAccess(multiRootDevice));
    }
}

TEST(MemoryPropertiesHelperTests, givenAllocateBuffersInLocalMemoryForMultiRootDeviceContextsWhenQueryingUseSystemMemoryForCrossRootDeviceAccessThenReturnFalseForMultiRootDeviceContexts) {
    DebugManagerStateRestore restore;

    for (auto localMemory : {false, true}) {
        DebugManager.flags.AllocateBuffersInLocalMemoryForMultiRootDeviceContexts.set(localMemory);
        EXPECT_FALSE(MemoryPropertiesHelper::useSystemMemoryForCrossRootDeviceAccess(false));
    }

    for (auto localMemory : {false, true}) {
        DebugManager.flags.AllocateBuffersInLocalMemoryForMultiRootDeviceContexts.set(localMemory);
        EXPECT_NE(localMemory, MemoryPropertiesHelper::useSystemMemoryForCrossRootDeviceAccess(true));
    }
}

TEST(MemoryPropertiesHelperTests, whenQueryingUseMultiStorageForCrossRootDeviceAccessThenReturnFalseForMultiRootDeviceContexts) {
    for (auto multiRootDevice : {false, true}) {
        EXPECT_FALSE(MemoryPropertiesHelper::useMultiStorageForCrossRootDeviceAccess(multiRootDevice));
    }
}

TEST(MemoryPropertiesHelperTests, givenAllocateBuffersInLocalMemoryForMultiRootDeviceContextsWhenQueryingUseMultiStorageForCrossRootDeviceAccessThenReturnTrueForMultiRootDeviceContexts) {
    DebugManagerStateRestore restore;

    for (auto localMemory : {false, true}) {
        DebugManager.flags.AllocateBuffersInLocalMemoryForMultiRootDeviceContexts.set(localMemory);
        EXPECT_FALSE(MemoryPropertiesHelper::useMultiStorageForCrossRootDeviceAccess(false));
    }

    for (auto localMemory : {false, true}) {
        DebugManager.flags.AllocateBuffersInLocalMemoryForMultiRootDeviceContexts.set(localMemory);
        EXPECT_EQ(localMemory, MemoryPropertiesHelper::useMultiStorageForCrossRootDeviceAccess(true));
    }
}

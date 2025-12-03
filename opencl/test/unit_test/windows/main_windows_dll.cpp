/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/indirect_heap/heap_size.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

#include <typeinfo>

using namespace NEO;

TEST(wddmCreateTests, givenInputVersionWhenCreatingThenCreateRequestedObject) {
    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment rootDeviceEnvironment(executionEnvironment);
    auto hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    std::unique_ptr<Wddm> wddm(Wddm::createWddm(std::unique_ptr<HwDeviceIdWddm>(hwDeviceIds[0].release()->as<HwDeviceIdWddm>()), rootDeviceEnvironment));
    EXPECT_NE(nullptr, wddm);
}

TEST(DefaultHeapSizeTest, whenGetDefaultHeapSizeThenReturnCorrectValue) {
    EXPECT_EQ(4 * MemoryConstants::megaByte, NEO::HeapSize::getDefaultHeapSize(IndirectHeapType::indirectObject));
    EXPECT_EQ(MemoryConstants::pageSize64k, NEO::HeapSize::getDefaultHeapSize(IndirectHeapType::surfaceState));
    EXPECT_EQ(MemoryConstants::pageSize64k, NEO::HeapSize::getDefaultHeapSize(IndirectHeapType::dynamicState));
}

TEST(SmallBuffersParamsTest, WhenGettingDefaultParamsThenReturnCorrectValues) {
    auto defaultParams = NEO::SmallBuffersParams::getDefaultParams();

    EXPECT_EQ(2 * MemoryConstants::megaByte, defaultParams.aggregatedSmallBuffersPoolSize);
    EXPECT_EQ(1 * MemoryConstants::megaByte, defaultParams.smallBufferThreshold);
    EXPECT_EQ(MemoryConstants::pageSize64k, defaultParams.chunkAlignment);
    EXPECT_EQ(MemoryConstants::pageSize64k, defaultParams.startingOffset);
}

TEST(SmallBuffersParamsTest, WhenGettingLargePagesParamsThenReturnCorrectValues) {
    auto largePagesParams = NEO::SmallBuffersParams::getLargePagesParams();

    EXPECT_EQ(16 * MemoryConstants::megaByte, largePagesParams.aggregatedSmallBuffersPoolSize);
    EXPECT_EQ(2 * MemoryConstants::megaByte, largePagesParams.smallBufferThreshold);
    EXPECT_EQ(MemoryConstants::pageSize64k, largePagesParams.chunkAlignment);
    EXPECT_EQ(MemoryConstants::pageSize64k, largePagesParams.startingOffset);
}
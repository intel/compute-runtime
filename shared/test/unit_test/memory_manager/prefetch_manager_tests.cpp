/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/prefetch_manager.h"
#include "shared/test/common/memory_manager/mock_prefetch_manager.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(PrefetchManagerTests, givenPrefetchManagerWhenCallingInterfaceFunctionsThenUpdateAllocationsInPrefetchContext) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    auto device = deviceFactory->rootDevices[0];
    auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager(), false);
    auto prefetchManager = std::make_unique<MockPrefetchManager>();
    PrefetchContext prefetchContext;

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::SHARED_UNIFIED_MEMORY, rootDeviceIndices, deviceBitfields);
    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(4096u, unifiedMemoryProperties, nullptr);
    ASSERT_NE(nullptr, ptr);

    auto svmData = svmManager->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, svmData);

    EXPECT_EQ(0u, prefetchContext.allocations.size());

    prefetchManager->insertAllocation(prefetchContext, *svmData);
    EXPECT_EQ(1u, prefetchContext.allocations.size());

    prefetchManager->migrateAllocationsToGpu(prefetchContext, *svmManager.get(), *device, *csr.get());
    EXPECT_EQ(1u, prefetchContext.allocations.size());

    prefetchManager->removeAllocations(prefetchContext);
    EXPECT_EQ(0u, prefetchContext.allocations.size());

    svmManager->freeSVMAlloc(ptr);
}

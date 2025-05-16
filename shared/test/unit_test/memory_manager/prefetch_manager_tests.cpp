/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/prefetch_manager.h"
#include "shared/source/unified_memory/usm_memory_support.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
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
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    auto prefetchManager = std::make_unique<MockPrefetchManager>();
    PrefetchContext prefetchContext;

    DebugManagerStateRestore restore;

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(4096u, unifiedMemoryProperties, nullptr);
    ASSERT_NE(nullptr, ptr);

    auto &hwInfo = *device->getRootDeviceEnvironment().getMutableHardwareInfo();

    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};
    sharedSystemMemCapabilities = (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess);

    auto svmData = svmManager->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, svmData);

    auto ptr2 = malloc(1024);

    debugManager.flags.EnableSharedSystemUsmSupport.set(1);

    EXPECT_EQ(0u, prefetchContext.allocations.size());

    prefetchManager->insertAllocation(prefetchContext, *svmManager.get(), *device, ptr, 4096);
    EXPECT_EQ(1u, prefetchContext.allocations.size());

    debugManager.flags.EnableRecoverablePageFaults.set(0);
    prefetchManager->insertAllocation(prefetchContext, *svmManager.get(), *device, ptr2, 1024);
    EXPECT_EQ(1u, prefetchContext.allocations.size());

    debugManager.flags.EnableRecoverablePageFaults.set(1);
    prefetchManager->insertAllocation(prefetchContext, *svmManager.get(), *device, ptr2, 1024);
    EXPECT_EQ(2u, prefetchContext.allocations.size());

    prefetchManager->migrateAllocationsToGpu(prefetchContext, *svmManager.get(), *device, *csr.get());
    EXPECT_EQ(2u, prefetchContext.allocations.size());

    prefetchManager->removeAllocations(prefetchContext);
    EXPECT_EQ(0u, prefetchContext.allocations.size());

    svmManager->freeSVMAlloc(ptr);
    free(ptr2);
}

TEST(PrefetchManagerTests, givenPrefetchManagerWhenCallingInterfaceFunctionsThenNoUpdateAllocationsInPrefetchContextForInvalidAllocations) {
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    auto device = deviceFactory->rootDevices[0];
    auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    auto prefetchManager = std::make_unique<MockPrefetchManager>();
    PrefetchContext prefetchContext;

    DebugManagerStateRestore restore;

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(4096u, unifiedMemoryProperties, nullptr);
    ASSERT_NE(nullptr, ptr);

    auto svmData = svmManager->getSVMAlloc(ptr);
    svmData->memoryType = InternalMemoryType::deviceUnifiedMemory;

    ASSERT_NE(nullptr, svmData);

    EXPECT_EQ(0u, prefetchContext.allocations.size());

    prefetchManager->insertAllocation(prefetchContext, *svmManager.get(), *device, ptr, 4096);
    EXPECT_EQ(0u, prefetchContext.allocations.size());

    prefetchManager->migrateAllocationsToGpu(prefetchContext, *svmManager.get(), *device, *csr.get());
    EXPECT_EQ(0u, prefetchContext.allocations.size());

    svmManager->freeSVMAlloc(ptr);
}

TEST(PrefetchManagerTests, givenPrefetchManagerWhenCallingMigrateAllocationsToGpuThenPrefetchMemoryForValidSVMAllocPtrs) {
    DebugManagerStateRestore restore;
    debugManager.flags.UseKmdMigration.set(1);

    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    auto device = deviceFactory->rootDevices[0];
    auto csr = std::make_unique<MockCommandStreamReceiver>(*device->getExecutionEnvironment(), device->getRootDeviceIndex(), device->getDeviceBitfield());
    auto svmManager = std::make_unique<MockSVMAllocsManager>(device->getMemoryManager());
    auto prefetchManager = std::make_unique<MockPrefetchManager>();
    PrefetchContext prefetchContext;

    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto ptr = svmManager->createSharedUnifiedMemoryAllocation(4096u, unifiedMemoryProperties, nullptr);
    ASSERT_NE(nullptr, ptr);

    auto svmData = svmManager->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, svmData);

    prefetchManager->insertAllocation(prefetchContext, *svmManager.get(), *device, ptr, 4096);

    EXPECT_FALSE(prefetchManager->migrateAllocationsToGpuCalled);
    EXPECT_FALSE(svmManager->prefetchMemoryCalled);

    prefetchManager->migrateAllocationsToGpu(prefetchContext, *svmManager.get(), *device, *csr.get());
    EXPECT_TRUE(prefetchManager->migrateAllocationsToGpuCalled);
    EXPECT_TRUE(svmManager->prefetchMemoryCalled);

    svmManager->freeSVMAlloc(ptr);
}

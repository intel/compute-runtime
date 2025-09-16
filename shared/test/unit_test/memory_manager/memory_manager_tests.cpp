/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/blit_helper.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/program/program_initialization.h"
#include "shared/source/utilities/heap_allocator.h"
#include "shared/test/common/compiler_interface/linker_mock.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/fixtures/memory_allocator_fixture.h"
#include "shared/test/common/fixtures/memory_manager_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/raii_gfx_core_helper.h"
#include "shared/test/common/mocks/mock_align_malloc_memory_manager.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_aub_center.h"
#include "shared/test/common/mocks/mock_aub_manager.h"
#include "shared/test/common/mocks/mock_aub_memory_operations_handler.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_deferrable_deletion.h"
#include "shared/test/common/mocks/mock_deferred_deleter.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gfx_partition.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_page_table_mngr.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_host_ptr_manager.h"
#include "shared/test/common/mocks/mock_internal_allocation_storage.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/mocks/mock_release_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

namespace NEO {
enum class AtomicAccessMode : uint32_t;
}

using namespace NEO;

TEST(MemoryManagerTest, WhenCallingSetSharedSystemMemAdviseThenReturnTrue) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    auto subDeviceId = SubDeviceIdsVec{0};
    EXPECT_TRUE(memoryManager.setSharedSystemMemAdvise(nullptr, 0u, MemAdvise::invalidAdvise, subDeviceId, 0u));
}

TEST(MemoryManagerTest, WhenCallingSetSharedSystemAtomicAccessThenReturnTrue) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    auto subDeviceId = SubDeviceIdsVec{0};
    EXPECT_TRUE(memoryManager.setSharedSystemAtomicAccess(nullptr, 0u, AtomicAccessMode::none, subDeviceId, 0u));
}

TEST(MemoryManagerTest, WhenCallingGetSharedSystemAtomicAccessThenReturnTrue) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    auto subDeviceId = SubDeviceIdsVec{0};
    EXPECT_EQ(AtomicAccessMode::none, memoryManager.getSharedSystemAtomicAccess(nullptr, 0u, subDeviceId, 0u));
}

TEST(MemoryManagerTest, WhenCallingHasPageFaultsEnabledThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    MockDevice device;
    EXPECT_FALSE(memoryManager.hasPageFaultsEnabled(device));
}

TEST(MemoryManagerTest, WhenCallingCloseInternalHandleWithOsAgnosticThenNoChanges) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    uint64_t handle = 0u;
    memoryManager.closeInternalHandle(handle, 0u, nullptr);
}

TEST(MemoryManagerTest, WhenCallingSetAtomicAccessWithOsAgnosticThenNoChanges) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    EXPECT_TRUE(memoryManager.setAtomicAccess(nullptr, 16, AtomicAccessMode::none, 1));
}

TEST(MemoryManagerTest, whenGettingPreferredAllocationMethodThenNotDefinedIsReturned) {
    MockMemoryManager memoryManager;
    for (auto i = 0; i < static_cast<int>(AllocationType::count); i++) {
        AllocationProperties allocationProperties{0u, 0u, static_cast<AllocationType>(i), {}};
        EXPECT_EQ(GfxMemoryAllocationMethod::notDefined, memoryManager.getPreferredAllocationMethod(allocationProperties));
    }
}

TEST(MemoryManagerTest, WhenCallingIsAllocationTypeToCaptureThenScratchAndPrivateTypesReturnTrue) {
    MockMemoryManager mockMemoryManager;

    EXPECT_TRUE(mockMemoryManager.isAllocationTypeToCapture(AllocationType::scratchSurface));
    EXPECT_TRUE(mockMemoryManager.isAllocationTypeToCapture(AllocationType::privateSurface));
    EXPECT_TRUE(mockMemoryManager.isAllocationTypeToCapture(AllocationType::linearStream));
    EXPECT_TRUE(mockMemoryManager.isAllocationTypeToCapture(AllocationType::internalHeap));
}

TEST(MemoryManagerTest, givenAllocationWithNullCpuPtrThenMemoryCopyToAllocationReturnsFalse) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    constexpr uint8_t allocationSize = 10;
    uint8_t allocationStorage[allocationSize] = {0};
    MockGraphicsAllocation allocation{allocationStorage, allocationSize};
    allocation.cpuPtr = nullptr;
    constexpr size_t offset = 0;

    EXPECT_FALSE(memoryManager.copyMemoryToAllocation(&allocation, offset, nullptr, 0));
}

TEST(MemoryManagerTest, givenDeviceGraphicsAllocationWhenMapCalledThenDontResetCpuAddress) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);

    uint8_t allocationStorage = 0;
    MockGraphicsAllocation allocation{&allocationStorage, 1};
    EXPECT_NE(nullptr, allocation.getUnderlyingBuffer());

    EXPECT_TRUE(memoryManager.mapPhysicalDeviceMemoryToVirtualMemory(&allocation, 0x12300, 0));
    EXPECT_EQ(&allocationStorage, allocation.getUnderlyingBuffer());
    EXPECT_EQ(0x12300u, allocation.getGpuAddress());

    EXPECT_TRUE(memoryManager.unMapPhysicalDeviceMemoryFromVirtualMemory(&allocation, 1, 1, nullptr, 0));
    EXPECT_EQ(&allocationStorage, allocation.getUnderlyingBuffer());
    EXPECT_EQ(0u, allocation.getGpuAddress());
}

TEST(MemoryManagerTest, givenHostGraphicsAllocationWhenMapCalledThenDontResetCpuAddress) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);

    uint8_t allocationStorage = 0;
    MockGraphicsAllocation allocation{&allocationStorage, 1};
    EXPECT_NE(nullptr, allocation.getUnderlyingBuffer());

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    rootDeviceIndices.pushUnique(1);
    MultiGraphicsAllocation multiGraphicsAllocation{static_cast<uint32_t>(rootDeviceIndices.size())};
    EXPECT_TRUE(memoryManager.mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, &allocation, 0x12300, 0));
    EXPECT_EQ(&allocationStorage, allocation.getUnderlyingBuffer());
    EXPECT_NE(0x12300u, allocation.getGpuAddress());
    for (size_t i = 0; i < rootDeviceIndices.size(); i++) {
        EXPECT_NE(multiGraphicsAllocation.getGraphicsAllocation(static_cast<uint32_t>(i)), nullptr);
    }

    EXPECT_TRUE(memoryManager.unMapPhysicalHostMemoryFromVirtualMemory(multiGraphicsAllocation, static_cast<GraphicsAllocation *>(&allocation), 0x12300, 0));
    EXPECT_EQ(&allocationStorage, allocation.getUnderlyingBuffer());
    EXPECT_NE(0x12300u, allocation.getGpuAddress());
    for (size_t i = 0; i < rootDeviceIndices.size(); i++) {
        EXPECT_EQ(multiGraphicsAllocation.getGraphicsAllocation(static_cast<uint32_t>(i)), nullptr);
    }
}

TEST(MemoryManagerTest, givenDefaultMemoryManagerWhenItIsAskedForBudgetExhaustionThenFalseIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    EXPECT_FALSE(memoryManager.isMemoryBudgetExhausted());
}

TEST(MemoryManagerTest, givenMemoryManagerWhenGettingDefaultContextThenCorrectContextForSubdeviceBitfieldIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    auto mockMemoryManager = new MockMemoryManager(false, false, executionEnvironment);
    executionEnvironment.memoryManager.reset(mockMemoryManager);
    auto csr0 = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 0, 1);
    auto csr1 = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 0, 1);
    auto csr2 = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 0, 3);

    csr0->internalAllocationStorage.reset(new MockInternalAllocationStorage(*csr0));
    csr1->internalAllocationStorage.reset(new MockInternalAllocationStorage(*csr1));
    csr2->internalAllocationStorage.reset(new MockInternalAllocationStorage(*csr2));

    auto osContext0 = executionEnvironment.memoryManager->createAndRegisterOsContext(csr0.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_RCS, EngineUsage::lowPriority}));
    auto osContext1 = executionEnvironment.memoryManager->createAndRegisterOsContext(csr1.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_RCS, EngineUsage::regular}));
    auto osContext2 = executionEnvironment.memoryManager->createAndRegisterOsContext(csr2.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_RCS, EngineUsage::regular}, DeviceBitfield(0x3)));
    osContext1->setDefaultContext(true);
    osContext2->setDefaultContext(true);

    EXPECT_NE(nullptr, osContext0);
    EXPECT_NE(nullptr, osContext1);
    EXPECT_NE(nullptr, osContext2);
    EXPECT_EQ(osContext1, executionEnvironment.memoryManager->getDefaultEngineContext(0, 1));
    EXPECT_EQ(osContext2, executionEnvironment.memoryManager->getDefaultEngineContext(0, 3));
    EXPECT_EQ(mockMemoryManager->getRegisteredEngines(0)[mockMemoryManager->defaultEngineIndex[0]].osContext,
              executionEnvironment.memoryManager->getDefaultEngineContext(0, 2));
}

TEST(MemoryManagerTest, givenMultipleDevicesMemoryManagerWhenGettingDefaultContextThenCorrectContextForDeviceAndSubdeviceBitfieldIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get(), true, 2);
    auto mockMemoryManager = new MockMemoryManager(false, false, executionEnvironment);
    executionEnvironment.memoryManager.reset(mockMemoryManager);
    auto csr0 = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 0, 1);
    auto csr1 = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 0, 1);
    auto csr2 = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 0, 3);
    auto csr3 = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 1, 1);
    auto csr4 = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 1, 3);

    csr0->internalAllocationStorage.reset(new MockInternalAllocationStorage(*csr0));
    csr1->internalAllocationStorage.reset(new MockInternalAllocationStorage(*csr1));
    csr2->internalAllocationStorage.reset(new MockInternalAllocationStorage(*csr2));
    csr3->internalAllocationStorage.reset(new MockInternalAllocationStorage(*csr3));
    csr4->internalAllocationStorage.reset(new MockInternalAllocationStorage(*csr4));

    auto osContext0 = executionEnvironment.memoryManager->createAndRegisterOsContext(csr0.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_RCS, EngineUsage::lowPriority}));
    auto osContext1 = executionEnvironment.memoryManager->createAndRegisterOsContext(csr1.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_RCS, EngineUsage::regular}));
    auto osContext2 = executionEnvironment.memoryManager->createAndRegisterOsContext(csr2.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_RCS, EngineUsage::regular}, DeviceBitfield(0x3)));
    osContext1->setDefaultContext(true);
    osContext2->setDefaultContext(true);

    auto osContext3 = executionEnvironment.memoryManager->createAndRegisterOsContext(csr3.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_RCS, EngineUsage::regular}));
    auto osContext4 = executionEnvironment.memoryManager->createAndRegisterOsContext(csr4.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_RCS, EngineUsage::regular}, DeviceBitfield(0x3)));
    osContext3->setDefaultContext(true);
    osContext4->setDefaultContext(true);

    EXPECT_NE(nullptr, osContext0);
    EXPECT_NE(nullptr, osContext1);
    EXPECT_NE(nullptr, osContext2);
    EXPECT_NE(nullptr, osContext3);
    EXPECT_NE(nullptr, osContext4);

    EXPECT_EQ(osContext3, executionEnvironment.memoryManager->getDefaultEngineContext(1, 1));
    EXPECT_EQ(osContext4, executionEnvironment.memoryManager->getDefaultEngineContext(1, 3));
    EXPECT_EQ(mockMemoryManager->getRegisteredEngines(1)[mockMemoryManager->defaultEngineIndex[1]].osContext,
              executionEnvironment.memoryManager->getDefaultEngineContext(1, 2));
}

TEST(MemoryManagerTest, givenFailureOnRegisterSystemMemoryAllocationWhenAllocatingMemoryThenNullptrIsReturned) {
    AllocationProperties properties(mockRootDeviceIndex, true, MemoryConstants::cacheLineSize, AllocationType::buffer, false, mockDeviceBitfield);
    MockMemoryManager memoryManager;
    memoryManager.registerSysMemAllocResult = MemoryManager::AllocationStatus::Error;
    EXPECT_EQ(nullptr, memoryManager.allocateGraphicsMemoryWithProperties(properties));
}

TEST(MemoryManagerTest, givenFailureOnRegisterLocalMemoryAllocationWhenAllocatingMemoryThenNullptrIsReturned) {
    AllocationProperties properties(mockRootDeviceIndex, true, MemoryConstants::cacheLineSize, AllocationType::buffer, false, mockDeviceBitfield);
    MockMemoryManager memoryManager(true, true);
    memoryManager.registerLocalMemAllocResult = MemoryManager::AllocationStatus::Error;
    EXPECT_EQ(nullptr, memoryManager.allocateGraphicsMemoryWithProperties(properties));
}

using MemoryhManagerMultiContextResourceTests = ::testing::Test;
HWTEST_F(MemoryhManagerMultiContextResourceTests, givenAllocationUsedByManyOsContextsWhenCheckingUsageBeforeDestroyThenMultiContextDestructorIsUsedForWaitingForAllOsContexts) {
    auto executionEnvironment = new MockExecutionEnvironment(defaultHwInfo.get(), true, 2);
    auto memoryManager = new MockMemoryManager(false, false, *executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);
    auto multiContextDestructor = new MockDeferredDeleter();

    memoryManager->multiContextResourceDestructor.reset(multiContextDestructor);

    auto device = std::unique_ptr<MockDevice>(MockDevice::create<MockDevice>(executionEnvironment, 0u));

    auto &lowPriorityEngine = device->getEngine(device->getHardwareInfo().capabilityTable.defaultEngineType, EngineUsage::lowPriority);

    auto nonDefaultOsContext = lowPriorityEngine.osContext;
    auto nonDefaultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(lowPriorityEngine.commandStreamReceiver);
    auto defaultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getDefaultEngine().commandStreamReceiver);
    auto defaultOsContext = device->getDefaultEngine().osContext;

    EXPECT_FALSE(defaultOsContext->isLowPriority());
    EXPECT_TRUE(nonDefaultOsContext->isLowPriority());

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    multiContextDestructor->expectDrainBlockingValue(false);

    nonDefaultCsr->taskCount = *nonDefaultCsr->getTagAddress();
    nonDefaultCsr->latestFlushedTaskCount = *nonDefaultCsr->getTagAddress();
    graphicsAllocation->updateTaskCount(*nonDefaultCsr->getTagAddress(), nonDefaultOsContext->getContextId());
    graphicsAllocation->updateTaskCount(0, defaultOsContext->getContextId()); // used and ready

    EXPECT_TRUE(graphicsAllocation->isUsedByManyOsContexts());

    memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(graphicsAllocation);
    EXPECT_EQ(1, multiContextDestructor->deferDeletionCalled);
    EXPECT_TRUE(nonDefaultCsr->getInternalAllocationStorage()->getTemporaryAllocations().peekIsEmpty());
    EXPECT_TRUE(defaultCsr->getInternalAllocationStorage()->getTemporaryAllocations().peekIsEmpty());
}

TEST(OsAgnosticMemoryManager, givenOsAgnosticMemoryManagerWhenGpuAddressIsReservedOnSpecifiedHeapAndFreedThenAddressFromGfxPartitionIsUsed) {
    MockExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    uint32_t rootDeviceIndexReserved = 10;
    auto gmmHelper = memoryManager.getGmmHelper(0);
    HeapIndex heap = HeapIndex::heapStandard64KB;
    auto alignment = memoryManager.selectAlignmentAndHeap(MemoryConstants::pageSize, &heap);
    EXPECT_EQ(heap, HeapIndex::heapStandard);
    EXPECT_EQ(MemoryConstants::pageSize64k, alignment);
    auto addressRange = memoryManager.reserveGpuAddressOnHeap(0ull, MemoryConstants::pageSize, rootDeviceIndices, &rootDeviceIndexReserved, heap, alignment);
    EXPECT_EQ(0u, rootDeviceIndexReserved);
    EXPECT_LE(memoryManager.getGfxPartition(0)->getHeapBase(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memoryManager.getGfxPartition(0)->getHeapLimit(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));

    memoryManager.freeGpuAddress(addressRange, 0);
}

TEST(OsAgnosticMemoryManager, givenOsAgnosticMemoryManagerWhenGpuAddressIsReservedAndFreedThenAddressFromGfxPartitionIsUsed) {
    MockExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    uint32_t rootDeviceIndexReserved = 10;
    auto gmmHelper = memoryManager.getGmmHelper(0);
    auto addressRange = memoryManager.reserveGpuAddressOnHeap(0ull, MemoryConstants::pageSize, rootDeviceIndices, &rootDeviceIndexReserved, NEO::HeapIndex::heapStandard, MemoryConstants::pageSize64k);
    EXPECT_EQ(0u, rootDeviceIndexReserved);
    EXPECT_LE(memoryManager.getGfxPartition(0)->getHeapBase(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memoryManager.getGfxPartition(0)->getHeapLimit(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));

    memoryManager.freeGpuAddress(addressRange, 0);

    addressRange = memoryManager.reserveGpuAddress(0ull, MemoryConstants::pageSize, rootDeviceIndices, &rootDeviceIndexReserved);
    EXPECT_EQ(0u, rootDeviceIndexReserved);
    EXPECT_LE(memoryManager.getGfxPartition(0)->getHeapBase(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memoryManager.getGfxPartition(0)->getHeapLimit(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));

    memoryManager.freeGpuAddress(addressRange, 0);
}

TEST(OsAgnosticMemoryManager, givenOsAgnosticMemoryManagerWhenGpuAddressIsReservedOnIndex1AndFreedThenAddressFromGfxPartitionIsUsed) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get(), true, 2u);
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(1);
    uint32_t rootDeviceIndexReserved = 10;
    auto gmmHelper = memoryManager.getGmmHelper(1);
    auto addressRange = memoryManager.reserveGpuAddressOnHeap(0ull, MemoryConstants::pageSize, rootDeviceIndices, &rootDeviceIndexReserved, NEO::HeapIndex::heapStandard, MemoryConstants::pageSize64k);
    EXPECT_EQ(1u, rootDeviceIndexReserved);
    EXPECT_LE(memoryManager.getGfxPartition(1)->getHeapBase(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memoryManager.getGfxPartition(1)->getHeapLimit(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));

    memoryManager.freeGpuAddress(addressRange, 1);

    addressRange = memoryManager.reserveGpuAddress(0ull, MemoryConstants::pageSize, rootDeviceIndices, &rootDeviceIndexReserved);
    EXPECT_EQ(1u, rootDeviceIndexReserved);
    EXPECT_LE(memoryManager.getGfxPartition(1)->getHeapBase(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memoryManager.getGfxPartition(1)->getHeapLimit(HeapIndex::heapStandard), gmmHelper->decanonize(addressRange.address));

    memoryManager.freeGpuAddress(addressRange, 1);
}

TEST(OsAgnosticMemoryManager, givenOsAgnosticMemoryManagerWhenGpuAddressReservationIsAttemptedWihtInvalidSizeThenFailureReturnsNullAddressRange) {
    MockExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    uint32_t rootDeviceIndexReserved = 10;
    // emulate GPU address space exhaust
    memoryManager.getGfxPartition(0)->heapInit(HeapIndex::heapStandard, 0x0, 0x10000);
    auto addressRange = memoryManager.reserveGpuAddressOnHeap(0ull, (size_t)(memoryManager.getGfxPartition(0)->getHeapLimit(HeapIndex::heapStandard) * 2), rootDeviceIndices, &rootDeviceIndexReserved, NEO::HeapIndex::heapStandard, MemoryConstants::pageSize64k);
    EXPECT_EQ(static_cast<int>(addressRange.address), 0);
    addressRange = memoryManager.reserveGpuAddress(0ull, (size_t)(memoryManager.getGfxPartition(0)->getHeapLimit(HeapIndex::heapStandard) * 2), rootDeviceIndices, &rootDeviceIndexReserved);
    EXPECT_EQ(static_cast<int>(addressRange.address), 0);
}

TEST(OsAgnosticMemoryManager, givenOsAgnosticMemoryManagerWhenGpuAddressReservationIsAttemptedWithAnInvalidRequiredPtrThenADifferentRangeIsReturned) {
    MockExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0);
    uint32_t rootDeviceIndexReserved = 10;
    auto addressRange = memoryManager.reserveGpuAddressOnHeap(0x1234, MemoryConstants::pageSize, rootDeviceIndices, &rootDeviceIndexReserved, NEO::HeapIndex::heapStandard, MemoryConstants::pageSize64k);
    EXPECT_EQ(0u, rootDeviceIndexReserved);
    EXPECT_NE(static_cast<int>(addressRange.address), 0x1234);
    EXPECT_NE(static_cast<int>(addressRange.size), 0);
    addressRange = memoryManager.reserveGpuAddress(0x1234, MemoryConstants::pageSize, rootDeviceIndices, &rootDeviceIndexReserved);
    EXPECT_EQ(0u, rootDeviceIndexReserved);
    EXPECT_NE(static_cast<int>(addressRange.address), 0x1234);
    EXPECT_NE(static_cast<int>(addressRange.size), 0);
}

TEST(OsAgnosticMemoryManager, whenCallingCreateGraphicsAllocationFromMultipleSharedHandlesWithBasePointerFromOsAgnosticMemoryManagerThenNullptrIsReturned) {
    MockExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    uint32_t mockRootDeviceIndex = 0u;
    DeviceBitfield mockDeviceBitfield(0b1);

    std::vector<osHandle> handles{6, 7};
    AllocationProperties properties = {mockRootDeviceIndex,
                                       true,
                                       MemoryConstants::pageSize,
                                       AllocationType::buffer,
                                       false,
                                       mockDeviceBitfield};
    bool requireSpecificBitness{};
    bool isHostIpcAllocation{};
    uint64_t basePointer = 0x1234;
    auto ptr = memoryManager.createGraphicsAllocationFromMultipleSharedHandles(handles, properties, requireSpecificBitness, isHostIpcAllocation, true, reinterpret_cast<void *>(basePointer));
    EXPECT_EQ(nullptr, ptr);
}

TEST(OsAgnosticMemoryManager, whenCallingCreateGraphicsAllocationFromSharedHandleWithNullBasePointerFromOsAgnosticMemoryManagerThenBasicPointerReturned) {
    uint32_t mockRootDeviceIndex = 0u;
    DeviceBitfield mockDeviceBitfield(0b1);
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);
    OsAgnosticMemoryManager::OsHandleData osHandleData{1u};
    auto size = 4096u;
    AllocationProperties properties(mockRootDeviceIndex, false, size, AllocationType::sharedBuffer, false, mockDeviceBitfield);
    auto sharedAllocation = memoryManager.createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    EXPECT_NE(nullptr, sharedAllocation);
    EXPECT_EQ(reinterpret_cast<void *>(1u), sharedAllocation->getUnderlyingBuffer());
    EXPECT_FALSE(sharedAllocation->isCoherent());
    EXPECT_NE(nullptr, sharedAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, sharedAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(MemoryPool::systemCpuInaccessible, sharedAllocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(sharedAllocation);
}

TEST(OsAgnosticMemoryManager, whenCallingCreateGraphicsAllocationFromSharedHandleWithBasePointerFromOsAgnosticMemoryManagerThenBasePointerReturned) {
    uint32_t mockRootDeviceIndex = 0u;
    DeviceBitfield mockDeviceBitfield(0b1);
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);
    OsAgnosticMemoryManager::OsHandleData osHandleData{1u};
    auto size = 4096u;
    uint64_t basePointer = 0x1234;
    AllocationProperties properties(mockRootDeviceIndex, false, size, AllocationType::sharedBuffer, false, mockDeviceBitfield);
    auto sharedAllocation = memoryManager.createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, reinterpret_cast<void *>(basePointer));
    EXPECT_NE(nullptr, sharedAllocation);
    EXPECT_EQ(reinterpret_cast<void *>(0x1234), sharedAllocation->getUnderlyingBuffer());
    EXPECT_FALSE(sharedAllocation->isCoherent());
    EXPECT_NE(nullptr, sharedAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, sharedAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(MemoryPool::systemCpuInaccessible, sharedAllocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(sharedAllocation);
}

TEST(MemoryManagerTest, givenDebugVariableWhenCreatingMemoryManagerThenSetSupportForMultiStorageResources) {
    DebugManagerStateRestore restore;

    {
        MockMemoryManager memoryManager;
        EXPECT_TRUE(memoryManager.supportsMultiStorageResources);
    }

    {
        debugManager.flags.EnableMultiStorageResources.set(0);
        MockMemoryManager memoryManager;
        EXPECT_FALSE(memoryManager.supportsMultiStorageResources);
    }

    {
        debugManager.flags.EnableMultiStorageResources.set(1);
        MockMemoryManager memoryManager;
        EXPECT_TRUE(memoryManager.supportsMultiStorageResources);
    }
}

TEST(MemoryManagerTest, givenLocalMemoryRequiredWhenSelectingHeapThenPickDeviceHeapIndex) {
    EXPECT_EQ(HeapIndex::heapInternalDeviceMemory, MemoryManager::selectInternalHeap(true));
    EXPECT_EQ(HeapIndex::heapInternal, MemoryManager::selectInternalHeap(false));

    EXPECT_EQ(HeapIndex::heapExternalDeviceMemory, MemoryManager::selectExternalHeap(true));
    EXPECT_EQ(HeapIndex::heapExternal, MemoryManager::selectExternalHeap(false));
}

TEST(MemoryManagerTest, whenCreatingAllocPropertiesForMultiStorageResourceThenMultiStorageResourcesFlagIsSetToTrue) {
    AllocationProperties properties{0, false, 0u, AllocationType::scratchSurface, false, true, 0};
    EXPECT_TRUE(properties.multiStorageResource);
}

TEST(MemoryBank, givenDifferentDeviceOrdinalsWhenGettingBankThenCorrectBanksAreReturned) {
    auto bank = MemoryBanks::getBank(0);
    EXPECT_EQ(MemoryBanks::mainBank, bank);

    bank = MemoryBanks::getBank(1);
    EXPECT_EQ(MemoryBanks::mainBank, bank);

    bank = MemoryBanks::getBank(100);
    EXPECT_EQ(MemoryBanks::mainBank, bank);

    bank = MemoryBanks::getBankForLocalMemory(0);
    EXPECT_EQ(1u, bank);

    bank = MemoryBanks::getBankForLocalMemory(1);
    EXPECT_EQ(2u, bank);

    bank = MemoryBanks::getBankForLocalMemory(2);
    EXPECT_EQ(3u, bank);

    bank = MemoryBanks::getBankForLocalMemory(3);
    EXPECT_EQ(4u, bank);
}

TEST(GraphicsAllocationTest, WhenGraphicsAllocationIsCreatedThenItIsNotCopyable) {
    EXPECT_FALSE(std::is_copy_constructible<GraphicsAllocation>::value);
    EXPECT_FALSE(std::is_copy_assignable<GraphicsAllocation>::value);
}

TEST(GraphicsAllocationTest, WhenAllocationIsCreatedThenItsAddressIsCorrect) {
    void *cpuPtr = (void *)0x30000;
    size_t size = 0x1000;

    MockGraphicsAllocation gfxAllocation(cpuPtr, size);
    uint64_t expectedGpuAddr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gfxAllocation.getUnderlyingBuffer()));

    EXPECT_EQ(expectedGpuAddr, gfxAllocation.getGpuAddress());
    EXPECT_EQ(0u, gfxAllocation.getGpuBaseAddress());
}

TEST(GraphicsAllocationTest, GivenNonSharedResourceHandleWhenAllocationIsCreatedThenItsAddressIsCorrect) {
    void *cpuPtr = (void *)0x30000;
    size_t size = 0x1000;
    MockExecutionEnvironment executionEnvironment{};
    auto gmmHelper = executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(cpuPtr));
    osHandle sharedHandle = Sharing::nonSharedResource;
    GraphicsAllocation gfxAllocation(0, 1u /*num gmms*/, AllocationType::unknown, cpuPtr, size, sharedHandle, MemoryPool::memoryNull, 0u, canonizedGpuAddress);
    uint64_t expectedGpuAddr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gfxAllocation.getUnderlyingBuffer()));

    EXPECT_EQ(expectedGpuAddr, gfxAllocation.getGpuAddress());
    EXPECT_EQ(0u, gfxAllocation.getGpuBaseAddress());
    EXPECT_EQ(sharedHandle, gfxAllocation.peekSharedHandle());
}

TEST(GraphicsAllocationTest, WhenGettingAddressesThenAddressesAreCorrect) {
    void *cpuPtr = (void *)0x30000;
    uint64_t gpuAddr = 0x30000;
    uint64_t gpuBaseAddr = 0x10000;
    size_t size = 0x1000;

    GraphicsAllocation gfxAllocation(0, 1u /*num gmms*/, AllocationType::unknown, cpuPtr, gpuAddr, gpuBaseAddr, size, MemoryPool::memoryNull, 0u);

    EXPECT_EQ(gpuAddr, gfxAllocation.getGpuAddress());

    cpuPtr = (void *)65535;
    gpuAddr = 1ULL;
    MockExecutionEnvironment executionEnvironment{};
    auto gmmHelper = executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(gpuAddr);

    gfxAllocation.setCpuPtrAndGpuAddress(cpuPtr, canonizedGpuAddress);
    EXPECT_EQ(canonizedGpuAddress, gfxAllocation.getGpuAddress());
    EXPECT_EQ(cpuPtr, gfxAllocation.getUnderlyingBuffer());
}

TEST(GraphicsAllocationTest, WhenGettingGpuAddressToPatchThenOffsetIsCorrect) {
    void *cpuPtr = (void *)0x30000;
    uint64_t gpuAddr = 0x30000;
    uint64_t gpuBaseAddr = 0x10000;
    size_t size = 0x1000;

    GraphicsAllocation gfxAllocation(0, 1u /*num gmms*/, AllocationType::unknown, cpuPtr, gpuAddr, gpuBaseAddr, size, MemoryPool::memoryNull, 0u);

    EXPECT_EQ(gpuAddr - gpuBaseAddr, gfxAllocation.getGpuAddressToPatch());
}

TEST(GraphicsAllocationTest, WhenSetSizeThenUnderlyingBufferSizeIsSet) {
    void *cpuPtr = (void *)0x30000;
    uint64_t gpuAddr = 0x30000;
    uint64_t gpuBaseAddr = 0x10000;
    size_t size = 0x2000;

    GraphicsAllocation gfxAllocation(0, 1u /*num gmms*/, AllocationType::unknown, cpuPtr, gpuAddr, gpuBaseAddr, size, MemoryPool::memoryNull, 0u);
    EXPECT_EQ(size, gfxAllocation.getUnderlyingBufferSize());

    size = 0x3000;
    gfxAllocation.setSize(size);
    EXPECT_EQ(size, gfxAllocation.getUnderlyingBufferSize());
}

using MemoryAllocatorTest = ::Test<MemoryAllocatorFixture>;

TEST_F(MemoryAllocatorTest, WhenAllocatingSystemMemoryThenNonNullPointerIsReturned) {
    auto ptr = memoryManager->allocateSystemMemory(sizeof(char), 0);
    EXPECT_NE(nullptr, ptr);
    memoryManager->freeSystemMemory(ptr);
}

TEST_F(MemoryAllocatorTest, GivenGraphicsAllocationWhenAddAndRemoveAllocationToHostPtrManagerThenfragmentHasCorrectValues) {
    void *cpuPtr = (void *)0x30000;
    size_t size = 0x1000;

    MockGraphicsAllocation gfxAllocation(cpuPtr, size);
    memoryManager->addAllocationToHostPtrManager(&gfxAllocation);
    auto fragment = memoryManager->getHostPtrManager()->getFragment({gfxAllocation.getUnderlyingBuffer(), csr->getRootDeviceIndex()});
    EXPECT_NE(fragment, nullptr);
    EXPECT_TRUE(fragment->driverAllocation);
    EXPECT_EQ(fragment->refCount, 1);
    EXPECT_EQ(fragment->fragmentCpuPointer, cpuPtr);
    EXPECT_EQ(fragment->fragmentSize, size);
    EXPECT_NE(fragment->osInternalStorage, nullptr);
    EXPECT_NE(fragment->residency, nullptr);

    FragmentStorage fragmentStorage = {};
    fragmentStorage.fragmentCpuPointer = cpuPtr;
    memoryManager->getHostPtrManager()->storeFragment(csr->getRootDeviceIndex(), fragmentStorage);
    fragment = memoryManager->getHostPtrManager()->getFragment({gfxAllocation.getUnderlyingBuffer(), csr->getRootDeviceIndex()});
    EXPECT_EQ(fragment->refCount, 2);

    fragment->driverAllocation = false;
    memoryManager->removeAllocationFromHostPtrManager(&gfxAllocation);
    fragment = memoryManager->getHostPtrManager()->getFragment({gfxAllocation.getUnderlyingBuffer(), csr->getRootDeviceIndex()});
    EXPECT_EQ(fragment->refCount, 2);
    fragment->driverAllocation = true;

    memoryManager->removeAllocationFromHostPtrManager(&gfxAllocation);
    fragment = memoryManager->getHostPtrManager()->getFragment({gfxAllocation.getUnderlyingBuffer(), csr->getRootDeviceIndex()});
    EXPECT_EQ(fragment->refCount, 1);

    memoryManager->removeAllocationFromHostPtrManager(&gfxAllocation);
    fragment = memoryManager->getHostPtrManager()->getFragment({gfxAllocation.getUnderlyingBuffer(), csr->getRootDeviceIndex()});
    EXPECT_EQ(fragment, nullptr);
}

TEST_F(MemoryAllocatorTest, GivenAlignmentWhenAllocatingSystemMemoryThenAllocatedMemoryIsAligned) {
    unsigned int alignment = 0x100;

    auto ptr = memoryManager->allocateSystemMemory(sizeof(char), alignment);
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(ptr) & (alignment - 1));
    memoryManager->freeSystemMemory(ptr);
}

TEST_F(MemoryAllocatorTest, WhenAllocatingGraphicsMemoryThenAllocationHasCorrectProperties) {
    unsigned int alignment = 4096;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});

    ASSERT_NE(nullptr, allocation);
    // initial taskCount must be -1. if not, we may kill allocation before it will be used
    EXPECT_EQ(std::numeric_limits<TaskCountType>::max(), allocation->getTaskCount(csr->getOsContext().getContextId()));
    // We know we want graphics memory to be page aligned
    EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(allocation->getUnderlyingBuffer()) & (alignment - 1));
    EXPECT_EQ(Sharing::nonSharedResource, allocation->peekSharedHandle());

    // Gpu address equal to cpu address
    if (defaultHwInfo->capabilityTable.gpuAddressSpace == MemoryConstants::max48BitAddress) {
        EXPECT_EQ(reinterpret_cast<uint64_t>(allocation->getUnderlyingBuffer()), allocation->getGpuAddress());
    }

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(MemoryAllocatorTest, WhenAllocatingGraphicsMemoryThenAllocationIsPageAligned) {
    unsigned int alignment = 4096;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(allocation->getUnderlyingBuffer()) & (alignment - 1));
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(MemoryAllocatorTest, GivenAlignedHostPtrWithAlignedSizeWhenAllocatingGraphicsThenOneFragmentIsAllocated) {
    auto ptr = (void *)0x1000;
    MockMemoryManager mockMemoryManager(*executionEnvironment);
    auto hostPtrManager = static_cast<MockHostPtrManager *>(mockMemoryManager.getHostPtrManager());
    auto graphicsAllocation = mockMemoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), false, 4096, device->getDeviceBitfield()}, ptr);
    EXPECT_NE(nullptr, graphicsAllocation);

    EXPECT_EQ(1u, hostPtrManager->getFragmentCount());
    auto fragmentData = hostPtrManager->getFragment({ptr, device->getRootDeviceIndex()});

    ASSERT_NE(nullptr, fragmentData);

    EXPECT_NE(nullptr, fragmentData->osInternalStorage);

    mockMemoryManager.freeGraphicsMemory(graphicsAllocation);
}

TEST_F(MemoryAllocatorTest, GivenAlignedHostPtrAndCacheAlignedSizeWhenAskedForL3AllowanceThenTrueIsReturned) {
    auto ptr = (void *)0x1000;
    auto alignedSize = MemoryConstants::cacheLineSize;

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), false, alignedSize, device->getDeviceBitfield()}, ptr);

    EXPECT_TRUE(isL3Capable(*graphicsAllocation));

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(MemoryAllocatorTest, GivenAlignedHostPtrAndNotCacheAlignedSizeWhenAskedForL3AllowanceThenFalseIsReturned) {
    auto ptr = (void *)0x1000;
    auto alignedSize = MemoryConstants::cacheLineSize - 1;

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), false, alignedSize, device->getDeviceBitfield()}, ptr);

    EXPECT_FALSE(isL3Capable(*graphicsAllocation));

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(MemoryAllocatorTest, GivenMisAlignedHostPtrAndNotCacheAlignedSizeWhenAskedForL3AllowanceThenFalseIsReturned) {
    auto ptr = (void *)0x1001;
    auto alignedSize = MemoryConstants::cacheLineSize - 1;

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), false, alignedSize, device->getDeviceBitfield()}, ptr);

    EXPECT_FALSE(isL3Capable(*graphicsAllocation));

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(MemoryAllocatorTest, GivenHostPtrAlignedToCacheLineWhenAskedForL3AllowanceThenTrueIsReturned) {
    auto ptr = (void *)0x1040;
    auto alignedSize = MemoryConstants::cacheLineSize;

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), false, alignedSize, device->getDeviceBitfield()}, ptr);

    EXPECT_TRUE(isL3Capable(*graphicsAllocation));

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(MemoryAllocatorTest, WhenPopulatingOsHandleThenOneFragmentIsReturned) {
    OsHandleStorage storage;
    storage.fragmentStorageData[0].cpuPtr = (void *)0x1000;
    memoryManager->populateOsHandles(storage, 0);
    EXPECT_NE(nullptr, storage.fragmentStorageData[0].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[1].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[2].osHandleStorage);
    memoryManager->getHostPtrManager()->releaseHandleStorage(csr->getRootDeviceIndex(), storage);
    memoryManager->cleanOsHandles(storage, 0);
}

TEST_F(MemoryAllocatorTest, givenOsHandleStorageWhenOsHandlesAreCleanedAndAubManagerIsNotAvailableThenFreeMemoryIsNotCalledOnAubManager) {
    MockExecutionEnvironment mockExecutionEnvironment(defaultHwInfo.get());
    MockMemoryManager mockMemoryManager(mockExecutionEnvironment);
    auto mockAubCenter = new MockAubCenter(*mockExecutionEnvironment.rootDeviceEnvironments[0], false, "aubfile", CommandStreamReceiverType::aub);
    mockAubCenter->aubManager.reset(nullptr);
    mockExecutionEnvironment.rootDeviceEnvironments[0]->aubCenter.reset(mockAubCenter);

    OsHandleStorage storage;
    storage.fragmentStorageData[0].cpuPtr = (void *)0x1000;
    mockMemoryManager.populateOsHandles(storage, 0);
    mockMemoryManager.getHostPtrManager()->releaseHandleStorage(csr->getRootDeviceIndex(), storage);
    mockMemoryManager.cleanOsHandles(storage, 0);

    EXPECT_EQ(nullptr, mockAubCenter->aubManager);
}

TEST_F(MemoryAllocatorTest, givenOsHandleStorageAndFreeMemoryEnabledWhenOsHandlesAreCleanedAndAubManagerIsAvailableThenFreeMemoryIsCalledOnAubManager) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableFreeMemory.set(true);
    const uint32_t rootDeviceIndex = 1u;
    MockExecutionEnvironment mockExecutionEnvironment(defaultHwInfo.get(), true, 3);
    MockMemoryManager mockMemoryManager(mockExecutionEnvironment);
    auto mockManager0 = new MockAubManager();
    auto mockAubCenter0 = new MockAubCenter(*mockExecutionEnvironment.rootDeviceEnvironments[0], false, "aubfile", CommandStreamReceiverType::aub);
    mockAubCenter0->aubManager.reset(mockManager0);
    mockExecutionEnvironment.rootDeviceEnvironments[0]->aubCenter.reset(mockAubCenter0);

    auto mockManager1 = new MockAubManager();
    auto mockAubCenter1 = new MockAubCenter(*mockExecutionEnvironment.rootDeviceEnvironments[0], false, "aubfile", CommandStreamReceiverType::aub);
    mockAubCenter1->aubManager.reset(mockManager1);
    mockExecutionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->aubCenter.reset(mockAubCenter1);

    OsHandleStorage storage;
    storage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    mockMemoryManager.populateOsHandles(storage, rootDeviceIndex);
    mockMemoryManager.getHostPtrManager()->releaseHandleStorage(rootDeviceIndex, storage);
    mockMemoryManager.cleanOsHandles(storage, rootDeviceIndex);

    EXPECT_FALSE(mockManager0->freeMemoryCalled);
    EXPECT_TRUE(mockManager1->freeMemoryCalled);
}

HWTEST_F(MemoryAllocatorTest, givenAllocationUsedByContextWhenFreeingThenHandleCompletionIsCalled) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableFreeMemory.set(true);
    const uint32_t rootDeviceIndex = 0u;
    auto mockManager0 = new MockAubManager();
    auto mockAubCenter0 = new MockAubCenter(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex], false, "aubfile", CommandStreamReceiverType::aub);
    mockAubCenter0->aubManager.reset(mockManager0);
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->aubCenter.reset(mockAubCenter0);

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), true, 0x1000, device->getDeviceBitfield()}, nullptr);

    ASSERT_NE(nullptr, allocation);
    allocation->updateTaskCount(0u, csr->getOsContext().getContextId());

    memoryManager->freeGraphicsMemory(allocation);

    EXPECT_TRUE(mockManager0->freeMemoryCalled);
    EXPECT_TRUE(static_cast<UltCommandStreamReceiver<FamilyType> *>(csr)->pollForCompletionCalled);
}

TEST_F(MemoryAllocatorTest, GivenEmptyMemoryManagerAndMisalingedHostPtrWithHugeSizeWhenAskedForHostPtrAllocationThenGraphicsAllocationIsBeignCreatedWithAllFragmentsPresent) {
    void *cpuPtr = (void *)0x1005;
    auto size = MemoryConstants::pageSize * 10 - 1;

    MockMemoryManager mockMemoryManager(*executionEnvironment);
    auto hostPtrManager = static_cast<MockHostPtrManager *>(mockMemoryManager.getHostPtrManager());
    auto reqs = MockHostPtrManager::getAllocationRequirements(device->getRootDeviceIndex(), cpuPtr, size);

    ASSERT_EQ(3u, reqs.requiredFragmentsCount);

    auto graphicsAllocation = mockMemoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), false, size, device->getDeviceBitfield()}, cpuPtr);
    for (int i = 0; i < maxFragmentsCount; i++) {
        EXPECT_NE(nullptr, graphicsAllocation->fragmentsStorage.fragmentStorageData[i].osHandleStorage);
        EXPECT_EQ(reqs.allocationFragments[i].allocationPtr, graphicsAllocation->fragmentsStorage.fragmentStorageData[i].cpuPtr);
        EXPECT_EQ(reqs.allocationFragments[i].allocationSize, graphicsAllocation->fragmentsStorage.fragmentStorageData[i].fragmentSize);
    }

    EXPECT_EQ(3u, hostPtrManager->getFragmentCount());
    EXPECT_EQ(Sharing::nonSharedResource, graphicsAllocation->peekSharedHandle());
    mockMemoryManager.freeGraphicsMemory(graphicsAllocation);
}

TEST_F(MemoryAllocatorTest, GivenPointerAndSizeWhenAskedToCreateGrahicsAllocationThenGraphicsAllocationIsCreated) {
    OsHandleStorage handleStorage;
    auto ptr = (void *)0x1000;
    auto ptr2 = (void *)0x1001;
    auto size = MemoryConstants::pageSize;

    handleStorage.fragmentStorageData[0].cpuPtr = ptr;
    handleStorage.fragmentStorageData[1].cpuPtr = ptr2;
    handleStorage.fragmentStorageData[2].cpuPtr = nullptr;

    handleStorage.fragmentStorageData[0].fragmentSize = size;
    handleStorage.fragmentStorageData[1].fragmentSize = size * 2;
    handleStorage.fragmentStorageData[2].fragmentSize = size * 3;

    AllocationData allocationData;
    allocationData.size = size;
    allocationData.hostPtr = ptr;
    auto allocation = std::unique_ptr<GraphicsAllocation>(memoryManager->createGraphicsAllocation(handleStorage, allocationData));

    EXPECT_EQ(MemoryPool::system4KBPages, allocation->getMemoryPool());
    EXPECT_EQ(ptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());

    EXPECT_EQ(ptr, allocation->fragmentsStorage.fragmentStorageData[0].cpuPtr);
    EXPECT_EQ(ptr2, allocation->fragmentsStorage.fragmentStorageData[1].cpuPtr);
    EXPECT_EQ(nullptr, allocation->fragmentsStorage.fragmentStorageData[2].cpuPtr);

    EXPECT_EQ(size, allocation->fragmentsStorage.fragmentStorageData[0].fragmentSize);
    EXPECT_EQ(size * 2, allocation->fragmentsStorage.fragmentStorageData[1].fragmentSize);
    EXPECT_EQ(size * 3, allocation->fragmentsStorage.fragmentStorageData[2].fragmentSize);

    EXPECT_NE(&allocation->fragmentsStorage, &handleStorage);
}

TEST_F(MemoryAllocatorTest, givenMemoryManagerWhenAskedFor32bitAllocationThen32bitGraphicsAllocationIsReturned) {
    size_t size = 10;
    auto allocation = memoryManager->allocate32BitGraphicsMemory(device->getRootDeviceIndex(), size, nullptr, AllocationType::buffer);
    EXPECT_NE(nullptr, allocation);
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_TRUE(allocation->is32BitAllocation());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(MemoryAllocatorTest, givenNotEnoughSpaceInAllocatorWhenAskedFor32bitAllocationThenNullptrIsReturned) {
    size_t size = 0xfffff000;
    auto allocationFirst = memoryManager->allocate32BitGraphicsMemory(device->getRootDeviceIndex(), 0x5000, nullptr, AllocationType::buffer);
    auto allocation = memoryManager->allocate32BitGraphicsMemory(device->getRootDeviceIndex(), size, nullptr, AllocationType::buffer);
    EXPECT_EQ(nullptr, allocation);
    memoryManager->freeGraphicsMemory(allocationFirst);
}

TEST_F(MemoryAllocatorTest, givenNotEnoughSpaceInAllocatorWhenAskedFor32bitAllocationWithHostPtrThenNullptrIsReturned) {
    size_t size = 0xfffff000;
    void *ptr = (void *)0x10000;
    auto allocationFirst = memoryManager->allocate32BitGraphicsMemory(device->getRootDeviceIndex(), 0x5000, nullptr, AllocationType::buffer);
    auto allocation = memoryManager->allocate32BitGraphicsMemory(device->getRootDeviceIndex(), size, ptr, AllocationType::buffer);
    EXPECT_EQ(nullptr, allocation);
    memoryManager->freeGraphicsMemory(allocationFirst);
}

TEST_F(MemoryAllocatorTest, givenMemoryManagerWhenAskedFor32bitAllocationWithPtrThen32bitGraphicsAllocationWithGpuAddressIsReturned) {
    size_t size = 10;
    void *ptr = (void *)0x1000;
    auto allocation = memoryManager->allocate32BitGraphicsMemory(device->getRootDeviceIndex(), size, ptr, AllocationType::buffer);
    EXPECT_NE(nullptr, allocation);
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_TRUE(allocation->is32BitAllocation());

    EXPECT_EQ(ptr, allocation->getUnderlyingBuffer());
    EXPECT_NE(0u, allocation->getGpuAddress());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(MemoryAllocatorTest, givenAllocationWithFragmentsWhenCallingFreeGraphicsMemoryThenDoNotCallHandleFenceCompletion) {
    auto size = 3u * MemoryConstants::pageSize;
    auto *ptr = reinterpret_cast<void *>(0xbeef1);
    AllocationProperties properties{device->getRootDeviceIndex(), false, size, AllocationType::buffer, false, device->getDeviceBitfield()};

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, ptr);
    EXPECT_EQ(3u, allocation->fragmentsStorage.fragmentCount);

    EXPECT_EQ(0u, memoryManager->handleFenceCompletionCalled);
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(0u, memoryManager->handleFenceCompletionCalled);
}

TEST_F(MemoryAllocatorTest, GivenShareableEnabledAndDisabledWhenAskedToCreateGrahicsAllocationThenValidAllocationIsReturned) {
    AllocationData allocationData;
    allocationData.type = AllocationType::buffer;

    allocationData.flags.shareable = 1u;
    auto shareableAllocation = memoryManager->allocateGraphicsMemory(allocationData);
    EXPECT_NE(nullptr, shareableAllocation);

    allocationData.flags.shareable = 0u;
    auto nonShareableAllocation = memoryManager->allocateGraphicsMemory(allocationData);
    EXPECT_NE(nullptr, nonShareableAllocation);

    memoryManager->freeGraphicsMemory(shareableAllocation);
    memoryManager->freeGraphicsMemory(nonShareableAllocation);
}

TEST_F(MemoryAllocatorTest, givenAllocationWithoutFragmentsWhenCallingFreeGraphicsMemoryThenCallHandleFenceCompletion) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::buffer, device->getDeviceBitfield()});
    EXPECT_EQ(0u, allocation->fragmentsStorage.fragmentCount);

    EXPECT_EQ(0u, memoryManager->handleFenceCompletionCalled);
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(1u, memoryManager->handleFenceCompletionCalled);
}

HWTEST_F(MemoryAllocatorTest, givenSupportFor1MbAlignmentWhenAllocateGraphicsMemoryThenAlignmentIsSetCorrect) {
    class MockGfxCoreHelperHw : public GfxCoreHelperHw<FamilyType> {
      public:
        using GfxCoreHelperHw<FamilyType>::GfxCoreHelperHw;
        bool is1MbAlignmentSupported(const HardwareInfo &hwInfo, bool isCompressionEnabled) const override {
            return isEnable;
        }
        bool isEnable = false;
    };
    auto raiiFactory = RAIIGfxCoreHelperFactory<MockGfxCoreHelperHw>(*executionEnvironment->rootDeviceEnvironments[0]);
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto size = MemoryConstants::pageSize;

    raiiFactory.mockGfxCoreHelper->isEnable = true;

    auto osAgnosticMemoryManager = std::make_unique<MemoryManagerCreate<MockMemoryManager>>(true, false, *executionEnvironment);
    osAgnosticMemoryManager->failInDevicePool = true;

    MockAllocationProperties properties(mockRootDeviceIndex, true, size, AllocationType::buffer, mockDeviceBitfield);
    properties.flags.preferCompressed = true;

    auto allocationWithEnabled1MbAlignment = osAgnosticMemoryManager->allocateGraphicsMemoryWithProperties(properties, ptr);

    ASSERT_NE(nullptr, allocationWithEnabled1MbAlignment);
    EXPECT_EQ(MemoryConstants::megaByte, osAgnosticMemoryManager->alignAllocationData.alignment);

    osAgnosticMemoryManager->freeGraphicsMemory(allocationWithEnabled1MbAlignment);

    raiiFactory.mockGfxCoreHelper->isEnable = false;
    auto allocationWithoutEnabled1MbAlignment = osAgnosticMemoryManager->allocateGraphicsMemoryWithProperties(properties, ptr);

    ASSERT_NE(nullptr, allocationWithoutEnabled1MbAlignment);
    EXPECT_NE(MemoryConstants::megaByte, osAgnosticMemoryManager->alignAllocationData.alignment);

    osAgnosticMemoryManager->freeGraphicsMemory(allocationWithoutEnabled1MbAlignment);
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenItIsCreatedThenItIsInitialized) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    EXPECT_TRUE(memoryManager.isInitialized());
}

TEST(OsAgnosticMemoryManager, givenDefaultAubUsageMemoryManagerWhenItIsCreatedThenItIsInitialized) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    OsAgnosticMemoryManager memoryManager(true, executionEnvironment);
    EXPECT_TRUE(memoryManager.isInitialized());
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenItIsCreatedAndGfxPartitionInitIsFailedThenItIsNotInitialized) {
    class TestedOsAgnosticMemoryManager : public OsAgnosticMemoryManager {
      public:
        using OsAgnosticMemoryManager::gfxPartitions;
        using OsAgnosticMemoryManager::OsAgnosticMemoryManager;
    };

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    TestedOsAgnosticMemoryManager memoryManager(executionEnvironment);
    EXPECT_TRUE(memoryManager.isInitialized());

    auto failedInitGfxPartition = std::make_unique<FailedInitGfxPartition>();
    memoryManager.gfxPartitions[0].reset(failedInitGfxPartition.release());
    memoryManager.initialize(false /*aubUsage*/);
    EXPECT_FALSE(memoryManager.isInitialized());
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenItIsCreatedThenForce32BitAllocationsIsFalse) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    EXPECT_FALSE(memoryManager.peekForce32BitAllocations());
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenForce32bitallocationIsCalledWithTrueThenMemoryManagerForces32BitAlloactions) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    memoryManager.setForce32BitAllocations(true);
    EXPECT_TRUE(memoryManager.peekForce32BitAllocations());
}

TEST(OsAgnosticMemoryManager, givenMultipleRootDevicesWhenUpdateLatestContextIdForRootDeviceThenRootDeviceIndexToContextIdUpdateProperly) {
    class TestedOsAgnosticMemoryManager : public OsAgnosticMemoryManager {
      public:
        using OsAgnosticMemoryManager::latestContextId;
        using OsAgnosticMemoryManager::OsAgnosticMemoryManager;
        using OsAgnosticMemoryManager::rootDeviceIndexToContextId;
        using OsAgnosticMemoryManager::updateLatestContextIdForRootDevice;
    };
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    TestedOsAgnosticMemoryManager memoryManager(executionEnvironment);
    uint32_t rootDeviceArray[] = {0, 1, 2, 3};
    for (auto &rootDeviceIndex : rootDeviceArray) {
        for (int count = 0; count < 10; count++) {
            memoryManager.updateLatestContextIdForRootDevice(rootDeviceIndex);
            ++memoryManager.latestContextId;
        }
    }
    EXPECT_EQ(memoryManager.rootDeviceIndexToContextId[0], std::numeric_limits<uint32_t>::max());
    EXPECT_EQ(memoryManager.rootDeviceIndexToContextId[1], 9u);
    EXPECT_EQ(memoryManager.rootDeviceIndexToContextId[2], 19u);
    EXPECT_EQ(memoryManager.rootDeviceIndexToContextId[3], 29u);

    memoryManager.reInitLatestContextId();
    for (int count = 0; count < 10; count++) {
        memoryManager.updateLatestContextIdForRootDevice(2);
        ++memoryManager.latestContextId;
    }
    EXPECT_EQ(memoryManager.rootDeviceIndexToContextId[2], 19u);
}

TEST(OsAgnosticMemoryManager, givenCreateOrReleaseDeviceSpecificMemResourcesWhenCreatingMemoryManagerObjectThenTheseMethodsAreEmpty) {
    class TestedOsAgnosticMemoryManager : public OsAgnosticMemoryManager {
      public:
        using OsAgnosticMemoryManager::OsAgnosticMemoryManager;
    };
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    TestedOsAgnosticMemoryManager memoryManager(executionEnvironment);
    memoryManager.releaseDeviceSpecificMemResources(1);
    memoryManager.createDeviceSpecificMemResources(1);
}

class MyOsAgnosticMemoryManager : public OsAgnosticMemoryManager {
  public:
    bool peek32bit() override {
        return is32bit;
    }
    MyOsAgnosticMemoryManager(bool, ExecutionEnvironment &executionEnvironment) : OsAgnosticMemoryManager(false, executionEnvironment) {}
    MyOsAgnosticMemoryManager(ExecutionEnvironment &executionEnvironment) : MyOsAgnosticMemoryManager(false, executionEnvironment) {}
    bool is32bit = false;
};

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenIsLimitedGPUIsCalledThenCorrectValueIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MyOsAgnosticMemoryManager memoryManager(executionEnvironment);
    memoryManager.is32bit = false;
    EXPECT_FALSE(memoryManager.isLimitedGPU(mockRootDeviceIndex));
    memoryManager.is32bit = true;
    if (executionEnvironment.rootDeviceEnvironments[mockRootDeviceIndex]->isFullRangeSvm()) {
        EXPECT_FALSE(memoryManager.isLimitedGPU(mockRootDeviceIndex));
    } else {
        EXPECT_TRUE(memoryManager.isLimitedGPU(mockRootDeviceIndex));
    }
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenIsLimitedGPUOnTypeIsCalledThenCorrectValueIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MyOsAgnosticMemoryManager memoryManager(executionEnvironment);
    memoryManager.is32bit = false;
    EXPECT_FALSE(memoryManager.isLimitedGPUOnType(mockRootDeviceIndex, AllocationType::buffer));
    EXPECT_FALSE(memoryManager.isLimitedGPUOnType(mockRootDeviceIndex, AllocationType::image));
    EXPECT_FALSE(memoryManager.isLimitedGPUOnType(mockRootDeviceIndex, AllocationType::mapAllocation));

    memoryManager.is32bit = true;
    if (executionEnvironment.rootDeviceEnvironments[mockRootDeviceIndex]->isFullRangeSvm()) {
        EXPECT_FALSE(memoryManager.isLimitedGPUOnType(mockRootDeviceIndex, AllocationType::buffer));
        EXPECT_FALSE(memoryManager.isLimitedGPUOnType(mockRootDeviceIndex, AllocationType::image));
        EXPECT_FALSE(memoryManager.isLimitedGPUOnType(mockRootDeviceIndex, AllocationType::mapAllocation));
    } else {
        EXPECT_TRUE(memoryManager.isLimitedGPUOnType(mockRootDeviceIndex, AllocationType::buffer));
        EXPECT_FALSE(memoryManager.isLimitedGPUOnType(mockRootDeviceIndex, AllocationType::image));
        EXPECT_FALSE(memoryManager.isLimitedGPUOnType(mockRootDeviceIndex, AllocationType::mapAllocation));
    }
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenAskedFor32BitAllocationWhenLimitedAllocationIsEnabledThenGpuRangeFromExternalHeapIsAllocatiedAndBaseAddressIsSet) {
    if (is32bit) {
        GTEST_SKIP();
    }

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(executionEnvironment);
    memoryManager.setForce32BitAllocations(true);
    memoryManager.forceLimitedRangeAllocator(0, 0xFFFFFFFFF);

    AllocationData allocationData;
    memoryManager.getAllocationData(allocationData, {mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::buffer, mockDeviceBitfield}, nullptr, StorageInfo{});
    auto gfxAllocation = memoryManager.allocateGraphicsMemoryWithAlignment(allocationData);
    ASSERT_NE(gfxAllocation, nullptr);
    EXPECT_NE(gfxAllocation->getGpuBaseAddress(), 0ull);
    EXPECT_EQ(gfxAllocation->getGpuBaseAddress(), memoryManager.getExternalHeapBaseAddress(gfxAllocation->getRootDeviceIndex(), gfxAllocation->isAllocatedInLocalMemoryPool()));
    memoryManager.freeGraphicsMemory(gfxAllocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenAskedForNon32BitAllocationWhenLimitedAllocationIsEnabledThenGpuRangeFromiStandardHeapIsAllocatiedAndBaseAddressIsNotSet) {
    if (is32bit) {
        GTEST_SKIP();
    }

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(executionEnvironment);
    memoryManager.forceLimitedRangeAllocator(0, 0xFFFFFFFFF);

    AllocationData allocationData;
    memoryManager.getAllocationData(allocationData, {mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::buffer, mockDeviceBitfield}, nullptr, StorageInfo{});
    auto gfxAllocation = memoryManager.allocateGraphicsMemoryWithAlignment(allocationData);
    ASSERT_NE(gfxAllocation, nullptr);
    EXPECT_EQ(gfxAllocation->getGpuBaseAddress(), 0ull);
    EXPECT_EQ(gfxAllocation->getGpuAddress(), memoryManager.getGfxPartition(allocationData.rootDeviceIndex)->getHeapLimit(HeapIndex::heapStandard) + 1 - GfxPartition::heapGranularity - MemoryConstants::pageSize);
    memoryManager.freeGraphicsMemory(gfxAllocation);
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenAllocateGraphicsMemoryForImageIsCalledThenGraphicsAllocationIsReturned) {

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(executionEnvironment);
    ImageDescriptor imgDesc = {};
    imgDesc.imageWidth = 512;
    imgDesc.imageHeight = 1;
    imgDesc.imageType = ImageType::image2D;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    AllocationData allocationData;
    allocationData.imgInfo = &imgInfo;

    auto imageAllocation = memoryManager.allocateGraphicsMemoryForImage(allocationData);
    ASSERT_NE(nullptr, imageAllocation);
    EXPECT_TRUE(imageAllocation->getDefaultGmm()->resourceParams.Usage == GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE);
    EXPECT_EQ(executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo()->featureTable.flags.ftrLocalMemory,
              imageAllocation->getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly);
    memoryManager.freeGraphicsMemory(imageAllocation);
}

TEST(OsAgnosticMemoryManager, givenDestroyedTagAllocationWhenWaitForCompletiionThenWaitForTaskCountIsNotCalled) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    auto memoryManager = new OsAgnosticMemoryManager(executionEnvironment);
    DeviceBitfield deviceBitfield(1);
    std::unique_ptr<CommandStreamReceiver> csr(createCommandStream(executionEnvironment, 0u, deviceBitfield));
    executionEnvironment.memoryManager.reset(memoryManager);
    auto &gfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    auto osContext = memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment.rootDeviceEnvironments[0])[0],
                                                                                                                       PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{0, MemoryConstants::pageSize});
    allocation->updateTaskCount(10, osContext->getContextId());

    EXPECT_GT(allocation->getTaskCount(osContext->getContextId()), csr->peekTaskCount());
    memoryManager->waitForEnginesCompletion(*allocation);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenEnabledLocalMemoryWhenAllocateGraphicsMemoryForImageIsCalledThenUseLocalMemoryIsNotSet) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);
    ImageDescriptor imgDesc = {};
    imgDesc.imageWidth = 1;
    imgDesc.imageHeight = 1;
    imgDesc.imageType = ImageType::image2D;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    AllocationData allocationData;
    allocationData.imgInfo = &imgInfo;

    auto imageAllocation = memoryManager.allocateGraphicsMemoryForImage(allocationData);
    ASSERT_NE(nullptr, imageAllocation);
    EXPECT_FALSE(imgInfo.useLocalMemory);
    memoryManager.freeGraphicsMemory(imageAllocation);
}
TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerAndUnifiedAuxCapableAllocationWhenMappingThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = new Gmm(executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper(), nullptr, 123, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{0, MemoryConstants::pageSize});
    allocation->setDefaultGmm(gmm);

    auto mockGmmRes = reinterpret_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    mockGmmRes->setUnifiedAuxTranslationCapable();

    EXPECT_FALSE(memoryManager.mapAuxGpuVA(allocation));

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenAllocateGraphicsMemoryIsCalledThenMemoryPoolIsSystem4KBPages) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);
    if (memoryManager.isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    auto size = 4096u;

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{0, size});
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::system4KBPages, allocation->getMemoryPool());
    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenCompressionEnabledWhenAllocateGraphicsMemoryWithAlignmentIsCalledThenGmmIsAllocated) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.RenderCompressedBuffersEnabled.set(true);
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MockMemoryManager memoryManager(true, false, executionEnvironment);
    AllocationData allocationData;
    allocationData.size = 4096u;
    allocationData.alignment = MemoryConstants::pageSize;
    allocationData.flags.preferCompressed = true;
    auto allocation = memoryManager.allocateGraphicsMemoryWithAlignment(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::system4KBPages, allocation->getMemoryPool());
    EXPECT_NE(nullptr, allocation->getDefaultGmm());
    EXPECT_EQ(true, allocation->getDefaultGmm()->isCompressionEnabled());
    EXPECT_EQ(MemoryConstants::pageSize, allocation->getDefaultGmm()->resourceParams.BaseAlignment);
    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWith64KBPagesEnabledWhenAllocateGraphicsMemory64kbIsCalledThenMemoryPoolIsSystem64KBPages) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MockMemoryManager memoryManager(true, false, executionEnvironment);
    AllocationData allocationData;
    allocationData.size = 4096u;
    allocationData.alignment = MemoryConstants::pageSize;
    auto allocation = memoryManager.allocateGraphicsMemory64kb(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::system64KBPages, allocation->getMemoryPool());
    EXPECT_EQ(MemoryConstants::pageSize64k, allocation->getDefaultGmm()->resourceParams.BaseAlignment);
    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWith64KBPagesEnabledAndCompressionEnabledWhenAllocateGraphicsMemory64kbIsCalledThenMemoryPoolIsSystem64KBPages) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.RenderCompressedBuffersEnabled.set(true);
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MockMemoryManager memoryManager(true, false, executionEnvironment);
    AllocationData allocationData;
    allocationData.size = 4096u;
    allocationData.alignment = MemoryConstants::pageSize;
    allocationData.flags.preferCompressed = true;
    auto allocation = memoryManager.allocateGraphicsMemory64kb(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::system64KBPages, allocation->getMemoryPool());
    EXPECT_GT(allocation->getDefaultGmm()->resourceParams.BaseAlignment, MemoryConstants::pageSize);
    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWith64KBPagesEnabledWhenAllocateGraphicsMemoryFailsThenNullptrIsReturned) {
    class MockOsAgnosticManagerWithFailingAllocate : public MemoryManagerCreate<OsAgnosticMemoryManager> {
      public:
        using OsAgnosticMemoryManager::allocateGraphicsMemory64kb;
        MockOsAgnosticManagerWithFailingAllocate(bool enable64kbPages, ExecutionEnvironment &executionEnvironment) : MemoryManagerCreate(enable64kbPages, false, executionEnvironment) {}

        GraphicsAllocation *allocateGraphicsMemoryWithAlignment(const AllocationData &allocationData) override {
            return nullptr;
        }
    };
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockOsAgnosticManagerWithFailingAllocate memoryManager(true, executionEnvironment);
    AllocationData allocationData;
    allocationData.size = 4096u;
    auto allocation = memoryManager.allocateGraphicsMemory64kb(allocationData);
    EXPECT_EQ(nullptr, allocation);
    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenAllocateGraphicsMemoryWithPtrIsCalledThenMemoryPoolIsSystem4KBPages) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);
    if (memoryManager.isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto size = MemoryConstants::pageSize;

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{mockRootDeviceIndex, false, size, mockDeviceBitfield}, ptr);

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::system4KBPages, allocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenAllocate32BitGraphicsMemoryWithPtrIsCalledThenMemoryPoolIsSystem4KBPagesWith32BitGpuAddressing) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto size = MemoryConstants::pageSize;

    auto allocation = memoryManager.allocate32BitGraphicsMemory(mockRootDeviceIndex, size, ptr, AllocationType::buffer);

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::system4KBPagesWith32BitGpuAddressing, allocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenAllocate32BitGraphicsMemoryWithoutPtrIsCalledThenMemoryPoolIsSystem4KBPagesWith32BitGpuAddressing) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    void *ptr = nullptr;
    auto size = MemoryConstants::pageSize;

    auto allocation = memoryManager.allocate32BitGraphicsMemory(mockRootDeviceIndex, size, ptr, AllocationType::buffer);

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::system4KBPagesWith32BitGpuAddressing, allocation->getMemoryPool());
    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWith64KBPagesEnabledWhenAllocateGraphicsMemoryThenMemoryPoolIsSystem64KBPages) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(true, false, executionEnvironment);
    if (memoryManager.isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    auto svmAllocation = memoryManager.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::svmZeroCopy, mockDeviceBitfield});
    EXPECT_NE(nullptr, svmAllocation);
    EXPECT_EQ(MemoryPool::system64KBPages, svmAllocation->getMemoryPool());
    memoryManager.freeGraphicsMemory(svmAllocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWith64KBPagesDisabledWhenAllocateGraphicsMemoryThen4KBGraphicsAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);
    if (memoryManager.isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    auto svmAllocation = memoryManager.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::svmZeroCopy, mockDeviceBitfield});
    EXPECT_EQ(MemoryPool::system4KBPages, svmAllocation->getMemoryPool());
    memoryManager.freeGraphicsMemory(svmAllocation);
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenCreateGraphicsAllocationFromSharedObjectIsCalledThenGraphicsAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);
    OsAgnosticMemoryManager::OsHandleData osHandleData{1u};
    auto size = 4096u;
    AllocationProperties properties(mockRootDeviceIndex, false, size, AllocationType::sharedBuffer, false, mockDeviceBitfield);
    auto sharedAllocation = memoryManager.createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    EXPECT_NE(nullptr, sharedAllocation);
    EXPECT_FALSE(sharedAllocation->isCoherent());
    EXPECT_NE(nullptr, sharedAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, sharedAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(MemoryPool::systemCpuInaccessible, sharedAllocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(sharedAllocation);
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenCreateGraphicsAllocationFromSharedObjectIsCalledWithAMappedPointerThenGraphicsAllocationIsReturnedWithTheSameAddress) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);
    OsAgnosticMemoryManager::OsHandleData osHandleData{1u};
    auto size = 4096u;
    void *mapPointer = reinterpret_cast<void *>(0x1234);
    AllocationProperties properties(mockRootDeviceIndex, false, size, AllocationType::sharedBuffer, false, mockDeviceBitfield);
    auto sharedAllocation = memoryManager.createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, mapPointer);
    EXPECT_NE(nullptr, sharedAllocation);
    EXPECT_FALSE(sharedAllocation->isCoherent());
    EXPECT_EQ(mapPointer, sharedAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, sharedAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(MemoryPool::systemCpuInaccessible, sharedAllocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(sharedAllocation);
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenCreateGraphicsAllocationFromSharedObjectIsCalledAndRootDeviceIndexIsSpecifiedThenGraphicsAllocationIsReturnedWithCorrectRootDeviceIndex) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);
    OsAgnosticMemoryManager::OsHandleData osHandleData{1u};
    auto size = 4096u;
    AllocationProperties properties(mockRootDeviceIndex, false, size, AllocationType::sharedBuffer, false, false, mockDeviceBitfield);
    EXPECT_EQ(properties.subDevicesBitfield, mockDeviceBitfield);
    EXPECT_EQ(properties.rootDeviceIndex, 0u);

    auto sharedAllocation = memoryManager.createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    EXPECT_NE(nullptr, sharedAllocation);
    EXPECT_EQ(0u, sharedAllocation->getRootDeviceIndex());
    EXPECT_FALSE(sharedAllocation->isCoherent());
    EXPECT_NE(nullptr, sharedAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, sharedAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(MemoryPool::systemCpuInaccessible, sharedAllocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(sharedAllocation);
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenCreateGraphicsAllocationFromSharedObjectIsCalledWithSpecificBitnessThen32BitGraphicsAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);
    OsAgnosticMemoryManager::OsHandleData osHandleData{1u};
    auto size = 4096u;
    AllocationProperties properties(mockRootDeviceIndex, false, size, AllocationType::sharedBuffer, false, mockDeviceBitfield);
    auto sharedAllocation = memoryManager.createGraphicsAllocationFromSharedHandle(osHandleData, properties, true, false, true, nullptr);
    EXPECT_NE(nullptr, sharedAllocation);
    EXPECT_TRUE(sharedAllocation->is32BitAllocation());
    EXPECT_FALSE(sharedAllocation->isCoherent());
    EXPECT_NE(nullptr, sharedAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, sharedAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(MemoryPool::systemCpuInaccessible, sharedAllocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(sharedAllocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenLockUnlockCalledThenReturnCpuPtr) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{0, MemoryConstants::pageSize});
    ASSERT_NE(nullptr, allocation);

    EXPECT_FALSE(allocation->isLocked());
    auto ptr = memoryManager.lockResource(allocation);
    EXPECT_EQ(ptrOffset(allocation->getUnderlyingBuffer(), static_cast<size_t>(allocation->getAllocationOffset())), ptr);
    EXPECT_TRUE(allocation->isLocked());
    memoryManager.unlockResource(allocation);
    EXPECT_FALSE(allocation->isLocked());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenGraphicsAllocationContainsOffsetWhenAddressIsObtainedThenOffsetIsAdded) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);

    auto graphicsAllocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{0, MemoryConstants::pageSize});

    auto graphicsAddress = graphicsAllocation->getGpuAddress();
    auto graphicsAddressToPatch = graphicsAllocation->getGpuAddressToPatch();

    graphicsAllocation->setAllocationOffset(4);

    auto offsetGraphicsAddress = graphicsAllocation->getGpuAddress();
    auto offsetGraphicsAddressToPatch = graphicsAllocation->getGpuAddressToPatch();

    EXPECT_EQ(offsetGraphicsAddress, graphicsAddress + graphicsAllocation->getAllocationOffset());
    EXPECT_EQ(offsetGraphicsAddressToPatch, graphicsAddressToPatch + graphicsAllocation->getAllocationOffset());

    memoryManager.freeGraphicsMemory(graphicsAllocation);
}

TEST(OsAgnosticMemoryManager, WhenPointerIsCreatedThenLeakIsDetected) {
    void *ptr = new int[10];
    EXPECT_NE(nullptr, ptr); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    MemoryManagement::fastLeaksDetectionMode = MemoryManagement::LeakDetectionMode::EXPECT_TO_LEAK;
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenAllocateMemoryWithNoAlignmentProvidedThenAllocationIsAlignedToPageSize) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);
    MockAllocationProperties properties(0, MemoryConstants::pageSize >> 1);
    properties.alignment = 0;
    auto ga = memoryManager.allocateGraphicsMemoryWithProperties(properties);
    uintptr_t ptr = reinterpret_cast<uintptr_t>(ga->getUnderlyingBuffer());
    ptr &= (MemoryConstants::allocationAlignment - 1);
    EXPECT_EQ(ptr, 0u);

    memoryManager.freeGraphicsMemory(ga);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenAllocateMemoryWithAlignmentNotAlignedToPageSizeThenAlignmentIsAlignedUp) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);
    MockAllocationProperties properties(0, MemoryConstants::pageSize >> 1);
    properties.alignment = MemoryConstants::pageSize - 1;
    auto ga = memoryManager.allocateGraphicsMemoryWithProperties(properties);
    uintptr_t ptr = reinterpret_cast<uintptr_t>(ga->getUnderlyingBuffer());
    ptr &= (MemoryConstants::allocationAlignment - 1);
    EXPECT_EQ(ptr, 0u);

    memoryManager.freeGraphicsMemory(ga);
}

TEST(OsAgnosticMemoryManager, givenCommonMemoryManagerWhenIsAskedIfApplicationMemoryBudgetIsExhaustedThenFalseIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    EXPECT_FALSE(memoryManager.isMemoryBudgetExhausted());
}

TEST(OsAgnosticMemoryManager, givenDebugModuleAreaTypeWhenCreatingAllocationThen32BitAllocationWithFrontWindowGpuVaIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);
    const auto size = MemoryConstants::pageSize64k;

    NEO::AllocationProperties properties{0, true, size,
                                         NEO::AllocationType::debugModuleArea,
                                         false,
                                         mockDeviceBitfield};

    auto moduleDebugArea = memoryManager.allocateGraphicsMemoryWithProperties(properties);

    EXPECT_NE(nullptr, moduleDebugArea);
    EXPECT_NE(nullptr, moduleDebugArea->getUnderlyingBuffer());
    EXPECT_GE(moduleDebugArea->getUnderlyingBufferSize(), size);

    auto address64bit = moduleDebugArea->getGpuAddressToPatch();
    EXPECT_LT(address64bit, MemoryConstants::max32BitAddress);
    EXPECT_TRUE(moduleDebugArea->is32BitAllocation());

    auto gmmHelper = memoryManager.getGmmHelper(moduleDebugArea->getRootDeviceIndex());
    auto frontWindowBase = gmmHelper->canonize(memoryManager.getGfxPartition(moduleDebugArea->getRootDeviceIndex())->getHeapBase(memoryManager.selectInternalHeap(moduleDebugArea->isAllocatedInLocalMemoryPool())));
    EXPECT_EQ(frontWindowBase, moduleDebugArea->getGpuBaseAddress());
    EXPECT_EQ(frontWindowBase, moduleDebugArea->getGpuAddress());

    memoryManager.freeGraphicsMemory(moduleDebugArea);
}

TEST(OsAgnosticMemoryManager, givenLocalMemoryAndDebugModuleAreaTypeWhenCreatingAllocationThenAllocationWithFrontWindowGpuVaIsReturned) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrLocalMemory = true;
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableLocalMemory.set(true);

    MockExecutionEnvironment executionEnvironment(&hwInfo);
    auto &gfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    if (!gfxCoreHelper.isLocalMemoryEnabled(hwInfo)) {
        GTEST_SKIP();
    }

    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, true, executionEnvironment);
    const auto size = MemoryConstants::pageSize64k;

    NEO::AllocationProperties properties{0, true, size,
                                         NEO::AllocationType::debugModuleArea,
                                         false,
                                         mockDeviceBitfield};

    auto moduleDebugArea = memoryManager.allocateGraphicsMemoryWithProperties(properties);

    EXPECT_NE(nullptr, moduleDebugArea);
    EXPECT_NE(nullptr, moduleDebugArea->getUnderlyingBuffer());
    EXPECT_GE(moduleDebugArea->getUnderlyingBufferSize(), size);

    auto gmmHelper = memoryManager.getGmmHelper(moduleDebugArea->getRootDeviceIndex());
    auto frontWindowBase = gmmHelper->canonize(memoryManager.getGfxPartition(moduleDebugArea->getRootDeviceIndex())->getHeapBase(memoryManager.selectInternalHeap(moduleDebugArea->isAllocatedInLocalMemoryPool())));
    EXPECT_EQ(frontWindowBase, moduleDebugArea->getGpuBaseAddress());
    EXPECT_EQ(frontWindowBase, moduleDebugArea->getGpuAddress());

    memoryManager.freeGraphicsMemory(moduleDebugArea);
}

TEST(OsAgnosticMemoryManager, givenEnabledLocalMemoryWhenAllocatingGraphicsMemoryForIsaInSystemMemoryThenBaseAddressIsEqualToInternalHeapBaseAddress) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo();
    hwInfo->featureTable.flags.ftrLocalMemory = true;

    MockMemoryManager memoryManager(false, true, executionEnvironment);

    AllocationData allocationData;
    allocationData.type = AllocationType::kernelIsa;
    allocationData.flags.useSystemMemory = 1;
    allocationData.size = 4096;

    auto allocation = memoryManager.allocateGraphicsMemory(allocationData);
    ASSERT_NE(nullptr, allocation);

    auto instructionHeapBaseAddress = memoryManager.getInternalHeapBaseAddress(0, false);

    auto gmmHelper = memoryManager.getGmmHelper(allocation->getRootDeviceIndex());
    EXPECT_EQ(instructionHeapBaseAddress, gmmHelper->decanonize(allocation->getGpuBaseAddress()));

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenForcedSystemMemoryForIsaAndEnabledLocalMemoryWhenAllocatingGraphicsMemoryThenBaseAddressIsEqualToInternalHeapBaseAddress) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.ForceSystemMemoryPlacement.set(1 << (static_cast<uint32_t>(AllocationType::kernelIsa) - 1));
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo();
    hwInfo->featureTable.flags.ftrLocalMemory = true;

    MockMemoryManager memoryManager(false, true, executionEnvironment);

    size_t kernelIsaSize = 4096;
    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties({0, kernelIsaSize, AllocationType::kernelIsa, 1});
    ASSERT_NE(nullptr, allocation);

    auto instructionHeapBaseAddress = memoryManager.getInternalHeapBaseAddress(0, false);
    auto gmmHelper = memoryManager.getGmmHelper(allocation->getRootDeviceIndex());
    EXPECT_EQ(instructionHeapBaseAddress, gmmHelper->decanonize(allocation->getGpuBaseAddress()));

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenDifferentIsaPaddingIncludedFlagValuesWhenAllocatingGraphicsMemoryForIsaThenUnderlyingBufferSizeMatchesExpectation) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.ForceSystemMemoryPlacement.set(1 << (static_cast<uint32_t>(AllocationType::kernelIsa) - 1));
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo();
    hwInfo->featureTable.flags.ftrLocalMemory = true;
    auto &gfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    const auto isaPadding = gfxCoreHelper.getPaddingForISAAllocation();

    MockMemoryManager memoryManager(false, true, executionEnvironment);

    size_t kernelIsaSize = 4096;
    AllocationProperties allocProperties = {0, kernelIsaSize, AllocationType::kernelIsa, 1};

    for (auto isaPaddingIncluded : {false, true}) {
        allocProperties.isaPaddingIncluded = isaPaddingIncluded;
        auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(allocProperties);
        ASSERT_NE(nullptr, allocation);

        if (isaPaddingIncluded) {
            EXPECT_EQ(kernelIsaSize, allocation->getUnderlyingBufferSize());
        } else {
            EXPECT_EQ(kernelIsaSize + isaPadding, allocation->getUnderlyingBufferSize());
        }

        memoryManager.freeGraphicsMemory(allocation);
    }
}

class MemoryManagerWithAsyncDeleterTest : public ::testing::Test {
  public:
    MemoryManagerWithAsyncDeleterTest() : memoryManager(false, false){};
    void SetUp() override {
        memoryManager.overrideAsyncDeleterFlag(true);
    }
    const uint32_t rootDeviceIndex = 0u;
    MockMemoryManager memoryManager;
};

TEST_F(MemoryManagerWithAsyncDeleterTest, givenMemoryManagerWhenWaitForDeletionsIsCalledThenDeferredDeleterIsNullptr) {
    auto deleter = new MockDeferredDeleter();
    memoryManager.setDeferredDeleter(deleter);
    deleter->expectDrainBlockingValue(false);
    EXPECT_EQ(deleter, memoryManager.getDeferredDeleter());
    EXPECT_EQ(0, deleter->drainCalled);
    memoryManager.waitForDeletions();
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
}

TEST_F(MemoryManagerWithAsyncDeleterTest, givenMemoryManagerWhenWaitForDeletionsIsCalledTwiceThenItDoesntCrash) {
    EXPECT_NE(nullptr, memoryManager.getDeferredDeleter());
    memoryManager.waitForDeletions();
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    memoryManager.waitForDeletions();
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
}

TEST_F(MemoryManagerWithAsyncDeleterTest, givenMemoryManagerWhenAllocateGraphicsMemoryIsCalledWithPtrAndDeleterIsNotNullptrThenDeletersQueueIsReleased) {
    MockDeferredDeleter *deleter = new MockDeferredDeleter();
    memoryManager.setDeferredDeleter(deleter);
    EXPECT_NE(nullptr, memoryManager.getDeferredDeleter());
    auto deletion = new MockDeferrableDeletion();
    deleter->DeferredDeleter::deferDeletion(deletion);
    EXPECT_FALSE(deleter->isQueueEmpty());

    char ptr[128];
    EXPECT_EQ(0, deleter->drainCalled);
    deleter->expectDrainBlockingValue(true);
    auto allocation = memoryManager.MemoryManager::allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, false, sizeof(char)}, ptr);
    EXPECT_TRUE(deleter->isQueueEmpty());
    memoryManager.freeGraphicsMemoryImpl(allocation);
}

TEST_F(MemoryManagerWithAsyncDeleterTest, givenMemoryManagerWhenAllocateGraphicsMemoryIsCalledWithPtrAndDeleterIsNullptrThenItDoesntCrash) {
    memoryManager.setDeferredDeleter(nullptr);
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    char ptr[128];
    auto allocation = memoryManager.MemoryManager::allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, false, sizeof(char)}, ptr);
    memoryManager.freeGraphicsMemoryImpl(allocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenIsAsyncDeleterEnabledCalledThenReturnsValueOfFlag) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(executionEnvironment);
    memoryManager.overrideAsyncDeleterFlag(false);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    memoryManager.overrideAsyncDeleterFlag(true);
    EXPECT_TRUE(memoryManager.isAsyncDeleterEnabled());
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenItIsCreatedThenAsyncDeleterEnabledIsFalse) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
}

TEST(OsAgnosticMemoryManager, givenEnabledAsyncDeleterFlagWhenMemoryManagerIsCreatedThenAsyncDeleterEnabledIsFalseAndDeleterIsNullptr) {
    bool defaultEnableDeferredDeleterFlag = debugManager.flags.EnableDeferredDeleter.get();
    debugManager.flags.EnableDeferredDeleter.set(true);
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    debugManager.flags.EnableDeferredDeleter.set(defaultEnableDeferredDeleterFlag);
}

TEST(OsAgnosticMemoryManager, givenDisabledAsyncDeleterFlagWhenMemoryManagerIsCreatedThenAsyncDeleterEnabledIsFalseAndDeleterIsNullptr) {
    bool defaultEnableDeferredDeleterFlag = debugManager.flags.EnableDeferredDeleter.get();
    debugManager.flags.EnableDeferredDeleter.set(false);
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    debugManager.flags.EnableDeferredDeleter.set(defaultEnableDeferredDeleterFlag);
}

TEST(OsAgnosticMemoryManager, GivenEnabled64kbPagesWhenHostMemoryAllocationIsCreatedThenAlignedto64KbAllocationIsReturned) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.Enable64kbpages.set(true);
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(true, false, executionEnvironment);
    if (memoryManager.isLimitedGPU(0)) {
        GTEST_SKIP();
    }

    GraphicsAllocation *galloc = memoryManager.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, MemoryConstants::pageSize64k, AllocationType::bufferHostMemory, mockDeviceBitfield});
    EXPECT_NE(nullptr, galloc);
    memoryManager.freeGraphicsMemory(galloc);

    galloc = memoryManager.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, MemoryConstants::pageSize64k, AllocationType::bufferHostMemory, mockDeviceBitfield});
    EXPECT_NE(nullptr, galloc);
    EXPECT_NE(nullptr, galloc->getUnderlyingBuffer());
    size_t size = MemoryConstants::pageSize;
    EXPECT_EQ(0u, (uintptr_t)galloc->getUnderlyingBuffer() % size);

    EXPECT_NE(0u, galloc->getGpuAddress());
    EXPECT_EQ(0u, (uintptr_t)galloc->getGpuAddress() % size);
    memoryManager.freeGraphicsMemory(galloc);
}

TEST(OsAgnosticMemoryManager, givenPointerAndSizeWhenCreateInternalAllocationIsCalledThenGraphicsAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    auto ptr = (void *)0x100000;
    size_t allocationSize = 4096;
    auto graphicsAllocation = memoryManager.allocate32BitGraphicsMemory(mockRootDeviceIndex, allocationSize, ptr, AllocationType::internalHeap);
    EXPECT_EQ(ptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(allocationSize, graphicsAllocation->getUnderlyingBufferSize());
    memoryManager.freeGraphicsMemory(graphicsAllocation);
}

TEST(OsAgnosticMemoryManager, givenOsAgnosticMemoryManagerWhenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledThenAllocationIsCreated) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(executionEnvironment);
    AllocationData allocationData;
    allocationData.size = 13;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x5001);
    auto allocation = memoryManager.allocateGraphicsMemoryForNonSvmHostPtr(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(13u, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(1u, allocation->getAllocationOffset());

    memoryManager.freeGraphicsMemory(allocation);
}
using OsAgnosticMemoryManagerWithParams = ::testing::TestWithParam<bool>;

TEST_P(OsAgnosticMemoryManagerWithParams, givenReducedGpuAddressSpaceWhenAllocateGraphicsMemoryForHostPtrIsCalledThenAllocationWithoutFragmentsIsCreated) {
    bool requiresL3Flush = GetParam();
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    if (executionEnvironment.rootDeviceEnvironments[0]->isFullRangeSvm() || is32bit) {
        return;
    }
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    auto hostPtr = reinterpret_cast<const void *>(0x5001);
    AllocationProperties properties{0, false, 13, AllocationType::externalHostPtr, false, mockDeviceBitfield};
    properties.flags.flushL3RequiredForRead = properties.flags.flushL3RequiredForWrite = requiresL3Flush;
    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(properties, hostPtr);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(0u, allocation->fragmentsStorage.fragmentCount);
    EXPECT_EQ(requiresL3Flush, allocation->isFlushL3Required());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST_P(OsAgnosticMemoryManagerWithParams, givenDisabledHostPtrTrackingWhenAllocateGraphicsMemoryForHostPtrIsCalledThenAllocationWithoutFragmentsIsCreated) {
    if (is32bit) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restore;
    debugManager.flags.EnableHostPtrTracking.set(0);

    bool requiresL3Flush = GetParam();
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    if (!executionEnvironment.rootDeviceEnvironments[0]->isFullRangeSvm()) {
        return;
    }
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    auto hostPtr = reinterpret_cast<const void *>(0x5001);

    AllocationProperties properties{0, false, 13, AllocationType::externalHostPtr, false, mockDeviceBitfield};
    properties.flags.flushL3RequiredForRead = properties.flags.flushL3RequiredForWrite = requiresL3Flush;
    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(properties, hostPtr);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(0u, allocation->fragmentsStorage.fragmentCount);
    EXPECT_EQ(requiresL3Flush, allocation->isFlushL3Required());

    memoryManager.freeGraphicsMemory(allocation);
}

INSTANTIATE_TEST_SUITE_P(OsAgnosticMemoryManagerWithParams,
                         OsAgnosticMemoryManagerWithParams,
                         ::testing::Values(false, true));

TEST(OsAgnosticMemoryManager, givenFreeMemoryEnabledAndNonExternalHostPtrAllocationWhenGraphicsAllocationIsDestroyedThenFreeMemoryOnAubManagerShouldBeCalled) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableFreeMemory.set(true);
    MockExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    MockAubManager *mockManager = new MockAubManager();
    MockAubCenter *mockAubCenter = new MockAubCenter(*executionEnvironment.rootDeviceEnvironments[0], false, "file_name.aub", CommandStreamReceiverType::aub);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);
    executionEnvironment.rootDeviceEnvironments[0]->aubCenter.reset(mockAubCenter);
    auto gmmHelper = executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper();

    {
        auto gfxAllocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{0, MemoryConstants::pageSize});
        EXPECT_FALSE(mockManager->freeMemoryCalled);

        gfxAllocation->setCpuPtrAndGpuAddress(gfxAllocation->getUnderlyingBuffer(), 1ull << (gmmHelper->getAddressWidth() - 1));
        auto canonizedGpuAddress = gmmHelper->canonize(gfxAllocation->getGpuAddress());
        auto decanonizedGpuAddress = gmmHelper->decanonize(canonizedGpuAddress);

        memoryManager.freeGraphicsMemory(gfxAllocation);
        EXPECT_TRUE(mockManager->freeMemoryCalled);

        EXPECT_NE(canonizedGpuAddress, decanonizedGpuAddress);
        EXPECT_EQ(decanonizedGpuAddress, mockManager->freedGfxAddress);
    }

    mockManager->freeMemoryCalled = false;
    mockManager->freedGfxAddress = 0;

    {
        MockAllocationProperties properties{0, MemoryConstants::pageSize};
        properties.allocationType = AllocationType::externalHostPtr;

        auto gfxAllocation = memoryManager.allocateGraphicsMemoryWithProperties(properties);
        EXPECT_FALSE(mockManager->freeMemoryCalled);

        memoryManager.freeGraphicsMemory(gfxAllocation);
        EXPECT_FALSE(mockManager->freeMemoryCalled);
        EXPECT_EQ(0u, mockManager->freedGfxAddress);
    }
}

TEST(OsAgnosticMemoryManager, givenOsAgnosticMemoryManagerAndFreeMemoryDisabledWhenGraphicsAllocationIsDestroyedThenFreeMemoryOnAubManagerShouldBeCalled) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableFreeMemory.set(false);
    MockExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    MockAubManager *mockManager = new MockAubManager();
    MockAubCenter *mockAubCenter = new MockAubCenter(*executionEnvironment.rootDeviceEnvironments[0], false, "file_name.aub", CommandStreamReceiverType::aub);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);
    executionEnvironment.rootDeviceEnvironments[0]->aubCenter.reset(mockAubCenter);

    auto gfxAllocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{0, MemoryConstants::pageSize});
    EXPECT_FALSE(mockManager->freeMemoryCalled);
    memoryManager.freeGraphicsMemory(gfxAllocation);
    EXPECT_FALSE(mockManager->freeMemoryCalled);
}

TEST(OsAgnosticMemoryManager, givenOsAgnosticMemoryManagerWhenCheckedForIndirectAllocationsAsPackSupportThenTrueIsReturned) {
    MockExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    EXPECT_TRUE(memoryManager.allowIndirectAllocationsAsPack(0u));
}

TEST(OsAgnosticMemoryManager, givenOsAgnosticMemoryManagerWhenVerifyHandleThenReturnTrue) {
    MockExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    osHandle testOSHandle = 1;
    EXPECT_TRUE(memoryManager.verifyHandle(testOSHandle, 0, 0));
}

TEST(OsAgnosticMemoryManager, givenOsAgnosticMemoryManagerWhenisNTHandleThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    osHandle testOSHandle = 1;
    EXPECT_FALSE(memoryManager.isNTHandle(testOSHandle, 0));
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenGpuAddressIsSetThenAllocationWithSpecifiedGpuAddressInSystemMemoryIsCreated) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    auto memoryManager = new OsAgnosticMemoryManager(executionEnvironment);
    if (memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    DeviceBitfield deviceBitfield(1);
    std::unique_ptr<CommandStreamReceiver> csr(createCommandStream(executionEnvironment, 0u, deviceBitfield));
    executionEnvironment.memoryManager.reset(memoryManager);
    auto &gfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    auto osContext = memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment.rootDeviceEnvironments[0])[0],
                                                                                                                       PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));

    MockAllocationProperties properties = {0, MemoryConstants::pageSize};
    properties.osContext = osContext;
    properties.gpuAddress = 0x2000;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    EXPECT_EQ(MemoryPool::system4KBPages, allocation->getMemoryPool());
    EXPECT_EQ(0x2000u, allocation->getGpuAddress());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenOsAgnosticMemoryManagerWhenOsEnabled64kbPagesFalseThenIs64kbPagesEnabledReturnTrue) {
    MockExecutionEnvironment executionEnvironment;
    VariableBackup<bool> osEnabled64kbPagesBackup(&OSInterface::osEnabled64kbPages);
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    auto hwInfo = *defaultHwInfo;
    OSInterface::osEnabled64kbPages = false;
    EXPECT_TRUE(memoryManager.is64kbPagesEnabled(&hwInfo));
    OSInterface::osEnabled64kbPages = true;
    EXPECT_TRUE(memoryManager.is64kbPagesEnabled(&hwInfo));
}

TEST(OsAgnosticMemoryManager, givenStartAddressAndSizeWhenReservingCpuAddressThenPageAlignedAddressRangeIsReturned) {
    MockExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    auto addressRange = memoryManager.reserveCpuAddress(0, 1234);
    EXPECT_NE(0u, addressRange.address);
    EXPECT_EQ(1234u, addressRange.size);
    EXPECT_EQ(0u, addressRange.address & (MemoryConstants::pageSize - 1));
    memoryManager.freeCpuAddress(addressRange);
}

TEST(MemoryManager, givenSharedResourceCopyWhenAllocatingGraphicsMemoryThenAllocateGraphicsMemoryForImageIsCalled) {
    MockExecutionEnvironment executionEnvironment{};
    MockMemoryManager memoryManager(false, true, executionEnvironment);
    ImageDescriptor imgDesc = {};
    imgDesc.imageHeight = 1;
    imgDesc.imageWidth = 1;
    imgDesc.imageType = ImageType::image2D;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    AllocationData allocationData;
    allocationData.imgInfo = &imgInfo;
    allocationData.type = AllocationType::sharedResourceCopy;

    auto imageAllocation = memoryManager.allocateGraphicsMemory(allocationData);
    EXPECT_NE(nullptr, imageAllocation);
    EXPECT_TRUE(memoryManager.allocateForImageCalled);
    memoryManager.freeGraphicsMemory(imageAllocation);
}

TEST(MemoryManager, givenImageAndLimitedGPUWhenAllocatingGraphicsMemoryThenAllocate32BitGraphicsMemoryImplIsNotCalled) {
    MockExecutionEnvironment executionEnvironment{};
    MockMemoryManager memoryManager(false, true, executionEnvironment);
    memoryManager.limitedGPU = true;
    ImageDescriptor imgDesc = {};
    imgDesc.imageHeight = 1;
    imgDesc.imageWidth = 1;
    imgDesc.imageType = ImageType::image2D;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    AllocationData allocationData;
    allocationData.imgInfo = &imgInfo;
    allocationData.type = AllocationType::image;

    auto imageAllocation = memoryManager.allocateGraphicsMemory(allocationData);
    EXPECT_NE(nullptr, imageAllocation);
    EXPECT_TRUE(memoryManager.allocateForImageCalled);
    EXPECT_FALSE(memoryManager.allocate32BitGraphicsMemoryImplCalled);
    memoryManager.freeGraphicsMemory(imageAllocation);
}

TEST(MemoryManager, givenBufferAndLimitedGPUWhenAllocatingGraphicsMemoryThenAllocate32BitGraphicsMemoryImplIsCalled) {
    MockExecutionEnvironment executionEnvironment{};
    MockMemoryManager memoryManager(false, true, executionEnvironment);
    memoryManager.limitedGPU = true;

    AllocationData allocationData;
    allocationData.size = 4096u;
    allocationData.type = AllocationType::buffer;

    auto bufferAllocation = memoryManager.allocateGraphicsMemory(allocationData);
    EXPECT_NE(nullptr, bufferAllocation);
    EXPECT_FALSE(memoryManager.allocateForImageCalled);
    EXPECT_TRUE(memoryManager.allocate32BitGraphicsMemoryImplCalled);
    memoryManager.freeGraphicsMemory(bufferAllocation);
}

TEST(MemoryManager, givenBufferHostMemoryWhenAllocatingGraphicsMemoryThenAllocateGraphicsMemoryForNonSvmHostPtrIsCalled) {
    MockExecutionEnvironment executionEnvironment{};
    HardwareInfo hwInfoLocal = *defaultHwInfo;
    executionEnvironment.rootDeviceEnvironments[0u]->setHwInfoAndInitHelpers(&hwInfoLocal);
    executionEnvironment.rootDeviceEnvironments[0u]->initGmm();

    MockMemoryManager memoryManager(false, true, executionEnvironment);
    char bufferData[4096]{};
    AllocationData allocationData{};
    allocationData.hostPtr = bufferData;
    allocationData.size = 4096u;
    allocationData.type = AllocationType::bufferHostMemory;

    auto bufferAllocation = memoryManager.allocateGraphicsMemory(allocationData);
    EXPECT_NE(nullptr, bufferAllocation);
    EXPECT_FALSE(memoryManager.allocateForImageCalled);
    if (!is32bit) {
        EXPECT_TRUE(memoryManager.allocateGraphicsMemoryForNonSvmHostPtrCalled);
    }
    memoryManager.freeGraphicsMemory(bufferAllocation);
}

TEST(MemoryManager, givenBufferHostMemoryAndForce32bitAllocationsWhenAllocatingGraphicsMemoryThenAllocateGraphicsMemoryForNonSvmHostPtrIsNotCalled) {
    MockExecutionEnvironment executionEnvironment{};
    HardwareInfo hwInfoLocal = *defaultHwInfo;
    executionEnvironment.rootDeviceEnvironments[0u]->setHwInfoAndInitHelpers(&hwInfoLocal);
    executionEnvironment.rootDeviceEnvironments[0u]->initGmm();

    MockMemoryManager memoryManager(false, true, executionEnvironment);
    memoryManager.setForce32BitAllocations(true);
    char bufferData[4096]{};
    AllocationData allocationData{};
    allocationData.hostPtr = bufferData;
    allocationData.size = 4096u;
    allocationData.type = AllocationType::bufferHostMemory;

    auto bufferAllocation = memoryManager.allocateGraphicsMemory(allocationData);
    EXPECT_NE(nullptr, bufferAllocation);
    EXPECT_FALSE(memoryManager.allocateGraphicsMemoryForNonSvmHostPtrCalled);
    memoryManager.freeGraphicsMemory(bufferAllocation);
}

TEST(MemoryManager, givenShareableWhenAllocatingGraphicsMemoryThenAllocateShareableIsCalled) {
    MockExecutionEnvironment executionEnvironment{};
    MockMemoryManager memoryManager(false, true, executionEnvironment);

    AllocationData allocationData;
    allocationData.size = 4096u;
    allocationData.type = AllocationType::buffer;
    allocationData.flags.shareable = true;

    auto allocation = memoryManager.allocateGraphicsMemory(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_TRUE(memoryManager.allocateForShareableCalled);
    memoryManager.freeGraphicsMemory(allocation);
}

TEST_F(MemoryAllocatorTest, GivenSizeWhenGmmIsCreatedThenNonNullPointerIsReturned) {
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    Gmm *gmm = new Gmm(device->getGmmHelper(), nullptr, 65536, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    EXPECT_NE(nullptr, gmm);
    delete gmm;
}

typedef Test<MemoryManagerWithCsrFixture> MemoryManagerWithCsrTest;

TEST_F(MemoryManagerWithCsrTest, GivenAllocationsInHostPtrManagerWhenBiggerOverllapingAllocationIsCreatedAndNothingToCleanThenAbortExecution) {
    void *cpuPtr1 = (void *)0x100004;
    void *cpuPtr2 = (void *)0x101008;
    void *cpuPtr3 = (void *)0x100000;

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());
    auto graphicsAllocation1 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, MemoryConstants::pageSize}, cpuPtr1);
    EXPECT_EQ(2u, hostPtrManager->getFragmentCount());

    auto graphicsAllocation2 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, MemoryConstants::pageSize * 3}, cpuPtr2);
    EXPECT_EQ(4u, hostPtrManager->getFragmentCount());

    GraphicsAllocation *graphicsAllocation3 = nullptr;

    EXPECT_ANY_THROW(graphicsAllocation3 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, MemoryConstants::pageSize * 10}, cpuPtr3));

    EXPECT_NE(nullptr, graphicsAllocation1);
    EXPECT_NE(nullptr, graphicsAllocation2);
    EXPECT_EQ(nullptr, graphicsAllocation3);

    EXPECT_EQ((uintptr_t)cpuPtr1 & ~MemoryConstants::pageMask, (uintptr_t)graphicsAllocation1->fragmentsStorage.fragmentStorageData[0].cpuPtr);
    EXPECT_EQ((uintptr_t)cpuPtr2 & ~MemoryConstants::pageMask, (uintptr_t)graphicsAllocation2->fragmentsStorage.fragmentStorageData[0].cpuPtr);
    EXPECT_EQ(((uintptr_t)cpuPtr2 + MemoryConstants::pageSize) & ~MemoryConstants::pageMask, (uintptr_t)graphicsAllocation2->fragmentsStorage.fragmentStorageData[1].cpuPtr);

    memoryManager->freeGraphicsMemory(graphicsAllocation1);
    memoryManager->freeGraphicsMemory(graphicsAllocation2);
    memoryManager->freeGraphicsMemory(graphicsAllocation3);
}

TEST_F(MemoryManagerWithCsrTest, GivenAllocationsInHostPtrManagerReadyForCleaningWhenBiggerOverllapingAllocationIsCreatedThenTemporaryAllocationsAreCleaned) {

    void *cpuPtr1 = (void *)0x100004;
    void *cpuPtr2 = (void *)0x101008;
    void *cpuPtr3 = (void *)0x100000;

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());
    auto graphicsAllocation1 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, MemoryConstants::pageSize}, cpuPtr1);
    EXPECT_EQ(2u, hostPtrManager->getFragmentCount());

    auto graphicsAllocation2 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, MemoryConstants::pageSize * 3}, cpuPtr2);
    EXPECT_EQ(4u, hostPtrManager->getFragmentCount());

    EXPECT_NE(nullptr, graphicsAllocation1);
    EXPECT_NE(nullptr, graphicsAllocation2);

    auto fragment1 = hostPtrManager->getFragment({alignDown(cpuPtr1, MemoryConstants::pageSize), csr->getRootDeviceIndex()});
    EXPECT_NE(nullptr, fragment1);
    auto fragment2 = hostPtrManager->getFragment({alignUp(cpuPtr1, MemoryConstants::pageSize), csr->getRootDeviceIndex()});
    EXPECT_NE(nullptr, fragment2);
    auto fragment3 = hostPtrManager->getFragment({alignDown(cpuPtr2, MemoryConstants::pageSize), csr->getRootDeviceIndex()});
    EXPECT_NE(nullptr, fragment3);
    auto fragment4 = hostPtrManager->getFragment({alignUp(cpuPtr2, MemoryConstants::pageSize), csr->getRootDeviceIndex()});
    EXPECT_NE(nullptr, fragment4);

    TaskCountType taskCountReady = 1;
    auto storage = csr->getInternalAllocationStorage();
    storage->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(graphicsAllocation1), TEMPORARY_ALLOCATION, taskCountReady);
    storage->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(graphicsAllocation2), TEMPORARY_ALLOCATION, taskCountReady);

    EXPECT_EQ(4u, hostPtrManager->getFragmentCount());

    // All fragments ready for release
    taskCount = taskCountReady;
    csr->latestSentTaskCount = taskCountReady;

    auto graphicsAllocation3 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, MemoryConstants::pageSize * 10}, cpuPtr3);

    EXPECT_NE(nullptr, graphicsAllocation3);

    // no more overlapping allocation, previous allocations cleaned
    EXPECT_EQ(1u, graphicsAllocation3->fragmentsStorage.fragmentCount);
    EXPECT_EQ(cpuPtr3, graphicsAllocation3->fragmentsStorage.fragmentStorageData[0].cpuPtr);

    memoryManager->freeGraphicsMemory(graphicsAllocation3);
}

TEST_F(MemoryManagerWithCsrTest, givenAllocationThatWasNotUsedWhencheckGpuUsageAndDestroyGraphicsAllocationsIsCalledThenItIsDestroyedInPlace) {
    auto notUsedAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(notUsedAllocation);
    EXPECT_TRUE(csr->getTemporaryAllocations().peekIsEmpty());
}

TEST_F(MemoryManagerWithCsrTest, givenAllocationThatWasUsedAndIsCompletedWhencheckGpuUsageAndDestroyGraphicsAllocationsIsCalledThenItIsDestroyedInPlace) {
    auto usedAllocationButGpuCompleted = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});

    auto tagAddress = csr->getTagAddress();
    ASSERT_NE(0u, *tagAddress);

    usedAllocationButGpuCompleted->updateTaskCount(*tagAddress - 1, csr->getOsContext().getContextId());

    memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(usedAllocationButGpuCompleted);
    EXPECT_TRUE(csr->getTemporaryAllocations().peekIsEmpty());
}

TEST_F(MemoryManagerWithCsrTest, givenAllocationThatWasUsedAndIsNotCompletedWhencheckGpuUsageAndDestroyGraphicsAllocationsIsCalledThenItIsAddedToTemporaryAllocationList) {
    auto &gfxCoreHelper = csr->getGfxCoreHelper();
    csr->initializeTagAllocation();

    memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment.rootDeviceEnvironments[0])[0],
                                                                                                      PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));
    auto usedAllocationAndNotGpuCompleted = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});

    auto tagAddress = csr->getTagAddress();

    usedAllocationAndNotGpuCompleted->updateTaskCount(*tagAddress + 1, csr->getOsContext().getContextId());

    memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(usedAllocationAndNotGpuCompleted);
    EXPECT_TRUE(csr->getTemporaryAllocations().peekIsEmpty());
    EXPECT_FALSE(csr->getDeferredAllocations().peekIsEmpty());
    EXPECT_EQ(csr->getDeferredAllocations().peekHead(), usedAllocationAndNotGpuCompleted);

    // change task count so cleanup will not clear alloc in use
    usedAllocationAndNotGpuCompleted->updateTaskCount(csr->peekLatestFlushedTaskCount(), csr->getOsContext().getContextId());
}

struct MockAlignMallocMemoryManagerTest : public MemoryAllocatorTest {
    void SetUp() override {
        MemoryAllocatorTest::SetUp();

        alignedMemoryManager = new (std::nothrow) MockAlignMallocMemoryManager(*executionEnvironment);
        // assert we have memory manager
        ASSERT_NE(nullptr, memoryManager);
    }

    void TearDown() override {
        alignedMemoryManager->alignedFreeWrapper(nullptr);
        delete alignedMemoryManager;

        MemoryAllocatorTest::TearDown();
    }

    MockAlignMallocMemoryManager *alignedMemoryManager = nullptr;
};

TEST_F(MockAlignMallocMemoryManagerTest, givenMemoryManagerWhenNullAlignRestrictionsThenNotUseRestrictions) {
    EXPECT_EQ(nullptr, memoryManager->getAlignedMallocRestrictions());
    EXPECT_EQ(nullptr, alignedMemoryManager->getAlignedMallocRestrictions());

    uintptr_t expectedVal = MockAlignMallocMemoryManager::alignMallocMinAddress - MockAlignMallocMemoryManager::alignMallocStep;
    uintptr_t memVal = reinterpret_cast<uintptr_t>(alignedMemoryManager->allocateSystemMemory(0x1000, 0x1000));
    EXPECT_EQ(expectedVal, memVal);
}

TEST_F(MockAlignMallocMemoryManagerTest, givenMemoryManagerWhenZeroAlignRestrictionsThenNotUseRestrictions) {
    alignedMemoryManager->alignMallocRestrictions = &alignedMemoryManager->testMallocRestrictions;
    EXPECT_NE(nullptr, alignedMemoryManager->getAlignedMallocRestrictions());

    alignedMemoryManager->alignMallocCount = 0;
    uintptr_t expectedVal = MockAlignMallocMemoryManager::alignMallocMinAddress - MockAlignMallocMemoryManager::alignMallocStep;
    uintptr_t memVal = reinterpret_cast<uintptr_t>(alignedMemoryManager->allocateSystemMemory(0x1000, 0x1000));
    EXPECT_EQ(expectedVal, memVal);

    alignedMemoryManager->alignMallocCount = alignedMemoryManager->alignMallocMaxIter + 1;
    expectedVal = MockAlignMallocMemoryManager::alignMallocMinAddress + MockAlignMallocMemoryManager::alignMallocStep;
    memVal = reinterpret_cast<uintptr_t>(alignedMemoryManager->allocateSystemMemory(0x1000, 0x1000));
    EXPECT_EQ(expectedVal, memVal);
}

TEST_F(MockAlignMallocMemoryManagerTest, givenMemoryManagerWitNonZeroAlignRestrictionsWhenFirstGoodAddressThenUseRestrictionsAndReturnFirst) {
    alignedMemoryManager->alignMallocRestrictions = &alignedMemoryManager->testMallocRestrictions;
    alignedMemoryManager->testMallocRestrictions.minAddress = MockAlignMallocMemoryManager::alignMallocMinAddress;
    EXPECT_NE(nullptr, alignedMemoryManager->getAlignedMallocRestrictions());

    alignedMemoryManager->alignMallocCount = alignedMemoryManager->alignMallocMaxIter + 1;
    uintptr_t expectedVal = MockAlignMallocMemoryManager::alignMallocMinAddress + MockAlignMallocMemoryManager::alignMallocStep;
    uintptr_t memVal = reinterpret_cast<uintptr_t>(alignedMemoryManager->allocateSystemMemory(0x1000, 0x1000));
    EXPECT_EQ(expectedVal, memVal);
}

TEST_F(MockAlignMallocMemoryManagerTest, givenMemoryManagerWitNonZeroAlignRestrictionsWhenFirstNullAddressThenUseRestrictionsAndReturnFirstNull) {
    alignedMemoryManager->alignMallocRestrictions = &alignedMemoryManager->testMallocRestrictions;
    alignedMemoryManager->testMallocRestrictions.minAddress = MockAlignMallocMemoryManager::alignMallocMinAddress;
    EXPECT_NE(nullptr, alignedMemoryManager->getAlignedMallocRestrictions());

    alignedMemoryManager->alignMallocCount = alignedMemoryManager->alignMallocMaxIter + 1;
    alignedMemoryManager->returnNullGood = true;
    uintptr_t expectedVal = 0;
    uintptr_t memVal = reinterpret_cast<uintptr_t>(alignedMemoryManager->allocateSystemMemory(0x1000, 0x1000));
    EXPECT_EQ(expectedVal, memVal);
}

TEST_F(MockAlignMallocMemoryManagerTest, givenMemoryManagerWitNonZeroAlignRestrictionsWhenFirstBadAnotherGoodAddressThenUseRestrictionsAndReturnAnother) {
    alignedMemoryManager->alignMallocRestrictions = &alignedMemoryManager->testMallocRestrictions;
    alignedMemoryManager->testMallocRestrictions.minAddress = MockAlignMallocMemoryManager::alignMallocMinAddress;
    EXPECT_NE(nullptr, alignedMemoryManager->getAlignedMallocRestrictions());

    alignedMemoryManager->alignMallocCount = 0;
    uintptr_t expectedVal = MockAlignMallocMemoryManager::alignMallocMinAddress + MockAlignMallocMemoryManager::alignMallocStep;
    uintptr_t memVal = reinterpret_cast<uintptr_t>(alignedMemoryManager->allocateSystemMemory(0x1000, 0x1000));
    EXPECT_EQ(expectedVal, memVal);
}

TEST_F(MockAlignMallocMemoryManagerTest, givenMemoryManagerWitNonZeroAlignRestrictionsWhenFirstBadAnotherNullAddressThenUseRestrictionsAndReturnNull) {
    alignedMemoryManager->alignMallocRestrictions = &alignedMemoryManager->testMallocRestrictions;
    alignedMemoryManager->testMallocRestrictions.minAddress = MockAlignMallocMemoryManager::alignMallocMinAddress;
    EXPECT_NE(nullptr, alignedMemoryManager->getAlignedMallocRestrictions());

    alignedMemoryManager->alignMallocCount = 0;
    alignedMemoryManager->returnNullGood = true;
    uintptr_t expectedVal = 0;
    uintptr_t memVal = reinterpret_cast<uintptr_t>(alignedMemoryManager->allocateSystemMemory(0x1000, 0x1000));
    EXPECT_EQ(expectedVal, memVal);
}

TEST(GraphicsAllocation, givenCpuPointerBasedConstructorWhenGraphicsAllocationIsCreatedThenGpuAddressHasCorrectValue) {
    uintptr_t address = 0xf0000000;
    void *addressWithTrailingBitSet = reinterpret_cast<void *>(address);
    uint64_t expectedGpuAddress = 0xf0000000;
    MockGraphicsAllocation graphicsAllocation(addressWithTrailingBitSet, 1u);
    EXPECT_EQ(expectedGpuAddress, graphicsAllocation.getGpuAddress());
}

using GraphicsAllocationDestroyTests = ::testing::Test;

HWTEST_F(GraphicsAllocationDestroyTests, givenAllocationUsedOnlyByNonDefaultCsrWhenCheckingUsageBeforeDestroyThenStoreItAsTemporaryAllocation) {
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    auto &lowPriorityEngine = device->getEngine(device->getHardwareInfo().capabilityTable.defaultEngineType, EngineUsage::lowPriority);

    auto nonDefaultOsContext = lowPriorityEngine.osContext;
    auto nonDefaultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(lowPriorityEngine.commandStreamReceiver);

    auto memoryManager = device->getExecutionEnvironment()->memoryManager.get();
    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    nonDefaultCsr->taskCount = *nonDefaultCsr->getTagAddress() + 1;
    nonDefaultCsr->latestFlushedTaskCount = *nonDefaultCsr->getTagAddress() + 1;
    graphicsAllocation->updateTaskCount(*nonDefaultCsr->getTagAddress() + 1, nonDefaultOsContext->getContextId());

    memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(graphicsAllocation);
    EXPECT_NE(nullptr, nonDefaultCsr->getInternalAllocationStorage()->getDeferredAllocations().peekHead());
    auto newTag = *nonDefaultCsr->getTagAddress() + 1;
    (*nonDefaultCsr->getTagAddress()) = newTag;
    // no need to call freeGraphicsAllocation
}

HWTEST_F(GraphicsAllocationDestroyTests, givenAllocationUsedOnlyByNonDefaultDeviceWhenCheckingUsageBeforeDestroyThenStoreItAsTemporaryAllocation) {
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    auto &defaultCommandStreamReceiver = device->getGpgpuCommandStreamReceiver();
    auto &nonDefaultCommandStreamReceiver = static_cast<UltCommandStreamReceiver<FamilyType> &>(*device->commandStreamReceivers[1]);
    auto memoryManager = device->getMemoryManager();
    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    auto notReadyTaskCount = *nonDefaultCommandStreamReceiver.getTagAddress() + 1;

    EXPECT_NE(defaultCommandStreamReceiver.getOsContext().getContextId(), nonDefaultCommandStreamReceiver.getOsContext().getContextId());

    nonDefaultCommandStreamReceiver.taskCount = notReadyTaskCount;
    nonDefaultCommandStreamReceiver.latestFlushedTaskCount = notReadyTaskCount;
    graphicsAllocation->updateTaskCount(notReadyTaskCount, nonDefaultCommandStreamReceiver.getOsContext().getContextId());

    EXPECT_TRUE(nonDefaultCommandStreamReceiver.getInternalAllocationStorage()->getDeferredAllocations().peekIsEmpty());
    EXPECT_TRUE(nonDefaultCommandStreamReceiver.getInternalAllocationStorage()->getTemporaryAllocations().peekIsEmpty());
    memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(graphicsAllocation);
    EXPECT_FALSE(nonDefaultCommandStreamReceiver.getInternalAllocationStorage()->getDeferredAllocations().peekIsEmpty());
    EXPECT_TRUE(nonDefaultCommandStreamReceiver.getInternalAllocationStorage()->getTemporaryAllocations().peekIsEmpty());
    auto newTag = *nonDefaultCommandStreamReceiver.getTagAddress() + 1;
    (*nonDefaultCommandStreamReceiver.getTagAddress()) = newTag;
    // no need to call freeGraphicsAllocation
}

TEST(GraphicsAllocation, givenSharedHandleBasedConstructorWhenGraphicsAllocationIsCreatedThenGpuAddressHasCorrectValue) {
    uintptr_t address = 0xf0000000;
    void *addressWithTrailingBitSet = reinterpret_cast<void *>(address);
    uint64_t expectedGpuAddress = 0xf0000000;
    osHandle sharedHandle{};
    MockExecutionEnvironment executionEnvironment{};
    auto gmmHelper = executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(addressWithTrailingBitSet));
    GraphicsAllocation graphicsAllocation(0, 1u /*num gmms*/, AllocationType::unknown, addressWithTrailingBitSet, 1u, sharedHandle, MemoryPool::memoryNull, 0u, canonizedGpuAddress);
    EXPECT_EQ(expectedGpuAddress, graphicsAllocation.getGpuAddress());
}

TEST(MemoryManagerRegisteredEnginesTest, givenOsContextWhenItIsUnregisteredFromMemoryManagerThenRefCountDecreases) {
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    auto memoryManager = device->getMemoryManager();
    auto &engine = device->getDefaultEngine();

    engine.osContext->incRefInternal();
    EXPECT_EQ(2, engine.osContext->getRefInternalCount());

    memoryManager->unregisterEngineForCsr(engine.commandStreamReceiver);
    EXPECT_EQ(1, engine.osContext->getRefInternalCount());

    engine.osContext->decRefInternal();
}

TEST(ResidencyDataTest, givenDeviceBitfieldWhenCreatingOsContextThenSetValidValue) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    auto memoryManager = new MockMemoryManager(false, false, executionEnvironment);
    executionEnvironment.memoryManager.reset(memoryManager);
    DeviceBitfield deviceBitfield(0b11);
    std::unique_ptr<CommandStreamReceiver> csr(createCommandStream(executionEnvironment, 0u, deviceBitfield));
    PreemptionMode preemptionMode = PreemptionMode::MidThread;
    auto &gfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment.rootDeviceEnvironments[0])[0],
                                                                                                      preemptionMode,
                                                                                                      deviceBitfield));
    auto &engine = memoryManager->getRegisteredEngines(0u)[0];
    EXPECT_EQ(2u, engine.osContext->getNumSupportedDevices());
    EXPECT_EQ(deviceBitfield, engine.osContext->getDeviceBitfield());
    EXPECT_EQ(preemptionMode, engine.osContext->getPreemptionMode());
}

TEST(ResidencyDataTest, givenTwoOsContextsWhenTheyAreRegisteredFromHigherToLowerThenProperSizeIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get(), true, 2u);
    auto memoryManager = new MockMemoryManager(false, false, executionEnvironment);
    executionEnvironment.memoryManager.reset(memoryManager);
    DeviceBitfield deviceBitfield(1);
    std::unique_ptr<CommandStreamReceiver> csr(createCommandStream(executionEnvironment, 0u, deviceBitfield));
    std::unique_ptr<CommandStreamReceiver> csr1(createCommandStream(executionEnvironment, 1u, deviceBitfield));
    auto &gfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment.rootDeviceEnvironments[0])[0],
                                                                                                      PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));
    memoryManager->createAndRegisterOsContext(csr1.get(), EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment.rootDeviceEnvironments[0])[0],
                                                                                                       PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));
    EXPECT_EQ(3u, memoryManager->allRegisteredEngines.size());
    EXPECT_EQ(1u, memoryManager->allRegisteredEngines[0].size());
    EXPECT_EQ(1u, memoryManager->allRegisteredEngines[1].size());
    EXPECT_EQ(1, memoryManager->allRegisteredEngines[0][0].osContext->getRefInternalCount());
    EXPECT_EQ(1, memoryManager->allRegisteredEngines[1][0].osContext->getRefInternalCount());
}

TEST(ResidencyDataTest, givenGpgpuEnginesWhenAskedForMaxOsContextCountThenValueIsGreaterOrEqual) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(*mockExecutionEnvironment.rootDeviceEnvironments[0]);
    EXPECT_TRUE(MemoryManager::maxOsContextCount >= engines.size());
}

TEST(ResidencyDataTest, givenResidencyDataWhenUpdateCompletionDataIsCalledThenItIsProperlyUpdated) {
    struct MockResidencyData : public ResidencyData {
        using ResidencyData::lastFenceValues;
        using ResidencyData::ResidencyData;
    };
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();

    MockResidencyData residency(MemoryManager::maxOsContextCount);

    MockOsContext osContext(0u, EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*mockExecutionEnvironment.rootDeviceEnvironments[0])[0],
                                                                             PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));
    MockOsContext osContext2(1u, EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*mockExecutionEnvironment.rootDeviceEnvironments[0])[0],
                                                                              PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));

    auto lastFenceValue = 45llu;
    auto lastFenceValue2 = 23llu;
    auto lastFenceValue3 = 373llu;

    EXPECT_EQ(MemoryManager::maxOsContextCount, residency.lastFenceValues.size());

    residency.updateCompletionData(lastFenceValue, osContext.getContextId());
    EXPECT_EQ(MemoryManager::maxOsContextCount, residency.lastFenceValues.size());
    EXPECT_EQ(lastFenceValue, residency.lastFenceValues[0]);
    EXPECT_EQ(lastFenceValue, residency.getFenceValueForContextId(osContext.getContextId()));

    residency.updateCompletionData(lastFenceValue2, osContext2.getContextId());

    EXPECT_EQ(MemoryManager::maxOsContextCount, residency.lastFenceValues.size());
    EXPECT_EQ(lastFenceValue2, residency.lastFenceValues[1]);
    EXPECT_EQ(lastFenceValue2, residency.getFenceValueForContextId(osContext2.getContextId()));

    residency.updateCompletionData(lastFenceValue3, osContext2.getContextId());
    EXPECT_EQ(lastFenceValue3, residency.lastFenceValues[1]);
    EXPECT_EQ(lastFenceValue3, residency.getFenceValueForContextId(osContext2.getContextId()));
}

TEST(MemoryManagerTest, givenMemoryManagerWhenLockIsCalledOnLockedResourceThenDoesNothing) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{0, MemoryConstants::pageSize});

    EXPECT_FALSE(allocation->isLocked());
    auto ptr = memoryManager.MemoryManager::lockResource(allocation);
    EXPECT_TRUE(allocation->isLocked());
    EXPECT_EQ(1u, memoryManager.lockResourceCalled);
    EXPECT_EQ(0u, memoryManager.unlockResourceCalled);

    auto ptr2 = memoryManager.MemoryManager::lockResource(allocation);
    EXPECT_TRUE(allocation->isLocked());
    EXPECT_EQ(1u, memoryManager.lockResourceCalled);
    EXPECT_EQ(0u, memoryManager.unlockResourceCalled);

    EXPECT_EQ(ptr, ptr2);
    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenMemoryManagerWhenAllocationWasNotUnlockedThenItIsUnlockedDuringDestruction) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{0, MemoryConstants::pageSize});

    EXPECT_FALSE(allocation->isLocked());
    memoryManager.MemoryManager::lockResource(allocation);
    EXPECT_TRUE(allocation->isLocked());
    EXPECT_EQ(1u, memoryManager.lockResourceCalled);
    EXPECT_EQ(0u, memoryManager.unlockResourceCalled);

    memoryManager.freeGraphicsMemory(allocation);
    EXPECT_EQ(1u, memoryManager.unlockResourceCalled);
}

TEST(MemoryManagerTest, givenExecutionEnvironmentWithCleanedRootDeviceExecutionsWhenFreeGraphicsMemoryIsCalledThenMemoryManagerDoesntCrash) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{0, MemoryConstants::pageSize});

    EXPECT_NE(nullptr, allocation);

    executionEnvironment.rootDeviceEnvironments.clear();

    EXPECT_NO_THROW(memoryManager.freeGraphicsMemory(allocation));
}

TEST(MemoryManagerTest, givenAllocationTypesThatMayNeedL3FlushWhenCallingGetAllocationDataThenFlushL3FlagIsCorrectlySet) {
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, 1, AllocationType::unknown, mockDeviceBitfield);
    properties.flags.flushL3RequiredForRead = 1;
    properties.flags.flushL3RequiredForWrite = 1;

    AllocationType allocationTypesThatMayNeedL3Flush[] = {
        AllocationType::buffer,
        AllocationType::bufferHostMemory, AllocationType::externalHostPtr,
        AllocationType::globalSurface, AllocationType::image,
        AllocationType::sharedImage,
        AllocationType::sharedBuffer, AllocationType::sharedResourceCopy,
        AllocationType::svmZeroCopy, AllocationType::svmGpu,
        AllocationType::svmCpu, AllocationType::writeCombined,
        AllocationType::mapAllocation};

    MockMemoryManager mockMemoryManager;
    for (auto allocationType : allocationTypesThatMayNeedL3Flush) {
        properties.allocationType = allocationType;
        mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
        EXPECT_TRUE(allocData.flags.flushL3);
    }

    properties.flags.flushL3RequiredForRead = 0;
    properties.flags.flushL3RequiredForWrite = 0;

    for (auto allocationType : allocationTypesThatMayNeedL3Flush) {
        properties.allocationType = allocationType;
        mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
        EXPECT_FALSE(allocData.flags.flushL3);
    }
}

TEST(MemoryManagerTest, givenSystemMemoryBitfieldInAllocationPropertiesWhenGetAllocationDataThenUseSystemMemoryIsSet) {
    AllocationData allocData{};
    AllocationProperties properties(mockRootDeviceIndex, 1, AllocationType::buffer, systemMemoryBitfield);

    MockMemoryManager mockMemoryManager;
    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));

    EXPECT_TRUE(allocData.flags.useSystemMemory);
}

TEST(MemoryManagerTest, givenNullHostPtrWhenIsCopyRequiredIsCalledThenFalseIsReturned) {
    ImageInfo imgInfo{};
    EXPECT_FALSE(MockMemoryManager::isCopyRequired(imgInfo, nullptr));
}

class HeapSelectorTest : public ::Test<DeviceFixture> {
  public:
    using BaseClass = DeviceFixture;
    void SetUp() override {
        executionEnvironment = std::unique_ptr<MockExecutionEnvironment>(new MockExecutionEnvironment(defaultHwInfo.get()));
        memoryManager = std::unique_ptr<MockMemoryManager>(new MockMemoryManager(false, false, *executionEnvironment.get()));
        BaseClass::setUp();
    }
    void TearDown() override {
        BaseClass::tearDown();
    }
    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;
    std::unique_ptr<MockMemoryManager> memoryManager;
};

TEST_F(HeapSelectorTest, given32bitInternalAllocationWhenSelectingHeapThenInternalHeapIsUsed) {
    GraphicsAllocation allocation{0, 1u /*num gmms*/, AllocationType::kernelIsa, nullptr, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu};
    allocation.set32BitAllocation(true);
    EXPECT_EQ(MemoryManager::selectInternalHeap(allocation.isAllocatedInLocalMemoryPool()), memoryManager->selectHeap(&allocation, false, false, false));

    GraphicsAllocation allocation2{0, 1u /*num gmms*/, AllocationType::kernelIsaInternal, nullptr, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu};
    allocation2.set32BitAllocation(true);
    EXPECT_EQ(MemoryManager::selectInternalHeap(allocation2.isAllocatedInLocalMemoryPool()), memoryManager->selectHeap(&allocation2, false, false, false));
}

TEST_F(HeapSelectorTest, givenNon32bitInternalAllocationWhenSelectingHeapThenInternalHeapIsUsed) {
    GraphicsAllocation allocation{0, 1u /*num gmms*/, AllocationType::kernelIsa, nullptr, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu};
    allocation.set32BitAllocation(false);
    EXPECT_EQ(MemoryManager::selectInternalHeap(allocation.isAllocatedInLocalMemoryPool()), memoryManager->selectHeap(&allocation, false, false, false));

    GraphicsAllocation allocation2{0, 1u /*num gmms*/, AllocationType::kernelIsaInternal, nullptr, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu};
    allocation2.set32BitAllocation(false);
    EXPECT_EQ(MemoryManager::selectInternalHeap(allocation2.isAllocatedInLocalMemoryPool()), memoryManager->selectHeap(&allocation2, false, false, false));
}

TEST_F(HeapSelectorTest, given32bitExternalAllocationWhenSelectingHeapThenExternalHeapIsUsed) {
    GraphicsAllocation allocation{0, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu};
    allocation.set32BitAllocation(true);
    EXPECT_EQ(MemoryManager::selectExternalHeap(allocation.isAllocatedInLocalMemoryPool()), memoryManager->selectHeap(&allocation, false, false, false));
}

TEST_F(HeapSelectorTest, givenLimitedAddressSpaceWhenSelectingHeapForExternalAllocationThenStandardHeapIsUsed) {
    GraphicsAllocation allocation{0, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu};
    EXPECT_EQ(HeapIndex::heapStandard, memoryManager->selectHeap(&allocation, true, false, false));
}

TEST_F(HeapSelectorTest, givenFullAddressSpaceWhenSelectingHeapForExternalAllocationWithPtrThenSvmHeapIsUsed) {
    GraphicsAllocation allocation{0, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu};
    EXPECT_EQ(HeapIndex::heapSvm, memoryManager->selectHeap(&allocation, true, true, false));
}

TEST_F(HeapSelectorTest, givenFullAddressSpaceWhenSelectingHeapForExternalAllocationWithoutPtrAndResourceIs64KSuitableThenStandard64kHeapIsUsed) {
    GraphicsAllocation allocation{0, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu};
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::make_unique<Gmm>(rootDeviceEnvironment->getGmmHelper(), nullptr, 0, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, StorageInfo{}, gmmRequirements);
    auto resourceInfo = static_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    resourceInfo->is64KBPageSuitableValue = true;
    allocation.setDefaultGmm(gmm.get());
    EXPECT_EQ(HeapIndex::heapStandard64KB, memoryManager->selectHeap(&allocation, false, true, false));
}

TEST_F(HeapSelectorTest, givenFullAddressSpaceWhenSelectingHeapForExternalAllocationWithoutPtrAndResourceIsNot64KSuitableThenStandardHeapIsUsed) {
    GraphicsAllocation allocation{0, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu};
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::make_unique<Gmm>(rootDeviceEnvironment->getGmmHelper(), nullptr, 0, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, StorageInfo{}, gmmRequirements);
    auto resourceInfo = static_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    resourceInfo->is64KBPageSuitableValue = false;
    allocation.setDefaultGmm(gmm.get());
    EXPECT_EQ(HeapIndex::heapStandard, memoryManager->selectHeap(&allocation, false, true, false));
}

TEST_F(HeapSelectorTest, givenFullAddressSpaceWhenSelectingHeapForNullAllocationWithoutPtrThenStandardHeapIsUsed) {
    EXPECT_EQ(HeapIndex::heapStandard, memoryManager->selectHeap(nullptr, false, true, false));
}

TEST_F(HeapSelectorTest, givenLimitedAddressSpaceWhenSelectingHeapForNullAllocationWithoutPtrThenStandardHeapIsUsed) {
    EXPECT_EQ(HeapIndex::heapStandard, memoryManager->selectHeap(nullptr, false, false, false));
}

TEST_F(HeapSelectorTest, givenDebugModuleAreaAllocationAndUseFrontWindowWhenSelectingHeapThenInternalFrontWindowHeapIsReturned) {
    GraphicsAllocation allocation{0, 1u /*num gmms*/, AllocationType::debugModuleArea, nullptr, 0, 0, 0, MemoryPool::memoryNull, 1};
    allocation.set32BitAllocation(true);
    EXPECT_EQ(HeapIndex::heapInternalFrontWindow, memoryManager->selectHeap(&allocation, false, false, true));
}

TEST_F(HeapSelectorTest, givenDebugModuleAreaAllocationInLocalMemoryAndUseFrontWindowWhenSelectingHeapThenInternalDeviceFrontWindowHeapIsReturned) {
    GraphicsAllocation allocation{0, 1u /*num gmms*/, AllocationType::debugModuleArea, nullptr, 0, 0, 0, MemoryPool::localMemory, 1};
    allocation.set32BitAllocation(true);
    EXPECT_TRUE(allocation.isAllocatedInLocalMemoryPool());
    EXPECT_EQ(HeapIndex::heapInternalDeviceFrontWindow, memoryManager->selectHeap(&allocation, false, false, true));
}

TEST(MemoryAllocationTest, givenAllocationTypeWhenPassedToMemoryAllocationConstructorThenAllocationTypeIsStored) {
    MemoryAllocation allocation{0, 1u /*num gmms*/, AllocationType::commandBuffer, nullptr, nullptr, 0, 0, 0,
                                MemoryPool::memoryNull, false, false, MemoryManager::maxOsContextCount};
    EXPECT_EQ(AllocationType::commandBuffer, allocation.getAllocationType());
}

TEST(MemoryAllocationTest, givenMemoryPoolWhenPassedToMemoryAllocationConstructorThenMemoryPoolIsStored) {
    MemoryAllocation allocation{0, 1u /*num gmms*/, AllocationType::commandBuffer, nullptr, nullptr, 0, 0, 0,
                                MemoryPool::system64KBPages, false, false, MemoryManager::maxOsContextCount};
    EXPECT_EQ(MemoryPool::system64KBPages, allocation.getMemoryPool());
}

TEST_F(MemoryAllocatorTest, whenCommandStreamerIsRegisteredThenReturnAssociatedEngineControl) {
    auto engineControl = memoryManager->getRegisteredEngineForCsr(csr);
    ASSERT_NE(nullptr, engineControl);
    EXPECT_EQ(csr, engineControl->commandStreamReceiver);
}

TEST_F(MemoryAllocatorTest, whenCommandStreamerIsNotRegisteredThenReturnNullEngineControl) {
    MockCommandStreamReceiver csr{*executionEnvironment, 0u, {}};
    auto engineControl = memoryManager->getRegisteredEngineForCsr(&csr);
    EXPECT_EQ(nullptr, engineControl);
}

TEST(MemoryManagerCopyMemoryTest, givenAllocationWithNoStorageWhenCopyMemoryToAllocationThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    uint8_t memory = 1;
    MockGraphicsAllocation invalidAllocation{nullptr, 0u};
    EXPECT_FALSE(memoryManager.copyMemoryToAllocation(&invalidAllocation, 0, &memory, sizeof(memory)));
}

TEST(MemoryManagerCopyMemoryTest, givenValidAllocationAndMemoryWhenCopyMemoryToAllocationThenDataIsCopied) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    constexpr uint8_t allocationSize = 10;
    uint8_t allocationStorage[allocationSize] = {0};
    MockGraphicsAllocation allocation{allocationStorage, allocationSize};
    uint8_t memory = 1u;
    EXPECT_EQ(0u, allocationStorage[0]);

    size_t offset = 2;

    EXPECT_TRUE(memoryManager.copyMemoryToAllocation(&allocation, offset, &memory, sizeof(memory)));
    EXPECT_EQ(memory, allocationStorage[offset]);
}

TEST_F(MemoryAllocatorTest, whenReservingAddressRangeThenExpectProperAddressAndReleaseWhenFreeing) {
    size_t size = 0x1000;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), size});
    ASSERT_NE(nullptr, allocation);
    void *reserve = memoryManager->reserveCpuAddressRange(size, 0);
    EXPECT_NE(nullptr, reserve);
    allocation->setReservedAddressRange(reserve, size);
    EXPECT_EQ(reserve, allocation->getReservedAddressPtr());
    EXPECT_EQ(size, allocation->getReservedAddressSize());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenMemoryManagerWhenGettingReservedMemoryThenAllocateIt) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    EXPECT_EQ(nullptr, memoryManager.reservedMemory);
    memoryManager.getReservedMemory(MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);
    EXPECT_NE(nullptr, memoryManager.reservedMemory);
}

TEST(MemoryManagerTest, givenMemoryManagerWhenGetReservedMemoryIsCalledManyTimesThenReuseSameMemory) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    auto reservedMemory = memoryManager.getReservedMemory(MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);
    memoryManager.getReservedMemory(MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);
    memoryManager.getReservedMemory(MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);
    EXPECT_EQ(reservedMemory, memoryManager.reservedMemory);
}

class MemoryManagerWithFailure : public MockMemoryManager {
  public:
    using MockMemoryManager::MockMemoryManager;
    GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) override {
        recentlyPassedDeviceBitfield = properties.subDevicesBitfield;
        return nullptr;
    }

    GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties, const void *ptr) override {
        recentlyPassedDeviceBitfield = properties.subDevicesBitfield;
        return nullptr;
    }
};

TEST(MemoryManagerTest, whenMemoryManagerReturnsNullptrThenAllocateGlobalsSurfaceAlsoReturnsNullptr) {
    MockDevice device{};
    auto deviceBitfield = device.getDeviceBitfield();
    auto memoryManager = new MemoryManagerWithFailure{*device.getExecutionEnvironment()};
    device.injectMemoryManager(memoryManager);

    WhiteBox<NEO::LinkerInput> linkerInput;
    linkerInput.traits.exportsGlobalConstants = true;
    linkerInput.traits.exportsGlobalVariables = true;
    memoryManager->recentlyPassedDeviceBitfield = {};
    std::unique_ptr<SharedPoolAllocation> globalSurface = std::unique_ptr<SharedPoolAllocation>(allocateGlobalsSurface(nullptr, device, 1024, 0u, false, &linkerInput, nullptr));
    EXPECT_EQ(nullptr, globalSurface);
    EXPECT_EQ(deviceBitfield, memoryManager->recentlyPassedDeviceBitfield);

    auto svmAllocsManager = std::make_unique<SVMAllocsManager>(memoryManager);
    memoryManager->recentlyPassedDeviceBitfield = {};
    globalSurface.reset(allocateGlobalsSurface(svmAllocsManager.get(), device, 1024, 0u, false, &linkerInput, nullptr));
    EXPECT_EQ(nullptr, globalSurface);
    EXPECT_EQ(deviceBitfield, memoryManager->recentlyPassedDeviceBitfield);
}

class FailFirstCpuReserveMemoryManager : public MockMemoryManager {
  public:
    using MockMemoryManager::MockMemoryManager;
    AddressRange reserveCpuAddress(const uint64_t requiredStartAddress, size_t size) override {
        cpuReservationCallCount++;
        if (isFirstCpuReservationCall || alwaysFail) {
            isFirstCpuReservationCall = false;
            return {};
        }
        return AddressRange{0xDEADBEEF, size};
    }
    void freeCpuAddress(AddressRange addressRange) override{};

    bool alwaysFail = false;
    bool isFirstCpuReservationCall = true;
    int8_t cpuReservationCallCount = 0;
};

TEST(MemoryManagerTest, givenFirstCpuReservationFailsAndRequiredStartAddressIsZeroThenReservationIsNotTriedAgain) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    auto failFirstMemoryManager = std::make_unique<FailFirstCpuReserveMemoryManager>(executionEnvironment);

    EXPECT_EQ(0, failFirstMemoryManager->cpuReservationCallCount);
    auto addressRange = failFirstMemoryManager->reserveCpuAddressWithZeroBaseRetry(0, 1234);
    EXPECT_EQ(0u, addressRange.address);
    EXPECT_EQ(0u, addressRange.size);
    EXPECT_EQ(1, failFirstMemoryManager->cpuReservationCallCount);
}

TEST(MemoryManagerTest, givenFirstCpuReservationFailsAndRequiredStartAddressIsNotZeroThenReservationIsTriedAgain) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    auto failFirstMemoryManager = std::make_unique<FailFirstCpuReserveMemoryManager>(executionEnvironment);

    EXPECT_EQ(0, failFirstMemoryManager->cpuReservationCallCount);
    auto addressRange = failFirstMemoryManager->reserveCpuAddressWithZeroBaseRetry(42, 1234);
    EXPECT_EQ(0xDEADBEEF, addressRange.address);
    EXPECT_EQ(1234u, addressRange.size);
    EXPECT_EQ(2, failFirstMemoryManager->cpuReservationCallCount);
    failFirstMemoryManager->freeCpuAddress(addressRange);
}

TEST(MemoryManagerTest, givenCpuReservationFailsThenReservationIsTriedAgain) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    auto failFirstMemoryManager = std::make_unique<FailFirstCpuReserveMemoryManager>(executionEnvironment);

    failFirstMemoryManager->alwaysFail = true;

    EXPECT_EQ(0, failFirstMemoryManager->cpuReservationCallCount);
    auto addressRangeZeroStart = failFirstMemoryManager->reserveCpuAddressWithZeroBaseRetry(0, 1234);
    EXPECT_EQ(0u, addressRangeZeroStart.address);
    EXPECT_EQ(0u, addressRangeZeroStart.size);
    EXPECT_EQ(1, failFirstMemoryManager->cpuReservationCallCount);
    auto addressRangeNonZeroStart = failFirstMemoryManager->reserveCpuAddressWithZeroBaseRetry(42, 1234);
    EXPECT_EQ(0u, addressRangeNonZeroStart.address);
    EXPECT_EQ(0u, addressRangeNonZeroStart.size);
    EXPECT_EQ(3, failFirstMemoryManager->cpuReservationCallCount);
}

TEST(MemoryManagerTest, givenCpuReservationPassesThenReservationIsNotTriedAgain) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    auto failFirstMemoryManager = std::make_unique<FailFirstCpuReserveMemoryManager>(executionEnvironment);

    failFirstMemoryManager->isFirstCpuReservationCall = false;

    EXPECT_EQ(0, failFirstMemoryManager->cpuReservationCallCount);
    auto addressRangeZeroStart = failFirstMemoryManager->reserveCpuAddressWithZeroBaseRetry(0, 1234);
    EXPECT_EQ(0xDEADBEEF, addressRangeZeroStart.address);
    EXPECT_EQ(1234u, addressRangeZeroStart.size);
    EXPECT_EQ(1, failFirstMemoryManager->cpuReservationCallCount);
    failFirstMemoryManager->freeCpuAddress(addressRangeZeroStart);
    auto addressRangeNonZeroStart = failFirstMemoryManager->reserveCpuAddressWithZeroBaseRetry(42, 1234);
    EXPECT_EQ(0xDEADBEEF, addressRangeNonZeroStart.address);
    EXPECT_EQ(1234u, addressRangeNonZeroStart.size);
    EXPECT_EQ(2, failFirstMemoryManager->cpuReservationCallCount);
    failFirstMemoryManager->freeCpuAddress(addressRangeNonZeroStart);
}

HWTEST_F(MemoryAllocatorTest, givenMemoryManagerWhenEnableHostPtrTrackingFlagIsSetTo0ThenHostPointerTrackingIsDisabled) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableHostPtrTracking.set(0);
    EXPECT_FALSE(memoryManager->isHostPointerTrackingEnabled(0u));
}

HWTEST_F(MemoryAllocatorTest, givenMemoryManagerWhenEnableHostPtrTrackingFlagIsNotSetTo1ThenHostPointerTrackingIsEnabled) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableHostPtrTracking.set(1);
    EXPECT_TRUE(memoryManager->isHostPointerTrackingEnabled(0u));
}

HWTEST_F(MemoryAllocatorTest, givenMemoryManagerWhenEnableHostPtrTrackingFlagIsSetNotSetThenHostPointerTrackingDependsOnCapabilityTable) {
    if (is32bit) {
        EXPECT_TRUE(memoryManager->isHostPointerTrackingEnabled(0u));
    } else {
        EXPECT_FALSE(memoryManager->isHostPointerTrackingEnabled(0u));
    }
}

HWTEST_F(MemoryAllocatorTest, givenMemoryManagerWhen64BitAndHostPtrTrackingDisabledThenNonSvmHostPtrUsageIsSet) {

    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableHostPtrTracking.set(0);

    bool expectedValue = !is32bit;

    auto result = memoryManager->useNonSvmHostPtrAlloc(AllocationType::externalHostPtr, 0u);
    EXPECT_EQ(expectedValue, result);

    result = memoryManager->useNonSvmHostPtrAlloc(AllocationType::mapAllocation, 0u);
    EXPECT_EQ(expectedValue, result);
}

HWTEST_F(MemoryAllocatorTest, givenMemoryManagerWhenHostPtrTrackingModeThenNonSvmHostPtrUsageIsSet) {
    memoryManager->setForceNonSvmForExternalHostPtr(true);

    auto result = memoryManager->useNonSvmHostPtrAlloc(AllocationType::externalHostPtr, 0u);
    EXPECT_EQ(true, result);
    result = memoryManager->useNonSvmHostPtrAlloc(AllocationType::bufferHostMemory, 0u);
    EXPECT_EQ(false, result);
}

HWTEST_F(MemoryAllocatorTest, givenMemoryManagerThenNonSvmBufferIsSetForBufferHostMemory) {
    HardwareInfo hwInfoLocal = *defaultHwInfo;
    memoryManager->peekExecutionEnvironment().rootDeviceEnvironments[0u]->setHwInfoAndInitHelpers(&hwInfoLocal);
    int buffer = 0;
    EXPECT_FALSE(memoryManager->isNonSvmBuffer(&buffer, AllocationType::externalHostPtr, 0u));
    EXPECT_EQ(!is32bit, memoryManager->isNonSvmBuffer(&buffer, AllocationType::bufferHostMemory, 0u));
}

HWTEST_F(MemoryAllocatorTest, givenMemoryManagerWhenHostPtrTrackingEnabledThenNonSvmHostPtrUsageDependsOnFullRangeSvm) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableHostPtrTracking.set(1);

    auto result = memoryManager->useNonSvmHostPtrAlloc(AllocationType::externalHostPtr, 0u);
    EXPECT_EQ(!executionEnvironment->rootDeviceEnvironments[0]->isFullRangeSvm() && !is32bit, result);

    result = memoryManager->useNonSvmHostPtrAlloc(AllocationType::mapAllocation, 0u);
    EXPECT_EQ(!executionEnvironment->rootDeviceEnvironments[0]->isFullRangeSvm() && !is32bit, result);
}

using PageTableManagerTest = ::testing::Test;
HWTEST_F(PageTableManagerTest, givenPageTableManagerWhenMapAuxGpuVaThenForAllEnginesWithPageTableUpdateAuxTableAreCalled) {
    MockExecutionEnvironment executionEnvironment{};
    executionEnvironment.prepareRootDeviceEnvironments(2);
    for (auto i = 0u; i < executionEnvironment.rootDeviceEnvironments.size(); i++) {
        executionEnvironment.rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment.rootDeviceEnvironments[i]->initGmm();
    }

    auto memoryManager = new MockMemoryManager(false, false, executionEnvironment);

    executionEnvironment.memoryManager.reset(memoryManager);
    auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(executionEnvironment, 1u, 1));
    auto csr2 = std::unique_ptr<CommandStreamReceiver>(createCommandStream(executionEnvironment, 1u, 1));
    auto hwInfo = *defaultHwInfo;
    EngineInstancesContainer regularEngines = {
        {aub_stream::ENGINE_CCS, EngineUsage::regular},
        {aub_stream::ENGINE_BCS, EngineUsage::regular},
    };

    memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(regularEngines[0],
                                                                                                      PreemptionHelper::getDefaultPreemptionMode(hwInfo)));
    memoryManager->createAndRegisterOsContext(csr2.get(), EngineDescriptorHelper::getDefaultDescriptor(regularEngines[1],
                                                                                                       PreemptionHelper::getDefaultPreemptionMode(hwInfo)));

    auto mockMngr = new MockGmmPageTableMngr();
    auto mockMngr2 = new MockGmmPageTableMngr();

    memoryManager->getRegisteredEngines(1)[0].commandStreamReceiver->pageTableManager.reset(mockMngr);
    memoryManager->getRegisteredEngines(1)[1].commandStreamReceiver->pageTableManager.reset(mockMngr2);

    MockGraphicsAllocation allocation(1u, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu);
    MockGmm gmm(executionEnvironment.rootDeviceEnvironments[allocation.getRootDeviceIndex()]->getGmmHelper());
    gmm.setCompressionEnabled(true);
    allocation.setDefaultGmm(&gmm);
    GMM_DDI_UPDATEAUXTABLE expectedDdiUpdateAuxTable = {};
    expectedDdiUpdateAuxTable.BaseGpuVA = allocation.getGpuAddress();
    expectedDdiUpdateAuxTable.BaseResInfo = allocation.getDefaultGmm()->gmmResourceInfo->peekGmmResourceInfo();
    expectedDdiUpdateAuxTable.DoNotWait = true;
    expectedDdiUpdateAuxTable.Map = true;

    bool result = memoryManager->mapAuxGpuVA(&allocation);
    EXPECT_TRUE(result);

    EXPECT_EQ(1u, mockMngr->updateAuxTableCalled);
    EXPECT_EQ(expectedDdiUpdateAuxTable.BaseGpuVA, mockMngr->updateAuxTableParamsPassed[0].ddiUpdateAuxTable.BaseGpuVA);
    EXPECT_EQ(expectedDdiUpdateAuxTable.BaseResInfo, mockMngr->updateAuxTableParamsPassed[0].ddiUpdateAuxTable.BaseResInfo);
    EXPECT_EQ(expectedDdiUpdateAuxTable.DoNotWait, mockMngr->updateAuxTableParamsPassed[0].ddiUpdateAuxTable.DoNotWait);
    EXPECT_EQ(expectedDdiUpdateAuxTable.Map, mockMngr->updateAuxTableParamsPassed[0].ddiUpdateAuxTable.Map);

    EXPECT_EQ(1u, mockMngr2->updateAuxTableCalled);
    EXPECT_EQ(expectedDdiUpdateAuxTable.BaseGpuVA, mockMngr2->updateAuxTableParamsPassed[0].ddiUpdateAuxTable.BaseGpuVA);
    EXPECT_EQ(expectedDdiUpdateAuxTable.BaseResInfo, mockMngr2->updateAuxTableParamsPassed[0].ddiUpdateAuxTable.BaseResInfo);
    EXPECT_EQ(expectedDdiUpdateAuxTable.DoNotWait, mockMngr2->updateAuxTableParamsPassed[0].ddiUpdateAuxTable.DoNotWait);
    EXPECT_EQ(expectedDdiUpdateAuxTable.Map, mockMngr2->updateAuxTableParamsPassed[0].ddiUpdateAuxTable.Map);
}
HWTEST_F(PageTableManagerTest, givenPageTableManagerWhenUpdateAuxTableGmmErrorThenMapAuxGpuVaReturnFalse) {
    MockExecutionEnvironment executionEnvironment{};
    executionEnvironment.prepareRootDeviceEnvironments(2);
    for (auto i = 0u; i < executionEnvironment.rootDeviceEnvironments.size(); i++) {
        executionEnvironment.rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment.rootDeviceEnvironments[i]->initGmm();
    }

    auto memoryManager = new MockMemoryManager(false, false, executionEnvironment);

    executionEnvironment.memoryManager.reset(memoryManager);
    auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(executionEnvironment, 1u, 1));
    auto hwInfo = *defaultHwInfo;
    EngineInstancesContainer regularEngines = {
        {aub_stream::ENGINE_CCS, EngineUsage::regular}};

    memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(regularEngines[0],
                                                                                                      PreemptionHelper::getDefaultPreemptionMode(hwInfo)));

    auto mockMngr = new MockGmmPageTableMngr();
    mockMngr->updateAuxTableResult = GMM_ERROR;

    memoryManager->getRegisteredEngines(1)[0].commandStreamReceiver->pageTableManager.reset(mockMngr);

    MockGraphicsAllocation allocation(1u, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu);
    MockGmm gmm(executionEnvironment.rootDeviceEnvironments[allocation.getRootDeviceIndex()]->getGmmHelper());
    gmm.setCompressionEnabled(true);
    allocation.setDefaultGmm(&gmm);

    bool result = memoryManager->mapAuxGpuVA(&allocation);
    EXPECT_FALSE(result);
    EXPECT_EQ(1u, mockMngr->updateAuxTableCalled);
}

HWTEST_F(PageTableManagerTest, givenNullPageTableManagerWhenMapAuxGpuVaThenNoThrow) {
    MockExecutionEnvironment executionEnvironment{};
    executionEnvironment.prepareRootDeviceEnvironments(2);
    for (auto i = 0u; i < executionEnvironment.rootDeviceEnvironments.size(); i++) {
        executionEnvironment.rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment.rootDeviceEnvironments[i]->initGmm();
    }

    auto memoryManager = new MockMemoryManager(false, false, executionEnvironment);

    executionEnvironment.memoryManager.reset(memoryManager);
    auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(executionEnvironment, 1u, 1));
    auto hwInfo = *defaultHwInfo;
    EngineInstancesContainer regularEngines = {
        {aub_stream::ENGINE_CCS, EngineUsage::regular},
    };

    memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(regularEngines[0],
                                                                                                      PreemptionHelper::getDefaultPreemptionMode(hwInfo)));

    memoryManager->getRegisteredEngines(1)[0].commandStreamReceiver->pageTableManager.reset(nullptr);

    MockGraphicsAllocation allocation(1u, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu);
    MockGmm gmm(executionEnvironment.rootDeviceEnvironments[allocation.getRootDeviceIndex()]->getGmmHelper());
    gmm.setCompressionEnabled(true);
    allocation.setDefaultGmm(&gmm);

    EXPECT_NO_THROW(memoryManager->mapAuxGpuVA(&allocation));
}

HWTEST_F(PageTableManagerTest, givenNullPageTableManagerWhenMapAuxGpuVaThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment{};
    executionEnvironment.prepareRootDeviceEnvironments(2);
    for (auto i = 0u; i < executionEnvironment.rootDeviceEnvironments.size(); i++) {
        executionEnvironment.rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment.rootDeviceEnvironments[i]->initGmm();
    }

    auto memoryManager = new MockMemoryManager(false, false, executionEnvironment);

    executionEnvironment.memoryManager.reset(memoryManager);
    auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(executionEnvironment, 1u, 1));
    auto hwInfo = *defaultHwInfo;
    auto &gfxCoreHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*executionEnvironment.rootDeviceEnvironments[0])[0],
                                                                                                      PreemptionHelper::getDefaultPreemptionMode(hwInfo)));

    for (auto engine : memoryManager->getRegisteredEngines(1)) {
        engine.commandStreamReceiver->pageTableManager.reset(nullptr);
    }

    MockGraphicsAllocation allocation(1u, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu);

    bool result = memoryManager->mapAuxGpuVA(&allocation);
    EXPECT_FALSE(result);
}

HWTEST_F(PageTableManagerTest, givenMemoryManagerThatSupportsPageTableManagerWhenMapAuxGpuVAIsCalledThenItReturnsTrue) {
    MockExecutionEnvironment executionEnvironment{};
    executionEnvironment.prepareRootDeviceEnvironments(2);
    for (auto i = 0u; i < executionEnvironment.rootDeviceEnvironments.size(); i++) {
        executionEnvironment.rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment.rootDeviceEnvironments[i]->initGmm();
    }

    auto memoryManager = new MockMemoryManager(false, false, executionEnvironment);
    executionEnvironment.memoryManager.reset(memoryManager);
    MockGraphicsAllocation allocation(1u, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu);
    MockGmm gmm(executionEnvironment.rootDeviceEnvironments[allocation.getRootDeviceIndex()]->getGmmHelper());
    allocation.setDefaultGmm(&gmm);
    bool mapped = memoryManager->mapAuxGpuVA(&allocation);
    auto &hwInfo = *executionEnvironment.rootDeviceEnvironments[allocation.getRootDeviceIndex()]->getHardwareInfo();
    auto &productHelper = executionEnvironment.rootDeviceEnvironments[allocation.getRootDeviceIndex()]->getHelper<ProductHelper>();
    EXPECT_EQ(productHelper.isPageTableManagerSupported(hwInfo), mapped);
}

TEST(MemoryManagerTest, givenDebugModuleAreaAllocationTypeWhenCallingGetAllocationDataThenUse32BitFrontWindowsIsSet) {
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, 1, AllocationType::debugModuleArea, mockDeviceBitfield);

    MockMemoryManager mockMemoryManager;

    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_EQ(1u, allocData.flags.use32BitFrontWindow);
}

TEST(MemoryManagerTest, givenStorageInfoWithParamsWhenGettingAllocDataForSystemMemoryThenSetSystemMemoryFlag) {
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, 1, AllocationType::bufferHostMemory, mockDeviceBitfield);

    EXPECT_NE(0lu, mockDeviceBitfield.to_ulong());

    MockMemoryManager mockMemoryManager;
    auto storageInfo = mockMemoryManager.createStorageInfoFromProperties(properties);
    EXPECT_NE(0lu, storageInfo.memoryBanks.to_ulong());

    mockMemoryManager.getAllocationData(allocData, properties, nullptr, storageInfo);
    EXPECT_EQ(1u, allocData.flags.useSystemMemory);
    EXPECT_TRUE(allocData.storageInfo.systemMemoryPlacement);
}

TEST(MemoryManagerTest, givenStorageInfoWithParamsWhenGettingAllocDataForLocalMemoryThenClearSystemMemoryFlag) {
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, 1, AllocationType::buffer, mockDeviceBitfield);

    EXPECT_NE(0lu, mockDeviceBitfield.to_ulong());

    MockMemoryManager mockMemoryManager;
    auto storageInfo = mockMemoryManager.createStorageInfoFromProperties(properties);
    EXPECT_NE(0lu, storageInfo.memoryBanks.to_ulong());

    mockMemoryManager.getAllocationData(allocData, properties, nullptr, storageInfo);
    EXPECT_EQ(0u, allocData.flags.useSystemMemory);
    EXPECT_FALSE(allocData.storageInfo.systemMemoryPlacement);
}

HWTEST_F(MemoryAllocatorTest, givenUseLocalPreferredForCacheableBuffersAndCompressionNotPreferredWhenGettingAllocDataForLocalMemoryThenLocalPreferredSetCorrectly) {
    // localPreferred is implicit with gmm flags LocalOnly=0 and NonLocalOnly=0
    DebugManagerStateRestore restorer;
    debugManager.flags.UseLocalPreferredForCacheableBuffers.set(0);
    AllocationData allocData;
    allocData.flags.useSystemMemory = true;
    allocData.flags.preferCompressed = false;
    AllocationProperties properties(mockRootDeviceIndex, 1, AllocationType::buffer, mockDeviceBitfield);
    MockMemoryManager mockMemoryManager;
    auto releaseHelper = std::make_unique<MockReleaseHelper>();
    releaseHelper->isLocalOnlyAllowedResult = true;
    mockMemoryManager.executionEnvironment.rootDeviceEnvironments[properties.rootDeviceIndex]->releaseHelper.reset(releaseHelper.release());
    AllocationType shouldUseLocalPreferredAllocationTypes[] = {
        AllocationType::buffer,
        AllocationType::svmGpu,
        AllocationType::image};

    for (auto allocationType : shouldUseLocalPreferredAllocationTypes) {
        properties.allocationType = allocationType;
        auto storageInfo = mockMemoryManager.createStorageInfoFromProperties(properties);

        mockMemoryManager.getAllocationData(allocData, properties, nullptr, storageInfo);
        EXPECT_EQ(AllocationType::svmGpu == allocationType || AllocationType::buffer == allocationType, allocData.storageInfo.localOnlyRequired);
        EXPECT_EQ(true, allocData.storageInfo.systemMemoryPlacement);
        EXPECT_EQ(false, allocData.storageInfo.systemMemoryForced);
    }

    debugManager.flags.UseLocalPreferredForCacheableBuffers.set(1);

    for (auto allocationType : shouldUseLocalPreferredAllocationTypes) {
        properties.allocationType = allocationType;
        auto storageInfo = mockMemoryManager.createStorageInfoFromProperties(properties);
        mockMemoryManager.getAllocationData(allocData, properties, nullptr, storageInfo);
        EXPECT_EQ(false, allocData.storageInfo.localOnlyRequired);
        EXPECT_EQ(false, allocData.storageInfo.systemMemoryPlacement);
        EXPECT_EQ(false, allocData.storageInfo.systemMemoryForced);
    }

    properties.flags.uncacheable = true;

    for (auto allocationType : shouldUseLocalPreferredAllocationTypes) {
        properties.allocationType = allocationType;
        auto storageInfo = mockMemoryManager.createStorageInfoFromProperties(properties);

        mockMemoryManager.getAllocationData(allocData, properties, nullptr, storageInfo);
        EXPECT_EQ(AllocationType::svmGpu == allocationType || AllocationType::buffer == allocationType, allocData.storageInfo.localOnlyRequired);
        EXPECT_EQ(true, allocData.storageInfo.systemMemoryPlacement);
        EXPECT_EQ(false, allocData.storageInfo.systemMemoryForced);
    }
}

HWTEST_F(MemoryAllocatorTest, givenNonDefaultLocalMemoryAllocationModeAndLocalPreferredForCacheableBuffersWhenGettingAllocDataForDeviceUsmThenLocalOnlyRequiredIsNotOverridden) {
    DebugManagerStateRestore restorer;
    debugManager.flags.UseLocalPreferredForCacheableBuffers.set(1);

    AllocationProperties properties(mockRootDeviceIndex, 1, AllocationType::buffer, mockDeviceBitfield);
    properties.flags.uncacheable = false;

    MockMemoryManager mockMemoryManager;
    auto releaseHelper = std::make_unique<MockReleaseHelper>();
    mockMemoryManager.executionEnvironment.rootDeviceEnvironments[properties.rootDeviceIndex]->releaseHelper.reset(releaseHelper.get());

    for (const auto debugKeyValue : std::to_array({1, 2})) {
        mockMemoryManager.usmDeviceAllocationMode = toLocalMemAllocationMode(debugKeyValue);
        releaseHelper->isLocalOnlyAllowedResult = (debugKeyValue == 1);
        auto storageInfo{mockMemoryManager.createStorageInfoFromProperties(properties)};
        bool expectedValue{storageInfo.localOnlyRequired};

        AllocationData allocData;
        mockMemoryManager.getAllocationData(allocData, properties, nullptr, storageInfo);
        EXPECT_EQ(expectedValue, allocData.storageInfo.localOnlyRequired);
    }
    releaseHelper.release();
}

TEST(MemoryTransferHelperTest, WhenBlitterIsSelectedButBlitCopyFailsThenFallbackToCopyOnCPU) {
    constexpr uint32_t dataSize = 16;
    uint8_t destData[dataSize] = {};
    uint8_t srcData[dataSize] = {};
    for (uint8_t i = 0u; i < dataSize; i++) {
        srcData[i] = i;
    }
    MockGraphicsAllocation graphicsAllocation{destData, sizeof(destData)};
    graphicsAllocation.setAllocationType(AllocationType::bufferHostMemory);

    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = false;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));

    EXPECT_EQ(BlitOperationResult::unsupported, BlitHelperFunctions::blitMemoryToAllocation(*device, &graphicsAllocation, 0, srcData, {dataSize, 1, 1}));

    auto result = MemoryTransferHelper::transferMemoryToAllocation(true, *device, &graphicsAllocation, 0u, srcData, dataSize);
    EXPECT_TRUE(result);
    EXPECT_EQ(0, memcmp(destData, srcData, dataSize));
}

TEST(MemoryTransferHelperTest, givenBlitOperationSupportedWhenBcsEngineNotAvailableThenReturnUnsupported) {
    constexpr uint32_t dataSize = 16;
    uint8_t destData[dataSize] = {};
    uint8_t srcData[dataSize] = {};

    MockGraphicsAllocation graphicsAllocation{destData, sizeof(destData)};
    graphicsAllocation.storageInfo.memoryBanks = 1;
    graphicsAllocation.setAllocationType(AllocationType::buffer);

    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo = 0;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));

    auto bcsEngine = device->tryGetEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular);
    EXPECT_EQ(nullptr, bcsEngine);

    EXPECT_EQ(BlitOperationResult::unsupported, BlitHelperFunctions::blitMemoryToAllocation(*device, &graphicsAllocation, 0, srcData, {dataSize, 1, 1}));
}

TEST(MemoryManagerTest, givenMemoryManagerWithLocalMemoryWhenCreatingMultiGraphicsAllocationInSystemMemoryThenForceSystemMemoryPlacement) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MockMemoryManager memoryManager(true, true, executionEnvironment);

    DeviceBitfield localMemoryBitfield{1};
    AllocationProperties allocationProperties{mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::svmGpu, localMemoryBitfield};

    auto localMemoryAllocation = memoryManager.allocateGraphicsMemoryWithProperties(allocationProperties);

    EXPECT_NE(nullptr, localMemoryAllocation);
    EXPECT_TRUE(localMemoryAllocation->isAllocatedInLocalMemoryPool());

    memoryManager.freeGraphicsMemory(localMemoryAllocation);

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(mockRootDeviceIndex);

    MultiGraphicsAllocation multiGraphicsAllocation(mockRootDeviceIndex);

    auto ptr = memoryManager.createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, allocationProperties, multiGraphicsAllocation);

    EXPECT_NE(nullptr, ptr);
    auto systemMemoryAllocation = multiGraphicsAllocation.getGraphicsAllocation(mockRootDeviceIndex);
    EXPECT_NE(nullptr, systemMemoryAllocation);
    EXPECT_FALSE(systemMemoryAllocation->isAllocatedInLocalMemoryPool());

    memoryManager.freeGraphicsMemory(systemMemoryAllocation);
}

TEST(MemoryManagerTest, givenDuplicateRootDeviceIndicesWhenCreatingMultiGraphicsAllocationInSystemMemoryThenDontLeakMemory) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MockMemoryManager memoryManager(true, true, executionEnvironment);

    AllocationProperties allocationProperties{mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::svmGpu, systemMemoryBitfield};

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(mockRootDeviceIndex);
    rootDeviceIndices.pushUnique(mockRootDeviceIndex);
    rootDeviceIndices.pushUnique(mockRootDeviceIndex);

    EXPECT_EQ(1u, rootDeviceIndices.size());

    MultiGraphicsAllocation multiGraphicsAllocation(mockRootDeviceIndex);

    auto ptr = memoryManager.createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, allocationProperties, multiGraphicsAllocation);

    EXPECT_NE(nullptr, ptr);
    auto allocation = multiGraphicsAllocation.getGraphicsAllocation(mockRootDeviceIndex);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(mockRootDeviceIndex, allocation->getRootDeviceIndex());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenMemoryAllocationWhenFreedThenFreeCalledOnMemoryOperationsHandler) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    auto mockManager = new MockAubManager();
    auto mockAubCenter = new MockAubCenter(*executionEnvironment.rootDeviceEnvironments[0], false, "aubfile", CommandStreamReceiverType::aub);
    mockAubCenter->aubManager.reset(mockManager);
    executionEnvironment.rootDeviceEnvironments[0]->aubCenter.reset(mockAubCenter);
    executionEnvironment.incRefInternal();
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(nullptr, &executionEnvironment, 0));

    auto memoryOperationsHandler = new NEO::MockAubMemoryOperationsHandler(mockManager);
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface.reset(memoryOperationsHandler);

    MockMemoryManager memoryManager(true, true, executionEnvironment);

    DeviceBitfield localMemoryBitfield{1};
    AllocationProperties allocationProperties{mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::buffer, localMemoryBitfield};

    auto memoryAllocation = memoryManager.allocateGraphicsMemoryWithProperties(allocationProperties);
    EXPECT_NE(nullptr, memoryAllocation);

    memoryOperationsHandler->makeResident(device.get(), ArrayRef<GraphicsAllocation *>(&memoryAllocation, 1), false, false);

    EXPECT_EQ(1u, memoryOperationsHandler->residentAllocations.size());

    memoryManager.freeGraphicsMemory(memoryAllocation);

    EXPECT_TRUE(memoryOperationsHandler->freeCalled);
    EXPECT_EQ(0u, memoryOperationsHandler->residentAllocations.size());
}

TEST(AllocationListTest, givenAllocationInListWhenFreeAllGraphicsAllocationsCalledThenHeadAndTailIsNullptr) {
    AllocationsList allocList;
    EXPECT_EQ(nullptr, allocList.peekHead());
    EXPECT_EQ(nullptr, allocList.peekTail());
    EXPECT_TRUE(allocList.peekIsEmpty());

    auto mockGfxAllocation = std::make_unique<MockGraphicsAllocation>();
    allocList.pushFrontOne(*mockGfxAllocation.release());
    EXPECT_NE(nullptr, allocList.peekHead());
    EXPECT_NE(nullptr, allocList.peekTail());
    EXPECT_FALSE(allocList.peekIsEmpty());

    MockDevice mockDevice;
    allocList.freeAllGraphicsAllocations(&mockDevice);
    EXPECT_EQ(nullptr, allocList.peekHead());
    EXPECT_EQ(nullptr, allocList.peekTail());
    EXPECT_TRUE(allocList.peekIsEmpty());
}

TEST(MemoryManagerTest, givenIsCompressionSupportedForShareableThenReturnTrue) {
    MockMemoryManager memoryManager;
    EXPECT_TRUE(memoryManager.isCompressionSupportedForShareable(true));
    EXPECT_TRUE(memoryManager.isCompressionSupportedForShareable(false));
}

TEST(MemoryManagerTest, WhenGettingExtraDevicePropertiesThenNoExceptionIsThrown) {
    MockMemoryManager memoryManager;
    uint32_t moduleId = 0;
    uint16_t serverType = 0;

    EXPECT_NO_THROW(memoryManager.getExtraDeviceProperties(0, &moduleId, &serverType));
}

TEST(MemoryManagerTest, WhenGettingExtraDevicePropertiesThenPropertiesRemainUnchanged) {
    NEO::ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(executionEnvironment);

    uint32_t moduleId = 0;
    uint16_t serverType = 0;

    memoryManager.getExtraDeviceProperties(0, &moduleId, &serverType);

    EXPECT_EQ(moduleId, 0u);
    EXPECT_EQ(serverType, 0u);
}

TEST(MemoryManagerTest, WhenAddingCustomHeapAllocatorConfigsThenCanRetrieveAndMatchConfigs) {
    uint64_t heapBase = 0xAAAAAAAA;

    uint64_t heapFrontStart = 0xAAAABBBB;
    uint64_t heapRegularStart = 0xEEEEFFFF;

    size_t heapFrontSize = 1 * MemoryConstants::gigaByte;
    size_t heapRegularSize = 2 * MemoryConstants::gigaByte;

    auto allocator1 = std::make_unique<HeapAllocator>(heapFrontStart, heapFrontSize, MemoryConstants::pageSize64k, 0);
    auto allocator2 = std::make_unique<HeapAllocator>(heapRegularStart, heapRegularSize, MemoryConstants::pageSize64k, 0);

    MockMemoryManager memoryManager;

    memoryManager.addCustomHeapAllocatorConfig(AllocationType::linearStream, true, {allocator1.get(), heapBase});
    memoryManager.addCustomHeapAllocatorConfig(AllocationType::linearStream, false, {allocator2.get(), heapBase});

    auto config1 = memoryManager.getCustomHeapAllocatorConfig(AllocationType::linearStream, true);
    auto config2 = memoryManager.getCustomHeapAllocatorConfig(AllocationType::linearStream, false);
    auto configNonExisting = memoryManager.getCustomHeapAllocatorConfig(AllocationType::buffer, false);

    EXPECT_TRUE(config1.has_value());
    EXPECT_TRUE(config2.has_value());
    EXPECT_FALSE(configNonExisting.has_value());

    EXPECT_EQ(allocator1.get(), config1->get().allocator);
    EXPECT_EQ(heapBase, config1->get().gpuVaBase);

    EXPECT_EQ(allocator2.get(), config2->get().allocator);
    EXPECT_EQ(heapBase, config2->get().gpuVaBase);

    memoryManager.removeCustomHeapAllocatorConfig(AllocationType::linearStream, true);
    memoryManager.removeCustomHeapAllocatorConfig(AllocationType::linearStream, false);

    EXPECT_FALSE(memoryManager.getCustomHeapAllocatorConfig(AllocationType::linearStream, true).has_value());
    EXPECT_FALSE(memoryManager.getCustomHeapAllocatorConfig(AllocationType::linearStream, false).has_value());
}

TEST(MemoryManagerTest, givenGpuHangWhenAllocInUseCalledThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get(), true, 2);
    auto mockMemoryManager = new MockMemoryManager(false, false, executionEnvironment);
    executionEnvironment.memoryManager.reset(mockMemoryManager);
    auto csr = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 0, 1);
    auto osContext = executionEnvironment.memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_CCS, EngineUsage::regular}));

    mockMemoryManager->callBaseAllocInUse = true;
    constexpr uint8_t allocationSize = 10;
    uint8_t allocationStorage[allocationSize] = {0};
    MockGraphicsAllocation allocation{allocationStorage, allocationSize};
    allocation.updateTaskCount(10, osContext->getContextId());
    *csr->getTagAddress() = 0u;
    EXPECT_TRUE(mockMemoryManager->allocInUse(allocation));

    csr->isGpuHangDetectedReturnValue = true;
    csr->gpuHangCheckPeriod = std::chrono::microseconds::zero();
    EXPECT_FALSE(mockMemoryManager->allocInUse(allocation));
}
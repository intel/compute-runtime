/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/residency.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/program/program_initialization.h"
#include "shared/test/common/compiler_interface/linker_mock.h"
#include "shared/test/common/fixtures/memory_allocator_fixture.h"
#include "shared/test/common/fixtures/memory_manager_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/execution_environment_helper.h"
#include "shared/test/common/helpers/raii_hw_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_deferrable_deletion.h"
#include "shared/test/common/mocks/mock_deferred_deleter.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gfx_partition.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_page_table_mngr.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/event/event.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/program/printf_handler.h"
#include "opencl/source/program/program.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include <future>
#include <type_traits>

using namespace NEO;

typedef Test<MemoryAllocatorFixture> MemoryAllocatorTest;

TEST(MemoryManagerTest, givenDebugVariableWhenCreatingMemoryManagerThenSetSupportForMultiStorageResources) {
    DebugManagerStateRestore restore;

    {
        MockMemoryManager memoryManager;
        EXPECT_TRUE(memoryManager.supportsMultiStorageResources);
    }

    {
        DebugManager.flags.EnableMultiStorageResources.set(0);
        MockMemoryManager memoryManager;
        EXPECT_FALSE(memoryManager.supportsMultiStorageResources);
    }

    {
        DebugManager.flags.EnableMultiStorageResources.set(1);
        MockMemoryManager memoryManager;
        EXPECT_TRUE(memoryManager.supportsMultiStorageResources);
    }
}

TEST(MemoryManagerTest, givenLocalMemoryRequiredWhenSelectingHeapThenPickDeviceHeapIndex) {
    EXPECT_EQ(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY, MemoryManager::selectInternalHeap(true));
    EXPECT_EQ(HeapIndex::HEAP_INTERNAL, MemoryManager::selectInternalHeap(false));

    EXPECT_EQ(HeapIndex::HEAP_EXTERNAL_DEVICE_MEMORY, MemoryManager::selectExternalHeap(true));
    EXPECT_EQ(HeapIndex::HEAP_EXTERNAL, MemoryManager::selectExternalHeap(false));
}

TEST(MemoryManagerTest, whenCreatingAllocPropertiesForMultiStorageResourceThenMultiStorageResourcesFlagIsSetToTrue) {
    AllocationProperties properties{0, false, 0u, AllocationType::SCRATCH_SURFACE, false, true, 0};
    EXPECT_TRUE(properties.multiStorageResource);
}

TEST(MemoryBank, givenDifferentDeviceOrdinalsWhenGettingBankThenCorrectBanksAreReturned) {
    auto bank = MemoryBanks::getBank(0);
    EXPECT_EQ(MemoryBanks::MainBank, bank);

    bank = MemoryBanks::getBank(1);
    EXPECT_EQ(MemoryBanks::MainBank, bank);

    bank = MemoryBanks::getBank(100);
    EXPECT_EQ(MemoryBanks::MainBank, bank);
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
    GmmHelper gmmHelper(nullptr, defaultHwInfo.get());
    auto canonizedGpuAddress = gmmHelper.canonize(castToUint64(cpuPtr));
    osHandle sharedHandle = Sharing::nonSharedResource;
    GraphicsAllocation gfxAllocation(0, AllocationType::UNKNOWN, cpuPtr, size, sharedHandle, MemoryPool::MemoryNull, 0u, canonizedGpuAddress);
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

    GraphicsAllocation gfxAllocation(0, AllocationType::UNKNOWN, cpuPtr, gpuAddr, gpuBaseAddr, size, MemoryPool::MemoryNull, 0u);

    EXPECT_EQ(gpuAddr, gfxAllocation.getGpuAddress());

    cpuPtr = (void *)65535;
    gpuAddr = 1ULL;
    auto gmmHelper = std::make_unique<GmmHelper>(nullptr, defaultHwInfo.get());
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

    GraphicsAllocation gfxAllocation(0, AllocationType::UNKNOWN, cpuPtr, gpuAddr, gpuBaseAddr, size, MemoryPool::MemoryNull, 0u);

    EXPECT_EQ(gpuAddr - gpuBaseAddr, gfxAllocation.getGpuAddressToPatch());
}

TEST(GraphicsAllocationTest, WhenSetSizeThenUnderlyingBufferSizeIsSet) {
    void *cpuPtr = (void *)0x30000;
    uint64_t gpuAddr = 0x30000;
    uint64_t gpuBaseAddr = 0x10000;
    size_t size = 0x2000;

    GraphicsAllocation gfxAllocation(0, AllocationType::UNKNOWN, cpuPtr, gpuAddr, gpuBaseAddr, size, MemoryPool::MemoryNull, 0u);
    EXPECT_EQ(size, gfxAllocation.getUnderlyingBufferSize());

    size = 0x3000;
    gfxAllocation.setSize(size);
    EXPECT_EQ(size, gfxAllocation.getUnderlyingBufferSize());
}

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

    memoryManager->createAndRegisterOsContext(csr, EngineDescriptorHelper::getDefaultDescriptor(HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*defaultHwInfo)[0],
                                                                                                PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});

    ASSERT_NE(nullptr, allocation);
    // initial taskCount must be -1. if not, we may kill allocation before it will be used
    EXPECT_EQ((uint32_t)-1, allocation->getTaskCount(csr->getOsContext().getContextId()));
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
    GmmHelper gmmHelper(nullptr, defaultHwInfo.get());
    auto mockAubCenter = new MockAubCenter(defaultHwInfo.get(), gmmHelper, false, "aubfile", CommandStreamReceiverType::CSR_AUB);
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
    DebugManager.flags.EnableFreeMemory.set(true);
    const uint32_t rootDeviceIndex = 1u;
    MockExecutionEnvironment mockExecutionEnvironment(defaultHwInfo.get(), true, 3);
    MockMemoryManager mockMemoryManager(mockExecutionEnvironment);
    GmmHelper gmmHelper(nullptr, defaultHwInfo.get());
    auto mockManager0 = new MockAubManager();
    auto mockAubCenter0 = new MockAubCenter(defaultHwInfo.get(), gmmHelper, false, "aubfile", CommandStreamReceiverType::CSR_AUB);
    mockAubCenter0->aubManager.reset(mockManager0);
    mockExecutionEnvironment.rootDeviceEnvironments[0]->aubCenter.reset(mockAubCenter0);

    auto mockManager1 = new MockAubManager();
    auto mockAubCenter1 = new MockAubCenter(defaultHwInfo.get(), gmmHelper, false, "aubfile", CommandStreamReceiverType::CSR_AUB);
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

    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());
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
    auto allocation = memoryManager->allocate32BitGraphicsMemory(device->getRootDeviceIndex(), size, nullptr, AllocationType::BUFFER);
    EXPECT_NE(nullptr, allocation);
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_TRUE(allocation->is32BitAllocation());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(MemoryAllocatorTest, givenNotEnoughSpaceInAllocatorWhenAskedFor32bitAllocationThenNullptrIsReturned) {
    size_t size = 0xfffff000;
    auto allocationFirst = memoryManager->allocate32BitGraphicsMemory(device->getRootDeviceIndex(), 0x5000, nullptr, AllocationType::BUFFER);
    auto allocation = memoryManager->allocate32BitGraphicsMemory(device->getRootDeviceIndex(), size, nullptr, AllocationType::BUFFER);
    EXPECT_EQ(nullptr, allocation);
    memoryManager->freeGraphicsMemory(allocationFirst);
}

TEST_F(MemoryAllocatorTest, givenNotEnoughSpaceInAllocatorWhenAskedFor32bitAllocationWithHostPtrThenNullptrIsReturned) {
    size_t size = 0xfffff000;
    void *ptr = (void *)0x10000;
    auto allocationFirst = memoryManager->allocate32BitGraphicsMemory(device->getRootDeviceIndex(), 0x5000, nullptr, AllocationType::BUFFER);
    auto allocation = memoryManager->allocate32BitGraphicsMemory(device->getRootDeviceIndex(), size, ptr, AllocationType::BUFFER);
    EXPECT_EQ(nullptr, allocation);
    memoryManager->freeGraphicsMemory(allocationFirst);
}

TEST_F(MemoryAllocatorTest, givenMemoryManagerWhenAskedFor32bitAllocationWithPtrThen32bitGraphicsAllocationWithGpuAddressIsReturned) {
    size_t size = 10;
    void *ptr = (void *)0x1000;
    auto allocation = memoryManager->allocate32BitGraphicsMemory(device->getRootDeviceIndex(), size, ptr, AllocationType::BUFFER);
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
    AllocationProperties properties{device->getRootDeviceIndex(), false, size, AllocationType::BUFFER, false, device->getDeviceBitfield()};

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, ptr);
    EXPECT_EQ(3u, allocation->fragmentsStorage.fragmentCount);

    EXPECT_EQ(0u, memoryManager->handleFenceCompletionCalled);
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(0u, memoryManager->handleFenceCompletionCalled);
}

TEST_F(MemoryAllocatorTest, GivenShareableEnabledAndDisabledWhenAskedToCreateGrahicsAllocationThenValidAllocationIsReturned) {
    AllocationData allocationData;
    allocationData.type = AllocationType::BUFFER;

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
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), MemoryConstants::pageSize, AllocationType::BUFFER, device->getDeviceBitfield()});
    EXPECT_EQ(0u, allocation->fragmentsStorage.fragmentCount);

    EXPECT_EQ(0u, memoryManager->handleFenceCompletionCalled);
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(1u, memoryManager->handleFenceCompletionCalled);
}

TEST_F(MemoryAllocatorTest, givenStatelessKernelWithPrintfWhenPrintfSurfaceIsCreatedThenPrintfSurfaceIsPatchedWithBaseAddressOffset) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    MockKernelWithInternals kernel(*device);
    MockMultiDispatchInfo multiDispatchInfo(device.get(), kernel.mockKernel);

    kernel.kernelInfo.setBufferAddressingMode(KernelDescriptor::Stateless);
    kernel.kernelInfo.setPrintfSurface(sizeof(uintptr_t), 8);

    auto printfHandler = PrintfHandler::create(multiDispatchInfo, device->getDevice());

    printfHandler->prepareDispatch(multiDispatchInfo);

    auto printfAllocation = printfHandler->getSurface();
    auto allocationAddress = printfAllocation->getGpuAddressToPatch();

    auto printfPatchAddress = ptrOffset(reinterpret_cast<uintptr_t *>(kernel.mockKernel->getCrossThreadData()),
                                        kernel.mockKernel->getKernelInfo().kernelDescriptor.payloadMappings.implicitArgs.printfSurfaceAddress.stateless);

    EXPECT_EQ(allocationAddress, *(uintptr_t *)printfPatchAddress);

    delete printfHandler;
}

HWTEST_F(MemoryAllocatorTest, givenStatefulKernelWithPrintfWhenPrintfSurfaceIsCreatedThenPrintfSurfaceIsPatchedWithCpuAddress) {
    auto rootDeviceIndex = 1u;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get(), rootDeviceIndex));
    MockKernelWithInternals kernel(*device);
    MockMultiDispatchInfo multiDispatchInfo(device.get(), kernel.mockKernel);

    kernel.kernelInfo.setPrintfSurface(sizeof(uintptr_t), 8, 16);

    auto printfHandler = PrintfHandler::create(multiDispatchInfo, device->getDevice());

    printfHandler->prepareDispatch(multiDispatchInfo);

    auto printfAllocation = printfHandler->getSurface();
    auto allocationAddress = printfAllocation->getGpuAddress();

    EXPECT_NE(0u, kernel.mockKernel->getSurfaceStateHeapSize());

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(kernel.mockKernel->getSurfaceStateHeap(),
                  kernel.mockKernel->getKernelInfo().kernelDescriptor.payloadMappings.implicitArgs.printfSurfaceAddress.bindful));
    auto surfaceAddress = surfaceState->getSurfaceBaseAddress();

    EXPECT_EQ(allocationAddress, surfaceAddress);

    delete printfHandler;
}

TEST_F(MemoryAllocatorTest, given32BitDeviceWhenPrintfSurfaceIsCreatedThen32BitAllocationsIsMade) {
    DebugManagerStateRestore dbgRestorer;
    if constexpr (is64bit) {
        DebugManager.flags.Force32bitAddressing.set(true);
        auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

        MockKernelWithInternals kernel(*device);
        kernel.kernelInfo.setPrintfSurface(4, 0);
        for (int i = 0; i < 8; i++) {
            kernel.mockKernel->mockCrossThreadData[i] = 50;
        }

        MockMultiDispatchInfo multiDispatchInfo(device.get(), kernel.mockKernel);

        auto printfHandler = PrintfHandler::create(multiDispatchInfo, device->getDevice());
        printfHandler->prepareDispatch(multiDispatchInfo);

        uint32_t *ptr32Bit = (uint32_t *)kernel.mockKernel->mockCrossThreadData.data();
        auto printfAllocation = printfHandler->getSurface();
        auto allocationAddress = printfAllocation->getGpuAddressToPatch();
        uint32_t allocationAddress32bit = (uint32_t)(uintptr_t)allocationAddress;

        EXPECT_TRUE(printfAllocation->is32BitAllocation());
        EXPECT_EQ(allocationAddress32bit, *ptr32Bit);
        for (int i = 4; i < 8; i++) {
            EXPECT_EQ(50, kernel.mockKernel->mockCrossThreadData[i]);
        }

        delete printfHandler;

        DebugManager.flags.Force32bitAddressing.set(false);
    }
}
HWTEST_F(MemoryAllocatorTest, givenSupportFor1MbAlignmentWhenAllocateGraphicsMemoryThenAlignmentIsSetCorrect) {
    class MockHwHelperHw : public HwHelperHw<FamilyType> {
      public:
        using HwHelperHw<FamilyType>::HwHelperHw;
        bool is1MbAlignmentSupported(const HardwareInfo &hwInfo, bool isCompressionEnabled) const override {
            return isEnable;
        }
        bool isEnable = false;
    };
    auto raiiFactory = RAIIHwHelperFactory<MockHwHelperHw>(defaultHwInfo->platform.eRenderCoreFamily);
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto size = MemoryConstants::pageSize;

    raiiFactory.mockHwHelper.isEnable = true;

    auto osAgnosticMemoryManager = std::make_unique<MemoryManagerCreate<MockMemoryManager>>(true, false, *executionEnvironment);
    osAgnosticMemoryManager->failInDevicePool = true;

    MockAllocationProperties properties(mockRootDeviceIndex, true, size, AllocationType::BUFFER, mockDeviceBitfield);
    properties.flags.preferCompressed = true;

    auto allocationWithEnabled1MbAlignment = osAgnosticMemoryManager->allocateGraphicsMemoryWithProperties(properties, ptr);

    ASSERT_NE(nullptr, allocationWithEnabled1MbAlignment);
    EXPECT_EQ(MemoryConstants::megaByte, osAgnosticMemoryManager->alignAllocationData.alignment);

    osAgnosticMemoryManager->freeGraphicsMemory(allocationWithEnabled1MbAlignment);

    raiiFactory.mockHwHelper.isEnable = false;
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
    EXPECT_FALSE(memoryManager.isLimitedGPUOnType(mockRootDeviceIndex, AllocationType::BUFFER));
    EXPECT_FALSE(memoryManager.isLimitedGPUOnType(mockRootDeviceIndex, AllocationType::IMAGE));
    EXPECT_FALSE(memoryManager.isLimitedGPUOnType(mockRootDeviceIndex, AllocationType::MAP_ALLOCATION));

    memoryManager.is32bit = true;
    if (executionEnvironment.rootDeviceEnvironments[mockRootDeviceIndex]->isFullRangeSvm()) {
        EXPECT_FALSE(memoryManager.isLimitedGPUOnType(mockRootDeviceIndex, AllocationType::BUFFER));
        EXPECT_FALSE(memoryManager.isLimitedGPUOnType(mockRootDeviceIndex, AllocationType::IMAGE));
        EXPECT_FALSE(memoryManager.isLimitedGPUOnType(mockRootDeviceIndex, AllocationType::MAP_ALLOCATION));
    } else {
        EXPECT_TRUE(memoryManager.isLimitedGPUOnType(mockRootDeviceIndex, AllocationType::BUFFER));
        EXPECT_FALSE(memoryManager.isLimitedGPUOnType(mockRootDeviceIndex, AllocationType::IMAGE));
        EXPECT_FALSE(memoryManager.isLimitedGPUOnType(mockRootDeviceIndex, AllocationType::MAP_ALLOCATION));
    }
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenAskedFor32BitAllocationWhenLimitedAllocationIsEnabledThenGpuRangeFromExternalHeapIsAllocatiedAndBaseAddressIsSet) {
    if (is32bit) {
        GTEST_SKIP();
    }

    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    MockMemoryManager memoryManager(*executionEnvironment);
    memoryManager.setForce32BitAllocations(true);
    memoryManager.forceLimitedRangeAllocator(0, 0xFFFFFFFFF);

    AllocationData allocationData;
    memoryManager.getAllocationData(allocationData, {mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::BUFFER, mockDeviceBitfield}, nullptr, StorageInfo{});
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

    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    MockMemoryManager memoryManager(*executionEnvironment);
    memoryManager.forceLimitedRangeAllocator(0, 0xFFFFFFFFF);

    AllocationData allocationData;
    memoryManager.getAllocationData(allocationData, {mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::BUFFER, mockDeviceBitfield}, nullptr, StorageInfo{});
    auto gfxAllocation = memoryManager.allocateGraphicsMemoryWithAlignment(allocationData);
    ASSERT_NE(gfxAllocation, nullptr);
    EXPECT_EQ(gfxAllocation->getGpuBaseAddress(), 0ull);
    EXPECT_EQ(gfxAllocation->getGpuAddress(), memoryManager.getGfxPartition(allocationData.rootDeviceIndex)->getHeapLimit(HeapIndex::HEAP_STANDARD) + 1 - GfxPartition::heapGranularity - MemoryConstants::pageSize);
    memoryManager.freeGraphicsMemory(gfxAllocation);
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenAllocateGraphicsMemoryForImageIsCalledThenGraphicsAllocationIsReturned) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    MockMemoryManager memoryManager(*executionEnvironment);
    ImageDescriptor imgDesc = {};
    imgDesc.imageWidth = 512;
    imgDesc.imageHeight = 1;
    imgDesc.imageType = ImageType::Image2D;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    AllocationData allocationData;
    allocationData.imgInfo = &imgInfo;

    auto imageAllocation = memoryManager.allocateGraphicsMemoryForImage(allocationData);
    ASSERT_NE(nullptr, imageAllocation);
    EXPECT_TRUE(imageAllocation->getDefaultGmm()->resourceParams.Usage == GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE);
    EXPECT_EQ(executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->featureTable.flags.ftrLocalMemory,
              imageAllocation->getDefaultGmm()->resourceParams.Flags.Info.NonLocalOnly);
    memoryManager.freeGraphicsMemory(imageAllocation);
}

TEST(OsAgnosticMemoryManager, givenDestroyedTagAllocationWhenWaitForCompletiionThenWaitForTaskCountIsNotCalled) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    auto memoryManager = new OsAgnosticMemoryManager(executionEnvironment);
    DeviceBitfield deviceBitfield(1);
    std::unique_ptr<CommandStreamReceiver> csr(createCommandStream(executionEnvironment, 0u, deviceBitfield));
    executionEnvironment.memoryManager.reset(memoryManager);
    auto osContext = memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*defaultHwInfo)[0],
                                                                                                                       PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{0, MemoryConstants::pageSize});
    allocation->updateTaskCount(10, osContext->getContextId());

    EXPECT_GT(allocation->getTaskCount(osContext->getContextId()), csr->peekTaskCount());
    memoryManager->waitForEnginesCompletion(*allocation);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenEnabledLocalMemoryWhenAllocateGraphicsMemoryForImageIsCalledThenUseLocalMemoryIsNotSet) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    MockMemoryManager memoryManager(false, true, *executionEnvironment);
    ImageDescriptor imgDesc = {};
    imgDesc.imageWidth = 1;
    imgDesc.imageHeight = 1;
    imgDesc.imageType = ImageType::Image2D;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    AllocationData allocationData;
    allocationData.imgInfo = &imgInfo;

    auto imageAllocation = memoryManager.allocateGraphicsMemoryForImage(allocationData);
    ASSERT_NE(nullptr, imageAllocation);
    EXPECT_FALSE(imgInfo.useLocalMemory);
    memoryManager.freeGraphicsMemory(imageAllocation);
}

TEST(OsAgnosticMemoryManager, givenHostPointerNotRequiringCopyWhenAllocateGraphicsMemoryForImageFromHostPtrIsCalledThenGraphicsAllocationIsReturned) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    MockMemoryManager memoryManager(false, false, *executionEnvironment);

    ImageDescriptor imgDesc = {};
    imgDesc.imageWidth = 4;
    imgDesc.imageHeight = 1;
    imgDesc.imageType = ImageType::Image1D;

    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_RGBA;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, defaultHwInfo->capabilityTable.supportsOcl21Features)->surfaceFormat;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, &surfaceFormat);
    imgInfo.rowPitch = imgDesc.imageWidth * 4;
    imgInfo.slicePitch = imgInfo.rowPitch * imgDesc.imageHeight;
    imgInfo.size = imgInfo.slicePitch;
    imgInfo.linearStorage = true;

    auto hostPtr = alignedMalloc(imgDesc.imageWidth * imgDesc.imageHeight * 4, MemoryConstants::pageSize);

    bool copyRequired = MockMemoryManager::isCopyRequired(imgInfo, hostPtr);
    EXPECT_FALSE(copyRequired);

    AllocationData allocationData;
    allocationData.imgInfo = &imgInfo;
    allocationData.hostPtr = hostPtr;
    allocationData.size = imgInfo.size;

    auto imageAllocation = memoryManager.allocateGraphicsMemoryForImageFromHostPtr(allocationData);
    ASSERT_NE(nullptr, imageAllocation);
    EXPECT_EQ(hostPtr, imageAllocation->getUnderlyingBuffer());

    memoryManager.freeGraphicsMemory(imageAllocation);
    alignedFree(hostPtr);
}

TEST(OsAgnosticMemoryManager, givenHostPointerRequiringCopyWhenAllocateGraphicsMemoryForImageFromHostPtrIsCalledThenNullptrIsReturned) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    MockMemoryManager memoryManager(false, false, *executionEnvironment);

    ImageDescriptor imgDesc = {};
    imgDesc.imageHeight = 4;
    imgDesc.imageWidth = 4;
    imgDesc.imageType = ImageType::Image2D;

    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_RGBA;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, defaultHwInfo->capabilityTable.supportsOcl21Features);

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, &surfaceFormat->surfaceFormat);
    imgInfo.rowPitch = imgDesc.imageWidth * 4;
    imgInfo.slicePitch = imgInfo.rowPitch * imgDesc.imageHeight;
    imgInfo.size = imgInfo.slicePitch;

    auto hostPtr = alignedMalloc(imgDesc.imageWidth * imgDesc.imageHeight * 4, MemoryConstants::pageSize);

    bool copyRequired = MockMemoryManager::isCopyRequired(imgInfo, hostPtr);
    EXPECT_TRUE(copyRequired);

    AllocationData allocationData;
    allocationData.imgInfo = &imgInfo;
    allocationData.hostPtr = hostPtr;

    auto imageAllocation = memoryManager.allocateGraphicsMemoryForImageFromHostPtr(allocationData);
    EXPECT_EQ(nullptr, imageAllocation);

    alignedFree(hostPtr);
}

TEST(OsAgnosticMemoryManager, givenEnabledCrossRootDeviceAccessFlagWhenAllocateGraphicsMemoryForImageFromHostPtrIsCalledThenGraphicsAllocationIsReturned) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    MockMemoryManager memoryManager(false, false, *executionEnvironment);

    ImageDescriptor imgDesc = {};
    imgDesc.imageWidth = 4;
    imgDesc.imageHeight = 1;
    imgDesc.imageType = ImageType::Image1D;

    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_RGBA;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, defaultHwInfo->capabilityTable.supportsOcl21Features);

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, &surfaceFormat->surfaceFormat);
    imgInfo.rowPitch = imgDesc.imageWidth * 4;
    imgInfo.slicePitch = imgInfo.rowPitch * imgDesc.imageHeight;
    imgInfo.size = imgInfo.slicePitch;
    imgInfo.linearStorage = true;

    auto hostPtr = alignedMalloc(imgDesc.imageWidth * imgDesc.imageHeight * 4, MemoryConstants::pageSize);

    AllocationData allocationData;
    allocationData.imgInfo = &imgInfo;
    allocationData.hostPtr = hostPtr;
    allocationData.size = imgInfo.size;
    allocationData.flags.crossRootDeviceAccess = true;

    auto imageAllocation = memoryManager.allocateGraphicsMemoryForImageFromHostPtr(allocationData);
    ASSERT_NE(nullptr, imageAllocation);
    EXPECT_EQ(hostPtr, imageAllocation->getUnderlyingBuffer());

    memoryManager.freeGraphicsMemory(imageAllocation);
    alignedFree(hostPtr);
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerAndUnifiedAuxCapableAllocationWhenMappingThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    OsAgnosticMemoryManager memoryManager(executionEnvironment);

    auto gmm = new Gmm(executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper(), nullptr, 123, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
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
    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());
    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenCompressionEnabledWhenAllocateGraphicsMemoryWithAlignmentIsCalledThenGmmIsAllocated) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.RenderCompressedBuffersEnabled.set(true);
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MockMemoryManager memoryManager(true, false, executionEnvironment);
    AllocationData allocationData;
    allocationData.size = 4096u;
    allocationData.alignment = MemoryConstants::pageSize;
    allocationData.flags.preferCompressed = true;
    auto allocation = memoryManager.allocateGraphicsMemoryWithAlignment(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());
    EXPECT_NE(nullptr, allocation->getDefaultGmm());
    EXPECT_EQ(true, allocation->getDefaultGmm()->isCompressionEnabled);
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
    EXPECT_EQ(MemoryPool::System64KBPages, allocation->getMemoryPool());
    EXPECT_EQ(MemoryConstants::pageSize64k, allocation->getDefaultGmm()->resourceParams.BaseAlignment);
    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWith64KBPagesEnabledAndCompressionEnabledWhenAllocateGraphicsMemory64kbIsCalledThenMemoryPoolIsSystem64KBPages) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.RenderCompressedBuffersEnabled.set(true);
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MockMemoryManager memoryManager(true, false, executionEnvironment);
    AllocationData allocationData;
    allocationData.size = 4096u;
    allocationData.alignment = MemoryConstants::pageSize;
    allocationData.flags.preferCompressed = true;
    auto allocation = memoryManager.allocateGraphicsMemory64kb(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System64KBPages, allocation->getMemoryPool());
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
    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenAllocate32BitGraphicsMemoryWithPtrIsCalledThenMemoryPoolIsSystem4KBPagesWith32BitGpuAddressing) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto size = MemoryConstants::pageSize;

    auto allocation = memoryManager.allocate32BitGraphicsMemory(mockRootDeviceIndex, size, ptr, AllocationType::BUFFER);

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPagesWith32BitGpuAddressing, allocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenAllocate32BitGraphicsMemoryWithoutPtrIsCalledThenMemoryPoolIsSystem4KBPagesWith32BitGpuAddressing) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    void *ptr = nullptr;
    auto size = MemoryConstants::pageSize;

    auto allocation = memoryManager.allocate32BitGraphicsMemory(mockRootDeviceIndex, size, ptr, AllocationType::BUFFER);

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPagesWith32BitGpuAddressing, allocation->getMemoryPool());
    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWith64KBPagesEnabledWhenAllocateGraphicsMemoryThenMemoryPoolIsSystem64KBPages) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(true, false, executionEnvironment);
    if (memoryManager.isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    auto svmAllocation = memoryManager.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::SVM_ZERO_COPY, mockDeviceBitfield});
    EXPECT_NE(nullptr, svmAllocation);
    EXPECT_EQ(MemoryPool::System64KBPages, svmAllocation->getMemoryPool());
    memoryManager.freeGraphicsMemory(svmAllocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWith64KBPagesDisabledWhenAllocateGraphicsMemoryThen4KBGraphicsAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);
    if (memoryManager.isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    auto svmAllocation = memoryManager.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::SVM_ZERO_COPY, mockDeviceBitfield});
    EXPECT_EQ(MemoryPool::System4KBPages, svmAllocation->getMemoryPool());
    memoryManager.freeGraphicsMemory(svmAllocation);
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenCreateGraphicsAllocationFromSharedObjectIsCalledThenGraphicsAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);
    osHandle handle = 1;
    auto size = 4096u;
    AllocationProperties properties(mockRootDeviceIndex, false, size, AllocationType::SHARED_BUFFER, false, mockDeviceBitfield);
    auto sharedAllocation = memoryManager.createGraphicsAllocationFromSharedHandle(handle, properties, false, false);
    EXPECT_NE(nullptr, sharedAllocation);
    EXPECT_FALSE(sharedAllocation->isCoherent());
    EXPECT_NE(nullptr, sharedAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, sharedAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(MemoryPool::SystemCpuInaccessible, sharedAllocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(sharedAllocation);
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenCreateGraphicsAllocationFromSharedObjectIsCalledAndRootDeviceIndexIsSpecifiedThenGraphicsAllocationIsReturnedWithCorrectRootDeviceIndex) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);
    osHandle handle = 1;
    auto size = 4096u;
    AllocationProperties properties(mockRootDeviceIndex, false, size, AllocationType::SHARED_BUFFER, false, false, mockDeviceBitfield);
    EXPECT_EQ(properties.subDevicesBitfield, mockDeviceBitfield);
    EXPECT_EQ(properties.rootDeviceIndex, 0u);

    auto sharedAllocation = memoryManager.createGraphicsAllocationFromSharedHandle(handle, properties, false, false);
    EXPECT_NE(nullptr, sharedAllocation);
    EXPECT_EQ(0u, sharedAllocation->getRootDeviceIndex());
    EXPECT_FALSE(sharedAllocation->isCoherent());
    EXPECT_NE(nullptr, sharedAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, sharedAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(MemoryPool::SystemCpuInaccessible, sharedAllocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(sharedAllocation);
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenCreateGraphicsAllocationFromSharedObjectIsCalledWithSpecificBitnessThen32BitGraphicsAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);
    osHandle handle = 1;
    auto size = 4096u;
    AllocationProperties properties(mockRootDeviceIndex, false, size, AllocationType::SHARED_BUFFER, false, mockDeviceBitfield);
    auto sharedAllocation = memoryManager.createGraphicsAllocationFromSharedHandle(handle, properties, true, false);
    EXPECT_NE(nullptr, sharedAllocation);
    EXPECT_TRUE(sharedAllocation->is32BitAllocation());
    EXPECT_FALSE(sharedAllocation->isCoherent());
    EXPECT_NE(nullptr, sharedAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, sharedAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(MemoryPool::SystemCpuInaccessible, sharedAllocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(sharedAllocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenCreateAllocationFromNtHandleIsCalledThenReturnNullptr) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    auto graphicsAllocation = memoryManager.createGraphicsAllocationFromNTHandle((void *)1, 0, AllocationType::SHARED_IMAGE);
    EXPECT_EQ(nullptr, graphicsAllocation);
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

    auto offsetedGraphicsAddress = graphicsAllocation->getGpuAddress();
    auto offsetedGraphicsAddressToPatch = graphicsAllocation->getGpuAddressToPatch();

    EXPECT_EQ(offsetedGraphicsAddress, graphicsAddress + graphicsAllocation->getAllocationOffset());
    EXPECT_EQ(offsetedGraphicsAddressToPatch, graphicsAddressToPatch + graphicsAllocation->getAllocationOffset());

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
                                         NEO::AllocationType::DEBUG_MODULE_AREA,
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

TEST(OsAgnosticMemoryManager, givenLocalMemoryAndDebugModuleAreaTypeWhenCreatingAllocationThen32BitAllocationWithFrontWindowGpuVaIsReturned) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrLocalMemory = true;
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableLocalMemory.set(true);

    // Ensure family supports local memory
    if (!HwHelper::get(hwInfo.platform.eRenderCoreFamily).isLocalMemoryEnabled(hwInfo)) {
        GTEST_SKIP();
    }

    MockExecutionEnvironment executionEnvironment(&hwInfo);
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, true, executionEnvironment);
    const auto size = MemoryConstants::pageSize64k;

    NEO::AllocationProperties properties{0, true, size,
                                         NEO::AllocationType::DEBUG_MODULE_AREA,
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

TEST(OsAgnosticMemoryManager, givenEnabledLocalMemoryWhenAllocatingGraphicsMemoryForIsaInSystemMemoryThenBaseAddressIsEqualToInternalHeapBaseAddress) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    auto hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    hwInfo->featureTable.flags.ftrLocalMemory = true;

    MockMemoryManager memoryManager(false, true, *executionEnvironment);

    AllocationData allocationData;
    allocationData.type = AllocationType::KERNEL_ISA;
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
    DebugManager.flags.ForceSystemMemoryPlacement.set(1 << (static_cast<uint32_t>(AllocationType::KERNEL_ISA) - 1));

    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    auto hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    hwInfo->featureTable.flags.ftrLocalMemory = true;

    MockMemoryManager memoryManager(false, true, *executionEnvironment);

    size_t kernelIsaSize = 4096;
    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties({0, kernelIsaSize, AllocationType::KERNEL_ISA, 1});
    ASSERT_NE(nullptr, allocation);

    auto instructionHeapBaseAddress = memoryManager.getInternalHeapBaseAddress(0, false);
    auto gmmHelper = memoryManager.getGmmHelper(allocation->getRootDeviceIndex());
    EXPECT_EQ(instructionHeapBaseAddress, gmmHelper->decanonize(allocation->getGpuBaseAddress()));

    memoryManager.freeGraphicsMemory(allocation);
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
    bool defaultEnableDeferredDeleterFlag = DebugManager.flags.EnableDeferredDeleter.get();
    DebugManager.flags.EnableDeferredDeleter.set(true);
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    DebugManager.flags.EnableDeferredDeleter.set(defaultEnableDeferredDeleterFlag);
}

TEST(OsAgnosticMemoryManager, givenDisabledAsyncDeleterFlagWhenMemoryManagerIsCreatedThenAsyncDeleterEnabledIsFalseAndDeleterIsNullptr) {
    bool defaultEnableDeferredDeleterFlag = DebugManager.flags.EnableDeferredDeleter.get();
    DebugManager.flags.EnableDeferredDeleter.set(false);
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    DebugManager.flags.EnableDeferredDeleter.set(defaultEnableDeferredDeleterFlag);
}

TEST(OsAgnosticMemoryManager, GivenEnabled64kbPagesWhenHostMemoryAllocationIsCreatedThenAlignedto64KbAllocationIsReturned) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.Enable64kbpages.set(true);
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(true, false, executionEnvironment);
    if (memoryManager.isLimitedGPU(0)) {
        GTEST_SKIP();
    }

    GraphicsAllocation *galloc = memoryManager.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, MemoryConstants::pageSize64k, AllocationType::BUFFER_HOST_MEMORY, mockDeviceBitfield});
    EXPECT_NE(nullptr, galloc);
    memoryManager.freeGraphicsMemory(galloc);

    galloc = memoryManager.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, MemoryConstants::pageSize64k, AllocationType::BUFFER_HOST_MEMORY, mockDeviceBitfield});
    EXPECT_NE(nullptr, galloc);
    EXPECT_NE(nullptr, galloc->getUnderlyingBuffer());
    size_t size = (executionEnvironment.rootDeviceEnvironments[0u]->getHardwareInfo()->capabilityTable.hostPtrTrackingEnabled) ? MemoryConstants::pageSize64k : MemoryConstants::pageSize;
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
    auto graphicsAllocation = memoryManager.allocate32BitGraphicsMemory(mockRootDeviceIndex, allocationSize, ptr, AllocationType::INTERNAL_HEAP);
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
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    if (executionEnvironment.rootDeviceEnvironments[0]->isFullRangeSvm() || is32bit) {
        return;
    }
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    auto hostPtr = reinterpret_cast<const void *>(0x5001);
    AllocationProperties properties{0, false, 13, AllocationType::EXTERNAL_HOST_PTR, false, mockDeviceBitfield};
    properties.flags.flushL3RequiredForRead = properties.flags.flushL3RequiredForWrite = requiresL3Flush;
    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(properties, hostPtr);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(0u, allocation->fragmentsStorage.fragmentCount);
    EXPECT_EQ(requiresL3Flush, allocation->isFlushL3Required());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST_P(OsAgnosticMemoryManagerWithParams, givenFullGpuAddressSpaceWhenAllocateGraphicsMemoryForHostPtrIsCalledThenAllocationWithFragmentsIsCreated) {
    bool requiresL3Flush = GetParam();
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    if ((!executionEnvironment.rootDeviceEnvironments[0]->isFullRangeSvm()) || !defaultHwInfo->capabilityTable.hostPtrTrackingEnabled) {
        GTEST_SKIP();
    }
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    auto hostPtr = reinterpret_cast<const void *>(0x5001);
    AllocationProperties properties{0, false, 13, AllocationType::EXTERNAL_HOST_PTR, false, mockDeviceBitfield};
    properties.flags.flushL3RequiredForRead = properties.flags.flushL3RequiredForWrite = requiresL3Flush;
    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(properties, hostPtr);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(1u, allocation->fragmentsStorage.fragmentCount);
    EXPECT_EQ(requiresL3Flush, allocation->isFlushL3Required());
    EXPECT_EQ(AllocationType::EXTERNAL_HOST_PTR, allocation->getAllocationType());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST_P(OsAgnosticMemoryManagerWithParams, givenDisabledHostPtrTrackingWhenAllocateGraphicsMemoryForHostPtrIsCalledThenAllocationWithoutFragmentsIsCreated) {
    if (is32bit) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableHostPtrTracking.set(0);

    bool requiresL3Flush = GetParam();
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    if (!executionEnvironment.rootDeviceEnvironments[0]->isFullRangeSvm()) {
        return;
    }
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    auto hostPtr = reinterpret_cast<const void *>(0x5001);

    AllocationProperties properties{0, false, 13, AllocationType::EXTERNAL_HOST_PTR, false, mockDeviceBitfield};
    properties.flags.flushL3RequiredForRead = properties.flags.flushL3RequiredForWrite = requiresL3Flush;
    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(properties, hostPtr);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(0u, allocation->fragmentsStorage.fragmentCount);
    EXPECT_EQ(requiresL3Flush, allocation->isFlushL3Required());

    memoryManager.freeGraphicsMemory(allocation);
}

INSTANTIATE_TEST_CASE_P(OsAgnosticMemoryManagerWithParams,
                        OsAgnosticMemoryManagerWithParams,
                        ::testing::Values(false, true));

TEST(OsAgnosticMemoryManager, givenOsAgnosticMemoryManagerAndFreeMemoryEnabledWhenGraphicsAllocationIsDestroyedThenFreeMemoryOnAubManagerShouldBeCalled) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableFreeMemory.set(true);
    MockExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    GmmHelper gmmHelper(nullptr, defaultHwInfo.get());
    MockAubManager *mockManager = new MockAubManager();
    MockAubCenter *mockAubCenter = new MockAubCenter(defaultHwInfo.get(), gmmHelper, false, "file_name.aub", CommandStreamReceiverType::CSR_AUB);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);
    executionEnvironment.rootDeviceEnvironments[0]->aubCenter.reset(mockAubCenter);

    auto gfxAllocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{0, MemoryConstants::pageSize});
    EXPECT_FALSE(mockManager->freeMemoryCalled);
    memoryManager.freeGraphicsMemory(gfxAllocation);
    EXPECT_TRUE(mockManager->freeMemoryCalled);
}

TEST(OsAgnosticMemoryManager, givenOsAgnosticMemoryManagerAndFreeMemoryDisabledWhenGraphicsAllocationIsDestroyedThenFreeMemoryOnAubManagerShouldBeCalled) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableFreeMemory.set(false);
    MockExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    GmmHelper gmmHelper(nullptr, defaultHwInfo.get());
    MockAubManager *mockManager = new MockAubManager();
    MockAubCenter *mockAubCenter = new MockAubCenter(defaultHwInfo.get(), gmmHelper, false, "file_name.aub", CommandStreamReceiverType::CSR_AUB);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);
    executionEnvironment.rootDeviceEnvironments[0]->aubCenter.reset(mockAubCenter);

    auto gfxAllocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{0, MemoryConstants::pageSize});
    EXPECT_FALSE(mockManager->freeMemoryCalled);
    memoryManager.freeGraphicsMemory(gfxAllocation);
    EXPECT_FALSE(mockManager->freeMemoryCalled);
}

TEST(OsAgnosticMemoryManager, givenOsAgnosticMemoryManagerWhenGpuAddressIsReservedAndFreedThenAddressFromGfxPartitionIsUsed) {
    MockExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(executionEnvironment);

    auto addressRange = memoryManager.reserveGpuAddress(MemoryConstants::pageSize, 0);
    auto gmmHelper = memoryManager.getGmmHelper(0);
    EXPECT_LE(memoryManager.getGfxPartition(0)->getHeapBase(HeapIndex::HEAP_STANDARD), gmmHelper->decanonize(addressRange.address));
    EXPECT_GT(memoryManager.getGfxPartition(0)->getHeapLimit(HeapIndex::HEAP_STANDARD), gmmHelper->decanonize(addressRange.address));

    memoryManager.freeGpuAddress(addressRange, 0);
}

TEST(OsAgnosticMemoryManager, givenOsAgnosticMemoryManagerWhenCheckedForIndirectAllocationsAsPackSupportThenFalseIsReturned) {
    MockExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    EXPECT_FALSE(memoryManager.allowIndirectAllocationsAsPack(0u));
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
    auto osContext = memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*defaultHwInfo)[0],
                                                                                                                       PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));

    MockAllocationProperties properties = {0, MemoryConstants::pageSize};
    properties.osContext = osContext;
    properties.gpuAddress = 0x2000;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());
    EXPECT_EQ(0x2000u, allocation->getGpuAddress());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenOsAgnosticMemoryManagerWhenOsEnabled64kbPagesFalseThenIs64kbPagesEnabledReturnTrue) {
    MockExecutionEnvironment executionEnvironment;
    VariableBackup<bool> osEnabled64kbPagesBackup(&OSInterface::osEnabled64kbPages);
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    auto hwInfo = *defaultHwInfo;
    OSInterface::osEnabled64kbPages = false;
    hwInfo.capabilityTable.ftr64KBpages = true;
    EXPECT_TRUE(memoryManager.is64kbPagesEnabled(&hwInfo));
    OSInterface::osEnabled64kbPages = true;
    EXPECT_TRUE(memoryManager.is64kbPagesEnabled(&hwInfo));
}

TEST(OsAgnosticMemoryManager, givenOsAgnosticMemoryManagerWhenCheckIs64kbPagesEnabledThenOsEnabled64PkbPagesIsNotAffectedReturnedValue) {
    MockExecutionEnvironment executionEnvironment;
    VariableBackup<bool> osEnabled64kbPagesBackup(&OSInterface::osEnabled64kbPages);
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    auto hwInfo = *defaultHwInfo;
    OSInterface::osEnabled64kbPages = true;
    hwInfo.capabilityTable.ftr64KBpages = true;
    EXPECT_TRUE(memoryManager.is64kbPagesEnabled(&hwInfo));
    hwInfo.capabilityTable.ftr64KBpages = false;
    EXPECT_FALSE(memoryManager.is64kbPagesEnabled(&hwInfo));
}

TEST(OsAgnosticMemoryManager, givenOsAgnosticMemoryManagerWithFlagEnable64kbpagesWhenCheckIs64kbPagesEnabledThenProperValueIsReturned) {
    DebugManagerStateRestore dbgRestore;

    MockExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    auto hwInfo = *defaultHwInfo;
    DebugManager.flags.Enable64kbpages.set(true);
    hwInfo.capabilityTable.ftr64KBpages = true;
    EXPECT_TRUE(memoryManager.is64kbPagesEnabled(&hwInfo));
    DebugManager.flags.Enable64kbpages.set(true);
    hwInfo.capabilityTable.ftr64KBpages = false;
    EXPECT_FALSE(memoryManager.is64kbPagesEnabled(&hwInfo));
    DebugManager.flags.Enable64kbpages.set(false);
    hwInfo.capabilityTable.ftr64KBpages = false;
    EXPECT_FALSE(memoryManager.is64kbPagesEnabled(&hwInfo));
    DebugManager.flags.Enable64kbpages.set(false);
    hwInfo.capabilityTable.ftr64KBpages = true;
    EXPECT_FALSE(memoryManager.is64kbPagesEnabled(&hwInfo));
}

TEST(MemoryManager, givenSharedResourceCopyWhenAllocatingGraphicsMemoryThenAllocateGraphicsMemoryForImageIsCalled) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    MockMemoryManager memoryManager(false, true, *executionEnvironment);
    ImageDescriptor imgDesc = {};
    imgDesc.imageHeight = 1;
    imgDesc.imageWidth = 1;
    imgDesc.imageType = ImageType::Image2D;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    AllocationData allocationData;
    allocationData.imgInfo = &imgInfo;
    allocationData.type = AllocationType::SHARED_RESOURCE_COPY;

    auto imageAllocation = memoryManager.allocateGraphicsMemory(allocationData);
    EXPECT_NE(nullptr, imageAllocation);
    EXPECT_TRUE(memoryManager.allocateForImageCalled);
    memoryManager.freeGraphicsMemory(imageAllocation);
}

TEST(MemoryManager, givenImageAndLimitedGPUWhenAllocatingGraphicsMemoryThenAllocate32BitGraphicsMemoryImplIsNotCalled) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    MockMemoryManager memoryManager(false, true, *executionEnvironment);
    memoryManager.limitedGPU = true;
    ImageDescriptor imgDesc = {};
    imgDesc.imageHeight = 1;
    imgDesc.imageWidth = 1;
    imgDesc.imageType = ImageType::Image2D;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    AllocationData allocationData;
    allocationData.imgInfo = &imgInfo;
    allocationData.type = AllocationType::IMAGE;

    auto imageAllocation = memoryManager.allocateGraphicsMemory(allocationData);
    EXPECT_NE(nullptr, imageAllocation);
    EXPECT_TRUE(memoryManager.allocateForImageCalled);
    EXPECT_FALSE(memoryManager.allocate32BitGraphicsMemoryImplCalled);
    memoryManager.freeGraphicsMemory(imageAllocation);
}

TEST(MemoryManager, givenBufferAndLimitedGPUWhenAllocatingGraphicsMemoryThenAllocate32BitGraphicsMemoryImplIsCalled) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    MockMemoryManager memoryManager(false, true, *executionEnvironment);
    memoryManager.limitedGPU = true;

    AllocationData allocationData;
    allocationData.size = 4096u;
    allocationData.type = AllocationType::BUFFER;

    auto bufferAllocation = memoryManager.allocateGraphicsMemory(allocationData);
    EXPECT_NE(nullptr, bufferAllocation);
    EXPECT_FALSE(memoryManager.allocateForImageCalled);
    EXPECT_TRUE(memoryManager.allocate32BitGraphicsMemoryImplCalled);
    memoryManager.freeGraphicsMemory(bufferAllocation);
}

TEST(MemoryManager, givenBufferHostMemoryAndHostPtrTrackingDisabledWhenAllocatingGraphicsMemoryThenAllocateGraphicsMemoryForNonSvmHostPtrIsCalled) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    HardwareInfo hwInfoLocal = *defaultHwInfo;
    hwInfoLocal.capabilityTable.hostPtrTrackingEnabled = false;
    executionEnvironment->rootDeviceEnvironments[0u]->setHwInfo(&hwInfoLocal);
    executionEnvironment->rootDeviceEnvironments[0u]->initGmm();

    MockMemoryManager memoryManager(false, true, *executionEnvironment);
    char bufferData[4096]{};
    AllocationData allocationData{};
    allocationData.hostPtr = bufferData;
    allocationData.size = 4096u;
    allocationData.type = AllocationType::BUFFER_HOST_MEMORY;

    auto bufferAllocation = memoryManager.allocateGraphicsMemory(allocationData);
    EXPECT_NE(nullptr, bufferAllocation);
    EXPECT_FALSE(memoryManager.allocateForImageCalled);
    if (!is32bit) {
        EXPECT_TRUE(memoryManager.allocateGraphicsMemoryForNonSvmHostPtrCalled);
    }
    memoryManager.freeGraphicsMemory(bufferAllocation);
}

TEST(MemoryManager, givenBufferHostMemoryAndHostPtrTrackingDisabledAndForce32bitAllocationsWhenAllocatingGraphicsMemoryThenAllocateGraphicsMemoryForNonSvmHostPtrIsNotCalled) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    HardwareInfo hwInfoLocal = *defaultHwInfo;
    hwInfoLocal.capabilityTable.hostPtrTrackingEnabled = false;
    executionEnvironment->rootDeviceEnvironments[0u]->setHwInfo(&hwInfoLocal);
    executionEnvironment->rootDeviceEnvironments[0u]->initGmm();

    MockMemoryManager memoryManager(false, true, *executionEnvironment);
    memoryManager.setForce32BitAllocations(true);
    char bufferData[4096]{};
    AllocationData allocationData{};
    allocationData.hostPtr = bufferData;
    allocationData.size = 4096u;
    allocationData.type = AllocationType::BUFFER_HOST_MEMORY;

    auto bufferAllocation = memoryManager.allocateGraphicsMemory(allocationData);
    EXPECT_NE(nullptr, bufferAllocation);
    EXPECT_FALSE(memoryManager.allocateGraphicsMemoryForNonSvmHostPtrCalled);
    memoryManager.freeGraphicsMemory(bufferAllocation);
}

TEST(MemoryManager, givenShareableWhenAllocatingGraphicsMemoryThenAllocateShareableIsCalled) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    MockMemoryManager memoryManager(false, true, *executionEnvironment);

    AllocationData allocationData;
    allocationData.size = 4096u;
    allocationData.type = AllocationType::BUFFER;
    allocationData.flags.shareable = true;

    auto allocation = memoryManager.allocateGraphicsMemory(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_TRUE(memoryManager.allocateForShareableCalled);
    memoryManager.freeGraphicsMemory(allocation);
}

TEST_F(MemoryAllocatorTest, GivenSizeWhenGmmIsCreatedThenNonNullPointerIsReturned) {
    Gmm *gmm = new Gmm(device->getGmmHelper(), nullptr, 65536, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
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

    uint32_t taskCountReady = 1;
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
    memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*defaultHwInfo)[0],
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

class MockAlignMallocMemoryManager : public MockMemoryManager {
  public:
    MockAlignMallocMemoryManager(ExecutionEnvironment &executionEnvironment) : MockMemoryManager(executionEnvironment) {
        testMallocRestrictions.minAddress = 0;
        alignMallocRestrictions = nullptr;
        alignMallocCount = 0;
        alignMallocMaxIter = 3;
        returnNullBad = false;
        returnNullGood = false;
    }

    AlignedMallocRestrictions testMallocRestrictions;
    AlignedMallocRestrictions *alignMallocRestrictions;

    static const uintptr_t alignMallocMinAddress = 0x100000;
    static const uintptr_t alignMallocStep = 10;
    int alignMallocMaxIter;
    int alignMallocCount;
    bool returnNullBad;
    bool returnNullGood;

    void *alignedMallocWrapper(size_t size, size_t align) override {
        if (alignMallocCount < alignMallocMaxIter) {
            alignMallocCount++;
            if (!returnNullBad) {
                return reinterpret_cast<void *>(alignMallocMinAddress - alignMallocStep);
            } else {
                return nullptr;
            }
        }
        alignMallocCount = 0;
        if (!returnNullGood) {
            return reinterpret_cast<void *>(alignMallocMinAddress + alignMallocStep);
        } else {
            return nullptr;
        }
    };

    void alignedFreeWrapper(void *) override {
        alignMallocCount = 0;
    }

    AlignedMallocRestrictions *getAlignedMallocRestrictions() override {
        return alignMallocRestrictions;
    }
};

class MockAlignMallocMemoryManagerTest : public MemoryAllocatorTest {
  public:
    MockAlignMallocMemoryManager *alignedMemoryManager = nullptr;

    void SetUp() override {
        MemoryAllocatorTest::SetUp();

        MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
        alignedMemoryManager = new (std::nothrow) MockAlignMallocMemoryManager(executionEnvironment);
        // assert we have memory manager
        ASSERT_NE(nullptr, memoryManager);
    }

    void TearDown() override {
        alignedMemoryManager->alignedFreeWrapper(nullptr);
        delete alignedMemoryManager;

        MemoryAllocatorTest::TearDown();
    }
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

using GraphicsAllocationTests = ::testing::Test;

HWTEST_F(GraphicsAllocationTests, givenAllocationUsedOnlyByNonDefaultCsrWhenCheckingUsageBeforeDestroyThenStoreItAsTemporaryAllocation) {
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    auto &lowPriorityEngine = device->getEngine(device->getHardwareInfo().capabilityTable.defaultEngineType, EngineUsage::LowPriority);

    auto nonDefaultOsContext = lowPriorityEngine.osContext;
    auto nonDefaultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(lowPriorityEngine.commandStreamReceiver);

    auto memoryManager = device->getExecutionEnvironment()->memoryManager.get();
    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

    nonDefaultCsr->taskCount = *nonDefaultCsr->getTagAddress() + 1;
    nonDefaultCsr->latestFlushedTaskCount = *nonDefaultCsr->getTagAddress() + 1;
    graphicsAllocation->updateTaskCount(*nonDefaultCsr->getTagAddress() + 1, nonDefaultOsContext->getContextId());

    memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(graphicsAllocation);
    EXPECT_NE(nullptr, nonDefaultCsr->getInternalAllocationStorage()->getDeferredAllocations().peekHead());
    (*nonDefaultCsr->getTagAddress())++;
    // no need to call freeGraphicsAllocation
}

HWTEST_F(GraphicsAllocationTests, givenAllocationUsedOnlyByNonDefaultDeviceWhenCheckingUsageBeforeDestroyThenStoreItAsTemporaryAllocation) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    auto device = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 0u));
    auto &defaultCommandStreamReceiver = device->getGpgpuCommandStreamReceiver();
    auto &nonDefaultCommandStreamReceiver = static_cast<UltCommandStreamReceiver<FamilyType> &>(*device->commandStreamReceivers[1]);
    auto memoryManager = executionEnvironment->memoryManager.get();
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
    (*nonDefaultCommandStreamReceiver.getTagAddress())++;
    // no need to call freeGraphicsAllocation
}

HWTEST_F(GraphicsAllocationTests, givenAllocationUsedByManyOsContextsWhenCheckingUsageBeforeDestroyThenMultiContextDestructorIsUsedForWaitingForAllOsContexts) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    auto memoryManager = new MockMemoryManager(false, false, *executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);
    auto multiContextDestructor = new MockDeferredDeleter();
    multiContextDestructor->expectDrainBlockingValue(false);
    memoryManager->multiContextResourceDestructor.reset(multiContextDestructor);

    auto device = std::unique_ptr<MockDevice>(MockDevice::create<MockDevice>(executionEnvironment, 0u));

    auto &lowPriorityEngine = device->getEngine(device->getHardwareInfo().capabilityTable.defaultEngineType, EngineUsage::LowPriority);

    auto nonDefaultOsContext = lowPriorityEngine.osContext;
    auto nonDefaultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(lowPriorityEngine.commandStreamReceiver);
    auto defaultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getDefaultEngine().commandStreamReceiver);
    auto defaultOsContext = device->getDefaultEngine().osContext;

    EXPECT_FALSE(defaultOsContext->isLowPriority());
    EXPECT_TRUE(nonDefaultOsContext->isLowPriority());

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});

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

TEST(GraphicsAllocation, givenSharedHandleBasedConstructorWhenGraphicsAllocationIsCreatedThenGpuAddressHasCorrectValue) {
    uintptr_t address = 0xf0000000;
    void *addressWithTrailingBitSet = reinterpret_cast<void *>(address);
    uint64_t expectedGpuAddress = 0xf0000000;
    osHandle sharedHandle{};
    GmmHelper gmmHelper(nullptr, defaultHwInfo.get());
    auto canonizedGpuAddress = gmmHelper.canonize(castToUint64(addressWithTrailingBitSet));
    GraphicsAllocation graphicsAllocation(0, AllocationType::UNKNOWN, addressWithTrailingBitSet, 1u, sharedHandle, MemoryPool::MemoryNull, 0u, canonizedGpuAddress);
    EXPECT_EQ(expectedGpuAddress, graphicsAllocation.getGpuAddress());
}

TEST(ResidencyDataTest, givenOsContextWhenItIsRegisteredToMemoryManagerThenRefCountIncreases) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    auto memoryManager = new MockMemoryManager(false, false, executionEnvironment);
    executionEnvironment.memoryManager.reset(memoryManager);
    DeviceBitfield deviceBitfield(1);
    std::unique_ptr<CommandStreamReceiver> csr(createCommandStream(executionEnvironment, 0u, deviceBitfield));
    memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*defaultHwInfo)[0],
                                                                                                      PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));
    EXPECT_EQ(1u, memoryManager->getRegisteredEnginesCount());
    EXPECT_EQ(1, memoryManager->registeredEngines[0].osContext->getRefInternalCount());
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
    memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*defaultHwInfo)[0],
                                                                                                      preemptionMode,
                                                                                                      deviceBitfield));
    EXPECT_EQ(2u, memoryManager->registeredEngines[0].osContext->getNumSupportedDevices());
    EXPECT_EQ(deviceBitfield, memoryManager->registeredEngines[0].osContext->getDeviceBitfield());
    EXPECT_EQ(preemptionMode, memoryManager->registeredEngines[0].osContext->getPreemptionMode());
}

TEST(ResidencyDataTest, givenTwoOsContextsWhenTheyAreRegisteredFromHigherToLowerThenProperSizeIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get(), true, 2u);
    auto memoryManager = new MockMemoryManager(false, false, executionEnvironment);
    executionEnvironment.memoryManager.reset(memoryManager);
    DeviceBitfield deviceBitfield(1);
    std::unique_ptr<CommandStreamReceiver> csr(createCommandStream(executionEnvironment, 0u, deviceBitfield));
    std::unique_ptr<CommandStreamReceiver> csr1(createCommandStream(executionEnvironment, 1u, deviceBitfield));
    memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*defaultHwInfo)[0],
                                                                                                      PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));
    memoryManager->createAndRegisterOsContext(csr1.get(), EngineDescriptorHelper::getDefaultDescriptor(HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*defaultHwInfo)[0],
                                                                                                       PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));
    EXPECT_EQ(2u, memoryManager->getRegisteredEnginesCount());
    EXPECT_EQ(1, memoryManager->registeredEngines[0].osContext->getRefInternalCount());
    EXPECT_EQ(1, memoryManager->registeredEngines[1].osContext->getRefInternalCount());
}

TEST(ResidencyDataTest, givenGpgpuEnginesWhenAskedForMaxOsContextCountThenValueIsGreaterOrEqual) {
    auto &engines = HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*defaultHwInfo);
    EXPECT_TRUE(MemoryManager::maxOsContextCount >= engines.size());
}

TEST(ResidencyDataTest, givenResidencyDataWhenUpdateCompletionDataIsCalledThenItIsProperlyUpdated) {
    struct MockResidencyData : public ResidencyData {
        using ResidencyData::lastFenceValues;
        using ResidencyData::ResidencyData;
    };

    MockResidencyData residency(MemoryManager::maxOsContextCount);

    MockOsContext osContext(0u, EngineDescriptorHelper::getDefaultDescriptor(HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*defaultHwInfo)[0],
                                                                             PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));
    MockOsContext osContext2(1u, EngineDescriptorHelper::getDefaultDescriptor(HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*defaultHwInfo)[0],
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

TEST(MemoryManagerTest, givenExecutionEnvrionmentWithCleanedRootDeviceExecutionsWhenFreeGraphicsMemoryIsCalledThenMemoryManagerDoesntCrash) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{0, MemoryConstants::pageSize});

    EXPECT_NE(nullptr, allocation);

    executionEnvironment.rootDeviceEnvironments.clear();

    EXPECT_NO_THROW(memoryManager.freeGraphicsMemory(allocation));
}

TEST(MemoryManagerTest, givenAllocationTypesThatMayNeedL3FlushWhenCallingGetAllocationDataThenFlushL3FlagIsCorrectlySet) {
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, 1, AllocationType::UNKNOWN, mockDeviceBitfield);
    properties.flags.flushL3RequiredForRead = 1;
    properties.flags.flushL3RequiredForWrite = 1;

    AllocationType allocationTypesThatMayNeedL3Flush[] = {
        AllocationType::BUFFER,
        AllocationType::BUFFER_HOST_MEMORY, AllocationType::EXTERNAL_HOST_PTR,
        AllocationType::GLOBAL_SURFACE, AllocationType::IMAGE,
        AllocationType::PIPE, AllocationType::SHARED_IMAGE,
        AllocationType::SHARED_BUFFER, AllocationType::SHARED_RESOURCE_COPY,
        AllocationType::SVM_ZERO_COPY, AllocationType::SVM_GPU,
        AllocationType::SVM_CPU, AllocationType::WRITE_COMBINED,
        AllocationType::MAP_ALLOCATION};

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

TEST(MemoryManagerTest, givenNullHostPtrWhenIsCopyRequiredIsCalledThenFalseIsReturned) {
    ImageInfo imgInfo{};
    EXPECT_FALSE(MockMemoryManager::isCopyRequired(imgInfo, nullptr));
}

TEST(MemoryManagerTest, givenAllowedTilingWhenIsCopyRequiredIsCalledThenTrueIsReturned) {
    ImageInfo imgInfo{};
    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 1;
    imageDesc.image_height = 1;

    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, defaultHwInfo->capabilityTable.supportsOcl21Features);

    imgInfo.imgDesc = Image::convertDescriptor(imageDesc);
    imgInfo.surfaceFormat = &surfaceFormat->surfaceFormat;
    imgInfo.rowPitch = imageDesc.image_width * surfaceFormat->surfaceFormat.ImageElementSizeInBytes;
    imgInfo.slicePitch = imgInfo.rowPitch * imageDesc.image_height;
    imgInfo.size = imgInfo.slicePitch;

    char memory;

    EXPECT_TRUE(MockMemoryManager::isCopyRequired(imgInfo, &memory));
}

TEST(MemoryManagerTest, givenDifferentRowPitchWhenIsCopyRequiredIsCalledThenTrueIsReturned) {
    ImageInfo imgInfo{};
    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 1;
    imageDesc.image_height = 1;
    imageDesc.image_row_pitch = 10;

    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, defaultHwInfo->capabilityTable.supportsOcl21Features);

    imgInfo.imgDesc = Image::convertDescriptor(imageDesc);
    imgInfo.surfaceFormat = &surfaceFormat->surfaceFormat;
    imgInfo.rowPitch = imageDesc.image_width * surfaceFormat->surfaceFormat.ImageElementSizeInBytes;
    imgInfo.slicePitch = imgInfo.rowPitch * imageDesc.image_height;
    imgInfo.size = imgInfo.slicePitch;

    char memory[10];

    EXPECT_TRUE(MockMemoryManager::isCopyRequired(imgInfo, memory));
}

TEST(MemoryManagerTest, givenDifferentSlicePitchAndTilingNotAllowedWhenIsCopyRequiredIsCalledThenTrueIsReturned) {
    ImageInfo imgInfo{};

    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, defaultHwInfo->capabilityTable.supportsOcl21Features);

    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 4;
    imageDesc.image_height = 2;
    imageDesc.image_slice_pitch = imageDesc.image_width * (imageDesc.image_height + 3) * surfaceFormat->surfaceFormat.ImageElementSizeInBytes;

    imgInfo.imgDesc = Image::convertDescriptor(imageDesc);
    imgInfo.surfaceFormat = &surfaceFormat->surfaceFormat;
    imgInfo.rowPitch = imageDesc.image_width * surfaceFormat->surfaceFormat.ImageElementSizeInBytes;
    imgInfo.slicePitch = imgInfo.rowPitch * imageDesc.image_height;
    imgInfo.size = imgInfo.slicePitch;
    char memory[8];

    EXPECT_TRUE(MockMemoryManager::isCopyRequired(imgInfo, memory));
}

TEST(MemoryManagerTest, givenNotCachelinAlignedPointerWhenIsCopyRequiredIsCalledThenTrueIsReturned) {
    ImageInfo imgInfo{};

    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, defaultHwInfo->capabilityTable.supportsOcl21Features);

    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 4096;
    imageDesc.image_height = 1;

    imgInfo.imgDesc = Image::convertDescriptor(imageDesc);
    imgInfo.surfaceFormat = &surfaceFormat->surfaceFormat;
    imgInfo.rowPitch = imageDesc.image_width * surfaceFormat->surfaceFormat.ImageElementSizeInBytes;
    imgInfo.slicePitch = imgInfo.rowPitch * imageDesc.image_height;
    imgInfo.size = imgInfo.slicePitch;
    char memory[8];

    EXPECT_TRUE(MockMemoryManager::isCopyRequired(imgInfo, &memory[1]));
}

TEST(MemoryManagerTest, givenCachelineAlignedPointerAndProperDescriptorValuesWhenIsCopyRequiredIsCalledThenFalseIsReturned) {
    ImageInfo imgInfo{};

    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, defaultHwInfo->capabilityTable.supportsOcl21Features);

    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 2;
    imageDesc.image_height = 1;

    imgInfo.imgDesc = Image::convertDescriptor(imageDesc);
    imgInfo.surfaceFormat = &surfaceFormat->surfaceFormat;
    imgInfo.rowPitch = imageDesc.image_width * surfaceFormat->surfaceFormat.ImageElementSizeInBytes;
    imgInfo.slicePitch = imgInfo.rowPitch * imageDesc.image_height;
    imgInfo.size = imgInfo.slicePitch;
    imgInfo.linearStorage = true;

    auto hostPtr = alignedMalloc(imgInfo.size, MemoryConstants::cacheLineSize);

    EXPECT_FALSE(MockMemoryManager::isCopyRequired(imgInfo, hostPtr));
    alignedFree(hostPtr);
}

TEST(MemoryManagerTest, givenForcedLinearImages3DImageAndProperDescriptorValuesWhenIsCopyRequiredIsCalledThenFalseIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ForceLinearImages.set(true);
    auto &hwHelper = HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily);

    ImageInfo imgInfo{};

    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, defaultHwInfo->capabilityTable.supportsOcl21Features);

    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    imageDesc.image_width = 2;
    imageDesc.image_height = 2;
    imageDesc.image_depth = 2;

    imgInfo.imgDesc = Image::convertDescriptor(imageDesc);
    imgInfo.surfaceFormat = &surfaceFormat->surfaceFormat;
    imgInfo.rowPitch = imageDesc.image_width * surfaceFormat->surfaceFormat.ImageElementSizeInBytes;
    imgInfo.slicePitch = imgInfo.rowPitch * imageDesc.image_height;
    imgInfo.size = imgInfo.slicePitch;
    imgInfo.linearStorage = hwHelper.isLinearStoragePreferred(false, Image::isImage1d(Image::convertDescriptor(imgInfo.imgDesc)), false);

    auto hostPtr = alignedMalloc(imgInfo.size, MemoryConstants::cacheLineSize);

    EXPECT_FALSE(MockMemoryManager::isCopyRequired(imgInfo, hostPtr));
    alignedFree(hostPtr);
}

class HeapSelectorTest : public Test<ClDeviceFixture> {
  public:
    using BaseClass = ClDeviceFixture;
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
    GraphicsAllocation allocation{0, AllocationType::KERNEL_ISA, nullptr, 0, 0, MemoryPool::MemoryNull, MemoryManager::maxOsContextCount, 0llu};
    allocation.set32BitAllocation(true);
    EXPECT_EQ(MemoryManager::selectInternalHeap(allocation.isAllocatedInLocalMemoryPool()), memoryManager->selectHeap(&allocation, false, false, false));

    GraphicsAllocation allocation2{0, AllocationType::KERNEL_ISA_INTERNAL, nullptr, 0, 0, MemoryPool::MemoryNull, MemoryManager::maxOsContextCount, 0llu};
    allocation2.set32BitAllocation(true);
    EXPECT_EQ(MemoryManager::selectInternalHeap(allocation2.isAllocatedInLocalMemoryPool()), memoryManager->selectHeap(&allocation2, false, false, false));
}

TEST_F(HeapSelectorTest, givenNon32bitInternalAllocationWhenSelectingHeapThenInternalHeapIsUsed) {
    GraphicsAllocation allocation{0, AllocationType::KERNEL_ISA, nullptr, 0, 0, MemoryPool::MemoryNull, MemoryManager::maxOsContextCount, 0llu};
    allocation.set32BitAllocation(false);
    EXPECT_EQ(MemoryManager::selectInternalHeap(allocation.isAllocatedInLocalMemoryPool()), memoryManager->selectHeap(&allocation, false, false, false));

    GraphicsAllocation allocation2{0, AllocationType::KERNEL_ISA_INTERNAL, nullptr, 0, 0, MemoryPool::MemoryNull, MemoryManager::maxOsContextCount, 0llu};
    allocation2.set32BitAllocation(false);
    EXPECT_EQ(MemoryManager::selectInternalHeap(allocation2.isAllocatedInLocalMemoryPool()), memoryManager->selectHeap(&allocation2, false, false, false));
}

TEST_F(HeapSelectorTest, given32bitExternalAllocationWhenSelectingHeapThenExternalHeapIsUsed) {
    GraphicsAllocation allocation{0, AllocationType::UNKNOWN, nullptr, 0, 0, MemoryPool::MemoryNull, MemoryManager::maxOsContextCount, 0llu};
    allocation.set32BitAllocation(true);
    EXPECT_EQ(MemoryManager::selectExternalHeap(allocation.isAllocatedInLocalMemoryPool()), memoryManager->selectHeap(&allocation, false, false, false));
}

TEST_F(HeapSelectorTest, givenLimitedAddressSpaceWhenSelectingHeapForExternalAllocationThenStandardHeapIsUsed) {
    GraphicsAllocation allocation{0, AllocationType::UNKNOWN, nullptr, 0, 0, MemoryPool::MemoryNull, MemoryManager::maxOsContextCount, 0llu};
    EXPECT_EQ(HeapIndex::HEAP_STANDARD, memoryManager->selectHeap(&allocation, true, false, false));
}

TEST_F(HeapSelectorTest, givenFullAddressSpaceWhenSelectingHeapForExternalAllocationWithPtrThenSvmHeapIsUsed) {
    GraphicsAllocation allocation{0, AllocationType::UNKNOWN, nullptr, 0, 0, MemoryPool::MemoryNull, MemoryManager::maxOsContextCount, 0llu};
    EXPECT_EQ(HeapIndex::HEAP_SVM, memoryManager->selectHeap(&allocation, true, true, false));
}

TEST_F(HeapSelectorTest, givenFullAddressSpaceWhenSelectingHeapForExternalAllocationWithoutPtrAndResourceIs64KSuitableThenStandard64kHeapIsUsed) {
    GraphicsAllocation allocation{0, AllocationType::UNKNOWN, nullptr, 0, 0, MemoryPool::MemoryNull, MemoryManager::maxOsContextCount, 0llu};
    auto rootDeviceEnvironment = platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0].get();
    auto gmm = std::make_unique<Gmm>(rootDeviceEnvironment->getGmmHelper(), nullptr, 0, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, StorageInfo{}, true);
    auto resourceInfo = static_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    resourceInfo->is64KBPageSuitableValue = true;
    allocation.setDefaultGmm(gmm.get());
    EXPECT_EQ(HeapIndex::HEAP_STANDARD64KB, memoryManager->selectHeap(&allocation, false, true, false));
}

TEST_F(HeapSelectorTest, givenFullAddressSpaceWhenSelectingHeapForExternalAllocationWithoutPtrAndResourceIsNot64KSuitableThenStandardHeapIsUsed) {
    GraphicsAllocation allocation{0, AllocationType::UNKNOWN, nullptr, 0, 0, MemoryPool::MemoryNull, MemoryManager::maxOsContextCount, 0llu};
    auto rootDeviceEnvironment = platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0].get();
    auto gmm = std::make_unique<Gmm>(rootDeviceEnvironment->getGmmHelper(), nullptr, 0, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, StorageInfo{}, true);
    auto resourceInfo = static_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    resourceInfo->is64KBPageSuitableValue = false;
    allocation.setDefaultGmm(gmm.get());
    EXPECT_EQ(HeapIndex::HEAP_STANDARD, memoryManager->selectHeap(&allocation, false, true, false));
}

TEST_F(HeapSelectorTest, givenFullAddressSpaceWhenSelectingHeapForNullAllocationWithoutPtrThenStandardHeapIsUsed) {
    EXPECT_EQ(HeapIndex::HEAP_STANDARD, memoryManager->selectHeap(nullptr, false, true, false));
}

TEST_F(HeapSelectorTest, givenLimitedAddressSpaceWhenSelectingHeapForNullAllocationWithoutPtrThenStandardHeapIsUsed) {
    EXPECT_EQ(HeapIndex::HEAP_STANDARD, memoryManager->selectHeap(nullptr, false, false, false));
}

TEST_F(HeapSelectorTest, givenDebugModuleAreaAllocationAndUseFrontWindowWhenSelectingHeapThenInternalFrontWindowHeapIsReturned) {
    GraphicsAllocation allocation{0, AllocationType::DEBUG_MODULE_AREA, nullptr, 0, 0, 0, MemoryPool::MemoryNull, 1};
    allocation.set32BitAllocation(true);
    EXPECT_EQ(HeapIndex::HEAP_INTERNAL_FRONT_WINDOW, memoryManager->selectHeap(&allocation, false, false, true));
}

TEST_F(HeapSelectorTest, givenDebugModuleAreaAllocationInLocalMemoryAndUseFrontWindowWhenSelectingHeapThenInternalDeviceFrontWindowHeapIsReturned) {
    GraphicsAllocation allocation{0, AllocationType::DEBUG_MODULE_AREA, nullptr, 0, 0, 0, MemoryPool::LocalMemory, 1};
    allocation.set32BitAllocation(true);
    EXPECT_TRUE(allocation.isAllocatedInLocalMemoryPool());
    EXPECT_EQ(HeapIndex::HEAP_INTERNAL_DEVICE_FRONT_WINDOW, memoryManager->selectHeap(&allocation, false, false, true));
}

TEST(MemoryAllocationTest, givenAllocationTypeWhenPassedToMemoryAllocationConstructorThenAllocationTypeIsStored) {
    MemoryAllocation allocation{0, AllocationType::COMMAND_BUFFER, nullptr, nullptr, 0, 0, 0,
                                MemoryPool::MemoryNull, false, false, MemoryManager::maxOsContextCount};
    EXPECT_EQ(AllocationType::COMMAND_BUFFER, allocation.getAllocationType());
}

TEST(MemoryAllocationTest, givenMemoryPoolWhenPassedToMemoryAllocationConstructorThenMemoryPoolIsStored) {
    MemoryAllocation allocation{0, AllocationType::COMMAND_BUFFER, nullptr, nullptr, 0, 0, 0,
                                MemoryPool::System64KBPages, false, false, MemoryManager::maxOsContextCount};
    EXPECT_EQ(MemoryPool::System64KBPages, allocation.getMemoryPool());
}

TEST_F(MemoryAllocatorTest, whenCommandStreamerIsRegisteredThenReturnAssociatedEngineControl) {
    auto engineControl = memoryManager->getRegisteredEngineForCsr(csr);
    ASSERT_NE(nullptr, engineControl);
    EXPECT_EQ(csr, engineControl->commandStreamReceiver);
}

TEST_F(MemoryAllocatorTest, whenCommandStreamerIsNotRegisteredThenReturnNullEngineControl) {
    CommandStreamReceiver *dummyCsr = reinterpret_cast<CommandStreamReceiver *>(0x1);
    auto engineControl = memoryManager->getRegisteredEngineForCsr(dummyCsr);
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
};

TEST(MemoryManagerTest, whenMemoryManagerReturnsNullptrThenAllocateGlobalsSurfaceAlsoReturnsNullptr) {
    MockClDevice device{new MockDevice};
    auto deviceBitfield = device.getDeviceBitfield();
    auto memoryManager = new MemoryManagerWithFailure{*device.getExecutionEnvironment()};
    device.injectMemoryManager(memoryManager);

    WhiteBox<NEO::LinkerInput> linkerInput;
    linkerInput.traits.exportsGlobalConstants = true;
    linkerInput.traits.exportsGlobalVariables = true;
    memoryManager->recentlyPassedDeviceBitfield = {};
    GraphicsAllocation *allocation = allocateGlobalsSurface(nullptr, device.getDevice(), 1024, false, &linkerInput, nullptr);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(deviceBitfield, memoryManager->recentlyPassedDeviceBitfield);

    auto svmAllocsManager = std::make_unique<SVMAllocsManager>(device.getMemoryManager(), false);
    memoryManager->recentlyPassedDeviceBitfield = {};
    allocation = allocateGlobalsSurface(svmAllocsManager.get(), device.getDevice(), 1024, false, &linkerInput, nullptr);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(deviceBitfield, memoryManager->recentlyPassedDeviceBitfield);
}

HWTEST_F(MemoryAllocatorTest, givenMemoryManagerWhenEnableHostPtrTrackingFlagIsSetTo0ThenHostPointerTrackingIsDisabled) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableHostPtrTracking.set(0);
    EXPECT_FALSE(memoryManager->isHostPointerTrackingEnabled(0u));
}

HWTEST_F(MemoryAllocatorTest, givenMemoryManagerWhenEnableHostPtrTrackingFlagIsNotSetTo1ThenHostPointerTrackingIsEnabled) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableHostPtrTracking.set(1);
    EXPECT_TRUE(memoryManager->isHostPointerTrackingEnabled(0u));
}

HWTEST_F(MemoryAllocatorTest, givenMemoryManagerWhenEnableHostPtrTrackingFlagIsSetNotSetThenHostPointerTrackingDependsOnCapabilityTable) {
    if (is32bit) {
        EXPECT_TRUE(memoryManager->isHostPointerTrackingEnabled(0u));
    } else {
        EXPECT_EQ(device->getHardwareInfo().capabilityTable.hostPtrTrackingEnabled, memoryManager->isHostPointerTrackingEnabled(0u));
    }
}

using MemoryManagerMultiRootDeviceTests = MultiRootDeviceFixture;

TEST_F(MemoryManagerMultiRootDeviceTests, WhenAllocatingGlobalSurfaceThenItHasCorrectRootDeviceIndex) {
    if (device1->getMemoryManager()->isLimitedRange(expectedRootDeviceIndex)) {
        delete context->svmAllocsManager;
        context->svmAllocsManager = nullptr;
    }

    std::vector<unsigned char> initData(1024, 0x5B);
    WhiteBox<NEO::LinkerInput> linkerInput;
    linkerInput.traits.exportsGlobalConstants = true;
    linkerInput.traits.exportsGlobalVariables = true;
    GraphicsAllocation *allocation = allocateGlobalsSurface(context->svmAllocsManager, device1->getDevice(), initData.size(), false, &linkerInput, initData.data());

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(expectedRootDeviceIndex, allocation->getRootDeviceIndex());

    if (device1->getMemoryManager()->isLimitedRange(expectedRootDeviceIndex)) {
        device1->getMemoryManager()->freeGraphicsMemory(allocation);
    } else {
        context->getSVMAllocsManager()->freeSVMAlloc(allocation->getUnderlyingBuffer());
    }
}

TEST_F(MemoryManagerMultiRootDeviceTests, GivenMultipleRootDevicesWhenMemoryManagerInitializedThenIsaLocalMemoryPlacementIsCheckedForEveryRootDevice) {
    EXPECT_EQ(3u, mockMemoryManager->isaInLocalMemory.size());
}

HWTEST_F(MemoryAllocatorTest, givenMemoryManagerWhen64BitAndHostPtrTrackingDisabledThenNonSvmHostPtrUsageIsSet) {

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableHostPtrTracking.set(0);

    bool expectedValue = !is32bit;

    auto result = memoryManager->useNonSvmHostPtrAlloc(AllocationType::EXTERNAL_HOST_PTR, 0u);
    EXPECT_EQ(expectedValue, result);

    result = memoryManager->useNonSvmHostPtrAlloc(AllocationType::MAP_ALLOCATION, 0u);
    EXPECT_EQ(expectedValue, result);
}

HWTEST_F(MemoryAllocatorTest, givenMemoryManagerWhenHostPtrTrackingModeThenNonSvmHostPtrUsageIsSet) {
    memoryManager->setForceNonSvmForExternalHostPtr(true);

    auto result = memoryManager->useNonSvmHostPtrAlloc(AllocationType::EXTERNAL_HOST_PTR, 0u);
    EXPECT_EQ(true, result);
    result = memoryManager->useNonSvmHostPtrAlloc(AllocationType::BUFFER_HOST_MEMORY, 0u);
    EXPECT_EQ(false, result);
}

HWTEST_F(MemoryAllocatorTest, givenMemoryManagerWhenHostPtrTrackingModeThenNonSvmBufferIsNotSet) {
    HardwareInfo hwInfoLocal = *defaultHwInfo;
    hwInfoLocal.capabilityTable.hostPtrTrackingEnabled = true;
    memoryManager->peekExecutionEnvironment().rootDeviceEnvironments[0u]->setHwInfo(&hwInfoLocal);
    int buffer = 0;
    EXPECT_FALSE(memoryManager->isNonSvmBuffer(&buffer, AllocationType::EXTERNAL_HOST_PTR, 0u));
    EXPECT_FALSE(memoryManager->isNonSvmBuffer(&buffer, AllocationType::BUFFER_HOST_MEMORY, 0u));
}

HWTEST_F(MemoryAllocatorTest, givenMemoryManagerWhenHostPtrTrackingDisabledAnd64bitsThenNonSvmBufferIsSetForBufferHostMemory) {
    HardwareInfo hwInfoLocal = *defaultHwInfo;
    hwInfoLocal.capabilityTable.hostPtrTrackingEnabled = false;
    memoryManager->peekExecutionEnvironment().rootDeviceEnvironments[0u]->setHwInfo(&hwInfoLocal);
    int buffer = 0;
    EXPECT_FALSE(memoryManager->isNonSvmBuffer(&buffer, AllocationType::EXTERNAL_HOST_PTR, 0u));
    EXPECT_EQ(!is32bit, memoryManager->isNonSvmBuffer(&buffer, AllocationType::BUFFER_HOST_MEMORY, 0u));
}

HWTEST_F(MemoryAllocatorTest, givenMemoryManagerWhenHostPtrTrackingEnabledThenNonSvmHostPtrUsageDependsOnFullRangeSvm) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableHostPtrTracking.set(1);

    auto result = memoryManager->useNonSvmHostPtrAlloc(AllocationType::EXTERNAL_HOST_PTR, 0u);
    EXPECT_EQ(!executionEnvironment->rootDeviceEnvironments[0]->isFullRangeSvm() && !is32bit, result);

    result = memoryManager->useNonSvmHostPtrAlloc(AllocationType::MAP_ALLOCATION, 0u);
    EXPECT_EQ(!executionEnvironment->rootDeviceEnvironments[0]->isFullRangeSvm() && !is32bit, result);
}

using PageTableManagerTest = ::testing::Test;
using namespace ::testing;
HWTEST_F(PageTableManagerTest, givenPageTableManagerWhenMapAuxGpuVaThenForAllEnginesWithPageTableUpdateAuxTableAreCalled) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(2);
    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }

    auto memoryManager = new MockMemoryManager(false, false, *executionEnvironment);

    executionEnvironment->memoryManager.reset(memoryManager);
    auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(*executionEnvironment, 1u, 1));
    auto csr2 = std::unique_ptr<CommandStreamReceiver>(createCommandStream(*executionEnvironment, 1u, 1));
    auto hwInfo = *defaultHwInfo;
    EngineInstancesContainer regularEngines = {
        {aub_stream::ENGINE_CCS, EngineUsage::Regular},
        {aub_stream::ENGINE_BCS, EngineUsage::Regular},
    };

    memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(regularEngines[0],
                                                                                                      PreemptionHelper::getDefaultPreemptionMode(hwInfo)));
    memoryManager->createAndRegisterOsContext(csr2.get(), EngineDescriptorHelper::getDefaultDescriptor(regularEngines[1],
                                                                                                       PreemptionHelper::getDefaultPreemptionMode(hwInfo)));

    auto mockMngr = new MockGmmPageTableMngr();
    auto mockMngr2 = new MockGmmPageTableMngr();

    memoryManager->getRegisteredEngines()[0].commandStreamReceiver->pageTableManager.reset(mockMngr);
    memoryManager->getRegisteredEngines()[1].commandStreamReceiver->pageTableManager.reset(mockMngr2);

    MockGraphicsAllocation allocation(1u, AllocationType::UNKNOWN, nullptr, 0, 0, MemoryPool::MemoryNull, MemoryManager::maxOsContextCount, 0llu);
    MockGmm gmm(executionEnvironment->rootDeviceEnvironments[allocation.getRootDeviceIndex()]->getGmmHelper());
    gmm.isCompressionEnabled = true;
    allocation.setDefaultGmm(&gmm);
    GMM_DDI_UPDATEAUXTABLE expectedDdiUpdateAuxTable = {};
    expectedDdiUpdateAuxTable.BaseGpuVA = allocation.getGpuAddress();
    expectedDdiUpdateAuxTable.BaseResInfo = allocation.getDefaultGmm()->gmmResourceInfo->peekGmmResourceInfo();
    expectedDdiUpdateAuxTable.DoNotWait = true;
    expectedDdiUpdateAuxTable.Map = true;

    bool result = memoryManager->mapAuxGpuVA(&allocation);
    EXPECT_TRUE(result);

    EXPECT_EQ(1u, mockMngr->updateAuxTableCalled);
    EXPECT_TRUE(memcmp(&expectedDdiUpdateAuxTable, &mockMngr->updateAuxTableParamsPassed[0].ddiUpdateAuxTable, sizeof(GMM_DDI_UPDATEAUXTABLE)) == 0);

    EXPECT_EQ(1u, mockMngr2->updateAuxTableCalled);

    EXPECT_TRUE(memcmp(&expectedDdiUpdateAuxTable, &mockMngr2->updateAuxTableParamsPassed[0].ddiUpdateAuxTable, sizeof(GMM_DDI_UPDATEAUXTABLE)) == 0);
}
HWTEST_F(PageTableManagerTest, givenPageTableManagerWhenUpdateAuxTableGmmErrorThenMapAuxGpuVaReturnFalse) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(2);
    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }

    auto memoryManager = new MockMemoryManager(false, false, *executionEnvironment);

    executionEnvironment->memoryManager.reset(memoryManager);
    auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(*executionEnvironment, 1u, 1));
    auto hwInfo = *defaultHwInfo;
    EngineInstancesContainer regularEngines = {
        {aub_stream::ENGINE_CCS, EngineUsage::Regular}};

    memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(regularEngines[0],
                                                                                                      PreemptionHelper::getDefaultPreemptionMode(hwInfo)));

    auto mockMngr = new MockGmmPageTableMngr();
    mockMngr->updateAuxTableResult = GMM_ERROR;

    memoryManager->getRegisteredEngines()[0].commandStreamReceiver->pageTableManager.reset(mockMngr);

    MockGraphicsAllocation allocation(1u, AllocationType::UNKNOWN, nullptr, 0, 0, MemoryPool::MemoryNull, MemoryManager::maxOsContextCount, 0llu);
    MockGmm gmm(executionEnvironment->rootDeviceEnvironments[allocation.getRootDeviceIndex()]->getGmmHelper());
    gmm.isCompressionEnabled = true;
    allocation.setDefaultGmm(&gmm);

    bool result = memoryManager->mapAuxGpuVA(&allocation);
    EXPECT_FALSE(result);
    EXPECT_EQ(1u, mockMngr->updateAuxTableCalled);
}

HWTEST_F(PageTableManagerTest, givenNullPageTableManagerWhenMapAuxGpuVaThenNoThrow) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(2);
    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }

    auto memoryManager = new MockMemoryManager(false, false, *executionEnvironment);

    executionEnvironment->memoryManager.reset(memoryManager);
    auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(*executionEnvironment, 1u, 1));
    auto hwInfo = *defaultHwInfo;
    EngineInstancesContainer regularEngines = {
        {aub_stream::ENGINE_CCS, EngineUsage::Regular},
    };

    memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(regularEngines[0],
                                                                                                      PreemptionHelper::getDefaultPreemptionMode(hwInfo)));

    memoryManager->getRegisteredEngines()[0].commandStreamReceiver->pageTableManager.reset(nullptr);

    MockGraphicsAllocation allocation(1u, AllocationType::UNKNOWN, nullptr, 0, 0, MemoryPool::MemoryNull, MemoryManager::maxOsContextCount, 0llu);
    MockGmm gmm(executionEnvironment->rootDeviceEnvironments[allocation.getRootDeviceIndex()]->getGmmHelper());
    gmm.isCompressionEnabled = true;
    allocation.setDefaultGmm(&gmm);

    EXPECT_NO_THROW(memoryManager->mapAuxGpuVA(&allocation));
}

HWTEST_F(PageTableManagerTest, givenNullPageTableManagerWhenMapAuxGpuVaThenReturnFalse) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(2);
    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }

    auto memoryManager = new MockMemoryManager(false, false, *executionEnvironment);

    executionEnvironment->memoryManager.reset(memoryManager);
    auto csr = std::unique_ptr<CommandStreamReceiver>(createCommandStream(*executionEnvironment, 1u, 1));
    auto hwInfo = *defaultHwInfo;
    memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances(hwInfo)[0],
                                                                                                      PreemptionHelper::getDefaultPreemptionMode(hwInfo)));

    for (auto engine : memoryManager->getRegisteredEngines()) {
        engine.commandStreamReceiver->pageTableManager.reset(nullptr);
    }

    MockGraphicsAllocation allocation(1u, AllocationType::UNKNOWN, nullptr, 0, 0, MemoryPool::MemoryNull, MemoryManager::maxOsContextCount, 0llu);

    bool result = memoryManager->mapAuxGpuVA(&allocation);
    EXPECT_FALSE(result);
}

HWTEST_F(PageTableManagerTest, givenMemoryManagerThatSupportsPageTableManagerWhenMapAuxGpuVAIsCalledThenItReturnsTrue) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(2);
    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }
    auto memoryManager = new MockMemoryManager(false, false, *executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);
    MockGraphicsAllocation allocation(1u, AllocationType::UNKNOWN, nullptr, 0, 0, MemoryPool::MemoryNull, MemoryManager::maxOsContextCount, 0llu);
    MockGmm gmm(executionEnvironment->rootDeviceEnvironments[allocation.getRootDeviceIndex()]->getGmmHelper());
    allocation.setDefaultGmm(&gmm);
    bool mapped = memoryManager->mapAuxGpuVA(&allocation);
    auto hwInfo = executionEnvironment->rootDeviceEnvironments[allocation.getRootDeviceIndex()]->getHardwareInfo();

    EXPECT_EQ(HwInfoConfig::get(hwInfo->platform.eProductFamily)->isPageTableManagerSupported(*hwInfo), mapped);
}

TEST(MemoryManagerTest, givenDebugModuleAreaAllocationTypeWhenCallingGetAllocationDataThenUse32BitFrontWindowsIsSet) {
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, 1, AllocationType::DEBUG_MODULE_AREA, mockDeviceBitfield);

    MockMemoryManager mockMemoryManager;

    mockMemoryManager.getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
    EXPECT_EQ(1u, allocData.flags.use32BitFrontWindow);
}

TEST(MemoryManagerTest, givenStorageInfoWithParamsWhenGettingAllocDataForSystemMemoryThenSetSystemMemoryFlag) {
    AllocationData allocData;
    AllocationProperties properties(mockRootDeviceIndex, 1, AllocationType::BUFFER_HOST_MEMORY, mockDeviceBitfield);

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
    AllocationProperties properties(mockRootDeviceIndex, 1, AllocationType::BUFFER, mockDeviceBitfield);

    EXPECT_NE(0lu, mockDeviceBitfield.to_ulong());

    MockMemoryManager mockMemoryManager;
    auto storageInfo = mockMemoryManager.createStorageInfoFromProperties(properties);
    EXPECT_NE(0lu, storageInfo.memoryBanks.to_ulong());

    mockMemoryManager.getAllocationData(allocData, properties, nullptr, storageInfo);
    EXPECT_EQ(0u, allocData.flags.useSystemMemory);
    EXPECT_FALSE(allocData.storageInfo.systemMemoryPlacement);
}

TEST(MemoryTransferHelperTest, WhenBlitterIsSelectedButBlitCopyFailsThenFallbackToCopyOnCPU) {
    constexpr uint32_t dataSize = 16;
    uint8_t destData[dataSize] = {};
    uint8_t srcData[dataSize] = {};
    for (uint8_t i = 0u; i < dataSize; i++) {
        srcData[i] = i;
    }
    MockGraphicsAllocation graphicsAllocation{destData, sizeof(destData)};
    graphicsAllocation.setAllocationType(AllocationType::BUFFER_HOST_MEMORY);

    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = false;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));

    EXPECT_EQ(BlitOperationResult::Unsupported, BlitHelperFunctions::blitMemoryToAllocation(*device, &graphicsAllocation, 0, srcData, {dataSize, 1, 1}));

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
    graphicsAllocation.setAllocationType(AllocationType::BUFFER);

    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.featureTable.ftrBcsInfo = 0;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));

    auto bcsEngine = device->tryGetEngine(aub_stream::EngineType::ENGINE_BCS, EngineUsage::Regular);
    EXPECT_EQ(nullptr, bcsEngine);

    EXPECT_EQ(BlitOperationResult::Unsupported, BlitHelperFunctions::blitMemoryToAllocation(*device, &graphicsAllocation, 0, srcData, {dataSize, 1, 1}));
}

TEST(MemoryManagerTest, givenMemoryManagerWithLocalMemoryWhenCreatingMultiGraphicsAllocationInSystemMemoryThenForceSystemMemoryPlacement) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MockMemoryManager memoryManager(true, true, executionEnvironment);

    AllocationProperties allocationProperties{mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::SVM_GPU, systemMemoryBitfield};

    auto localMemoryAllocation = memoryManager.allocateGraphicsMemoryWithProperties(allocationProperties);

    EXPECT_NE(nullptr, localMemoryAllocation);
    EXPECT_TRUE(localMemoryAllocation->isAllocatedInLocalMemoryPool());

    memoryManager.freeGraphicsMemory(localMemoryAllocation);

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.push_back(mockRootDeviceIndex);

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

    AllocationProperties allocationProperties{mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::SVM_GPU, systemMemoryBitfield};

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.push_back(mockRootDeviceIndex);
    rootDeviceIndices.push_back(mockRootDeviceIndex);
    rootDeviceIndices.push_back(mockRootDeviceIndex);

    EXPECT_EQ(3u, rootDeviceIndices.size());

    MultiGraphicsAllocation multiGraphicsAllocation(mockRootDeviceIndex);

    auto ptr = memoryManager.createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, allocationProperties, multiGraphicsAllocation);

    EXPECT_NE(nullptr, ptr);
    auto allocation = multiGraphicsAllocation.getGraphicsAllocation(mockRootDeviceIndex);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(mockRootDeviceIndex, allocation->getRootDeviceIndex());

    memoryManager.freeGraphicsMemory(allocation);
}

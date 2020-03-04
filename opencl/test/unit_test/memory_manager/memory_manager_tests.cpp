/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_constants.h"
#include "shared/source/memory_manager/residency.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/program/program_initialization.h"
#include "shared/test/unit_test/compiler_interface/linker_mock.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/source/event/event.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/program/printf_handler.h"
#include "opencl/source/program/program.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/fixtures/memory_allocator_fixture.h"
#include "opencl/test/unit_test/fixtures/memory_manager_fixture.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/helpers/execution_environment_helper.h"
#include "opencl/test/unit_test/helpers/variable_backup.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_csr.h"
#include "opencl/test/unit_test/mocks/mock_deferrable_deletion.h"
#include "opencl/test/unit_test/mocks/mock_deferred_deleter.h"
#include "opencl/test/unit_test/mocks/mock_device.h"
#include "opencl/test/unit_test/mocks/mock_execution_environment.h"
#include "opencl/test/unit_test/mocks/mock_gmm.h"
#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "opencl/test/unit_test/mocks/mock_os_context.h"
#include "test.h"

#include <future>
#include <type_traits>

using namespace NEO;

typedef Test<MemoryAllocatorFixture> MemoryAllocatorTest;

TEST(MemoryManagerTest, whenCreatingOsAgnosticMemoryManagerThenSupportsMultiStorageResourcesFlagIsSetToTrue) {
    MockMemoryManager memoryManager;
    EXPECT_TRUE(memoryManager.supportsMultiStorageResources);
}

TEST(MemoryManagerTest, whenCreatingAllocPropertiesForMultiStorageResourceThenMultiStorageResourcesFlagIsSetToTrue) {
    AllocationProperties properties{0, false, 0u, GraphicsAllocation::AllocationType::SCRATCH_SURFACE, false, true, 0};
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

TEST(GraphicsAllocationTest, defaultTypeTraits) {
    EXPECT_FALSE(std::is_copy_constructible<GraphicsAllocation>::value);
    EXPECT_FALSE(std::is_copy_assignable<GraphicsAllocation>::value);
}

TEST(GraphicsAllocationTest, Ctor) {
    void *cpuPtr = (void *)0x30000;
    size_t size = 0x1000;

    MockGraphicsAllocation gfxAllocation(cpuPtr, size);
    uint64_t expectedGpuAddr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gfxAllocation.getUnderlyingBuffer()));

    EXPECT_EQ(expectedGpuAddr, gfxAllocation.getGpuAddress());
    EXPECT_EQ(0u, gfxAllocation.getGpuBaseAddress());
}

TEST(GraphicsAllocationTest, Ctor2) {
    void *cpuPtr = (void *)0x30000;
    size_t size = 0x1000;
    osHandle sharedHandle = Sharing::nonSharedResource;
    GraphicsAllocation gfxAllocation(0, GraphicsAllocation::AllocationType::UNKNOWN, cpuPtr, size, sharedHandle, MemoryPool::MemoryNull);
    uint64_t expectedGpuAddr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gfxAllocation.getUnderlyingBuffer()));

    EXPECT_EQ(expectedGpuAddr, gfxAllocation.getGpuAddress());
    EXPECT_EQ(0u, gfxAllocation.getGpuBaseAddress());
    EXPECT_EQ(sharedHandle, gfxAllocation.peekSharedHandle());
}

TEST(GraphicsAllocationTest, getGpuAddress) {
    void *cpuPtr = (void *)0x30000;
    uint64_t gpuAddr = 0x30000;
    uint64_t gpuBaseAddr = 0x10000;
    size_t size = 0x1000;

    GraphicsAllocation gfxAllocation(0, GraphicsAllocation::AllocationType::UNKNOWN, cpuPtr, gpuAddr, gpuBaseAddr, size, MemoryPool::MemoryNull);

    EXPECT_EQ(gpuAddr, gfxAllocation.getGpuAddress());

    cpuPtr = (void *)65535;
    gpuAddr = 1ULL;
    gfxAllocation.setCpuPtrAndGpuAddress(cpuPtr, gpuAddr);
    EXPECT_EQ(gpuAddr, gfxAllocation.getGpuAddress());
    EXPECT_EQ(cpuPtr, gfxAllocation.getUnderlyingBuffer());
}

TEST(GraphicsAllocationTest, getGpuAddressToPatch) {
    void *cpuPtr = (void *)0x30000;
    uint64_t gpuAddr = 0x30000;
    uint64_t gpuBaseAddr = 0x10000;
    size_t size = 0x1000;

    GraphicsAllocation gfxAllocation(0, GraphicsAllocation::AllocationType::UNKNOWN, cpuPtr, gpuAddr, gpuBaseAddr, size, MemoryPool::MemoryNull);

    EXPECT_EQ(gpuAddr - gpuBaseAddr, gfxAllocation.getGpuAddressToPatch());
}

TEST(GraphicsAllocationTest, setSize) {
    void *cpuPtr = (void *)0x30000;
    uint64_t gpuAddr = 0x30000;
    uint64_t gpuBaseAddr = 0x10000;
    size_t size = 0x2000;

    GraphicsAllocation gfxAllocation(0, GraphicsAllocation::AllocationType::UNKNOWN, cpuPtr, gpuAddr, gpuBaseAddr, size, MemoryPool::MemoryNull);
    EXPECT_EQ(size, gfxAllocation.getUnderlyingBufferSize());

    size = 0x3000;
    gfxAllocation.setSize(size);
    EXPECT_EQ(size, gfxAllocation.getUnderlyingBufferSize());
}

TEST_F(MemoryAllocatorTest, allocateSystem) {
    auto ptr = memoryManager->allocateSystemMemory(sizeof(char), 0);
    EXPECT_NE(nullptr, ptr);
    memoryManager->freeSystemMemory(ptr);
}

TEST_F(MemoryAllocatorTest, GivenGraphicsAllocationWhenAddAndRemoveAllocationToHostPtrManagerThenfragmentHasCorrectValues) {
    void *cpuPtr = (void *)0x30000;
    size_t size = 0x1000;

    MockGraphicsAllocation gfxAllocation(cpuPtr, size);
    memoryManager->addAllocationToHostPtrManager(&gfxAllocation);
    auto fragment = memoryManager->getHostPtrManager()->getFragment(gfxAllocation.getUnderlyingBuffer());
    EXPECT_NE(fragment, nullptr);
    EXPECT_TRUE(fragment->driverAllocation);
    EXPECT_EQ(fragment->refCount, 1);
    EXPECT_EQ(fragment->fragmentCpuPointer, cpuPtr);
    EXPECT_EQ(fragment->fragmentSize, size);
    EXPECT_NE(fragment->osInternalStorage, nullptr);
    EXPECT_NE(fragment->residency, nullptr);

    FragmentStorage fragmentStorage = {};
    fragmentStorage.fragmentCpuPointer = cpuPtr;
    memoryManager->getHostPtrManager()->storeFragment(fragmentStorage);
    fragment = memoryManager->getHostPtrManager()->getFragment(gfxAllocation.getUnderlyingBuffer());
    EXPECT_EQ(fragment->refCount, 2);

    fragment->driverAllocation = false;
    memoryManager->removeAllocationFromHostPtrManager(&gfxAllocation);
    fragment = memoryManager->getHostPtrManager()->getFragment(gfxAllocation.getUnderlyingBuffer());
    EXPECT_EQ(fragment->refCount, 2);
    fragment->driverAllocation = true;

    memoryManager->removeAllocationFromHostPtrManager(&gfxAllocation);
    fragment = memoryManager->getHostPtrManager()->getFragment(gfxAllocation.getUnderlyingBuffer());
    EXPECT_EQ(fragment->refCount, 1);

    memoryManager->removeAllocationFromHostPtrManager(&gfxAllocation);
    fragment = memoryManager->getHostPtrManager()->getFragment(gfxAllocation.getUnderlyingBuffer());
    EXPECT_EQ(fragment, nullptr);
}

TEST_F(MemoryAllocatorTest, allocateSystemAligned) {
    unsigned int alignment = 0x100;

    auto ptr = memoryManager->allocateSystemMemory(sizeof(char), alignment);
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(ptr) & (alignment - 1));
    memoryManager->freeSystemMemory(ptr);
}

TEST_F(MemoryAllocatorTest, allocateGraphics) {
    unsigned int alignment = 4096;

    memoryManager->createAndRegisterOsContext(csr,
                                              HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances(*platformDevices[0])[0],
                                              1, PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]),
                                              false, false, false);
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    ASSERT_NE(nullptr, allocation);
    // initial taskCount must be -1. if not, we may kill allocation before it will be used
    EXPECT_EQ((uint32_t)-1, allocation->getTaskCount(csr->getOsContext().getContextId()));
    // We know we want graphics memory to be page aligned
    EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(allocation->getUnderlyingBuffer()) & (alignment - 1));
    EXPECT_EQ(Sharing::nonSharedResource, allocation->peekSharedHandle());

    // Gpu address equal to cpu address
    if (platformDevices[0]->capabilityTable.gpuAddressSpace == MemoryConstants::max48BitAddress) {
        EXPECT_EQ(reinterpret_cast<uint64_t>(allocation->getUnderlyingBuffer()), allocation->getGpuAddress());
    }

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(MemoryAllocatorTest, allocateGraphicsPageAligned) {
    unsigned int alignment = 4096;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(allocation->getUnderlyingBuffer()) & (alignment - 1));
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(MemoryAllocatorTest, AlignedHostPtrWithAlignedSizeWhenAskedForGraphicsAllocationReturnsNullStorageFromHostPtrManager) {
    auto ptr = (void *)0x1000;
    MockMemoryManager mockMemoryManager(*executionEnvironment);
    auto hostPtrManager = static_cast<MockHostPtrManager *>(mockMemoryManager.getHostPtrManager());
    auto graphicsAllocation = mockMemoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, 4096}, ptr);
    EXPECT_NE(nullptr, graphicsAllocation);

    EXPECT_EQ(1u, hostPtrManager->getFragmentCount());
    auto fragmentData = hostPtrManager->getFragment(ptr);

    ASSERT_NE(nullptr, fragmentData);

    EXPECT_NE(nullptr, fragmentData->osInternalStorage);

    mockMemoryManager.freeGraphicsMemory(graphicsAllocation);
}

TEST_F(MemoryAllocatorTest, GivenAlignedHostPtrAndCacheAlignedSizeWhenAskedForL3AllowanceThenTrueIsReturned) {
    auto ptr = (void *)0x1000;
    auto alignedSize = MemoryConstants::cacheLineSize;

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, alignedSize}, ptr);

    EXPECT_TRUE(isL3Capable(*graphicsAllocation));

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(MemoryAllocatorTest, GivenAlignedHostPtrAndNotCacheAlignedSizeWhenAskedForL3AllowanceThenFalseIsReturned) {
    auto ptr = (void *)0x1000;
    auto alignedSize = MemoryConstants::cacheLineSize - 1;

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, alignedSize}, ptr);

    EXPECT_FALSE(isL3Capable(*graphicsAllocation));

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(MemoryAllocatorTest, GivenMisAlignedHostPtrAndNotCacheAlignedSizeWhenAskedForL3AllowanceThenFalseIsReturned) {
    auto ptr = (void *)0x1001;
    auto alignedSize = MemoryConstants::cacheLineSize - 1;

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, alignedSize}, ptr);

    EXPECT_FALSE(isL3Capable(*graphicsAllocation));

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(MemoryAllocatorTest, GivenHostPtrAlignedToCacheLineWhenAskedForL3AllowanceThenTrueIsReturned) {
    auto ptr = (void *)0x1040;
    auto alignedSize = MemoryConstants::cacheLineSize;

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, alignedSize}, ptr);

    EXPECT_TRUE(isL3Capable(*graphicsAllocation));

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(MemoryAllocatorTest, NullOsHandleStorageAskedForPopulationReturnsFilledPointer) {
    OsHandleStorage storage;
    storage.fragmentStorageData[0].cpuPtr = (void *)0x1000;
    memoryManager->populateOsHandles(storage, 0);
    EXPECT_NE(nullptr, storage.fragmentStorageData[0].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[1].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[2].osHandleStorage);
    memoryManager->getHostPtrManager()->releaseHandleStorage(storage);
    memoryManager->cleanOsHandles(storage, 0);
}

TEST_F(MemoryAllocatorTest, givenOsHandleStorageWhenOsHandlesAreCleanedAndAubManagerIsNotAvailableThenFreeMemoryIsNotCalledOnAubManager) {
    MockExecutionEnvironment mockExecutionEnvironment(*platformDevices);
    MockMemoryManager mockMemoryManager(mockExecutionEnvironment);
    auto mockAubCenter = new MockAubCenter(platformDevices[0], false, "aubfile", CommandStreamReceiverType::CSR_AUB);
    mockAubCenter->aubManager.reset(nullptr);
    mockExecutionEnvironment.rootDeviceEnvironments[0]->aubCenter.reset(mockAubCenter);

    OsHandleStorage storage;
    storage.fragmentStorageData[0].cpuPtr = (void *)0x1000;
    mockMemoryManager.populateOsHandles(storage, 0);
    mockMemoryManager.getHostPtrManager()->releaseHandleStorage(storage);
    mockMemoryManager.cleanOsHandles(storage, 0);

    EXPECT_EQ(nullptr, mockAubCenter->aubManager);
}

TEST_F(MemoryAllocatorTest, givenOsHandleStorageAndFreeMemoryEnabledWhenOsHandlesAreCleanedAndAubManagerIsAvailableThenFreeMemoryIsCalledOnAubManager) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableFreeMemory.set(true);
    const uint32_t rootDeviceIndex = 1u;
    MockExecutionEnvironment mockExecutionEnvironment(*platformDevices, true, 3);
    MockMemoryManager mockMemoryManager(mockExecutionEnvironment);
    auto mockManager0 = new MockAubManager();
    auto mockAubCenter0 = new MockAubCenter(platformDevices[0], false, "aubfile", CommandStreamReceiverType::CSR_AUB);
    mockAubCenter0->aubManager.reset(mockManager0);
    mockExecutionEnvironment.rootDeviceEnvironments[0]->aubCenter.reset(mockAubCenter0);

    auto mockManager1 = new MockAubManager();
    auto mockAubCenter1 = new MockAubCenter(platformDevices[0], false, "aubfile", CommandStreamReceiverType::CSR_AUB);
    mockAubCenter1->aubManager.reset(mockManager1);
    mockExecutionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->aubCenter.reset(mockAubCenter1);

    OsHandleStorage storage;
    storage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    mockMemoryManager.populateOsHandles(storage, 0);
    mockMemoryManager.getHostPtrManager()->releaseHandleStorage(storage);
    mockMemoryManager.cleanOsHandles(storage, rootDeviceIndex);

    EXPECT_FALSE(mockManager0->freeMemoryCalled);
    EXPECT_TRUE(mockManager1->freeMemoryCalled);
}

TEST_F(MemoryAllocatorTest, GivenEmptyMemoryManagerAndMisalingedHostPtrWithHugeSizeWhenAskedForHostPtrAllocationThenGraphicsAllocationIsBeignCreatedWithAllFragmentsPresent) {
    void *cpuPtr = (void *)0x1005;
    auto size = MemoryConstants::pageSize * 10 - 1;

    MockMemoryManager mockMemoryManager(*executionEnvironment);
    auto hostPtrManager = static_cast<MockHostPtrManager *>(mockMemoryManager.getHostPtrManager());
    auto reqs = MockHostPtrManager::getAllocationRequirements(cpuPtr, size);

    ASSERT_EQ(3u, reqs.requiredFragmentsCount);

    auto graphicsAllocation = mockMemoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, size}, cpuPtr);
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

    MockMemoryManager::AllocationData allocationData;
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
    auto allocation = memoryManager->allocate32BitGraphicsMemory(size, nullptr, GraphicsAllocation::AllocationType::BUFFER);
    EXPECT_NE(nullptr, allocation);
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_TRUE(allocation->is32BitAllocation());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(MemoryAllocatorTest, givenNotEnoughSpaceInAllocatorWhenAskedFor32bitAllocationNullptrIsReturned) {
    size_t size = 0xfffff000;
    auto allocationFirst = memoryManager->allocate32BitGraphicsMemory(0x5000, nullptr, GraphicsAllocation::AllocationType::BUFFER);
    auto allocation = memoryManager->allocate32BitGraphicsMemory(size, nullptr, GraphicsAllocation::AllocationType::BUFFER);
    EXPECT_EQ(nullptr, allocation);
    memoryManager->freeGraphicsMemory(allocationFirst);
}

TEST_F(MemoryAllocatorTest, givenNotEnoughSpaceInAllocatorWhenAskedFor32bitAllocationWithHostPtrThenNullptrIsReturned) {
    size_t size = 0xfffff000;
    void *ptr = (void *)0x10000;
    auto allocationFirst = memoryManager->allocate32BitGraphicsMemory(0x5000, nullptr, GraphicsAllocation::AllocationType::BUFFER);
    auto allocation = memoryManager->allocate32BitGraphicsMemory(size, ptr, GraphicsAllocation::AllocationType::BUFFER);
    EXPECT_EQ(nullptr, allocation);
    memoryManager->freeGraphicsMemory(allocationFirst);
}

TEST_F(MemoryAllocatorTest, givenMemoryManagerWhenAskedFor32bitAllocationWithPtrThen32bitGraphicsAllocationWithGpuAddressIsReturned) {
    size_t size = 10;
    void *ptr = (void *)0x1000;
    auto allocation = memoryManager->allocate32BitGraphicsMemory(size, ptr, GraphicsAllocation::AllocationType::BUFFER);
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
    AllocationProperties properties{0, false, size, GraphicsAllocation::AllocationType::BUFFER, false};

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, ptr);
    EXPECT_EQ(3u, allocation->fragmentsStorage.fragmentCount);

    EXPECT_EQ(0u, memoryManager->handleFenceCompletionCalled);
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(0u, memoryManager->handleFenceCompletionCalled);
}

TEST_F(MemoryAllocatorTest, GivenShareableEnabledAndDisabledWhenAskedToCreateGrahicsAllocationThenValidAllocationIsReturned) {
    MockMemoryManager::AllocationData allocationData;
    allocationData.type = GraphicsAllocation::AllocationType::BUFFER;

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
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties({0, MemoryConstants::pageSize, GraphicsAllocation::AllocationType::BUFFER});
    EXPECT_EQ(0u, allocation->fragmentsStorage.fragmentCount);

    EXPECT_EQ(0u, memoryManager->handleFenceCompletionCalled);
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(1u, memoryManager->handleFenceCompletionCalled);
}

class MockPrintfHandler : public PrintfHandler {
  public:
    static MockPrintfHandler *create(const MultiDispatchInfo &multiDispatchInfo, ClDevice &deviceArg) {
        return (MockPrintfHandler *)PrintfHandler::create(multiDispatchInfo, deviceArg);
    }
};

TEST_F(MemoryAllocatorTest, givenStatelessKernelWithPrintfWhenPrintfSurfaceIsCreatedThenPrintfSurfaceIsPatchedWithBaseAddressOffset) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    MockKernelWithInternals kernel(*device);
    MockMultiDispatchInfo multiDispatchInfo(kernel.mockKernel);
    SPatchAllocateStatelessPrintfSurface printfSurface;
    printfSurface.Token = iOpenCL::PATCH_TOKEN_ALLOCATE_STATELESS_PRINTF_SURFACE;
    printfSurface.Size = static_cast<uint32_t>(sizeof(SPatchAllocateStatelessPrintfSurface));

    printfSurface.PrintfSurfaceIndex = 11;
    printfSurface.SurfaceStateHeapOffset = 0;
    printfSurface.DataParamOffset = 8;
    printfSurface.DataParamSize = sizeof(void *);

    kernel.kernelInfo.patchInfo.pAllocateStatelessPrintfSurface = &printfSurface;

    // define stateless path
    kernel.kernelInfo.usesSsh = false;
    kernel.kernelInfo.requiresSshForBuffers = false;

    auto printfHandler = MockPrintfHandler::create(multiDispatchInfo, *device.get());

    printfHandler->prepareDispatch(multiDispatchInfo);

    auto printfAllocation = printfHandler->getSurface();
    auto allocationAddress = printfAllocation->getGpuAddressToPatch();

    auto printfPatchAddress = ptrOffset(reinterpret_cast<uintptr_t *>(kernel.mockKernel->getCrossThreadData()),
                                        kernel.mockKernel->getKernelInfo().patchInfo.pAllocateStatelessPrintfSurface->DataParamOffset);

    EXPECT_EQ(allocationAddress, *(uintptr_t *)printfPatchAddress);

    EXPECT_EQ(0u, kernel.mockKernel->getSurfaceStateHeapSize());

    delete printfHandler;
}

HWTEST_F(MemoryAllocatorTest, givenStatefulKernelWithPrintfWhenPrintfSurfaceIsCreatedThenPrintfSurfaceIsPatchedWithCpuAddress) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    MockKernelWithInternals kernel(*device);
    MockMultiDispatchInfo multiDispatchInfo(kernel.mockKernel);
    SPatchAllocateStatelessPrintfSurface printfSurface;
    printfSurface.Token = iOpenCL::PATCH_TOKEN_ALLOCATE_STATELESS_PRINTF_SURFACE;
    printfSurface.Size = static_cast<uint32_t>(sizeof(SPatchAllocateStatelessPrintfSurface));

    printfSurface.PrintfSurfaceIndex = 22;
    printfSurface.SurfaceStateHeapOffset = 16;
    printfSurface.DataParamOffset = 8;
    printfSurface.DataParamSize = sizeof(void *);

    kernel.kernelInfo.patchInfo.pAllocateStatelessPrintfSurface = &printfSurface;

    // define stateful path
    kernel.kernelInfo.usesSsh = true;
    kernel.kernelInfo.requiresSshForBuffers = true;

    auto printfHandler = MockPrintfHandler::create(multiDispatchInfo, *device.get());

    printfHandler->prepareDispatch(multiDispatchInfo);

    auto printfAllocation = printfHandler->getSurface();
    auto allocationAddress = printfAllocation->getGpuAddress();

    EXPECT_NE(0u, kernel.mockKernel->getSurfaceStateHeapSize());

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(kernel.mockKernel->getSurfaceStateHeap(),
                  kernel.mockKernel->getKernelInfo().patchInfo.pAllocateStatelessPrintfSurface->SurfaceStateHeapOffset));
    auto surfaceAddress = surfaceState->getSurfaceBaseAddress();

    EXPECT_EQ(allocationAddress, surfaceAddress);

    delete printfHandler;
}

TEST_F(MemoryAllocatorTest, given32BitDeviceWhenPrintfSurfaceIsCreatedThen32BitAllocationsIsMade) {
    DebugManagerStateRestore dbgRestorer;
    if (is64bit) {
        DebugManager.flags.Force32bitAddressing.set(true);
        auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
        MockKernelWithInternals kernel(*device);
        MockMultiDispatchInfo multiDispatchInfo(kernel.mockKernel);
        SPatchAllocateStatelessPrintfSurface printfSurface;
        printfSurface.Token = iOpenCL::PATCH_TOKEN_ALLOCATE_STATELESS_PRINTF_SURFACE;
        printfSurface.Size = static_cast<uint32_t>(sizeof(SPatchAllocateStatelessPrintfSurface));

        printfSurface.PrintfSurfaceIndex = 33;
        printfSurface.SurfaceStateHeapOffset = 0x1FF0;
        printfSurface.DataParamOffset = 0;
        printfSurface.DataParamSize = 4;

        kernel.kernelInfo.patchInfo.pAllocateStatelessPrintfSurface = &printfSurface;

        auto printfHandler = MockPrintfHandler::create(multiDispatchInfo, *device.get());

        for (int i = 0; i < 8; i++) {
            kernel.mockKernel->mockCrossThreadData[i] = 50;
        }

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

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenItIsCreatedThenForce32BitAllocationsIsFalse) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    EXPECT_FALSE(memoryManager.peekForce32BitAllocations());
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenForce32bitallocationIsCalledWithTrueThenMemoryManagerForces32BitAlloactions) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    memoryManager.setForce32BitAllocations(true);
    EXPECT_TRUE(memoryManager.peekForce32BitAllocations());
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
    MockMemoryManager::getAllocationData(allocationData, {0, MemoryConstants::pageSize, GraphicsAllocation::AllocationType::BUFFER}, nullptr, StorageInfo{});
    auto gfxAllocation = memoryManager.allocateGraphicsMemoryWithAlignment(allocationData);
    ASSERT_NE(gfxAllocation, nullptr);
    EXPECT_NE(gfxAllocation->getGpuBaseAddress(), 0ull);
    EXPECT_EQ(gfxAllocation->getGpuBaseAddress(), memoryManager.getExternalHeapBaseAddress(gfxAllocation->getRootDeviceIndex()));
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
    MockMemoryManager::getAllocationData(allocationData, {0, MemoryConstants::pageSize, GraphicsAllocation::AllocationType::BUFFER}, nullptr, StorageInfo{});
    auto gfxAllocation = memoryManager.allocateGraphicsMemoryWithAlignment(allocationData);
    ASSERT_NE(gfxAllocation, nullptr);
    EXPECT_EQ(gfxAllocation->getGpuBaseAddress(), 0ull);
    EXPECT_EQ(gfxAllocation->getGpuAddress(), memoryManager.getGfxPartition(allocationData.rootDeviceIndex)->getHeapLimit(HeapIndex::HEAP_STANDARD) + 1 - GfxPartition::heapGranularity - MemoryConstants::pageSize);
    memoryManager.freeGraphicsMemory(gfxAllocation);
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenAllocateGraphicsMemoryForImageIsCalledThenGraphicsAllocationIsReturned) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    MockMemoryManager memoryManager(*executionEnvironment);
    cl_image_desc imgDesc = {};
    imgDesc.image_width = 512;
    imgDesc.image_height = 1;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    MockMemoryManager::AllocationData allocationData;
    allocationData.imgInfo = &imgInfo;

    auto imageAllocation = memoryManager.allocateGraphicsMemoryForImage(allocationData);
    ASSERT_NE(nullptr, imageAllocation);
    EXPECT_TRUE(imageAllocation->getDefaultGmm()->resourceParams.Usage == GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE);
    EXPECT_TRUE(imageAllocation->getDefaultGmm()->useSystemMemoryPool);
    memoryManager.freeGraphicsMemory(imageAllocation);
}

TEST(OsAgnosticMemoryManager, givenEnabledLocalMemoryWhenAllocateGraphicsMemoryForImageIsCalledThenUseLocalMemoryIsNotSet) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    MockMemoryManager memoryManager(false, true, *executionEnvironment);
    cl_image_desc imgDesc = {};
    imgDesc.image_width = 1;
    imgDesc.image_height = 1;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    MockMemoryManager::AllocationData allocationData;
    allocationData.imgInfo = &imgInfo;

    auto imageAllocation = memoryManager.allocateGraphicsMemoryForImage(allocationData);
    ASSERT_NE(nullptr, imageAllocation);
    EXPECT_FALSE(imgInfo.useLocalMemory);
    memoryManager.freeGraphicsMemory(imageAllocation);
}

TEST(OsAgnosticMemoryManager, givenHostPointerNotRequiringCopyWhenAllocateGraphicsMemoryForImageFromHostPtrIsCalledThenGraphicsAllocationIsReturned) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    MockMemoryManager memoryManager(false, false, *executionEnvironment);

    cl_image_desc imgDesc = {};
    imgDesc.image_width = 4;
    imgDesc.image_height = 1;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE1D;

    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_RGBA;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, platformDevices[0]->capabilityTable.clVersionSupport);

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, surfaceFormat);
    imgInfo.rowPitch = imgDesc.image_width * 4;
    imgInfo.slicePitch = imgInfo.rowPitch * imgDesc.image_height;
    imgInfo.size = imgInfo.slicePitch;
    imgInfo.linearStorage = true;

    auto hostPtr = alignedMalloc(imgDesc.image_width * imgDesc.image_height * 4, MemoryConstants::pageSize);

    bool copyRequired = MockMemoryManager::isCopyRequired(imgInfo, hostPtr);
    EXPECT_FALSE(copyRequired);

    MockMemoryManager::AllocationData allocationData;
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

    cl_image_desc imgDesc = {};
    imgDesc.image_width = 4;
    imgDesc.image_height = 4;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;

    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_RGBA;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, platformDevices[0]->capabilityTable.clVersionSupport);

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, surfaceFormat);
    imgInfo.rowPitch = imgDesc.image_width * 4;
    imgInfo.slicePitch = imgInfo.rowPitch * imgDesc.image_height;
    imgInfo.size = imgInfo.slicePitch;

    auto hostPtr = alignedMalloc(imgDesc.image_width * imgDesc.image_height * 4, MemoryConstants::pageSize);

    bool copyRequired = MockMemoryManager::isCopyRequired(imgInfo, hostPtr);
    EXPECT_TRUE(copyRequired);

    MockMemoryManager::AllocationData allocationData;
    allocationData.imgInfo = &imgInfo;
    allocationData.hostPtr = hostPtr;

    auto imageAllocation = memoryManager.allocateGraphicsMemoryForImageFromHostPtr(allocationData);
    EXPECT_EQ(nullptr, imageAllocation);

    alignedFree(hostPtr);
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerAndUnifiedAuxCapableAllocationWhenMappingThenReturnFalse) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    executionEnvironment.initGmm();
    OsAgnosticMemoryManager memoryManager(executionEnvironment);

    auto gmm = new Gmm(executionEnvironment.rootDeviceEnvironments[0]->getGmmClientContext(), nullptr, 123, false);
    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    allocation->setDefaultGmm(gmm);

    auto mockGmmRes = reinterpret_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    mockGmmRes->setUnifiedAuxTranslationCapable();

    EXPECT_FALSE(memoryManager.mapAuxGpuVA(allocation));

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenAllocateGraphicsMemoryIsCalledThenMemoryPoolIsSystem4KBPages) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);
    auto size = 4096u;

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{size});
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());
    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWith64KBPagesEnabledWhenAllocateGraphicsMemory64kbIsCalledThenMemoryPoolIsSystem64KBPages) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    executionEnvironment.initGmm();
    MockMemoryManager memoryManager(true, false, executionEnvironment);
    AllocationData allocationData;
    allocationData.size = 4096u;
    auto allocation = memoryManager.allocateGraphicsMemory64kb(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System64KBPages, allocation->getMemoryPool());
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
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MockOsAgnosticManagerWithFailingAllocate memoryManager(true, executionEnvironment);
    AllocationData allocationData;
    allocationData.size = 4096u;
    auto allocation = memoryManager.allocateGraphicsMemory64kb(allocationData);
    EXPECT_EQ(nullptr, allocation);
    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenAllocateGraphicsMemoryWithPtrIsCalledThenMemoryPoolIsSystem4KBPages) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto size = MemoryConstants::pageSize;

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, size}, ptr);

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenAllocate32BitGraphicsMemoryWithPtrIsCalledThenMemoryPoolIsSystem4KBPagesWith32BitGpuAddressing) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto size = MemoryConstants::pageSize;

    auto allocation = memoryManager.allocate32BitGraphicsMemory(size, ptr, GraphicsAllocation::AllocationType::BUFFER);

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPagesWith32BitGpuAddressing, allocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenAllocate32BitGraphicsMemoryWithoutPtrIsCalledThenMemoryPoolIsSystem4KBPagesWith32BitGpuAddressing) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    void *ptr = nullptr;
    auto size = MemoryConstants::pageSize;

    auto allocation = memoryManager.allocate32BitGraphicsMemory(size, ptr, GraphicsAllocation::AllocationType::BUFFER);

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPagesWith32BitGpuAddressing, allocation->getMemoryPool());
    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWith64KBPagesEnabledWhenAllocateGraphicsMemoryThenMemoryPoolIsSystem64KBPages) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(true, false, executionEnvironment);
    auto svmAllocation = memoryManager.allocateGraphicsMemoryWithProperties({0, MemoryConstants::pageSize, GraphicsAllocation::AllocationType::SVM_ZERO_COPY});
    EXPECT_NE(nullptr, svmAllocation);
    EXPECT_EQ(MemoryPool::System64KBPages, svmAllocation->getMemoryPool());
    memoryManager.freeGraphicsMemory(svmAllocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWith64KBPagesDisabledWhenAllocateGraphicsMemoryThen4KBGraphicsAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);
    auto svmAllocation = memoryManager.allocateGraphicsMemoryWithProperties({0, MemoryConstants::pageSize, GraphicsAllocation::AllocationType::SVM_ZERO_COPY});
    EXPECT_EQ(MemoryPool::System4KBPages, svmAllocation->getMemoryPool());
    memoryManager.freeGraphicsMemory(svmAllocation);
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenCreateGraphicsAllocationFromSharedObjectIsCalledThenGraphicsAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);
    osHandle handle = 1;
    auto size = 4096u;
    AllocationProperties properties(0, false, size, GraphicsAllocation::AllocationType::SHARED_BUFFER, false);
    auto sharedAllocation = memoryManager.createGraphicsAllocationFromSharedHandle(handle, properties, false);
    EXPECT_NE(nullptr, sharedAllocation);
    EXPECT_FALSE(sharedAllocation->isCoherent());
    EXPECT_NE(nullptr, sharedAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, sharedAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(MemoryPool::SystemCpuInaccessible, sharedAllocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(sharedAllocation);
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenCreateGraphicsAllocationFromSharedObjectIsCalledAndRootDeviceIndexIsSpecifiedThenGraphicsAllocationIsReturnedWithCorrectRootDeviceIndex) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);
    osHandle handle = 1;
    auto size = 4096u;
    AllocationProperties properties(0u, false, size, GraphicsAllocation::AllocationType::SHARED_BUFFER, false, false, 0u);
    EXPECT_TRUE(properties.subDevicesBitfield.none());
    EXPECT_EQ(properties.rootDeviceIndex, 0u);

    auto sharedAllocation = memoryManager.createGraphicsAllocationFromSharedHandle(handle, properties, false);
    EXPECT_NE(nullptr, sharedAllocation);
    EXPECT_EQ(0u, sharedAllocation->getRootDeviceIndex());
    EXPECT_FALSE(sharedAllocation->isCoherent());
    EXPECT_NE(nullptr, sharedAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, sharedAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(MemoryPool::SystemCpuInaccessible, sharedAllocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(sharedAllocation);
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenCreateGraphicsAllocationFromSharedObjectIsCalledWithSpecificBitnessThen32BitGraphicsAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);
    osHandle handle = 1;
    auto size = 4096u;
    AllocationProperties properties(0, false, size, GraphicsAllocation::AllocationType::SHARED_BUFFER, false);
    auto sharedAllocation = memoryManager.createGraphicsAllocationFromSharedHandle(handle, properties, true);
    EXPECT_NE(nullptr, sharedAllocation);
    EXPECT_TRUE(sharedAllocation->is32BitAllocation());
    EXPECT_FALSE(sharedAllocation->isCoherent());
    EXPECT_NE(nullptr, sharedAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, sharedAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(MemoryPool::SystemCpuInaccessible, sharedAllocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(sharedAllocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenCreateAllocationFromNtHandleIsCalledThenReturnNullptr) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    auto graphicsAllocation = memoryManager.createGraphicsAllocationFromNTHandle((void *)1, 0);
    EXPECT_EQ(nullptr, graphicsAllocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenLockUnlockCalledThenReturnCpuPtr) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
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
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);

    auto graphicsAllocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    auto graphicsAddress = graphicsAllocation->getGpuAddress();
    auto graphicsAddressToPatch = graphicsAllocation->getGpuAddressToPatch();

    graphicsAllocation->setAllocationOffset(4);

    auto offsetedGraphicsAddress = graphicsAllocation->getGpuAddress();
    auto offsetedGraphicsAddressToPatch = graphicsAllocation->getGpuAddressToPatch();

    EXPECT_EQ(offsetedGraphicsAddress, graphicsAddress + graphicsAllocation->getAllocationOffset());
    EXPECT_EQ(offsetedGraphicsAddressToPatch, graphicsAddressToPatch + graphicsAllocation->getAllocationOffset());

    memoryManager.freeGraphicsMemory(graphicsAllocation);
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenGraphicsAllocationIsPaddedThenNewGraphicsAllocationIsCreated) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);
    auto graphicsAllocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    auto sizeWithPadding = 8192;
    auto paddedGraphicsAllocation = memoryManager.createGraphicsAllocationWithPadding(graphicsAllocation, sizeWithPadding);
    ASSERT_NE(nullptr, paddedGraphicsAllocation);
    EXPECT_NE(paddedGraphicsAllocation, graphicsAllocation);

    memoryManager.freeGraphicsMemory(paddedGraphicsAllocation);
    memoryManager.freeGraphicsMemory(graphicsAllocation);
}

TEST(OsAgnosticMemoryManager, pleaseDetectLeak) {
    void *ptr = new int[10];
    EXPECT_NE(nullptr, ptr);
    MemoryManagement::fastLeaksDetectionMode = MemoryManagement::LeakDetectionMode::EXPECT_TO_LEAK;
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenAllocateMemoryWithNoAlignmentProvidedThenAllocationIsAlignedToPageSize) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);
    MockAllocationProperties properties(MemoryConstants::pageSize >> 1);
    properties.alignment = 0;
    auto ga = memoryManager.allocateGraphicsMemoryWithProperties(properties);
    uintptr_t ptr = reinterpret_cast<uintptr_t>(ga->getUnderlyingBuffer());
    ptr &= (MemoryConstants::allocationAlignment - 1);
    EXPECT_EQ(ptr, 0u);

    memoryManager.freeGraphicsMemory(ga);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenAllocateMemoryWithAlignmentNotAlignedToPageSizeThenAlignmentIsAlignedUp) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);
    MockAllocationProperties properties(MemoryConstants::pageSize >> 1);
    properties.alignment = MemoryConstants::pageSize - 1;
    auto ga = memoryManager.allocateGraphicsMemoryWithProperties(properties);
    uintptr_t ptr = reinterpret_cast<uintptr_t>(ga->getUnderlyingBuffer());
    ptr &= (MemoryConstants::allocationAlignment - 1);
    EXPECT_EQ(ptr, 0u);

    memoryManager.freeGraphicsMemory(ga);
}

TEST(OsAgnosticMemoryManager, givenCommonMemoryManagerWhenIsAskedIfApplicationMemoryBudgetIsExhaustedThenFalseIsReturned) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    EXPECT_FALSE(memoryManager.isMemoryBudgetExhausted());
}

class MemoryManagerWithAsyncDeleterTest : public ::testing::Test {
  public:
    MemoryManagerWithAsyncDeleterTest() : memoryManager(false, false){};
    void SetUp() override {
        memoryManager.overrideAsyncDeleterFlag(true);
    }
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
    auto allocation = memoryManager.MemoryManager::allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, sizeof(char)}, ptr);
    EXPECT_TRUE(deleter->isQueueEmpty());
    memoryManager.freeGraphicsMemoryImpl(allocation);
}

TEST_F(MemoryManagerWithAsyncDeleterTest, givenMemoryManagerWhenAllocateGraphicsMemoryIsCalledWithPtrAndDeleterIsNullptrThenItDoesntCrash) {
    memoryManager.setDeferredDeleter(nullptr);
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    char ptr[128];
    auto allocation = memoryManager.MemoryManager::allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, sizeof(char)}, ptr);
    memoryManager.freeGraphicsMemoryImpl(allocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenIsAsyncDeleterEnabledCalledThenReturnsValueOfFlag) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MockMemoryManager memoryManager(executionEnvironment);
    memoryManager.overrideAsyncDeleterFlag(false);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    memoryManager.overrideAsyncDeleterFlag(true);
    EXPECT_TRUE(memoryManager.isAsyncDeleterEnabled());
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenItIsCreatedThenAsyncDeleterEnabledIsFalse) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
}

TEST(OsAgnosticMemoryManager, givenEnabledAsyncDeleterFlagWhenMemoryManagerIsCreatedThenAsyncDeleterEnabledIsFalseAndDeleterIsNullptr) {
    bool defaultEnableDeferredDeleterFlag = DebugManager.flags.EnableDeferredDeleter.get();
    DebugManager.flags.EnableDeferredDeleter.set(true);
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    DebugManager.flags.EnableDeferredDeleter.set(defaultEnableDeferredDeleterFlag);
}

TEST(OsAgnosticMemoryManager, givenDisabledAsyncDeleterFlagWhenMemoryManagerIsCreatedThenAsyncDeleterEnabledIsFalseAndDeleterIsNullptr) {
    bool defaultEnableDeferredDeleterFlag = DebugManager.flags.EnableDeferredDeleter.get();
    DebugManager.flags.EnableDeferredDeleter.set(false);
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    DebugManager.flags.EnableDeferredDeleter.set(defaultEnableDeferredDeleterFlag);
}

TEST(OsAgnosticMemoryManager, GivenEnabled64kbPagesWhenHostMemoryAllocationIsCreatedThenAlignedto64KbAllocationIsReturned) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.Enable64kbpages.set(true);
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(true, false, executionEnvironment);

    GraphicsAllocation *galloc = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize64k, GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY});
    EXPECT_NE(nullptr, galloc);
    memoryManager.freeGraphicsMemory(galloc);

    galloc = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize64k, GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY});
    EXPECT_NE(nullptr, galloc);
    EXPECT_NE(nullptr, galloc->getUnderlyingBuffer());
    EXPECT_EQ(0u, (uintptr_t)galloc->getUnderlyingBuffer() % MemoryConstants::pageSize64k);

    EXPECT_NE(0u, galloc->getGpuAddress());
    EXPECT_EQ(0u, (uintptr_t)galloc->getGpuAddress() % MemoryConstants::pageSize64k);
    memoryManager.freeGraphicsMemory(galloc);
}

TEST(OsAgnosticMemoryManager, givenPointerAndSizeWhenCreateInternalAllocationIsCalledThenGraphicsAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    auto ptr = (void *)0x100000;
    size_t allocationSize = 4096;
    auto graphicsAllocation = memoryManager.allocate32BitGraphicsMemory(allocationSize, ptr, GraphicsAllocation::AllocationType::INTERNAL_HEAP);
    EXPECT_EQ(ptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(allocationSize, graphicsAllocation->getUnderlyingBufferSize());
    memoryManager.freeGraphicsMemory(graphicsAllocation);
}

TEST(OsAgnosticMemoryManager, givenOsAgnosticMemoryManagerWhenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledThenAllocationIsCreated) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
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
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(platformDevices[0]);
    if (executionEnvironment.rootDeviceEnvironments[0]->isFullRangeSvm() || is32bit) {
        return;
    }
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    auto hostPtr = reinterpret_cast<const void *>(0x5001);
    AllocationProperties properties{0, false, 13, GraphicsAllocation::AllocationType::EXTERNAL_HOST_PTR, false};
    properties.flags.flushL3RequiredForRead = properties.flags.flushL3RequiredForWrite = requiresL3Flush;
    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(properties, hostPtr);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(0u, allocation->fragmentsStorage.fragmentCount);
    EXPECT_EQ(requiresL3Flush, allocation->isFlushL3Required());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST_P(OsAgnosticMemoryManagerWithParams, givenFullGpuAddressSpaceWhenAllocateGraphicsMemoryForHostPtrIsCalledThenAllocationWithFragmentsIsCreated) {
    bool requiresL3Flush = GetParam();
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(platformDevices[0]);
    if ((!executionEnvironment.rootDeviceEnvironments[0]->isFullRangeSvm() && !is32bit) || !platformDevices[0]->capabilityTable.hostPtrTrackingEnabled) {
        GTEST_SKIP();
    }
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    auto hostPtr = reinterpret_cast<const void *>(0x5001);
    AllocationProperties properties{0, false, 13, GraphicsAllocation::AllocationType::EXTERNAL_HOST_PTR, false};
    properties.flags.flushL3RequiredForRead = properties.flags.flushL3RequiredForWrite = requiresL3Flush;
    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(properties, hostPtr);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(1u, allocation->fragmentsStorage.fragmentCount);
    EXPECT_EQ(requiresL3Flush, allocation->isFlushL3Required());
    EXPECT_EQ(GraphicsAllocation::AllocationType::EXTERNAL_HOST_PTR, allocation->getAllocationType());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST_P(OsAgnosticMemoryManagerWithParams, givenDisabledHostPtrTrackingWhenAllocateGraphicsMemoryForHostPtrIsCalledThenAllocationWithoutFragmentsIsCreated) {
    if (is32bit) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableHostPtrTracking.set(0);

    bool requiresL3Flush = GetParam();
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    executionEnvironment.rootDeviceEnvironments[0]->setHwInfo(platformDevices[0]);
    if (!executionEnvironment.rootDeviceEnvironments[0]->isFullRangeSvm()) {
        return;
    }
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    auto hostPtr = reinterpret_cast<const void *>(0x5001);

    AllocationProperties properties{0, false, 13, GraphicsAllocation::AllocationType::EXTERNAL_HOST_PTR, false};
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
    MockAubManager *mockManager = new MockAubManager();
    MockAubCenter *mockAubCenter = new MockAubCenter(platformDevices[0], false, "file_name.aub", CommandStreamReceiverType::CSR_AUB);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);
    executionEnvironment.rootDeviceEnvironments[0]->aubCenter.reset(mockAubCenter);

    auto gfxAllocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    EXPECT_FALSE(mockManager->freeMemoryCalled);
    memoryManager.freeGraphicsMemory(gfxAllocation);
    EXPECT_TRUE(mockManager->freeMemoryCalled);
}

TEST(OsAgnosticMemoryManager, givenOsAgnosticMemoryManagerAndFreeMemoryDisabledWhenGraphicsAllocationIsDestroyedThenFreeMemoryOnAubManagerShouldBeCalled) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableFreeMemory.set(false);
    MockExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    MockAubManager *mockManager = new MockAubManager();
    MockAubCenter *mockAubCenter = new MockAubCenter(platformDevices[0], false, "file_name.aub", CommandStreamReceiverType::CSR_AUB);
    mockAubCenter->aubManager = std::unique_ptr<MockAubManager>(mockManager);
    executionEnvironment.rootDeviceEnvironments[0]->aubCenter.reset(mockAubCenter);

    auto gfxAllocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    EXPECT_FALSE(mockManager->freeMemoryCalled);
    memoryManager.freeGraphicsMemory(gfxAllocation);
    EXPECT_FALSE(mockManager->freeMemoryCalled);
}

TEST(MemoryManager, givenSharedResourceCopyWhenAllocatingGraphicsMemoryThenAllocateGraphicsMemoryForImageIsCalled) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    MockMemoryManager memoryManager(false, true, *executionEnvironment);
    cl_image_desc imgDesc = {};
    imgDesc.image_width = 1;
    imgDesc.image_height = 1;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    MockMemoryManager::AllocationData allocationData;
    allocationData.imgInfo = &imgInfo;
    allocationData.type = GraphicsAllocation::AllocationType::SHARED_RESOURCE_COPY;

    auto imageAllocation = memoryManager.allocateGraphicsMemory(allocationData);
    EXPECT_NE(nullptr, imageAllocation);
    EXPECT_TRUE(memoryManager.allocateForImageCalled);
    memoryManager.freeGraphicsMemory(imageAllocation);
}

TEST(MemoryManager, givenShareableWhenAllocatingGraphicsMemoryThenAllocateShareableIsCalled) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    MockMemoryManager memoryManager(false, true, *executionEnvironment);

    MockMemoryManager::AllocationData allocationData;
    allocationData.size = 4096u;
    allocationData.type = GraphicsAllocation::AllocationType::BUFFER;
    allocationData.flags.shareable = true;

    auto allocation = memoryManager.allocateGraphicsMemory(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_TRUE(memoryManager.allocateForShareableCalled);
    memoryManager.freeGraphicsMemory(allocation);
}

TEST_F(MemoryAllocatorTest, GivenSizeWhenGmmIsCreatedThenSuccess) {
    Gmm *gmm = new Gmm(device->getGmmClientContext(), nullptr, 65536, false);
    EXPECT_NE(nullptr, gmm);
    delete gmm;
}

typedef Test<MemoryManagerWithCsrFixture> MemoryManagerWithCsrTest;

TEST_F(MemoryManagerWithCsrTest, GivenAllocationsInHostPtrManagerWhenBiggerOverllapingAllcoationIsCreatedAndNothingToCleanThenAbortExecution) {
    void *cpuPtr1 = (void *)0x100004;
    void *cpuPtr2 = (void *)0x101008;
    void *cpuPtr3 = (void *)0x100000;

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());
    auto graphicsAllocation1 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, MemoryConstants::pageSize}, cpuPtr1);
    EXPECT_EQ(2u, hostPtrManager->getFragmentCount());

    auto graphicsAllocation2 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, MemoryConstants::pageSize * 3}, cpuPtr2);
    EXPECT_EQ(4u, hostPtrManager->getFragmentCount());

    GraphicsAllocation *graphicsAllocation3 = nullptr;

    bool catchMe = false;
    try {
        graphicsAllocation3 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, MemoryConstants::pageSize * 10}, cpuPtr3);
    } catch (...) {
        catchMe = true;
    }

    EXPECT_NE(nullptr, graphicsAllocation1);
    EXPECT_NE(nullptr, graphicsAllocation2);
    EXPECT_EQ(nullptr, graphicsAllocation3);

    EXPECT_TRUE(catchMe);

    EXPECT_EQ((uintptr_t)cpuPtr1 & ~MemoryConstants::pageMask, (uintptr_t)graphicsAllocation1->fragmentsStorage.fragmentStorageData[0].cpuPtr);
    EXPECT_EQ((uintptr_t)cpuPtr2 & ~MemoryConstants::pageMask, (uintptr_t)graphicsAllocation2->fragmentsStorage.fragmentStorageData[0].cpuPtr);
    EXPECT_EQ(((uintptr_t)cpuPtr2 + MemoryConstants::pageSize) & ~MemoryConstants::pageMask, (uintptr_t)graphicsAllocation2->fragmentsStorage.fragmentStorageData[1].cpuPtr);

    memoryManager->freeGraphicsMemory(graphicsAllocation1);
    memoryManager->freeGraphicsMemory(graphicsAllocation2);
    memoryManager->freeGraphicsMemory(graphicsAllocation3);
}

TEST_F(MemoryManagerWithCsrTest, GivenAllocationsInHostPtrManagerReadyForCleaningWhenBiggerOverllapingAllcoationIsCreatedThenTemporaryAllocationsAreCleaned) {

    void *cpuPtr1 = (void *)0x100004;
    void *cpuPtr2 = (void *)0x101008;
    void *cpuPtr3 = (void *)0x100000;

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());
    auto graphicsAllocation1 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, MemoryConstants::pageSize}, cpuPtr1);
    EXPECT_EQ(2u, hostPtrManager->getFragmentCount());

    auto graphicsAllocation2 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, MemoryConstants::pageSize * 3}, cpuPtr2);
    EXPECT_EQ(4u, hostPtrManager->getFragmentCount());

    EXPECT_NE(nullptr, graphicsAllocation1);
    EXPECT_NE(nullptr, graphicsAllocation2);

    auto fragment1 = hostPtrManager->getFragment(alignDown(cpuPtr1, MemoryConstants::pageSize));
    EXPECT_NE(nullptr, fragment1);
    auto fragment2 = hostPtrManager->getFragment(alignUp(cpuPtr1, MemoryConstants::pageSize));
    EXPECT_NE(nullptr, fragment2);
    auto fragment3 = hostPtrManager->getFragment(alignDown(cpuPtr2, MemoryConstants::pageSize));
    EXPECT_NE(nullptr, fragment3);
    auto fragment4 = hostPtrManager->getFragment(alignUp(cpuPtr2, MemoryConstants::pageSize));
    EXPECT_NE(nullptr, fragment4);

    uint32_t taskCountReady = 1;
    auto storage = csr->getInternalAllocationStorage();
    storage->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(graphicsAllocation1), TEMPORARY_ALLOCATION, taskCountReady);
    storage->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(graphicsAllocation2), TEMPORARY_ALLOCATION, taskCountReady);

    EXPECT_EQ(4u, hostPtrManager->getFragmentCount());

    // All fragments ready for release
    taskCount = taskCountReady;
    csr->latestSentTaskCount = taskCountReady;

    auto graphicsAllocation3 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, MemoryConstants::pageSize * 10}, cpuPtr3);

    EXPECT_NE(nullptr, graphicsAllocation3);

    // no more overlapping allocation, previous allocations cleaned
    EXPECT_EQ(1u, graphicsAllocation3->fragmentsStorage.fragmentCount);
    EXPECT_EQ(cpuPtr3, graphicsAllocation3->fragmentsStorage.fragmentStorageData[0].cpuPtr);

    memoryManager->freeGraphicsMemory(graphicsAllocation3);
}

TEST_F(MemoryManagerWithCsrTest, givenAllocationThatWasNotUsedWhencheckGpuUsageAndDestroyGraphicsAllocationsIsCalledThenItIsDestroyedInPlace) {
    auto notUsedAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(notUsedAllocation);
    EXPECT_TRUE(csr->getTemporaryAllocations().peekIsEmpty());
}

TEST_F(MemoryManagerWithCsrTest, givenAllocationThatWasUsedAndIsCompletedWhencheckGpuUsageAndDestroyGraphicsAllocationsIsCalledThenItIsDestroyedInPlace) {
    auto usedAllocationButGpuCompleted = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    auto tagAddress = csr->getTagAddress();
    ASSERT_NE(0u, *tagAddress);

    usedAllocationButGpuCompleted->updateTaskCount(*tagAddress - 1, csr->getOsContext().getContextId());

    memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(usedAllocationButGpuCompleted);
    EXPECT_TRUE(csr->getTemporaryAllocations().peekIsEmpty());
}

TEST_F(MemoryManagerWithCsrTest, givenAllocationThatWasUsedAndIsNotCompletedWhencheckGpuUsageAndDestroyGraphicsAllocationsIsCalledThenItIsAddedToTemporaryAllocationList) {
    memoryManager->createAndRegisterOsContext(csr.get(),
                                              HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances(*platformDevices[0])[0],
                                              1, PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]),
                                              false, false, false);
    auto usedAllocationAndNotGpuCompleted = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    auto tagAddress = csr->getTagAddress();

    usedAllocationAndNotGpuCompleted->updateTaskCount(*tagAddress + 1, csr->getOsContext().getContextId());

    memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(usedAllocationAndNotGpuCompleted);
    EXPECT_FALSE(csr->getTemporaryAllocations().peekIsEmpty());
    EXPECT_EQ(csr->getTemporaryAllocations().peekHead(), usedAllocationAndNotGpuCompleted);

    //change task count so cleanup will not clear alloc in use
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

        MockExecutionEnvironment executionEnvironment(*platformDevices);
        alignedMemoryManager = new (std::nothrow) MockAlignMallocMemoryManager(executionEnvironment);
        //assert we have memory manager
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
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    auto nonDefaultOsContext = device->engines[HwHelper::lowPriorityGpgpuEngineIndex].osContext;
    auto nonDefaultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->engines[HwHelper::lowPriorityGpgpuEngineIndex].commandStreamReceiver);

    auto memoryManager = device->getExecutionEnvironment()->memoryManager.get();
    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    nonDefaultCsr->taskCount = *nonDefaultCsr->getTagAddress() + 1;
    nonDefaultCsr->latestFlushedTaskCount = *nonDefaultCsr->getTagAddress() + 1;
    graphicsAllocation->updateTaskCount(*nonDefaultCsr->getTagAddress() + 1, nonDefaultOsContext->getContextId());

    memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(graphicsAllocation);
    EXPECT_NE(nullptr, nonDefaultCsr->getInternalAllocationStorage()->getTemporaryAllocations().peekHead());
    (*nonDefaultCsr->getTagAddress())++;
    // no need to call freeGraphicsAllocation
}

HWTEST_F(GraphicsAllocationTests, givenAllocationUsedOnlyByNonDefaultDeviceWhenCheckingUsageBeforeDestroyThenStoreItAsTemporaryAllocation) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    auto device = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 0u));
    auto &defaultCommandStreamReceiver = device->getGpgpuCommandStreamReceiver();
    auto &nonDefaultCommandStreamReceiver = static_cast<UltCommandStreamReceiver<FamilyType> &>(*device->commandStreamReceivers[1]);
    auto memoryManager = executionEnvironment->memoryManager.get();
    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    auto notReadyTaskCount = *nonDefaultCommandStreamReceiver.getTagAddress() + 1;

    EXPECT_NE(defaultCommandStreamReceiver.getOsContext().getContextId(), nonDefaultCommandStreamReceiver.getOsContext().getContextId());

    nonDefaultCommandStreamReceiver.taskCount = notReadyTaskCount;
    nonDefaultCommandStreamReceiver.latestFlushedTaskCount = notReadyTaskCount;
    graphicsAllocation->updateTaskCount(notReadyTaskCount, nonDefaultCommandStreamReceiver.getOsContext().getContextId());

    EXPECT_TRUE(nonDefaultCommandStreamReceiver.getInternalAllocationStorage()->getTemporaryAllocations().peekIsEmpty());
    memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(graphicsAllocation);
    EXPECT_FALSE(nonDefaultCommandStreamReceiver.getInternalAllocationStorage()->getTemporaryAllocations().peekIsEmpty());
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
    auto nonDefaultOsContext = device->engines[HwHelper::lowPriorityGpgpuEngineIndex].osContext;
    auto nonDefaultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->engines[HwHelper::lowPriorityGpgpuEngineIndex].commandStreamReceiver);
    auto defaultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getDefaultEngine().commandStreamReceiver);
    auto defaultOsContext = device->getDefaultEngine().osContext;

    EXPECT_FALSE(defaultOsContext->isLowPriority());
    EXPECT_TRUE(nonDefaultOsContext->isLowPriority());

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

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
    GraphicsAllocation graphicsAllocation(0, GraphicsAllocation::AllocationType::UNKNOWN, addressWithTrailingBitSet, 1u, sharedHandle, MemoryPool::MemoryNull);
    EXPECT_EQ(expectedGpuAddress, graphicsAllocation.getGpuAddress());
}

TEST(ResidencyDataTest, givenOsContextWhenItIsRegisteredToMemoryManagerThenRefCountIncreases) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    auto memoryManager = new MockMemoryManager(false, false, executionEnvironment);
    executionEnvironment.memoryManager.reset(memoryManager);
    std::unique_ptr<CommandStreamReceiver> csr(createCommandStream(executionEnvironment, 0u));
    memoryManager->createAndRegisterOsContext(csr.get(),
                                              HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances(*platformDevices[0])[0],
                                              1, PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]),
                                              false, false, false);
    EXPECT_EQ(1u, memoryManager->getRegisteredEnginesCount());
    EXPECT_EQ(1, memoryManager->registeredEngines[0].osContext->getRefInternalCount());
}

TEST(MemoryManagerRegisteredEnginesTest, givenOsContextWhenItIsUnregisteredFromMemoryManagerThenRefCountDecreases) {
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    auto memoryManager = device->getMemoryManager();
    auto &engine = device->getDefaultEngine();

    engine.osContext->incRefInternal();
    EXPECT_EQ(2, engine.osContext->getRefInternalCount());

    memoryManager->unregisterEngineForCsr(engine.commandStreamReceiver);
    EXPECT_EQ(1, engine.osContext->getRefInternalCount());

    engine.osContext->decRefInternal();
}

TEST(ResidencyDataTest, givenDeviceBitfieldWhenCreatingOsContextThenSetValidValue) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    auto memoryManager = new MockMemoryManager(false, false, executionEnvironment);
    executionEnvironment.memoryManager.reset(memoryManager);
    std::unique_ptr<CommandStreamReceiver> csr(createCommandStream(executionEnvironment, 0u));
    DeviceBitfield deviceBitfield = 0b11;
    PreemptionMode preemptionMode = PreemptionMode::MidThread;
    memoryManager->createAndRegisterOsContext(csr.get(),
                                              HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances(*platformDevices[0])[0],
                                              deviceBitfield, preemptionMode,
                                              false, false, false);
    EXPECT_EQ(2u, memoryManager->registeredEngines[0].osContext->getNumSupportedDevices());
    EXPECT_EQ(deviceBitfield, memoryManager->registeredEngines[0].osContext->getDeviceBitfield());
    EXPECT_EQ(preemptionMode, memoryManager->registeredEngines[0].osContext->getPreemptionMode());
}

TEST(ResidencyDataTest, givenTwoOsContextsWhenTheyAreRegisteredFromHigherToLowerThenProperSizeIsReturned) {
    MockExecutionEnvironment executionEnvironment(*platformDevices, true, 2u);
    auto memoryManager = new MockMemoryManager(false, false, executionEnvironment);
    executionEnvironment.memoryManager.reset(memoryManager);
    std::unique_ptr<CommandStreamReceiver> csr(createCommandStream(executionEnvironment, 0u));
    std::unique_ptr<CommandStreamReceiver> csr1(createCommandStream(executionEnvironment, 1u));
    memoryManager->createAndRegisterOsContext(csr.get(),
                                              HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances(*platformDevices[0])[0],
                                              1, PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]),
                                              false, false, false);
    memoryManager->createAndRegisterOsContext(csr1.get(),
                                              HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances(*platformDevices[0])[1],
                                              1, PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]),
                                              false, false, false);
    EXPECT_EQ(2u, memoryManager->getRegisteredEnginesCount());
    EXPECT_EQ(1, memoryManager->registeredEngines[0].osContext->getRefInternalCount());
    EXPECT_EQ(1, memoryManager->registeredEngines[1].osContext->getRefInternalCount());
}

TEST(ResidencyDataTest, givenGpgpuEnginesWhenAskedForMaxOsContextCountThenValueIsGreaterOrEqual) {
    auto &engines = HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances(*platformDevices[0]);
    EXPECT_TRUE(MemoryManager::maxOsContextCount >= engines.size());
}

TEST(ResidencyDataTest, givenResidencyDataWhenUpdateCompletionDataIsCalledThenItIsProperlyUpdated) {
    struct MockResidencyData : public ResidencyData {
        using ResidencyData::lastFenceValues;
    };

    MockResidencyData residency;

    MockOsContext osContext(0u, 1,
                            HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances(*platformDevices[0])[0],
                            PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]), false, false, false);
    MockOsContext osContext2(1u, 1,
                             HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances(*platformDevices[0])[1],
                             PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]), false, false, false);

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
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

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
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    EXPECT_FALSE(allocation->isLocked());
    memoryManager.MemoryManager::lockResource(allocation);
    EXPECT_TRUE(allocation->isLocked());
    EXPECT_EQ(1u, memoryManager.lockResourceCalled);
    EXPECT_EQ(0u, memoryManager.unlockResourceCalled);

    memoryManager.freeGraphicsMemory(allocation);
    EXPECT_EQ(1u, memoryManager.unlockResourceCalled);
}
TEST(MemoryManagerTest, givenExecutionEnvrionmentWithCleanedRootDeviceExecutionsWhenFreeGraphicsMemoryIsCalledThenMemoryManagerDoesntCrash) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    EXPECT_NE(nullptr, allocation);

    executionEnvironment.rootDeviceEnvironments.clear();

    EXPECT_NO_THROW(memoryManager.freeGraphicsMemory(allocation));
}

TEST(MemoryManagerTest, givenAllocationTypesThatMayNeedL3FlushWhenCallingGetAllocationDataThenFlushL3FlagIsCorrectlySet) {
    AllocationData allocData;
    AllocationProperties properties(0, 1, GraphicsAllocation::AllocationType::UNKNOWN);
    properties.flags.flushL3RequiredForRead = 1;
    properties.flags.flushL3RequiredForWrite = 1;

    GraphicsAllocation::AllocationType allocationTypesThatMayNeedL3Flush[] = {
        GraphicsAllocation::AllocationType::BUFFER, GraphicsAllocation::AllocationType::BUFFER_COMPRESSED,
        GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, GraphicsAllocation::AllocationType::EXTERNAL_HOST_PTR,
        GraphicsAllocation::AllocationType::GLOBAL_SURFACE, GraphicsAllocation::AllocationType::IMAGE,
        GraphicsAllocation::AllocationType::PIPE, GraphicsAllocation::AllocationType::SHARED_IMAGE,
        GraphicsAllocation::AllocationType::SHARED_BUFFER, GraphicsAllocation::AllocationType::SHARED_RESOURCE_COPY,
        GraphicsAllocation::AllocationType::SVM_ZERO_COPY, GraphicsAllocation::AllocationType::SVM_GPU,
        GraphicsAllocation::AllocationType::SVM_CPU, GraphicsAllocation::AllocationType::WRITE_COMBINED,
        GraphicsAllocation::AllocationType::MAP_ALLOCATION};

    MockMemoryManager mockMemoryManager;
    for (auto allocationType : allocationTypesThatMayNeedL3Flush) {
        properties.allocationType = allocationType;
        MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
        EXPECT_TRUE(allocData.flags.flushL3);
    }

    properties.flags.flushL3RequiredForRead = 0;
    properties.flags.flushL3RequiredForWrite = 0;

    for (auto allocationType : allocationTypesThatMayNeedL3Flush) {
        properties.allocationType = allocationType;
        MockMemoryManager::getAllocationData(allocData, properties, nullptr, mockMemoryManager.createStorageInfoFromProperties(properties));
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
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, platformDevices[0]->capabilityTable.clVersionSupport);

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
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, platformDevices[0]->capabilityTable.clVersionSupport);

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
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, platformDevices[0]->capabilityTable.clVersionSupport);

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
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, platformDevices[0]->capabilityTable.clVersionSupport);

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
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, platformDevices[0]->capabilityTable.clVersionSupport);

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
    auto &hwHelper = HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily);

    ImageInfo imgInfo{};

    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, platformDevices[0]->capabilityTable.clVersionSupport);

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
    imgInfo.linearStorage = !hwHelper.tilingAllowed(false, Image::isImage1d(Image::convertDescriptor(imgInfo.imgDesc)), false);

    auto hostPtr = alignedMalloc(imgInfo.size, MemoryConstants::cacheLineSize);

    EXPECT_FALSE(MockMemoryManager::isCopyRequired(imgInfo, hostPtr));
    alignedFree(hostPtr);
}

TEST(HeapSelectorTest, given32bitInternalAllocationWhenSelectingHeapThenInternalHeapIsUsed) {
    GraphicsAllocation allocation{0, GraphicsAllocation::AllocationType::KERNEL_ISA, nullptr, 0, 0, 0, MemoryPool::MemoryNull};
    allocation.set32BitAllocation(true);
    EXPECT_EQ(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY, MemoryManager::selectHeap(&allocation, false, false));
}

TEST(HeapSelectorTest, givenNon32bitInternalAllocationWhenSelectingHeapThenInternalHeapIsUsed) {
    GraphicsAllocation allocation{0, GraphicsAllocation::AllocationType::KERNEL_ISA, nullptr, 0, 0, 0, MemoryPool::MemoryNull};
    allocation.set32BitAllocation(false);
    EXPECT_EQ(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY, MemoryManager::selectHeap(&allocation, false, false));
}

TEST(HeapSelectorTest, given32bitExternalAllocationWhenSelectingHeapThenExternalHeapIsUsed) {
    GraphicsAllocation allocation{0, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, 0, 0, 0, MemoryPool::MemoryNull};
    allocation.set32BitAllocation(true);
    EXPECT_EQ(HeapIndex::HEAP_EXTERNAL, MemoryManager::selectHeap(&allocation, false, false));
}

TEST(HeapSelectorTest, givenLimitedAddressSpaceWhenSelectingHeapForExternalAllocationThenStandardHeapIsUsed) {
    GraphicsAllocation allocation{0, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, 0, 0, 0, MemoryPool::MemoryNull};
    EXPECT_EQ(HeapIndex::HEAP_STANDARD, MemoryManager::selectHeap(&allocation, true, false));
}

TEST(HeapSelectorTest, givenFullAddressSpaceWhenSelectingHeapForExternalAllocationWithPtrThenSvmHeapIsUsed) {
    GraphicsAllocation allocation{0, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, 0, 0, 0, MemoryPool::MemoryNull};
    EXPECT_EQ(HeapIndex::HEAP_SVM, MemoryManager::selectHeap(&allocation, true, true));
}

TEST(HeapSelectorTest, givenFullAddressSpaceWhenSelectingHeapForExternalAllocationWithoutPtrAndResourceIs64KSuitableThenStandard64kHeapIsUsed) {
    GraphicsAllocation allocation{0, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, 0, 0, 0, MemoryPool::MemoryNull};
    auto rootDeviceEnvironment = platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0].get();
    auto gmm = std::make_unique<Gmm>(rootDeviceEnvironment->getGmmClientContext(), nullptr, 0, false);
    auto resourceInfo = static_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    resourceInfo->is64KBPageSuitableValue = true;
    allocation.setDefaultGmm(gmm.get());
    EXPECT_EQ(HeapIndex::HEAP_STANDARD64KB, MemoryManager::selectHeap(&allocation, false, true));
}

TEST(HeapSelectorTest, givenFullAddressSpaceWhenSelectingHeapForExternalAllocationWithoutPtrAndResourceIsNot64KSuitableThenStandardHeapIsUsed) {
    GraphicsAllocation allocation{0, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, 0, 0, 0, MemoryPool::MemoryNull};
    auto rootDeviceEnvironment = platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0].get();
    auto gmm = std::make_unique<Gmm>(rootDeviceEnvironment->getGmmClientContext(), nullptr, 0, false);
    auto resourceInfo = static_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    resourceInfo->is64KBPageSuitableValue = false;
    allocation.setDefaultGmm(gmm.get());
    EXPECT_EQ(HeapIndex::HEAP_STANDARD, MemoryManager::selectHeap(&allocation, false, true));
}

TEST(HeapSelectorTest, givenFullAddressSpaceWhenSelectingHeapForNullAllocationWithoutPtrThenStandardHeapIsUsed) {
    EXPECT_EQ(HeapIndex::HEAP_STANDARD, MemoryManager::selectHeap(nullptr, false, true));
}

TEST(HeapSelectorTest, givenLimitedAddressSpaceWhenSelectingHeapForNullAllocationWithoutPtrThenStandardHeapIsUsed) {
    EXPECT_EQ(HeapIndex::HEAP_STANDARD, MemoryManager::selectHeap(nullptr, false, false));
}

TEST(MemoryAllocationTest, givenAllocationTypeWhenPassedToMemoryAllocationConstructorThenAllocationTypeIsStored) {
    MemoryAllocation allocation{0, GraphicsAllocation::AllocationType::COMMAND_BUFFER, nullptr, nullptr, 0, 0, 0,
                                MemoryPool::MemoryNull, false, false};
    EXPECT_EQ(GraphicsAllocation::AllocationType::COMMAND_BUFFER, allocation.getAllocationType());
}

TEST(MemoryAllocationTest, givenMemoryPoolWhenPassedToMemoryAllocationConstructorThenMemoryPoolIsStored) {
    MemoryAllocation allocation{0, GraphicsAllocation::AllocationType::COMMAND_BUFFER, nullptr, nullptr, 0, 0, 0,
                                MemoryPool::System64KBPages, false, false};
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
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    uint8_t memory = 1;
    MockGraphicsAllocation invalidAllocation{nullptr, 0u};
    EXPECT_FALSE(memoryManager.copyMemoryToAllocation(&invalidAllocation, &memory, sizeof(memory)));
}

TEST(MemoryManagerCopyMemoryTest, givenValidAllocationAndMemoryWhenCopyMemoryToAllocationThenDataIsCopied) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    constexpr uint8_t allocationSize = 10;
    uint8_t allocationStorage[allocationSize] = {0};
    MockGraphicsAllocation allocation{allocationStorage, allocationSize};
    uint8_t memory = 1u;
    EXPECT_EQ(0u, allocationStorage[0]);
    EXPECT_TRUE(memoryManager.copyMemoryToAllocation(&allocation, &memory, sizeof(memory)));
    EXPECT_EQ(memory, allocationStorage[0]);
}

TEST_F(MemoryAllocatorTest, whenReservingAddressRangeThenExpectProperAddressAndReleaseWhenFreeing) {
    size_t size = 0x1000;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{size});
    ASSERT_NE(nullptr, allocation);
    void *reserve = memoryManager->reserveCpuAddressRange(size, 0);
    EXPECT_NE(nullptr, reserve);
    allocation->setReservedAddressRange(reserve, size);
    EXPECT_EQ(reserve, allocation->getReservedAddressPtr());
    EXPECT_EQ(size, allocation->getReservedAddressSize());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenMemoryManagerWhenGettingReservedMemoryThenAllocateIt) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    EXPECT_EQ(nullptr, memoryManager.reservedMemory);
    memoryManager.getReservedMemory(MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);
    EXPECT_NE(nullptr, memoryManager.reservedMemory);
}

TEST(MemoryManagerTest, givenMemoryManagerWhenGetReservedMemoryIsCalledManyTimesThenReuseSameMemory) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    auto reservedMemory = memoryManager.getReservedMemory(MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);
    memoryManager.getReservedMemory(MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);
    memoryManager.getReservedMemory(MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);
    EXPECT_EQ(reservedMemory, memoryManager.reservedMemory);
}

class MemoryManagerWithFailure : public MockMemoryManager {
  public:
    GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) override {
        return nullptr;
    }
};

TEST(MemoryManagerTest, whenMemoryManagerReturnsNullptrThenAllocateGlobalsSurfaceAlsoReturnsNullptr) {
    MockClDevice device{new MockDevice};
    std::unique_ptr<MemoryManager> memoryManager(new MemoryManagerWithFailure());
    device.injectMemoryManager(memoryManager.release());

    WhiteBox<NEO::LinkerInput> linkerInput;
    linkerInput.traits.exportsGlobalConstants = true;
    linkerInput.traits.exportsGlobalVariables = true;
    GraphicsAllocation *allocation = allocateGlobalsSurface(nullptr, device.getDevice(), 1024, false, &linkerInput, nullptr);
    EXPECT_EQ(nullptr, allocation);

    auto svmAllocsManager = std::make_unique<SVMAllocsManager>(device.getMemoryManager());
    allocation = allocateGlobalsSurface(svmAllocsManager.get(), device.getDevice(), 1024, false, &linkerInput, nullptr);
    EXPECT_EQ(nullptr, allocation);
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

TEST_F(MemoryManagerMultiRootDeviceTests, globalsSurfaceHasCorrectRootDeviceIndex) {
    if (device->getMemoryManager()->isLimitedRange(expectedRootDeviceIndex)) {
        delete context->svmAllocsManager;
        context->svmAllocsManager = nullptr;
    }

    std::vector<unsigned char> initData(1024, 0x5B);
    WhiteBox<NEO::LinkerInput> linkerInput;
    linkerInput.traits.exportsGlobalConstants = true;
    linkerInput.traits.exportsGlobalVariables = true;
    GraphicsAllocation *allocation = allocateGlobalsSurface(context->svmAllocsManager, device->getDevice(), initData.size(), false, &linkerInput, initData.data());

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(expectedRootDeviceIndex, allocation->getRootDeviceIndex());

    if (device->getMemoryManager()->isLimitedRange(expectedRootDeviceIndex)) {
        device->getMemoryManager()->freeGraphicsMemory(allocation);
    } else {
        context->getSVMAllocsManager()->freeSVMAlloc(allocation->getUnderlyingBuffer());
    }
}

HWTEST_F(MemoryAllocatorTest, givenMemoryManagerWhen64BitAndHostPtrTrackingDisabledThenNonSvmHostPtrUsageIsSet) {

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableHostPtrTracking.set(0);

    bool expectedValue = !is32bit;

    auto result = memoryManager->useNonSvmHostPtrAlloc(GraphicsAllocation::AllocationType::EXTERNAL_HOST_PTR, 0u);
    EXPECT_EQ(expectedValue, result);

    result = memoryManager->useNonSvmHostPtrAlloc(GraphicsAllocation::AllocationType::MAP_ALLOCATION, 0u);
    EXPECT_EQ(expectedValue, result);
}

HWTEST_F(MemoryAllocatorTest, givenMemoryManagerWhenHostPtrTrackingModeThenNonSvmHostPtrUsageIsSet) {
    memoryManager->setForceNonSvmForExternalHostPtr(true);

    auto result = memoryManager->useNonSvmHostPtrAlloc(GraphicsAllocation::AllocationType::EXTERNAL_HOST_PTR, 0u);
    EXPECT_EQ(true, result);
    result = memoryManager->useNonSvmHostPtrAlloc(GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, 0u);
    EXPECT_EQ(false, result);
}

HWTEST_F(MemoryAllocatorTest, givenMemoryManagerWhenHostPtrTrackingEnabledThenNonSvmHostPtrUsageDependsOnFullRangeSvm) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableHostPtrTracking.set(1);

    auto result = memoryManager->useNonSvmHostPtrAlloc(GraphicsAllocation::AllocationType::EXTERNAL_HOST_PTR, 0u);
    EXPECT_EQ(!executionEnvironment->rootDeviceEnvironments[0]->isFullRangeSvm() && !is32bit, result);

    result = memoryManager->useNonSvmHostPtrAlloc(GraphicsAllocation::AllocationType::MAP_ALLOCATION, 0u);
    EXPECT_EQ(!executionEnvironment->rootDeviceEnvironments[0]->isFullRangeSvm() && !is32bit, result);
}

using PageTableManagerTest = ::testing::Test;

HWTEST_F(PageTableManagerTest, givenMemoryManagerThatSupportsPageTableManagerWhenMapAuxGpuVAIsCalledThenItReturnsTrue) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(2);
    for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(*platformDevices);
    }
    auto memoryManager = new MockMemoryManager(false, false, *executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);
    MockGraphicsAllocation allocation(1u, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, 0, 0, 0, MemoryPool::MemoryNull);
    MockGmm gmm;
    allocation.setDefaultGmm(&gmm);
    bool mapped = memoryManager->mapAuxGpuVA(&allocation);
    auto hwInfo = executionEnvironment->rootDeviceEnvironments[allocation.getRootDeviceIndex()]->getHardwareInfo();

    EXPECT_EQ(HwHelper::get(hwInfo->platform.eRenderCoreFamily).isPageTableManagerSupported(*hwInfo), mapped);
}

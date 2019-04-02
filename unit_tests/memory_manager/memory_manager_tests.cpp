/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/preemption.h"
#include "runtime/event/event.h"
#include "runtime/helpers/cache_policy.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/mem_obj/image.h"
#include "runtime/mem_obj/mem_obj_helper.h"
#include "runtime/memory_manager/internal_allocation_storage.h"
#include "runtime/memory_manager/memory_constants.h"
#include "runtime/os_interface/os_context.h"
#include "runtime/os_interface/os_interface.h"
#include "runtime/platform/platform.h"
#include "runtime/program/printf_handler.h"
#include "runtime/program/program.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/memory_allocator_fixture.h"
#include "unit_tests/fixtures/memory_manager_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/execution_environment_helper.h"
#include "unit_tests/helpers/memory_management.h"
#include "unit_tests/helpers/variable_backup.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_deferrable_deletion.h"
#include "unit_tests/mocks/mock_deferred_deleter.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_execution_environment.h"
#include "unit_tests/mocks/mock_gmm.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_mdi.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include "unit_tests/mocks/mock_os_context.h"

#include <future>
#include <type_traits>

using namespace NEO;

typedef Test<MemoryAllocatorFixture> MemoryAllocatorTest;

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
    GraphicsAllocation gfxAllocation(GraphicsAllocation::AllocationType::UNKNOWN, cpuPtr, size, sharedHandle, MemoryPool::MemoryNull, false);
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

    GraphicsAllocation gfxAllocation(GraphicsAllocation::AllocationType::UNKNOWN, cpuPtr, gpuAddr, gpuBaseAddr, size, MemoryPool::MemoryNull, false);

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

    GraphicsAllocation gfxAllocation(GraphicsAllocation::AllocationType::UNKNOWN, cpuPtr, gpuAddr, gpuBaseAddr, size, MemoryPool::MemoryNull, false);

    EXPECT_EQ(gpuAddr - gpuBaseAddr, gfxAllocation.getGpuAddressToPatch());
}

TEST(GraphicsAllocationTest, setSize) {
    void *cpuPtr = (void *)0x30000;
    uint64_t gpuAddr = 0x30000;
    uint64_t gpuBaseAddr = 0x10000;
    size_t size = 0x2000;

    GraphicsAllocation gfxAllocation(GraphicsAllocation::AllocationType::UNKNOWN, cpuPtr, gpuAddr, gpuBaseAddr, size, MemoryPool::MemoryNull, false);
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

    memoryManager->createAndRegisterOsContext(csr, HwHelper::get(platformDevices[0]->pPlatform->eRenderCoreFamily).getGpgpuEngineInstances()[0],
                                              1, PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]), false);
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    ASSERT_NE(nullptr, allocation);
    // initial taskCount must be -1. if not, we may kill allocation before it will be used
    EXPECT_EQ((uint32_t)-1, allocation->getTaskCount(csr->getOsContext().getContextId()));
    // We know we want graphics memory to be page aligned
    EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(allocation->getUnderlyingBuffer()) & (alignment - 1));
    EXPECT_EQ(Sharing::nonSharedResource, allocation->peekSharedHandle());

    // Gpu address equal to cpu address
    EXPECT_EQ(reinterpret_cast<uint64_t>(allocation->getUnderlyingBuffer()), allocation->getGpuAddress());

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
    memoryManager->populateOsHandles(storage);
    EXPECT_NE(nullptr, storage.fragmentStorageData[0].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[1].osHandleStorage);
    EXPECT_EQ(nullptr, storage.fragmentStorageData[2].osHandleStorage);
    memoryManager->getHostPtrManager()->releaseHandleStorage(storage);
    memoryManager->cleanOsHandles(storage);
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
        EXPECT_EQ(reqs.AllocationFragments[i].allocationPtr, graphicsAllocation->fragmentsStorage.fragmentStorageData[i].cpuPtr);
        EXPECT_EQ(reqs.AllocationFragments[i].allocationSize, graphicsAllocation->fragmentsStorage.fragmentStorageData[i].fragmentSize);
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

TEST_F(MemoryAllocatorTest, defaultInternalHeapBaseIsInitialized) {
    EXPECT_LE(0ull, memoryManager->MemoryManager::getInternalHeapBaseAddress());
}

TEST_F(MemoryAllocatorTest, defaultExternalHeapBaseIsNotNull) {
    EXPECT_LT(0ull, memoryManager->getExternalHeapBaseAddress());
}

TEST_F(MemoryAllocatorTest, givenMemoryManagerWhensetForce32BitAllocationsIsCalledWithTrueMutlipleTimesThenAllocatorIsReused) {
    memoryManager->setForce32BitAllocations(true);
    EXPECT_NE(nullptr, memoryManager->allocator32Bit.get());
    auto currentAllocator = memoryManager->allocator32Bit.get();
    memoryManager->setForce32BitAllocations(true);
    EXPECT_EQ(memoryManager->allocator32Bit.get(), currentAllocator);
}

TEST_F(MemoryAllocatorTest, givenMemoryManagerWhensetForce32BitAllocationsIsCalledWithFalseThenAllocatorIsNotDeleted) {
    memoryManager->setForce32BitAllocations(true);
    EXPECT_NE(nullptr, memoryManager->allocator32Bit.get());
    memoryManager->setForce32BitAllocations(false);
    EXPECT_NE(nullptr, memoryManager->allocator32Bit.get());
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
    AllocationProperties properties{false, size, GraphicsAllocation::AllocationType::UNDECIDED};

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, ptr);
    EXPECT_EQ(3u, allocation->fragmentsStorage.fragmentCount);

    EXPECT_EQ(0u, memoryManager->handleFenceCompletionCalled);
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(0u, memoryManager->handleFenceCompletionCalled);
}

TEST_F(MemoryAllocatorTest, givenAllocationWithoutFragmentsWhenCallingFreeGraphicsMemoryThenCallHandleFenceCompletion) {
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties({MemoryConstants::pageSize, GraphicsAllocation::AllocationType::BUFFER});
    EXPECT_EQ(0u, allocation->fragmentsStorage.fragmentCount);

    EXPECT_EQ(0u, memoryManager->handleFenceCompletionCalled);
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(1u, memoryManager->handleFenceCompletionCalled);
}

class MockPrintfHandler : public PrintfHandler {
  public:
    static MockPrintfHandler *create(const MultiDispatchInfo &multiDispatchInfo, Device &deviceArg) {
        return (MockPrintfHandler *)PrintfHandler::create(multiDispatchInfo, deviceArg);
    }
};

TEST_F(MemoryAllocatorTest, givenStatelessKernelWithPrintfWhenPrintfSurfaceIsCreatedThenPrintfSurfaceIsPatchedWithBaseAddressOffset) {
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
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

    auto printfHandler = MockPrintfHandler::create(multiDispatchInfo, *device);

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
    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
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

    auto printfHandler = MockPrintfHandler::create(multiDispatchInfo, *device);

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
        auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
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

        auto printfHandler = MockPrintfHandler::create(multiDispatchInfo, *device);

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

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenAllocateGraphicsMemoryForImageIsCalledThenGraphicsAllocationIsReturned) {
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
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
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
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
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
    MockMemoryManager memoryManager(false, false, *executionEnvironment);

    cl_image_desc imgDesc = {};
    imgDesc.image_width = 4;
    imgDesc.image_height = 1;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE1D;

    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_RGBA;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, surfaceFormat);
    imgInfo.rowPitch = imgDesc.image_width * 4;
    imgInfo.slicePitch = imgInfo.rowPitch * imgDesc.image_height;
    imgInfo.size = imgInfo.slicePitch;

    auto hostPtr = alignedMalloc(imgDesc.image_width * imgDesc.image_height * 4, MemoryConstants::pageSize);

    bool copyRequired = Image::isCopyRequired(imgInfo, hostPtr);
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
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
    MockMemoryManager memoryManager(false, false, *executionEnvironment);

    cl_image_desc imgDesc = {};
    imgDesc.image_width = 4;
    imgDesc.image_height = 4;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;

    cl_image_format imageFormat = {};
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_RGBA;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, surfaceFormat);
    imgInfo.rowPitch = imgDesc.image_width * 4;
    imgInfo.slicePitch = imgInfo.rowPitch * imgDesc.image_height;
    imgInfo.size = imgInfo.slicePitch;

    auto hostPtr = alignedMalloc(imgDesc.image_width * imgDesc.image_height * 4, MemoryConstants::pageSize);

    bool copyRequired = Image::isCopyRequired(imgInfo, hostPtr);
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
    OsAgnosticMemoryManager memoryManager(executionEnvironment);

    auto gmm = new Gmm(nullptr, 123, false);
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
    auto svmAllocation = memoryManager.allocateGraphicsMemoryWithProperties({MemoryConstants::pageSize, GraphicsAllocation::AllocationType::SVM});
    EXPECT_NE(nullptr, svmAllocation);
    EXPECT_EQ(MemoryPool::System64KBPages, svmAllocation->getMemoryPool());
    memoryManager.freeGraphicsMemory(svmAllocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWith64KBPagesDisabledWhenAllocateGraphicsMemoryThen4KBGraphicsAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);
    auto svmAllocation = memoryManager.allocateGraphicsMemoryWithProperties({MemoryConstants::pageSize, GraphicsAllocation::AllocationType::SVM});
    EXPECT_EQ(MemoryPool::System4KBPages, svmAllocation->getMemoryPool());
    memoryManager.freeGraphicsMemory(svmAllocation);
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenCreateGraphicsAllocationFromSharedObjectIsCalledThenGraphicsAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);
    osHandle handle = 1;
    auto size = 4096u;
    AllocationProperties properties(false, size, GraphicsAllocation::AllocationType::SHARED_BUFFER);
    auto sharedAllocation = memoryManager.createGraphicsAllocationFromSharedHandle(handle, properties, false);
    EXPECT_NE(nullptr, sharedAllocation);
    EXPECT_FALSE(sharedAllocation->isCoherent());
    EXPECT_NE(nullptr, sharedAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, sharedAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(MemoryPool::SystemCpuInaccessible, sharedAllocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(sharedAllocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenCreateAllocationFromNtHandleIsCalledThenReturnNullptr) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    auto graphicsAllocation = memoryManager.createGraphicsAllocationFromNTHandle((void *)1);
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

    //padding buffer was created
    ASSERT_NE(nullptr, memoryManager.peekPaddingAllocation());
    auto paddingAllocation = memoryManager.peekPaddingAllocation();
    EXPECT_EQ(paddingBufferSize, paddingAllocation->getUnderlyingBufferSize());

    memoryManager.freeGraphicsMemory(paddedGraphicsAllocation);
    memoryManager.freeGraphicsMemory(graphicsAllocation);
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenTwoGraphicsAllocationArePaddedThenOnlyOnePaddingBufferIsUsed) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MemoryManagerCreate<OsAgnosticMemoryManager> memoryManager(false, false, executionEnvironment);
    auto graphicsAllocation = memoryManager.allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    auto sizeWithPadding = 8192;
    auto paddedGraphicsAllocation = memoryManager.createGraphicsAllocationWithPadding(graphicsAllocation, sizeWithPadding);
    auto paddingAllocation = memoryManager.peekPaddingAllocation();
    auto paddedGraphicsAllocation2 = memoryManager.createGraphicsAllocationWithPadding(graphicsAllocation, sizeWithPadding);
    auto paddingAllocation2 = memoryManager.peekPaddingAllocation();

    EXPECT_EQ(paddingAllocation2, paddingAllocation);

    memoryManager.freeGraphicsMemory(paddedGraphicsAllocation2);
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
TEST(OsAgnosticMemoryManager, givenDefaultOsAgnosticMemoryManagerWhenItIsQueriedForInternalHeapBaseThen32BitAllocatorBaseIsReturned) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    auto heapBase = memoryManager.allocator32Bit->getBase();
    EXPECT_EQ(heapBase, memoryManager.getInternalHeapBaseAddress());
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
    executionEnvironment.setHwInfo(platformDevices[0]);
    if (executionEnvironment.isFullRangeSvm()) {
        return;
    }
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    auto hostPtr = reinterpret_cast<const void *>(0x5001);
    AllocationProperties properties{false, 13, GraphicsAllocation::AllocationType::EXTERNAL_HOST_PTR};
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
    executionEnvironment.setHwInfo(platformDevices[0]);
    if (!executionEnvironment.isFullRangeSvm()) {
        return;
    }
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    auto hostPtr = reinterpret_cast<const void *>(0x5001);
    AllocationProperties properties{false, 13, GraphicsAllocation::AllocationType::EXTERNAL_HOST_PTR};
    properties.flags.flushL3RequiredForRead = properties.flags.flushL3RequiredForWrite = requiresL3Flush;
    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(properties, hostPtr);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(1u, allocation->fragmentsStorage.fragmentCount);
    EXPECT_FALSE(allocation->isFlushL3Required());
    EXPECT_EQ(GraphicsAllocation::AllocationType::EXTERNAL_HOST_PTR, allocation->getAllocationType());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST_P(OsAgnosticMemoryManagerWithParams, givenDisabledHostPtrTrackingWhenAllocateGraphicsMemoryForHostPtrIsCalledThenAllocationWithoutFragmentsIsCreated) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableHostPtrTracking.set(false);

    bool requiresL3Flush = GetParam();
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    executionEnvironment.setHwInfo(platformDevices[0]);
    if (!executionEnvironment.isFullRangeSvm()) {
        return;
    }
    OsAgnosticMemoryManager memoryManager(executionEnvironment);
    auto hostPtr = reinterpret_cast<const void *>(0x5001);

    AllocationProperties properties{false, 13, GraphicsAllocation::AllocationType::EXTERNAL_HOST_PTR};
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

TEST(OsAgnosticMemoryManager, givenLocalMemoryNotSupportedWhenMemoryManagerIsCreatedThenAllocator32BitHasCorrectBaseAddress) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MockMemoryManager memoryManager(false, false, false, executionEnvironment);
    uint64_t heap32Base = 0x80000000000ul;

    if (is32bit) {
        heap32Base = 0;
    }
    EXPECT_EQ(heap32Base, memoryManager.allocator32Bit->getBase());
}

TEST(OsAgnosticMemoryManager, givenLocalMemorySupportedAndNotAubUsageWhenMemoryManagerIsCreatedThenAllocator32BitHasCorrectBaseAddress) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MockMemoryManager memoryManager(false, true, false, executionEnvironment);
    memoryManager.allocator32Bit.reset(memoryManager.create32BitAllocator(false));
    uint64_t heap32Base = 0x80000000000ul;

    if (is32bit) {
        heap32Base = 0;
    }
    EXPECT_EQ(heap32Base, memoryManager.allocator32Bit->getBase());
}

TEST(OsAgnosticMemoryManager, givenLocalMemoryNotSupportedAndAubUsageWhenMemoryManagerIsCreatedThenAllocator32BitHasCorrectBaseAddress) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MockMemoryManager memoryManager(false, false, true, executionEnvironment);
    uint64_t heap32Base = 0x80000000000ul;

    if (is32bit) {
        heap32Base = 0;
    }
    EXPECT_EQ(heap32Base, memoryManager.allocator32Bit->getBase());
}

TEST(OsAgnosticMemoryManager, givenLocalMemorySupportedAndAubUsageWhenMemoryManagerIsCreatedThenAllocator32BitHasCorrectBaseAddress) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MockMemoryManager memoryManager(false, true, true, executionEnvironment);
    memoryManager.allocator32Bit.reset(memoryManager.create32BitAllocator(true));
    uint64_t heap32Base = 0x80000000000ul;

    if (is32bit) {
        heap32Base = 0;
    } else {
        heap32Base = 0x40000000000ul;
    }
    EXPECT_EQ(heap32Base, memoryManager.allocator32Bit->getBase());
}

TEST(MemoryManager, givenSharedResourceCopyWhenAllocatingGraphicsMemoryThenAllocateGraphicsMemoryForImageIsCalled) {
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
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

TEST_F(MemoryAllocatorTest, GivenSizeWhenGmmIsCreatedThenSuccess) {
    Gmm *gmm = new Gmm(nullptr, 65536, false);
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
    memoryManager->createAndRegisterOsContext(csr, HwHelper::get(platformDevices[0]->pPlatform->eRenderCoreFamily).getGpgpuEngineInstances()[0],
                                              1, PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]), false);
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
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
    auto device = std::unique_ptr<MockDevice>(Device::create<MockDevice>(platformDevices[0], executionEnvironment, 0u));
    auto &defaultCommandStreamReceiver = device->getCommandStreamReceiver();
    auto &nonDefaultCommandStreamReceiver = static_cast<UltCommandStreamReceiver<FamilyType> &>(*executionEnvironment->commandStreamReceivers[0][1]);
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
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
    auto memoryManager = new MockMemoryManager(false, false, *executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);
    auto multiContextDestructor = new MockDeferredDeleter();
    multiContextDestructor->expectDrainBlockingValue(false);
    memoryManager->multiContextResourceDestructor.reset(multiContextDestructor);

    auto device = std::unique_ptr<MockDevice>(MockDevice::create<MockDevice>(platformDevices[0], executionEnvironment, 0u));
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
    GraphicsAllocation graphicsAllocation(GraphicsAllocation::AllocationType::UNKNOWN, addressWithTrailingBitSet, 1u, sharedHandle, MemoryPool::MemoryNull, false);
    EXPECT_EQ(expectedGpuAddress, graphicsAllocation.getGpuAddress());
}

TEST(ResidencyDataTest, givenOsContextWhenItIsRegisteredToMemoryManagerThenRefCountIncreases) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    memoryManager.createAndRegisterOsContext(nullptr, HwHelper::get(platformDevices[0]->pPlatform->eRenderCoreFamily).getGpgpuEngineInstances()[0],
                                             1, PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]), false);
    EXPECT_EQ(1u, memoryManager.getRegisteredEnginesCount());
    EXPECT_EQ(1, memoryManager.registeredEngines[0].osContext->getRefInternalCount());
}

TEST(ResidencyDataTest, givenDeviceBitfieldWhenCreatingOsContextThenSetValidValue) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    DeviceBitfield deviceBitfield = getDeviceBitfieldForNDevices(2);
    PreemptionMode preemptionMode = PreemptionMode::MidThread;
    memoryManager.createAndRegisterOsContext(nullptr, HwHelper::get(platformDevices[0]->pPlatform->eRenderCoreFamily).getGpgpuEngineInstances()[0],
                                             deviceBitfield, preemptionMode, false);
    EXPECT_EQ(2u, memoryManager.registeredEngines[0].osContext->getNumSupportedDevices());
    EXPECT_EQ(deviceBitfield, memoryManager.registeredEngines[0].osContext->getDeviceBitfield());
    EXPECT_EQ(preemptionMode, memoryManager.registeredEngines[0].osContext->getPreemptionMode());
}

TEST(ResidencyDataTest, givenTwoOsContextsWhenTheyAreRegisteredFromHigherToLowerThenProperSizeIsReturned) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    memoryManager.createAndRegisterOsContext(nullptr, HwHelper::get(platformDevices[0]->pPlatform->eRenderCoreFamily).getGpgpuEngineInstances()[0],
                                             1, PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]), false);
    memoryManager.createAndRegisterOsContext(nullptr, HwHelper::get(platformDevices[0]->pPlatform->eRenderCoreFamily).getGpgpuEngineInstances()[1],
                                             1, PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]), false);
    EXPECT_EQ(2u, memoryManager.getRegisteredEnginesCount());
    EXPECT_EQ(1, memoryManager.registeredEngines[0].osContext->getRefInternalCount());
    EXPECT_EQ(1, memoryManager.registeredEngines[1].osContext->getRefInternalCount());
}

TEST(ResidencyDataTest, givenGpgpuEnginesWhenAskedForMaxOscontextCountThenValueIsGreaterOrEqual) {
    auto &engines = HwHelper::get(platformDevices[0]->pPlatform->eRenderCoreFamily).getGpgpuEngineInstances();
    EXPECT_TRUE(maxOsContextCount >= engines.size());
}

TEST(ResidencyDataTest, givenResidencyDataWhenUpdateCompletionDataIsCalledThenItIsProperlyUpdated) {
    struct MockResidencyData : public ResidencyData {
        using ResidencyData::lastFenceValues;
    };

    MockResidencyData residency;

    MockOsContext osContext(0u, 1, HwHelper::get(platformDevices[0]->pPlatform->eRenderCoreFamily).getGpgpuEngineInstances()[0], PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]), false);
    MockOsContext osContext2(1u, 1, HwHelper::get(platformDevices[0]->pPlatform->eRenderCoreFamily).getGpgpuEngineInstances()[1], PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]), false);

    auto lastFenceValue = 45llu;
    auto lastFenceValue2 = 23llu;
    auto lastFenceValue3 = 373llu;

    EXPECT_EQ(maxOsContextCount, residency.lastFenceValues.size());

    residency.updateCompletionData(lastFenceValue, osContext.getContextId());
    EXPECT_EQ(maxOsContextCount, residency.lastFenceValues.size());
    EXPECT_EQ(lastFenceValue, residency.lastFenceValues[0]);
    EXPECT_EQ(lastFenceValue, residency.getFenceValueForContextId(osContext.getContextId()));

    residency.updateCompletionData(lastFenceValue2, osContext2.getContextId());

    EXPECT_EQ(maxOsContextCount, residency.lastFenceValues.size());
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

TEST(MemoryManagerTest, givenAllocationTypesThatMayNeedL3FlushWhenCallingGetAllocationDataThenFlushL3FlagIsCorrectlySet) {
    AllocationData allocData;
    AllocationProperties properties(1, GraphicsAllocation::AllocationType::UNDECIDED);
    properties.flags.flushL3RequiredForRead = 1;
    properties.flags.flushL3RequiredForWrite = 1;

    GraphicsAllocation::AllocationType allocationTypesThatMayNeedL3Flush[] = {
        GraphicsAllocation::AllocationType::BUFFER, GraphicsAllocation::AllocationType::BUFFER_COMPRESSED,
        GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, GraphicsAllocation::AllocationType::EXTERNAL_HOST_PTR,
        GraphicsAllocation::AllocationType::GLOBAL_SURFACE, GraphicsAllocation::AllocationType::IMAGE,
        GraphicsAllocation::AllocationType::PIPE, GraphicsAllocation::AllocationType::SHARED_IMAGE,
        GraphicsAllocation::AllocationType::SHARED_BUFFER, GraphicsAllocation::AllocationType::SHARED_RESOURCE_COPY,
        GraphicsAllocation::AllocationType::SVM, GraphicsAllocation::AllocationType::UNDECIDED};

    for (auto allocationType : allocationTypesThatMayNeedL3Flush) {
        properties.allocationType = allocationType;
        MockMemoryManager::getAllocationData(allocData, properties, {}, nullptr);
        EXPECT_TRUE(allocData.flags.flushL3);
    }

    properties.flags.flushL3RequiredForRead = 0;
    properties.flags.flushL3RequiredForWrite = 0;

    for (auto allocationType : allocationTypesThatMayNeedL3Flush) {
        properties.allocationType = allocationType;
        MockMemoryManager::getAllocationData(allocData, properties, {}, nullptr);
        EXPECT_FALSE(allocData.flags.flushL3);
    }
}

TEST(HeapSelectorTest, given32bitInternalAllocationWhenSelectingHeapThenInternalHeapIsUsed) {
    GraphicsAllocation allocation{GraphicsAllocation::AllocationType::KERNEL_ISA, nullptr, 0, 0, 0, MemoryPool::MemoryNull, false};
    allocation.set32BitAllocation(true);
    EXPECT_EQ(internalHeapIndex, MemoryManager::selectHeap(&allocation, false, false));
}

TEST(HeapSelectorTest, givenNon32bitInternalAllocationWhenSelectingHeapThenInternalHeapIsUsed) {
    GraphicsAllocation allocation{GraphicsAllocation::AllocationType::KERNEL_ISA, nullptr, 0, 0, 0, MemoryPool::MemoryNull, false};
    allocation.set32BitAllocation(false);
    EXPECT_EQ(internalHeapIndex, MemoryManager::selectHeap(&allocation, false, false));
}

TEST(HeapSelectorTest, given32bitExternalAllocationWhenSelectingHeapThenExternalHeapIsUsed) {
    GraphicsAllocation allocation{GraphicsAllocation::AllocationType::UNKNOWN, nullptr, 0, 0, 0, MemoryPool::MemoryNull, false};
    allocation.set32BitAllocation(true);
    EXPECT_EQ(HeapIndex::HEAP_EXTERNAL, MemoryManager::selectHeap(&allocation, false, false));
}

TEST(HeapSelectorTest, givenLimitedAddressSpaceWhenSelectingHeapForExternalAllocationThenStandardHeapIsUsed) {
    GraphicsAllocation allocation{GraphicsAllocation::AllocationType::UNKNOWN, nullptr, 0, 0, 0, MemoryPool::MemoryNull, false};
    EXPECT_EQ(HeapIndex::HEAP_STANDARD, MemoryManager::selectHeap(&allocation, true, false));
}

TEST(HeapSelectorTest, givenFullAddressSpaceWhenSelectingHeapForExternalAllocationWithPtrThenSvmHeapIsUsed) {
    GraphicsAllocation allocation{GraphicsAllocation::AllocationType::UNKNOWN, nullptr, 0, 0, 0, MemoryPool::MemoryNull, false};
    EXPECT_EQ(HeapIndex::HEAP_SVM, MemoryManager::selectHeap(&allocation, true, true));
}

TEST(HeapSelectorTest, givenFullAddressSpaceWhenSelectingHeapForExternalAllocationWithoutPtrAndResourceIs64KSuitableThenStandard64kHeapIsUsed) {
    GraphicsAllocation allocation{GraphicsAllocation::AllocationType::UNKNOWN, nullptr, 0, 0, 0, MemoryPool::MemoryNull, false};
    auto gmm = std::make_unique<Gmm>(nullptr, 0, false);
    auto resourceInfo = static_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    resourceInfo->is64KBPageSuitableValue = true;
    allocation.setDefaultGmm(gmm.get());
    EXPECT_EQ(HeapIndex::HEAP_STANDARD64KB, MemoryManager::selectHeap(&allocation, false, true));
}

TEST(HeapSelectorTest, givenFullAddressSpaceWhenSelectingHeapForExternalAllocationWithoutPtrAndResourceIsNot64KSuitableThenStandardHeapIsUsed) {
    GraphicsAllocation allocation{GraphicsAllocation::AllocationType::UNKNOWN, nullptr, 0, 0, 0, MemoryPool::MemoryNull, false};
    auto gmm = std::make_unique<Gmm>(nullptr, 0, false);
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
    MemoryAllocation allocation{GraphicsAllocation::AllocationType::COMMAND_BUFFER, nullptr, nullptr, 0, 0, 0,
                                MemoryPool::MemoryNull, false, false, false};
    EXPECT_EQ(GraphicsAllocation::AllocationType::COMMAND_BUFFER, allocation.getAllocationType());
}

TEST(MemoryAllocationTest, givenMemoryPoolWhenPassedToMemoryAllocationConstructorThenMemoryPoolIsStored) {
    MemoryAllocation allocation{GraphicsAllocation::AllocationType::COMMAND_BUFFER, nullptr, nullptr, 0, 0, 0,
                                MemoryPool::System64KBPages, false, false, false};
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
    void *reserve = memoryManager->reserveCpuAddressRange(size);
    EXPECT_NE(nullptr, reserve);
    allocation->setReservedAddressRange(reserve, size);
    EXPECT_EQ(reserve, allocation->getReservedAddressPtr());
    EXPECT_EQ(size, allocation->getReservedAddressSize());
    memoryManager->freeGraphicsMemory(allocation);
}

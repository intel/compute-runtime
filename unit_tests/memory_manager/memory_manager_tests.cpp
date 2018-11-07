/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/event/event.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/helpers/timestamp_packet.h"
#include "runtime/memory_manager/internal_allocation_storage.h"
#include "runtime/memory_manager/memory_constants.h"
#include "runtime/mem_obj/image.h"
#include "runtime/os_interface/os_context.h"
#include "runtime/os_interface/os_interface.h"
#include "runtime/program/printf_handler.h"
#include "runtime/program/program.h"
#include "runtime/utilities/tag_allocator.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/memory_allocator_fixture.h"
#include "unit_tests/fixtures/memory_manager_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/memory_management.h"
#include "unit_tests/helpers/variable_backup.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_deferrable_deletion.h"
#include "unit_tests/mocks/mock_deferred_deleter.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_gmm.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_mdi.h"
#include "unit_tests/mocks/mock_memory_manager.h"

#include "test.h"
#include <future>
#include <type_traits>

using namespace OCLRT;

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
    EXPECT_EQ(0u, gfxAllocation.gpuBaseAddress);
}

TEST(GraphicsAllocationTest, Ctor2) {
    void *cpuPtr = (void *)0x30000;
    size_t size = 0x1000;
    osHandle sharedHandle = Sharing::nonSharedResource;
    GraphicsAllocation gfxAllocation(cpuPtr, size, sharedHandle);
    uint64_t expectedGpuAddr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gfxAllocation.getUnderlyingBuffer()));

    EXPECT_EQ(expectedGpuAddr, gfxAllocation.getGpuAddress());
    EXPECT_EQ(0u, gfxAllocation.gpuBaseAddress);
    EXPECT_EQ(sharedHandle, gfxAllocation.peekSharedHandle());
}

TEST(GraphicsAllocationTest, getGpuAddress) {
    void *cpuPtr = (void *)0x30000;
    uint64_t gpuAddr = 0x30000;
    uint64_t gpuBaseAddr = 0x10000;
    size_t size = 0x1000;

    GraphicsAllocation gfxAllocation(cpuPtr, gpuAddr, gpuBaseAddr, size);

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

    GraphicsAllocation gfxAllocation(cpuPtr, gpuAddr, gpuBaseAddr, size);

    EXPECT_EQ(gpuAddr - gpuBaseAddr, gfxAllocation.getGpuAddressToPatch());
}

TEST(GraphicsAllocationTest, setSize) {
    void *cpuPtr = (void *)0x30000;
    uint64_t gpuAddr = 0x30000;
    uint64_t gpuBaseAddr = 0x10000;
    size_t size = 0x2000;

    GraphicsAllocation gfxAllocation(cpuPtr, gpuAddr, gpuBaseAddr, size);
    EXPECT_EQ(size, gfxAllocation.getUnderlyingBufferSize());

    size = 0x3000;
    gfxAllocation.setSize(size);
    EXPECT_EQ(size, gfxAllocation.getUnderlyingBufferSize());
}

TEST(GraphicsAllocationTest, GivenGraphicsAllocationWhenLockingThenIsLocked) {
    void *cpuPtr = (void *)0x30000;
    uint64_t gpuAddr = 0x30000;
    uint64_t gpuBaseAddr = 0x10000;
    size_t size = 0x1000;

    GraphicsAllocation gfxAllocation(cpuPtr, gpuAddr, gpuBaseAddr, size);

    gfxAllocation.setLocked(false);
    EXPECT_FALSE(gfxAllocation.isLocked());
    gfxAllocation.setLocked(true);
    EXPECT_TRUE(gfxAllocation.isLocked());

    gfxAllocation.setCpuPtrAndGpuAddress(cpuPtr, gpuAddr);
    cpuPtr = gfxAllocation.getUnderlyingBuffer();
    EXPECT_NE(nullptr, cpuPtr);
    gpuAddr = gfxAllocation.getGpuAddress();
    EXPECT_NE(0ULL, gpuAddr);
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

    auto allocation = memoryManager->allocateGraphicsMemory(sizeof(char));
    ASSERT_NE(nullptr, allocation);
    // initial taskCount must be -1. if not, we may kill allocation before it will be used
    EXPECT_EQ((uint32_t)-1, allocation->getTaskCount(0));
    // We know we want graphics memory to be page aligned
    EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(allocation->getUnderlyingBuffer()) & (alignment - 1));
    EXPECT_EQ(Sharing::nonSharedResource, allocation->peekSharedHandle());

    // Gpu address equal to cpu address
    EXPECT_EQ(reinterpret_cast<uint64_t>(allocation->getUnderlyingBuffer()), allocation->getGpuAddress());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(MemoryAllocatorTest, allocateGraphicsPageAligned) {
    unsigned int alignment = 4096;

    auto allocation = memoryManager->allocateGraphicsMemory(sizeof(char));
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(allocation->getUnderlyingBuffer()) & (alignment - 1));
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(MemoryAllocatorTest, allocateGraphicsMoreThanPageAligned) {
    unsigned int alignment = 6144;

    auto allocation = memoryManager->allocateGraphicsMemory(sizeof(char), 6144, false, false);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(allocation->getUnderlyingBuffer()) & (alignment - 1));
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(MemoryAllocatorTest, AlignedHostPtrWithAlignedSizeWhenAskedForGraphicsAllocationReturnsNullStorageFromHostPtrManager) {
    auto ptr = (void *)0x1000;
    MockMemoryManager mockMemoryManager(*executionEnvironment);
    auto hostPtrManager = static_cast<MockHostPtrManager *>(mockMemoryManager.getHostPtrManager());
    auto graphicsAllocation = mockMemoryManager.allocateGraphicsMemory(4096, ptr);
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

    auto graphicsAllocation = memoryManager->allocateGraphicsMemory(alignedSize, ptr);

    EXPECT_TRUE(graphicsAllocation->isL3Capable());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(MemoryAllocatorTest, GivenAlignedHostPtrAndNotCacheAlignedSizeWhenAskedForL3AllowanceThenFalseIsReturned) {
    auto ptr = (void *)0x1000;
    auto alignedSize = MemoryConstants::cacheLineSize - 1;

    auto graphicsAllocation = memoryManager->allocateGraphicsMemory(alignedSize, ptr);

    EXPECT_FALSE(graphicsAllocation->isL3Capable());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(MemoryAllocatorTest, GivenMisAlignedHostPtrAndNotCacheAlignedSizeWhenAskedForL3AllowanceThenFalseIsReturned) {
    auto ptr = (void *)0x1001;
    auto alignedSize = MemoryConstants::cacheLineSize - 1;

    auto graphicsAllocation = memoryManager->allocateGraphicsMemory(alignedSize, ptr);

    EXPECT_FALSE(graphicsAllocation->isL3Capable());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(MemoryAllocatorTest, GivenHostPtrAlignedToCacheLineWhenAskedForL3AllowanceThenTrueIsReturned) {
    auto ptr = (void *)0x1040;
    auto alignedSize = MemoryConstants::cacheLineSize;

    auto graphicsAllocation = memoryManager->allocateGraphicsMemory(alignedSize, ptr);

    EXPECT_TRUE(graphicsAllocation->isL3Capable());

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

    auto graphicsAllocation = mockMemoryManager.allocateGraphicsMemory(size, cpuPtr);
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

    auto allocation = memoryManager->createGraphicsAllocation(handleStorage, size, ptr);

    EXPECT_EQ(ptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());

    EXPECT_EQ(ptr, allocation->fragmentsStorage.fragmentStorageData[0].cpuPtr);
    EXPECT_EQ(ptr2, allocation->fragmentsStorage.fragmentStorageData[1].cpuPtr);
    EXPECT_EQ(nullptr, allocation->fragmentsStorage.fragmentStorageData[2].cpuPtr);

    EXPECT_EQ(size, allocation->fragmentsStorage.fragmentStorageData[0].fragmentSize);
    EXPECT_EQ(size * 2, allocation->fragmentsStorage.fragmentStorageData[1].fragmentSize);
    EXPECT_EQ(size * 3, allocation->fragmentsStorage.fragmentStorageData[2].fragmentSize);

    EXPECT_NE(&allocation->fragmentsStorage, &handleStorage);

    delete allocation;
}

TEST_F(MemoryAllocatorTest, getEventTsAllocator) {
    TagAllocator<HwTimeStamps> *allocator = memoryManager->getEventTsAllocator();
    EXPECT_NE(nullptr, allocator);
    TagAllocator<HwTimeStamps> *allocator2 = memoryManager->getEventTsAllocator();
    EXPECT_EQ(allocator2, allocator);
}

TEST_F(MemoryAllocatorTest, getEventPerfCountAllocator) {
    TagAllocator<HwPerfCounter> *allocator = memoryManager->getEventPerfCountAllocator();
    EXPECT_NE(nullptr, allocator);
    TagAllocator<HwPerfCounter> *allocator2 = memoryManager->getEventPerfCountAllocator();
    EXPECT_EQ(allocator2, allocator);
}

TEST_F(MemoryAllocatorTest, givenTimestampPacketAllocatorWhenAskingForTagThenReturnValidObject) {
    ExecutionEnvironment executionEnvironment;
    class MyMockMemoryManager : public OsAgnosticMemoryManager {
      public:
        using OsAgnosticMemoryManager::timestampPacketAllocator;
        MyMockMemoryManager(ExecutionEnvironment &executionEnvironment) : OsAgnosticMemoryManager(false, false, executionEnvironment){};
    } myMockMemoryManager(executionEnvironment);

    EXPECT_EQ(nullptr, myMockMemoryManager.timestampPacketAllocator.get());

    TagAllocator<TimestampPacket> *allocator = myMockMemoryManager.getTimestampPacketAllocator();
    EXPECT_NE(nullptr, myMockMemoryManager.timestampPacketAllocator.get());
    EXPECT_EQ(allocator, myMockMemoryManager.timestampPacketAllocator.get());

    TagAllocator<TimestampPacket> *allocator2 = myMockMemoryManager.getTimestampPacketAllocator();
    EXPECT_EQ(allocator, allocator2);

    auto node1 = allocator->getTag();
    auto node2 = allocator->getTag();
    EXPECT_NE(nullptr, node1);
    EXPECT_NE(nullptr, node2);
    EXPECT_NE(node1, node2);
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
    auto allocation = memoryManager->allocate32BitGraphicsMemory(size, nullptr, AllocationOrigin::EXTERNAL_ALLOCATION);
    EXPECT_NE(nullptr, allocation);
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_TRUE(allocation->is32BitAllocation);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(MemoryAllocatorTest, givenNotEnoughSpaceInAllocatorWhenAskedFor32bitAllocationNullptrIsReturned) {
    size_t size = 0xfffff000;
    auto allocationFirst = memoryManager->allocate32BitGraphicsMemory(0x5000, nullptr, AllocationOrigin::EXTERNAL_ALLOCATION);
    auto allocation = memoryManager->allocate32BitGraphicsMemory(size, nullptr, AllocationOrigin::EXTERNAL_ALLOCATION);
    EXPECT_EQ(nullptr, allocation);
    if (allocation)
        memoryManager->freeGraphicsMemory(allocation);
    memoryManager->freeGraphicsMemory(allocationFirst);
}

TEST_F(MemoryAllocatorTest, givenNotEnoughSpaceInAllocatorWhenAskedFor32bitAllocationWithHostPtrThenNullptrIsReturned) {
    size_t size = 0xfffff000;
    void *ptr = (void *)0x10000;
    auto allocationFirst = memoryManager->allocate32BitGraphicsMemory(0x5000, nullptr, AllocationOrigin::EXTERNAL_ALLOCATION);
    auto allocation = memoryManager->allocate32BitGraphicsMemory(size, ptr, AllocationOrigin::EXTERNAL_ALLOCATION);
    EXPECT_EQ(nullptr, allocation);
    if (allocation)
        memoryManager->freeGraphicsMemory(allocation);
    memoryManager->freeGraphicsMemory(allocationFirst);
}

TEST_F(MemoryAllocatorTest, givenMemoryManagerWhenAskedFor32bitAllocationWithPtrThen32bitGraphicsAllocationWithGpuAddressIsReturned) {
    size_t size = 10;
    void *ptr = (void *)0x1000;
    auto allocation = memoryManager->allocate32BitGraphicsMemory(size, ptr, AllocationOrigin::EXTERNAL_ALLOCATION);
    EXPECT_NE(nullptr, allocation);
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_TRUE(allocation->is32BitAllocation);

    EXPECT_EQ(ptr, allocation->getUnderlyingBuffer());
    EXPECT_NE(0u, allocation->getGpuAddress());

    memoryManager->freeGraphicsMemory(allocation);
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

        EXPECT_TRUE(printfAllocation->is32BitAllocation);
        EXPECT_EQ(allocationAddress32bit, *ptr32Bit);
        for (int i = 4; i < 8; i++) {
            EXPECT_EQ(50, kernel.mockKernel->mockCrossThreadData[i]);
        }

        delete printfHandler;

        DebugManager.flags.Force32bitAddressing.set(false);
    }
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenItIsCreatedThenForce32BitAllocationsIsFalse) {
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(false, false, executionEnvironment);
    EXPECT_FALSE(memoryManager.peekForce32BitAllocations());
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenForce32bitallocationIsCalledWithTrueThenMemoryManagerForces32BitAlloactions) {
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(false, false, executionEnvironment);
    memoryManager.setForce32BitAllocations(true);
    EXPECT_TRUE(memoryManager.peekForce32BitAllocations());
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenAllocateGraphicsMemoryForImageIsCalledThenGraphicsAllocationIsReturned) {
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(false, false, executionEnvironment);
    cl_image_desc imgDesc = {};
    imgDesc.image_width = 512;
    imgDesc.image_height = 1;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    executionEnvironment.initGmm(*platformDevices);
    auto queryGmm = MockGmm::queryImgParams(imgInfo);

    auto imageAllocation = memoryManager.allocateGraphicsMemoryForImage(imgInfo, queryGmm.get());
    ASSERT_NE(nullptr, imageAllocation);
    EXPECT_TRUE(imageAllocation->gmm->resourceParams.Usage == GMM_RESOURCE_USAGE_TYPE::GMM_RESOURCE_USAGE_OCL_IMAGE);
    EXPECT_TRUE(imageAllocation->gmm->useSystemMemoryPool);
    queryGmm.release();
    memoryManager.freeGraphicsMemory(imageAllocation);
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerAndUnifiedAuxCapableAllocationWhenMappingThenReturnFalse) {
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(false, false, executionEnvironment);

    auto gmm = new Gmm(nullptr, 123, false);
    auto allocation = memoryManager.allocateGraphicsMemory(123);
    allocation->gmm = gmm;

    auto mockGmmRes = reinterpret_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get());
    mockGmmRes->setUnifiedAuxTranslationCapable();

    EXPECT_FALSE(memoryManager.mapAuxGpuVA(allocation));

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenAllocateGraphicsMemoryIsCalledThenMemoryPoolIsSystem4KBPages) {
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(false, false, executionEnvironment);
    auto size = 4096u;

    auto allocation = memoryManager.allocateGraphicsMemory(size);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());
    memoryManager.freeGraphicsMemory(allocation);

    allocation = memoryManager.allocateGraphicsMemory(size, MemoryConstants::preferredAlignment, false, false);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());
    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWith64KBPagesEnabledWhenAllocateGraphicsMemory64kbIsCalledThenMemoryPoolIsSystem64KBPages) {
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(true, false, executionEnvironment);
    auto size = 4096u;

    auto allocation = memoryManager.allocateGraphicsMemory64kb(size, MemoryConstants::preferredAlignment, false, false);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System64KBPages, allocation->getMemoryPool());
    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWith64KBPagesEnabledWhenAllocateGraphicsMemoryFailsThenNullptrIsReturned) {
    class MockOsAgnosticManagerWithFailingAllocate : public OsAgnosticMemoryManager {
      public:
        MockOsAgnosticManagerWithFailingAllocate(bool enable64kbPages, ExecutionEnvironment &executionEnvironment) : OsAgnosticMemoryManager(enable64kbPages, false, executionEnvironment) {}

        GraphicsAllocation *allocateGraphicsMemory(size_t size, size_t alignment, bool forcePin, bool uncacheable) override {
            return nullptr;
        }
    };
    ExecutionEnvironment executionEnvironment;
    MockOsAgnosticManagerWithFailingAllocate memoryManager(true, executionEnvironment);
    auto size = 4096u;

    auto allocation = memoryManager.allocateGraphicsMemory64kb(size, MemoryConstants::preferredAlignment, false, false);
    EXPECT_EQ(nullptr, allocation);
    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenAllocateGraphicsMemoryWithPtrIsCalledThenMemoryPoolIsSystem4KBPages) {
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(false, false, executionEnvironment);
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto size = MemoryConstants::pageSize;

    auto allocation = memoryManager.allocateGraphicsMemory(size, ptr, false);

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPages, allocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenAllocate32BitGraphicsMemoryWithPtrIsCalledThenMemoryPoolIsSystem4KBPagesWith32BitGpuAddressing) {
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(false, false, executionEnvironment);
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto size = MemoryConstants::pageSize;

    auto allocation = memoryManager.allocate32BitGraphicsMemory(size, ptr, AllocationOrigin::EXTERNAL_ALLOCATION);

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPagesWith32BitGpuAddressing, allocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenAllocate32BitGraphicsMemoryWithoutPtrIsCalledThenMemoryPoolIsSystem4KBPagesWith32BitGpuAddressing) {
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(false, false, executionEnvironment);
    void *ptr = nullptr;
    auto size = MemoryConstants::pageSize;

    auto allocation = memoryManager.allocate32BitGraphicsMemory(size, ptr, AllocationOrigin::EXTERNAL_ALLOCATION);

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::System4KBPagesWith32BitGpuAddressing, allocation->getMemoryPool());
    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWith64KBPagesEnabledWhenAllocateGraphicsMemoryForSVMIsCalledThenMemoryPoolIsSystem64KBPages) {
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(true, false, executionEnvironment);
    auto size = 4096u;

    auto svmAllocation = memoryManager.allocateGraphicsMemoryForSVM(size, false);
    EXPECT_NE(nullptr, svmAllocation);
    EXPECT_EQ(MemoryPool::System64KBPages, svmAllocation->getMemoryPool());
    memoryManager.freeGraphicsMemory(svmAllocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWith64KBPagesDisabledWhenAllocateGraphicsMemoryForSVMIsCalledThen4KBGraphicsAllocationIsReturned) {
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(false, false, executionEnvironment);
    auto size = 4096u;
    auto isCoherent = true;

    auto svmAllocation = memoryManager.allocateGraphicsMemoryForSVM(size, isCoherent);
    EXPECT_NE(nullptr, svmAllocation);
    EXPECT_TRUE(svmAllocation->isCoherent());
    EXPECT_EQ(MemoryPool::System4KBPages, svmAllocation->getMemoryPool());

    EXPECT_EQ(size, svmAllocation->getUnderlyingBufferSize());

    uintptr_t address = reinterpret_cast<uintptr_t>(svmAllocation->getUnderlyingBuffer());
    EXPECT_EQ(0u, (address & MemoryConstants::pageMask));
    memoryManager.freeGraphicsMemory(svmAllocation);
}

TEST(OsAgnosticMemoryManager, givenDeviceWith64kbPagesEnabledWhenCreatingMemoryManagerThenAllowFor64kbAllocations) {
    VariableBackup<bool> os64kbPagesEnabled(&OSInterface::osEnabled64kbPages, true);

    HardwareInfo localHwInfo = *platformDevices[0];
    localHwInfo.capabilityTable.ftr64KBpages = true;

    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<Device>(&localHwInfo));
    EXPECT_TRUE(device->getEnabled64kbPages());
    EXPECT_TRUE(device->getMemoryManager()->peek64kbPagesEnabled());
}

TEST(OsAgnosticMemoryManager, givenDeviceWith64kbPagesDisbledWhenCreatingMemoryManagerThenDisallowFor64kbAllocations) {
    HardwareInfo localHwInfo = *platformDevices[0];
    localHwInfo.capabilityTable.ftr64KBpages = false;

    std::unique_ptr<Device> device(MockDevice::createWithNewExecutionEnvironment<Device>(&localHwInfo));
    EXPECT_FALSE(device->getEnabled64kbPages());
    EXPECT_FALSE(device->getMemoryManager()->peek64kbPagesEnabled());
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWith64KBPagesEnabledWhenAllocateGraphicsMemoryForSVMIsCalledThen64KBGraphicsAllocationIsReturned) {
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(true, false, executionEnvironment);
    auto size = 4096u;
    auto isCoherent = true;

    auto svmAllocation = memoryManager.allocateGraphicsMemoryForSVM(size, isCoherent);
    EXPECT_NE(nullptr, svmAllocation);
    EXPECT_TRUE(svmAllocation->isCoherent());
    EXPECT_EQ(MemoryPool::System64KBPages, svmAllocation->getMemoryPool());

    EXPECT_EQ(MemoryConstants::pageSize64k, svmAllocation->getUnderlyingBufferSize());

    uintptr_t address = reinterpret_cast<uintptr_t>(svmAllocation->getUnderlyingBuffer());
    EXPECT_EQ(0u, (address & MemoryConstants::page64kMask));
    memoryManager.freeGraphicsMemory(svmAllocation);
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenCreateGraphicsAllocationFromSharedObjectIsCalledThenGraphicsAllocationIsReturned) {
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(false, false, executionEnvironment);
    osHandle handle = 1;
    auto size = 4096u;
    auto sharedAllocation = memoryManager.createGraphicsAllocationFromSharedHandle(handle, false);
    EXPECT_NE(nullptr, sharedAllocation);
    EXPECT_FALSE(sharedAllocation->isCoherent());
    EXPECT_NE(nullptr, sharedAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, sharedAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(MemoryPool::SystemCpuInaccessible, sharedAllocation->getMemoryPool());

    memoryManager.freeGraphicsMemory(sharedAllocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenCreateAllocationFromNtHandleIsCalledThenReturnNullptr) {
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(false, false, executionEnvironment);
    auto graphicsAllocation = memoryManager.createGraphicsAllocationFromNTHandle((void *)1);
    EXPECT_EQ(nullptr, graphicsAllocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenLockUnlockCalledThenDoNothing) {
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(false, false, executionEnvironment);
    auto allocation = memoryManager.allocateGraphicsMemory(1);
    ASSERT_NE(nullptr, allocation);

    auto ptr = memoryManager.lockResource(allocation);
    EXPECT_EQ(nullptr, ptr);
    memoryManager.unlockResource(allocation);

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenGraphicsAllocationContainsOffsetWhenAddressIsObtainedThenOffsetIsAdded) {
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(false, false, executionEnvironment);

    auto graphicsAllocation = memoryManager.allocateGraphicsMemory(4096u);

    auto graphicsAddress = graphicsAllocation->getGpuAddress();
    auto graphicsAddressToPatch = graphicsAllocation->getGpuAddressToPatch();

    graphicsAllocation->allocationOffset = 4;

    auto offsetedGraphicsAddress = graphicsAllocation->getGpuAddress();
    auto offsetedGraphicsAddressToPatch = graphicsAllocation->getGpuAddressToPatch();

    EXPECT_EQ(offsetedGraphicsAddress, graphicsAddress + graphicsAllocation->allocationOffset);
    EXPECT_EQ(offsetedGraphicsAddressToPatch, graphicsAddressToPatch + graphicsAllocation->allocationOffset);

    memoryManager.freeGraphicsMemory(graphicsAllocation);
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenGraphicsAllocationIsPaddedThenNewGraphicsAllocationIsCreated) {
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(false, false, executionEnvironment);
    auto graphicsAllocation = memoryManager.allocateGraphicsMemory(4096u);

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
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(false, false, executionEnvironment);
    auto graphicsAllocation = memoryManager.allocateGraphicsMemory(4096u);

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

TEST(OsAgnosticMemoryManager, alignmentIsCorrect) {
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(false, false, executionEnvironment);
    const size_t alignment = 0;
    auto ga = memoryManager.allocateGraphicsMemory(MemoryConstants::pageSize >> 1, alignment, false, false);
    uintptr_t ptr = reinterpret_cast<uintptr_t>(ga->getUnderlyingBuffer());
    ptr &= (MemoryConstants::allocationAlignment - 1);
    EXPECT_EQ(ptr, 0u);

    memoryManager.freeGraphicsMemory(ga);
}

TEST(OsAgnosticMemoryManager, givenCommonMemoryManagerWhenIsAskedIfApplicationMemoryBudgetIsExhaustedThenFalseIsReturned) {
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(false, false, executionEnvironment);
    EXPECT_FALSE(memoryManager.isMemoryBudgetExhausted());
}

class MemoryManagerWithAsyncDeleterTest : public ::testing::Test {
  public:
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
    auto allocation = memoryManager.MemoryManager::allocateGraphicsMemory(sizeof(char), (void *)&ptr);
    EXPECT_TRUE(deleter->isQueueEmpty());
    memoryManager.freeGraphicsMemoryImpl(allocation);
}

TEST_F(MemoryManagerWithAsyncDeleterTest, givenMemoryManagerWhenAllocateGraphicsMemoryIsCalledWithPtrAndDeleterIsNullptrThenItDoesntCrash) {
    memoryManager.setDeferredDeleter(nullptr);
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    char ptr[128];
    auto allocation = memoryManager.MemoryManager::allocateGraphicsMemory(sizeof(char), (void *)&ptr);
    memoryManager.freeGraphicsMemoryImpl(allocation);
}

TEST(OsAgnosticMemoryManager, givenMemoryManagerWhenIsAsyncDeleterEnabledCalledThenReturnsValueOfFlag) {
    MockMemoryManager memoryManager;
    memoryManager.overrideAsyncDeleterFlag(false);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    memoryManager.overrideAsyncDeleterFlag(true);
    EXPECT_TRUE(memoryManager.isAsyncDeleterEnabled());
}

TEST(OsAgnosticMemoryManager, givenDefaultMemoryManagerWhenItIsCreatedThenAsyncDeleterEnabledIsFalse) {
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(false, false, executionEnvironment);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
}

TEST(OsAgnosticMemoryManager, givenEnabledAsyncDeleterFlagWhenMemoryManagerIsCreatedThenAsyncDeleterEnabledIsFalseAndDeleterIsNullptr) {
    bool defaultEnableDeferredDeleterFlag = DebugManager.flags.EnableDeferredDeleter.get();
    DebugManager.flags.EnableDeferredDeleter.set(true);
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(false, false, executionEnvironment);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    DebugManager.flags.EnableDeferredDeleter.set(defaultEnableDeferredDeleterFlag);
}

TEST(OsAgnosticMemoryManager, givenDisabledAsyncDeleterFlagWhenMemoryManagerIsCreatedThenAsyncDeleterEnabledIsFalseAndDeleterIsNullptr) {
    bool defaultEnableDeferredDeleterFlag = DebugManager.flags.EnableDeferredDeleter.get();
    DebugManager.flags.EnableDeferredDeleter.set(false);
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(false, false, executionEnvironment);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    DebugManager.flags.EnableDeferredDeleter.set(defaultEnableDeferredDeleterFlag);
}

TEST(OsAgnosticMemoryManager, GivenEnabled64kbPagesWhenHostMemoryAllocationIsCreatedThenAlignedto64KbAllocationIsReturned) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.Enable64kbpages.set(true);
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(true, false, executionEnvironment);

    GraphicsAllocation *galloc = memoryManager.allocateGraphicsMemoryInPreferredPool(AllocationFlags(true), 0, nullptr, 64 * 1024, GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY);
    EXPECT_NE(nullptr, galloc);
    memoryManager.freeGraphicsMemory(galloc);

    galloc = memoryManager.allocateGraphicsMemoryInPreferredPool(AllocationFlags(true), 0, nullptr, 64 * 1024, GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY);
    EXPECT_NE(nullptr, galloc);
    EXPECT_NE(nullptr, galloc->getUnderlyingBuffer());
    EXPECT_EQ(0u, (uintptr_t)galloc->getUnderlyingBuffer() % MemoryConstants::pageSize64k);

    EXPECT_NE(0u, galloc->getGpuAddress());
    EXPECT_EQ(0u, (uintptr_t)galloc->getGpuAddress() % MemoryConstants::pageSize64k);
    memoryManager.freeGraphicsMemory(galloc);
}

TEST(OsAgnosticMemoryManager, givenPointerAndSizeWhenCreateInternalAllocationIsCalledThenGraphicsAllocationIsReturned) {
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(false, false, executionEnvironment);
    auto ptr = (void *)0x100000;
    size_t allocationSize = 4096;
    auto graphicsAllocation = memoryManager.allocate32BitGraphicsMemory(allocationSize, ptr, AllocationOrigin::INTERNAL_ALLOCATION);
    EXPECT_EQ(ptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(allocationSize, graphicsAllocation->getUnderlyingBufferSize());
    memoryManager.freeGraphicsMemory(graphicsAllocation);
}
TEST(OsAgnosticMemoryManager, givenDefaultOsAgnosticMemoryManagerWhenItIsQueriedForInternalHeapBaseThen32BitAllocatorBaseIsReturned) {
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(false, false, executionEnvironment);
    auto heapBase = memoryManager.allocator32Bit->getBase();
    EXPECT_EQ(heapBase, memoryManager.getInternalHeapBaseAddress());
}

TEST(OsAgnosticMemoryManager, givenOsAgnosticMemoryManagerWhenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledThenAllocationIsCreated) {
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(false, false, executionEnvironment);
    auto hostPtr = reinterpret_cast<void *>(0x5001);
    auto allocation = memoryManager.allocateGraphicsMemoryForNonSvmHostPtr(13, hostPtr);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(13u, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(1u, allocation->allocationOffset);

    memoryManager.freeGraphicsMemory(allocation);
}

using OsAgnosticMemoryManagerWithParams = ::testing::TestWithParam<bool>;

TEST_P(OsAgnosticMemoryManagerWithParams, givenReducedGpuAddressSpaceWhenAllocateGraphicsMemoryForHostPtrIsCalledThenAllocationWithoutFragmentsIsCreated) {
    bool requiresL3Flush = GetParam();
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(false, false, executionEnvironment);
    auto hostPtr = reinterpret_cast<void *>(0x5001);

    auto allocation = memoryManager.allocateGraphicsMemoryForHostPtr(13, hostPtr, false, requiresL3Flush);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(0u, allocation->fragmentsStorage.fragmentCount);
    EXPECT_EQ(requiresL3Flush, allocation->flushL3Required);

    memoryManager.freeGraphicsMemory(allocation);
}

TEST_P(OsAgnosticMemoryManagerWithParams, givenFullGpuAddressSpaceWhenAllocateGraphicsMemoryForHostPtrIsCalledThenAllocationWithFragmentsIsCreated) {
    bool requiresL3Flush = GetParam();
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(false, false, executionEnvironment);
    auto hostPtr = reinterpret_cast<void *>(0x5001);

    auto allocation = memoryManager.allocateGraphicsMemoryForHostPtr(13, hostPtr, true, requiresL3Flush);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(1u, allocation->fragmentsStorage.fragmentCount);
    EXPECT_FALSE(allocation->flushL3Required);

    memoryManager.freeGraphicsMemory(allocation);
}

INSTANTIATE_TEST_CASE_P(OsAgnosticMemoryManagerWithParams,
                        OsAgnosticMemoryManagerWithParams,
                        ::testing::Values(false, true));

TEST(OsAgnosticMemoryManager, givenLocalMemoryNotSupportedWhenMemoryManagerIsCreatedThenAllocator32BitHasCorrectBaseAddress) {
    ExecutionEnvironment executionEnvironment;
    MockMemoryManager memoryManager(false, false, false, executionEnvironment);
    uint64_t heap32Base = 0x80000000000ul;

    if (is32bit) {
        heap32Base = 0;
    }
    EXPECT_EQ(heap32Base, memoryManager.allocator32Bit->getBase());
}

TEST(OsAgnosticMemoryManager, givenLocalMemorySupportedAndNotAubUsageWhenMemoryManagerIsCreatedThenAllocator32BitHasCorrectBaseAddress) {
    ExecutionEnvironment executionEnvironment;
    MockMemoryManager memoryManager(false, true, false, executionEnvironment);
    uint64_t heap32Base = 0x80000000000ul;

    if (is32bit) {
        heap32Base = 0;
    }
    EXPECT_EQ(heap32Base, memoryManager.allocator32Bit->getBase());
}

TEST(OsAgnosticMemoryManager, givenLocalMemoryNotSupportedAndAubUsageWhenMemoryManagerIsCreatedThenAllocator32BitHasCorrectBaseAddress) {
    ExecutionEnvironment executionEnvironment;
    MockMemoryManager memoryManager(false, false, true, executionEnvironment);
    uint64_t heap32Base = 0x80000000000ul;

    if (is32bit) {
        heap32Base = 0;
    }
    EXPECT_EQ(heap32Base, memoryManager.allocator32Bit->getBase());
}

TEST(OsAgnosticMemoryManager, givenLocalMemorySupportedAndAubUsageWhenMemoryManagerIsCreatedThenAllocator32BitHasCorrectBaseAddress) {
    ExecutionEnvironment executionEnvironment;
    MockMemoryManager memoryManager(false, true, true, executionEnvironment);
    uint64_t heap32Base = 0x80000000000ul;

    if (is32bit) {
        heap32Base = 0;
    } else {
        heap32Base = 0x40000000000ul;
    }
    EXPECT_EQ(heap32Base, memoryManager.allocator32Bit->getBase());
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
    auto graphicsAllocation1 = memoryManager->allocateGraphicsMemory(MemoryConstants::pageSize, cpuPtr1);
    EXPECT_EQ(2u, hostPtrManager->getFragmentCount());

    auto graphicsAllocation2 = memoryManager->allocateGraphicsMemory(MemoryConstants::pageSize * 3, cpuPtr2);
    EXPECT_EQ(4u, hostPtrManager->getFragmentCount());

    GraphicsAllocation *graphicsAllocation3 = nullptr;

    bool catchMe = false;
    try {
        graphicsAllocation3 = memoryManager->allocateGraphicsMemory(MemoryConstants::pageSize * 10, cpuPtr3);
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
    auto graphicsAllocation1 = memoryManager->allocateGraphicsMemory(MemoryConstants::pageSize, cpuPtr1);
    EXPECT_EQ(2u, hostPtrManager->getFragmentCount());

    auto graphicsAllocation2 = memoryManager->allocateGraphicsMemory(MemoryConstants::pageSize * 3, cpuPtr2);
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

    auto graphicsAllocation3 = memoryManager->allocateGraphicsMemory(MemoryConstants::pageSize * 10, cpuPtr3);

    EXPECT_NE(nullptr, graphicsAllocation3);

    // no more overlapping allocation, previous allocations cleaned
    EXPECT_EQ(1u, graphicsAllocation3->fragmentsStorage.fragmentCount);
    EXPECT_EQ((uintptr_t)cpuPtr3, (uintptr_t)graphicsAllocation3->fragmentsStorage.fragmentStorageData[0].cpuPtr);

    memoryManager->freeGraphicsMemory(graphicsAllocation3);
}

TEST_F(MemoryManagerWithCsrTest, givenAllocationThatWasNotUsedWhencheckGpuUsageAndDestroyGraphicsAllocationsIsCalledThenItIsDestroyedInPlace) {
    auto notUsedAllocation = memoryManager->allocateGraphicsMemory(4096);
    memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(notUsedAllocation);
    EXPECT_TRUE(csr->getTemporaryAllocations().peekIsEmpty());
}

TEST_F(MemoryManagerWithCsrTest, givenAllocationThatWasUsedAndIsCompletedWhencheckGpuUsageAndDestroyGraphicsAllocationsIsCalledThenItIsDestroyedInPlace) {
    auto usedAllocationButGpuCompleted = memoryManager->allocateGraphicsMemory(4096);

    auto tagAddress = csr->getTagAddress();
    ASSERT_NE(0u, *tagAddress);

    usedAllocationButGpuCompleted->updateTaskCount(*tagAddress - 1, 0);

    memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(usedAllocationButGpuCompleted);
    EXPECT_TRUE(csr->getTemporaryAllocations().peekIsEmpty());
}

TEST_F(MemoryManagerWithCsrTest, givenAllocationThatWasUsedAndIsNotCompletedWhencheckGpuUsageAndDestroyGraphicsAllocationsIsCalledThenItIsAddedToTemporaryAllocationList) {
    auto usedAllocationAndNotGpuCompleted = memoryManager->allocateGraphicsMemory(4096);

    auto tagAddress = csr->getTagAddress();

    usedAllocationAndNotGpuCompleted->updateTaskCount(*tagAddress + 1, 0);

    memoryManager->checkGpuUsageAndDestroyGraphicsAllocations(usedAllocationAndNotGpuCompleted);
    EXPECT_FALSE(csr->getTemporaryAllocations().peekIsEmpty());
    EXPECT_EQ(csr->getTemporaryAllocations().peekHead(), usedAllocationAndNotGpuCompleted);

    //change task count so cleanup will not clear alloc in use
    usedAllocationAndNotGpuCompleted->updateTaskCount(csr->peekLatestFlushedTaskCount(), 0);
}

class MockAlignMallocMemoryManager : public MockMemoryManager {
  public:
    MockAlignMallocMemoryManager() {
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

        alignedMemoryManager = new (std::nothrow) MockAlignMallocMemoryManager;
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

TEST(GraphicsAllocation, givenSharedHandleBasedConstructorWhenGraphicsAllocationIsCreatedThenGpuAddressHasCorrectValue) {
    uintptr_t address = 0xf0000000;
    void *addressWithTrailingBitSet = reinterpret_cast<void *>(address);
    uint64_t expectedGpuAddress = 0xf0000000;
    osHandle sharedHandle{};
    GraphicsAllocation graphicsAllocation(addressWithTrailingBitSet, 1u, sharedHandle);
    EXPECT_EQ(expectedGpuAddress, graphicsAllocation.getGpuAddress());
}

TEST(ResidencyDataTest, givenOsContextWhenItIsRegisteredToMemoryManagerThenRefCountIncreases) {
    auto osContext = new OsContext(nullptr, 0u);
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(false, false, executionEnvironment);
    memoryManager.registerOsContext(osContext);
    EXPECT_EQ(1u, memoryManager.getOsContextCount());
    EXPECT_EQ(1, osContext->getRefInternalCount());
}

TEST(ResidencyDataTest, givenTwoOsContextsWhenTheyAreRegistredFromHigherToLowerThenProperSizeIsReturned) {
    auto osContext2 = new OsContext(nullptr, 1u);
    auto osContext = new OsContext(nullptr, 0u);
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(false, false, executionEnvironment);
    memoryManager.registerOsContext(osContext2);
    memoryManager.registerOsContext(osContext);
    EXPECT_EQ(2u, memoryManager.getOsContextCount());
    EXPECT_EQ(1, osContext->getRefInternalCount());
    EXPECT_EQ(1, osContext2->getRefInternalCount());
}

TEST(ResidencyDataTest, givenResidencyDataWhenUpdateCompletionDataIsCalledThenItIsProperlyUpdated) {
    struct MockResidencyData : public ResidencyData {
        using ResidencyData::lastFenceValues;
    };

    MockResidencyData residency;

    OsContext osContext(nullptr, 0u);
    OsContext osContext2(nullptr, 1u);

    auto lastFenceValue = 45llu;
    auto lastFenceValue2 = 23llu;
    auto lastFenceValue3 = 373llu;

    EXPECT_EQ(0u, residency.lastFenceValues.size());

    residency.updateCompletionData(lastFenceValue, osContext.getContextId());
    EXPECT_EQ(1u, residency.lastFenceValues.size());
    EXPECT_EQ(lastFenceValue, residency.lastFenceValues[0]);
    EXPECT_EQ(lastFenceValue, residency.getFenceValueForContextId(osContext.getContextId()));

    residency.updateCompletionData(lastFenceValue2, osContext2.getContextId());

    EXPECT_EQ(2u, residency.lastFenceValues.size());
    EXPECT_EQ(lastFenceValue2, residency.lastFenceValues[1]);
    EXPECT_EQ(lastFenceValue2, residency.getFenceValueForContextId(osContext2.getContextId()));

    residency.updateCompletionData(lastFenceValue3, osContext2.getContextId());
    EXPECT_EQ(lastFenceValue3, residency.lastFenceValues[1]);
    EXPECT_EQ(lastFenceValue3, residency.getFenceValueForContextId(osContext2.getContextId()));
}

TEST(Heap32AllocationTests, givenDebugModeWhenMallocIsUsedToCreateAllocationWhenAllocationIsCreatedThenItDoesntRequireCpuPointerCleanup) {
    DebugManagerStateRestore restore;
    DebugManager.flags.UseMallocToObtainHeap32Base.set(true);
    ExecutionEnvironment executionEnvironment;
    executionEnvironment.incRefInternal();
    OsAgnosticMemoryManager memoryManager(true, true, executionEnvironment);
    auto internalBase = memoryManager.allocator32Bit->getBase();
    EXPECT_NE(0x40000000000ul, internalBase);
    EXPECT_NE(0x80000000000ul, internalBase);
    EXPECT_NE(0x0ul, internalBase);

    auto allocation = static_cast<MemoryAllocation *>(memoryManager.allocate32BitGraphicsMemory(4096u, nullptr, AllocationOrigin::EXTERNAL_ALLOCATION));
    EXPECT_EQ(nullptr, allocation->driverAllocatedCpuPointer);
    memoryManager.freeGraphicsMemory(allocation);
}

/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "test.h"

#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/host_pointer_manager_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_host_pointer_manager.h"

using ::testing::_;
using ::testing::Return;

namespace L0 {
namespace ult {

using HostPointerManagerTest = Test<HostPointerManagerFixure>;

TEST_F(HostPointerManagerTest,
       givenMultipleGraphicsAllocationWhenCopyingHostPointerDataThenCopyOnlyExistingAllocations) {
    HostPointerData originData(4);
    auto gfxAllocation = openHostPointerManager->createHostPointerAllocation(device->getRootDeviceIndex(),
                                                                             heapPointer,
                                                                             MemoryConstants::pageSize,
                                                                             device->getNEODevice()->getDeviceBitfield());
    originData.hostPtrAllocations.addAllocation(gfxAllocation);

    HostPointerData copyData(originData);
    for (auto allocation : copyData.hostPtrAllocations.getGraphicsAllocations()) {
        if (allocation != nullptr) {
            EXPECT_EQ(device->getRootDeviceIndex(), allocation->getRootDeviceIndex());
            EXPECT_EQ(gfxAllocation, allocation);
        }
    }
    hostDriverHandle->getMemoryManager()->freeGraphicsMemory(gfxAllocation);
}

TEST_F(HostPointerManagerTest, givenNoHeapManagerThenReturnFeatureUnsupported) {
    hostDriverHandle->hostPointerManager.reset(nullptr);

    void *testPtr = heapPointer;
    auto result = hostDriverHandle->importExternalPointer(testPtr, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);

    result = hostDriverHandle->getHostPointerBaseAddress(testPtr, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);

    result = hostDriverHandle->releaseImportedPointer(testPtr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);

    auto gfxAllocation = hostDriverHandle->getDriverSystemMemoryAllocation(testPtr, 1u, device->getRootDeviceIndex(), nullptr);
    EXPECT_EQ(nullptr, gfxAllocation);
}

TEST_F(HostPointerManagerTest, givenHostPointerImportedWhenGettingExistingAllocationThenRetrieveProperGpuAddress) {
    void *testPtr = heapPointer;

    auto result = hostDriverHandle->importExternalPointer(testPtr, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    uintptr_t gpuAddress = 0u;
    auto gfxAllocation = hostDriverHandle->getDriverSystemMemoryAllocation(testPtr,
                                                                           1u,
                                                                           device->getRootDeviceIndex(),
                                                                           &gpuAddress);
    ASSERT_NE(nullptr, gfxAllocation);
    EXPECT_EQ(testPtr, gfxAllocation->getUnderlyingBuffer());
    EXPECT_EQ(static_cast<uintptr_t>(gfxAllocation->getGpuAddress()), gpuAddress);

    size_t offset = 10u;
    testPtr = ptrOffset(testPtr, offset);
    gfxAllocation = hostDriverHandle->getDriverSystemMemoryAllocation(testPtr,
                                                                      1u,
                                                                      device->getRootDeviceIndex(),
                                                                      &gpuAddress);
    ASSERT_NE(nullptr, gfxAllocation);
    EXPECT_EQ(heapPointer, gfxAllocation->getUnderlyingBuffer());
    auto expectedGpuAddress = static_cast<uintptr_t>(gfxAllocation->getGpuAddress()) + offset;
    EXPECT_EQ(expectedGpuAddress, gpuAddress);

    result = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(HostPointerManagerTest, givenPointerRegisteredWhenSvmAllocationExistsThenRetrieveSvmFirst) {
    void *testPtr = heapPointer;

    size_t usmSize = MemoryConstants::pageSize;
    void *usmBuffer = hostDriverHandle->getMemoryManager()->allocateSystemMemory(usmSize, usmSize);
    NEO::GraphicsAllocation *usmAllocation = hostDriverHandle->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {device->getRootDeviceIndex(), false, usmSize, NEO::GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, false, neoDevice->getDeviceBitfield()},
        usmBuffer);
    ASSERT_NE(nullptr, usmAllocation);

    NEO::SvmAllocationData allocData(device->getRootDeviceIndex());
    allocData.gpuAllocations.addAllocation(usmAllocation);
    allocData.cpuAllocation = nullptr;
    allocData.size = usmSize;
    allocData.memoryType = InternalMemoryType::NOT_SPECIFIED;
    allocData.device = nullptr;
    hostDriverHandle->getSvmAllocsManager()->insertSVMAlloc(allocData);

    auto result = hostDriverHandle->importExternalPointer(testPtr, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto gfxAllocation = hostDriverHandle->getDriverSystemMemoryAllocation(usmBuffer, 1u, device->getRootDeviceIndex(), nullptr);
    ASSERT_NE(nullptr, gfxAllocation);
    EXPECT_EQ(usmBuffer, gfxAllocation->getUnderlyingBuffer());

    result = hostDriverHandle->releaseImportedPointer(testPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    hostDriverHandle->getMemoryManager()->freeGraphicsMemory(usmAllocation);
    hostDriverHandle->getMemoryManager()->freeSystemMemory(usmBuffer);
}

TEST_F(HostPointerManagerTest, givenSvmAllocationExistsWhenGettingExistingAllocationThenRetrieveProperGpuAddress) {
    size_t usmSize = MemoryConstants::pageSize;
    void *usmBuffer = hostDriverHandle->getMemoryManager()->allocateSystemMemory(usmSize, usmSize);
    NEO::GraphicsAllocation *usmAllocation = hostDriverHandle->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {device->getRootDeviceIndex(), false, usmSize, NEO::GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, false, neoDevice->getDeviceBitfield()},
        usmBuffer);
    ASSERT_NE(nullptr, usmAllocation);

    NEO::SvmAllocationData allocData(device->getRootDeviceIndex());
    allocData.gpuAllocations.addAllocation(usmAllocation);
    allocData.cpuAllocation = nullptr;
    allocData.size = usmSize;
    allocData.memoryType = InternalMemoryType::NOT_SPECIFIED;
    allocData.device = nullptr;
    hostDriverHandle->getSvmAllocsManager()->insertSVMAlloc(allocData);

    uintptr_t gpuAddress = 0u;
    auto gfxAllocation = hostDriverHandle->getDriverSystemMemoryAllocation(usmBuffer,
                                                                           1u,
                                                                           device->getRootDeviceIndex(),
                                                                           &gpuAddress);
    ASSERT_NE(nullptr, gfxAllocation);
    EXPECT_EQ(usmBuffer, gfxAllocation->getUnderlyingBuffer());
    EXPECT_EQ(static_cast<uintptr_t>(gfxAllocation->getGpuAddress()), gpuAddress);

    size_t offset = 10u;
    void *offsetUsmBuffer = ptrOffset(usmBuffer, offset);
    gfxAllocation = hostDriverHandle->getDriverSystemMemoryAllocation(offsetUsmBuffer,
                                                                      1u,
                                                                      device->getRootDeviceIndex(),
                                                                      &gpuAddress);
    ASSERT_NE(nullptr, gfxAllocation);
    EXPECT_EQ(usmBuffer, gfxAllocation->getUnderlyingBuffer());
    EXPECT_EQ(reinterpret_cast<uintptr_t>(offsetUsmBuffer), gpuAddress);

    hostDriverHandle->getMemoryManager()->freeGraphicsMemory(usmAllocation);
    hostDriverHandle->getMemoryManager()->freeSystemMemory(usmBuffer);
}

TEST_F(HostPointerManagerTest, WhenSizeIsZeroThenExpectInvalidArgument) {
    void *testPtr = heapPointer;

    auto result = hostDriverHandle->importExternalPointer(testPtr, 0u);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(HostPointerManagerTest, givenNoPointerWhenImportAddressThenRegisterNewHostData) {
    void *testPtr = heapPointer;
    void *baseAddress;

    auto result = hostDriverHandle->getHostPointerBaseAddress(testPtr, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    result = hostDriverHandle->releaseImportedPointer(testPtr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    result = hostDriverHandle->importExternalPointer(testPtr, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = hostDriverHandle->getHostPointerBaseAddress(testPtr, &baseAddress);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(heapPointer, baseAddress);

    result = hostDriverHandle->releaseImportedPointer(testPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = hostDriverHandle->getHostPointerBaseAddress(testPtr, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

TEST_F(HostPointerManagerTest, givenNoPointerWhenImportMisalignedAddressThenRegisterNewHostData) {
    void *testPtr = heapPointer;
    testPtr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(testPtr) + 0x10);
    size_t size = 0x10;

    void *baseAddress;

    auto result = hostDriverHandle->getHostPointerBaseAddress(testPtr, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    result = hostDriverHandle->releaseImportedPointer(testPtr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    result = hostDriverHandle->importExternalPointer(testPtr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = hostDriverHandle->getHostPointerBaseAddress(testPtr, &baseAddress);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(heapPointer, baseAddress);
    auto hostPointerData = openHostPointerManager->hostPointerAllocations.get(testPtr);
    ASSERT_NE(nullptr, hostPointerData);
    EXPECT_EQ(MemoryConstants::pageSize, hostPointerData->size);

    result = hostDriverHandle->releaseImportedPointer(testPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(HostPointerManagerTest, givenBiggerAddressImportedWhenImportingWithinThenReturnSuccess) {
    void *testPtr = heapPointer;

    auto result = hostDriverHandle->importExternalPointer(testPtr, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    testPtr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(testPtr) + 0x10);
    size_t size = 0x10;

    result = hostDriverHandle->importExternalPointer(testPtr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, openHostPointerManager->hostPointerAllocations.getNumAllocs());

    result = hostDriverHandle->getHostPointerBaseAddress(testPtr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = hostDriverHandle->releaseImportedPointer(testPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(HostPointerManagerTest, givenPointerRegisteredWhenUsingInvalidAddressThenReturnError) {
    void *testPtr = heapPointer;

    auto result = hostDriverHandle->importExternalPointer(testPtr, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = hostDriverHandle->getHostPointerBaseAddress(nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    result = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(HostPointerManagerTest, givenPointerRegisteredWhenSizeEndsInNoAllocationThenExpectObjectInUseError) {
    void *testPtr = heapPointer;

    auto result = hostDriverHandle->importExternalPointer(testPtr, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    testPtr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(testPtr) + 0x10);
    size_t size = MemoryConstants::pageSize + 0x10;

    result = hostDriverHandle->importExternalPointer(testPtr, size);
    EXPECT_EQ(ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE, result);

    result = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(HostPointerManagerTest, givenPointerRegisteredWhenSizeEndsInDifferentAllocationThenExpectOverlappingError) {
    void *testPtr = heapPointer;

    auto result = hostDriverHandle->importExternalPointer(testPtr, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    testPtr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(testPtr) + MemoryConstants::pageSize);
    size_t size = MemoryConstants::pageSize;

    result = hostDriverHandle->importExternalPointer(testPtr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *errorPtr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(heapPointer) + 0x10);
    result = hostDriverHandle->importExternalPointer(errorPtr, size);
    EXPECT_EQ(ZE_RESULT_ERROR_OVERLAPPING_REGIONS, result);

    result = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = hostDriverHandle->releaseImportedPointer(testPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(HostPointerManagerTest, givenPointerNotRegisteredWhenSizeEndsInDifferentAllocationThenExpectInvalidSizeError) {
    void *testPtr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(heapPointer) + MemoryConstants::pageSize);

    auto result = hostDriverHandle->importExternalPointer(testPtr, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *errorPtr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(heapPointer) + 0x10);
    size_t size = MemoryConstants::pageSize;
    result = hostDriverHandle->importExternalPointer(errorPtr, size);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, result);

    result = hostDriverHandle->releaseImportedPointer(testPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(HostPointerManagerTest, givenPointerUsesTwoPagesThenBothPagesAreAvailableAndSizeIsCorrect) {
    void *testPtr = heapPointer;
    testPtr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(testPtr) + 0x10);
    size_t size = MemoryConstants::pageSize;

    void *baseAddress;

    auto result = hostDriverHandle->importExternalPointer(testPtr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = hostDriverHandle->getHostPointerBaseAddress(testPtr, &baseAddress);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(heapPointer, baseAddress);
    auto hostPointerData = openHostPointerManager->hostPointerAllocations.get(testPtr);
    ASSERT_NE(nullptr, hostPointerData);
    EXPECT_EQ(2 * MemoryConstants::pageSize, hostPointerData->size);

    testPtr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(testPtr) + MemoryConstants::pageSize);
    size = 0x010;

    result = hostDriverHandle->importExternalPointer(testPtr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = hostDriverHandle->getHostPointerBaseAddress(testPtr, &baseAddress);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(heapPointer, baseAddress);

    result = hostDriverHandle->releaseImportedPointer(testPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(HostPointerManagerTest, givenPointerRegisteredWhenSizeFitsThenReturnGraphicsAllocation) {
    void *testPtr = heapPointer;

    auto result = hostDriverHandle->importExternalPointer(testPtr, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto gfxAllocation = hostDriverHandle->findHostPointerAllocation(testPtr, 0x10u, device->getRootDeviceIndex());
    EXPECT_NE(nullptr, gfxAllocation);
    EXPECT_EQ(testPtr, gfxAllocation->getUnderlyingBuffer());

    result = hostDriverHandle->releaseImportedPointer(testPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(HostPointerManagerTest, givenPointerNotRegisteredThenReturnNullptrGraphicsAllocation) {
    void *testPtr = heapPointer;

    auto result = hostDriverHandle->importExternalPointer(testPtr, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    testPtr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(testPtr) + MemoryConstants::pageSize);
    auto gfxAllocation = hostDriverHandle->findHostPointerAllocation(testPtr, 0x10u, device->getRootDeviceIndex());
    EXPECT_EQ(nullptr, gfxAllocation);

    result = hostDriverHandle->releaseImportedPointer(heapPointer);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(HostPointerManagerTest, givenPointerRegisteredWhenSizeExceedsAllocationThenReturnNullptrGraphicsAllocation) {
    void *testPtr = heapPointer;

    auto result = hostDriverHandle->importExternalPointer(testPtr, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto gfxAllocation = hostDriverHandle->findHostPointerAllocation(testPtr, MemoryConstants::pageSize + 0x10u, device->getRootDeviceIndex());
    EXPECT_EQ(nullptr, gfxAllocation);

    result = hostDriverHandle->releaseImportedPointer(testPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(HostPointerManagerTest, givenNoPointerRegisteredWhenAllocationCreationFailThenExpectOutOfMemoryError) {
    std::unique_ptr<MemoryManager> failMemoryManager = std::make_unique<NEO::FailMemoryManager>(0, *neoDevice->executionEnvironment);
    openHostPointerManager->memoryManager = failMemoryManager.get();

    auto result = hostDriverHandle->importExternalPointer(heapPointer, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, result);
}

TEST_F(HostPointerManagerTest, givenHostAllocationImportedWhenMakingResidentAddressThenAllocationMadeResident) {
    void *testPtr = heapPointer;

    EXPECT_CALL(*mockMemoryInterface, makeResident(_, _))
        .Times(1)
        .WillRepeatedly(::testing::Return(NEO::MemoryOperationsStatus::SUCCESS));
    EXPECT_CALL(*mockMemoryInterface, evict(_, _))
        .Times(1)
        .WillRepeatedly(::testing::Return(NEO::MemoryOperationsStatus::SUCCESS));

    auto result = context->makeMemoryResident(device, testPtr, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    result = hostDriverHandle->importExternalPointer(testPtr, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->makeMemoryResident(device, testPtr, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->evictMemory(device, testPtr, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = hostDriverHandle->releaseImportedPointer(testPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = context->evictMemory(device, testPtr, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
}

} // namespace ult
} // namespace L0

/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

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
    EXPECT_NE(nullptr, hostDriverHandle->hostPointerManager.get());

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

TEST_F(HostPointerManagerTest, givenHostPointerImportedWhenGettingExistingAllocationThenRetrieveProperGpuAddress) {
    EXPECT_NE(nullptr, hostDriverHandle->hostPointerManager.get());

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
    EXPECT_NE(nullptr, hostDriverHandle->hostPointerManager.get());

    void *testPtr = heapPointer;

    size_t usmSize = MemoryConstants::pageSize;
    void *usmBuffer = hostDriverHandle->getMemoryManager()->allocateSystemMemory(usmSize, usmSize);
    NEO::GraphicsAllocation *usmAllocation = hostDriverHandle->getMemoryManager()->allocateGraphicsMemoryWithProperties(
        {device->getRootDeviceIndex(), false, usmSize, NEO::AllocationType::BUFFER_HOST_MEMORY, false, neoDevice->getDeviceBitfield()},
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
        {device->getRootDeviceIndex(), false, usmSize, NEO::AllocationType::BUFFER_HOST_MEMORY, false, neoDevice->getDeviceBitfield()},
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

TEST_F(HostPointerManagerTest, WhenPointerIsNullThenExpectInvalidArgument) {
    auto result = hostDriverHandle->importExternalPointer(nullptr, 0x10);
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

    void *baseAddress = nullptr;

    auto result = hostDriverHandle->getHostPointerBaseAddress(testPtr, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    result = hostDriverHandle->releaseImportedPointer(testPtr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    result = hostDriverHandle->importExternalPointer(testPtr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = hostDriverHandle->getHostPointerBaseAddress(testPtr, &baseAddress);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testPtr, baseAddress);
    auto hostPointerData = openHostPointerManager->hostPointerAllocations.get(testPtr);
    ASSERT_NE(nullptr, hostPointerData);
    EXPECT_EQ(size, hostPointerData->size);

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

    void *baseAddress = nullptr;

    auto result = hostDriverHandle->importExternalPointer(testPtr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = hostDriverHandle->getHostPointerBaseAddress(testPtr, &baseAddress);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testPtr, baseAddress);
    auto hostPointerData = openHostPointerManager->hostPointerAllocations.get(testPtr);
    ASSERT_NE(nullptr, hostPointerData);
    EXPECT_EQ(size, hostPointerData->size);

    void *testPtr2 = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(testPtr) + MemoryConstants::pageSize);
    size = 0x010;

    result = hostDriverHandle->importExternalPointer(testPtr2, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = hostDriverHandle->getHostPointerBaseAddress(testPtr2, &baseAddress);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testPtr2, baseAddress);
    auto hostPointerData2 = openHostPointerManager->hostPointerAllocations.get(testPtr2);
    ASSERT_NE(nullptr, hostPointerData2);
    EXPECT_EQ(size, hostPointerData2->size);

    EXPECT_EQ(hostPointerData->hostPtrAllocations.getGraphicsAllocations().size(), hostPointerData2->hostPtrAllocations.getGraphicsAllocations().size());
    for (uint32_t i = 0; i < hostPointerData->hostPtrAllocations.getGraphicsAllocations().size(); i++) {
        auto hostPointerAllocation = hostPointerData->hostPtrAllocations.getGraphicsAllocation(i);
        auto hostPointerAllocation2 = hostPointerData2->hostPtrAllocations.getGraphicsAllocation(i);
        EXPECT_NE(hostPointerAllocation, hostPointerAllocation2);
    }

    result = hostDriverHandle->releaseImportedPointer(testPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = hostDriverHandle->releaseImportedPointer(testPtr2);
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

    mockMemoryInterface->makeResidentResult = NEO::MemoryOperationsStatus::SUCCESS;
    mockMemoryInterface->evictResult = NEO::MemoryOperationsStatus::SUCCESS;

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

    EXPECT_EQ(1u, mockMemoryInterface->makeResidentCalled);
    EXPECT_EQ(1u, mockMemoryInterface->evictCalled);
}

TEST_F(HostPointerManagerTest, givenMisalignedPointerRegisteredWhenGettingRelativeOffsetAddressThenRetrieveMisalignedPointerAsBaseAddress) {
    size_t mainOffset = 0x10;
    void *testPtr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(heapPointer) + mainOffset);
    size_t size = MemoryConstants::pageSize + 0x10;

    auto result = hostDriverHandle->importExternalPointer(testPtr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    size_t relativeOffset = 0x20;
    void *relativeAddress = ptrOffset(testPtr, relativeOffset);
    void *baseAddress = nullptr;
    result = hostDriverHandle->getHostPointerBaseAddress(relativeAddress, &baseAddress);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testPtr, baseAddress);

    auto gfxAllocation = hostDriverHandle->findHostPointerAllocation(testPtr, 0x10u, device->getRootDeviceIndex());
    ASSERT_NE(nullptr, gfxAllocation);
    size_t gpuVA = static_cast<size_t>(gfxAllocation->getGpuAddress());
    size_t gpuAddressOffset = gpuVA - alignDown(gpuVA, MemoryConstants::pageSize);
    EXPECT_EQ(mainOffset, gpuAddressOffset);

    result = hostDriverHandle->releaseImportedPointer(testPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

using ForceDisabledHostPointerManagerTest = Test<ForceDisabledHostPointerManagerFixure>;

TEST_F(ForceDisabledHostPointerManagerTest, givenHostPointerManagerForceDisabledThenReturnFeatureUnsupported) {
    EXPECT_EQ(nullptr, hostDriverHandle->hostPointerManager.get());

    void *testPtr = heapPointer;
    auto result = hostDriverHandle->importExternalPointer(testPtr, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);

    void *basePtr = nullptr;
    result = hostDriverHandle->getHostPointerBaseAddress(testPtr, &basePtr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);

    uintptr_t gpuAddress = 0;
    auto gfxAllocation = hostDriverHandle->getDriverSystemMemoryAllocation(testPtr, 1u, device->getRootDeviceIndex(), &gpuAddress);
    EXPECT_EQ(nullptr, gfxAllocation);

    result = hostDriverHandle->releaseImportedPointer(testPtr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

using ForceEnabledHostPointerManagerTest = Test<ForceEnabledHostPointerManagerFixure>;

TEST_F(ForceEnabledHostPointerManagerTest, givenHostPointerManagerForceEnabledThenReturnSuccess) {
    EXPECT_NE(nullptr, hostDriverHandle->hostPointerManager.get());

    void *testPtr = heapPointer;
    auto result = hostDriverHandle->importExternalPointer(testPtr, MemoryConstants::pageSize);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    void *basePtr = nullptr;
    result = hostDriverHandle->getHostPointerBaseAddress(testPtr, &basePtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(testPtr, basePtr);

    uintptr_t gpuAddress = 0;
    auto gfxAllocation = hostDriverHandle->getDriverSystemMemoryAllocation(testPtr, 1u, device->getRootDeviceIndex(), &gpuAddress);
    EXPECT_NE(nullptr, gfxAllocation);

    result = hostDriverHandle->releaseImportedPointer(testPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

} // namespace ult
} // namespace L0

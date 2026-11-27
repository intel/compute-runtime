/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/memory_manager/multi_graphics_allocation.h"
#include "shared/source/os_interface/windows/wddm_allocation.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_gmm_client_context_base.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/windows/mock_shared_section.h"
#include "shared/test/common/os_interface/windows/mock_sys_calls.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

namespace {
NTSTATUS APIENTRY failingQueryResourceInfo(D3DKMT_QUERYRESOURCEINFO *queryResourceInfo) {
    return STATUS_INVALID_PARAMETER;
}

NTSTATUS APIENTRY emptyQueryResourceInfo(D3DKMT_QUERYRESOURCEINFO *queryResourceInfo) {
    queryResourceInfo->NumAllocations = 0;
    return STATUS_SUCCESS;
}

uint32_t queryResourceInfoFromNtHandleCalled = 0u;
NTSTATUS APIENTRY emptyQueryResourceInfoFromNtHandle(D3DKMT_QUERYRESOURCEINFOFROMNTHANDLE *queryResourceInfo) {
    queryResourceInfoFromNtHandleCalled++;
    queryResourceInfo->NumAllocations = 0;
    return STATUS_SUCCESS;
}
} // namespace

struct WddmMemoryManagerSectionTest : public WddmFixture {
    void SetUp() override {
        WddmFixture::SetUp();
        wddm->openNTHandleFromNameCallBase = false;
    }

    VariableBackup<size_t> ntQueryObjectCalledBackup{&SysCalls::ntQueryObjectCalled, 0u};
    VariableBackup<const wchar_t *> ntQueryObjectTypeNameBackup{&SysCalls::ntQueryObjectTypeName, nullptr};
    VariableBackup<HANDLE> openFileMappingResultBackup{&SysCalls::openFileMappingResult, nullptr};
    VariableBackup<DWORD> openFileMappingLastDesiredAccessBackup{&SysCalls::openFileMappingLastDesiredAccess, 0u};
    VariableBackup<LPCWSTR> openFileMappingLastNameBackup{&SysCalls::openFileMappingLastName, nullptr};
};

TEST_F(WddmMemoryManagerSectionTest, givenNamedSectionWhenCreatingAllocationFromSharedNameThenSectionIsOpenedForReadAndWriteAndAllocationIsCreatedFromItAndSectionIsClosed) {
    SysCalls::openFileMappingResult = reinterpret_cast<HANDLE>(0x4321);
    MockGraphicsAllocation sectionAllocation;
    memoryManager->createGraphicsAllocationFromSectionHandleCallBase = false;
    memoryManager->createGraphicsAllocationFromSectionHandleResult = &sectionAllocation;
    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::bufferHostMemory, mockDeviceBitfield);
    auto status = MemoryManager::AllocationStatus::Error;
    auto closeHandleCalledBefore = SysCalls::closeHandleCalled;

    EXPECT_EQ(&sectionAllocation, memoryManager->createGraphicsAllocationFromSharedName(L"myMemory", ExternalMemoryType::opaqueWin32, MemoryManager::OsHandleData{0u}, properties, false, status));
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_STREQ(L"myMemory", SysCalls::openFileMappingLastName);
    EXPECT_EQ(static_cast<DWORD>(FILE_MAP_READ | FILE_MAP_WRITE), SysCalls::openFileMappingLastDesiredAccess);
    EXPECT_EQ(1u, memoryManager->createGraphicsAllocationFromSectionHandleCalled);
    EXPECT_EQ(closeHandleCalledBefore + 1, SysCalls::closeHandleCalled);
}

TEST_F(WddmMemoryManagerSectionTest, givenCreatingAllocationFromSectionHandleFailsWhenCreatingAllocationFromSharedNameThenErrorIsReturnedAndSectionIsClosed) {
    SysCalls::openFileMappingResult = reinterpret_cast<HANDLE>(0x4321);
    memoryManager->createGraphicsAllocationFromSectionHandleCallBase = false;
    memoryManager->createGraphicsAllocationFromSectionHandleResult = nullptr;
    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::bufferHostMemory, mockDeviceBitfield);
    auto status = MemoryManager::AllocationStatus::Success;
    auto closeHandleCalledBefore = SysCalls::closeHandleCalled;

    EXPECT_EQ(nullptr, memoryManager->createGraphicsAllocationFromSharedName(L"myMemory", ExternalMemoryType::opaqueWin32, MemoryManager::OsHandleData{0u}, properties, false, status));
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);
    EXPECT_EQ(1u, memoryManager->createGraphicsAllocationFromSectionHandleCalled);
    EXPECT_EQ(closeHandleCalledBefore + 1, SysCalls::closeHandleCalled);
}

TEST_F(WddmMemoryManagerSectionTest, givenD3DExternalMemoryTypeWhenNameIsNotSharedResourceThenSectionIsNotOpened) {
    SysCalls::openFileMappingResult = reinterpret_cast<HANDLE>(0x4321);
    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::bufferHostMemory, mockDeviceBitfield);
    auto status = MemoryManager::AllocationStatus::Success;

    EXPECT_EQ(nullptr, memoryManager->createGraphicsAllocationFromSharedName(L"myMemory", ExternalMemoryType::d3d12Resource, MemoryManager::OsHandleData{0u}, properties, false, status));
    EXPECT_EQ(MemoryManager::AllocationStatus::InvalidName, status);
    EXPECT_EQ(nullptr, SysCalls::openFileMappingLastName);
    EXPECT_EQ(0u, memoryManager->createGraphicsAllocationFromSectionHandleCalled);
}

TEST_F(WddmMemoryManagerSectionTest, givenNameIsNeitherSharedResourceNorSectionWhenCreatingAllocationFromSharedNameThenInvalidNameIsReturned) {
    SysCalls::openFileMappingResult = nullptr;
    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::bufferHostMemory, mockDeviceBitfield);
    auto status = MemoryManager::AllocationStatus::Success;

    EXPECT_EQ(nullptr, memoryManager->createGraphicsAllocationFromSharedName(L"myMemory", ExternalMemoryType::opaqueWin32, MemoryManager::OsHandleData{0u}, properties, false, status));
    EXPECT_EQ(MemoryManager::AllocationStatus::InvalidName, status);
    EXPECT_STREQ(L"myMemory", SysCalls::openFileMappingLastName);
    EXPECT_EQ(0u, memoryManager->createGraphicsAllocationFromSectionHandleCalled);
}

TEST_F(WddmMemoryManagerSectionTest, givenSectionHandleWhenCreatingAllocationFromSharedHandleThenAllocationIsCreatedFromSectionAndHandleIsNotClosed) {
    gdi->queryResourceInfo = reinterpret_cast<PFND3DKMT_QUERYRESOURCEINFO>(failingQueryResourceInfo);
    SysCalls::ntQueryObjectTypeName = L"Section";
    MockGraphicsAllocation sectionAllocation;
    memoryManager->createGraphicsAllocationFromSectionHandleCallBase = false;
    memoryManager->createGraphicsAllocationFromSectionHandleResult = &sectionAllocation;
    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, mockDeviceBitfield);
    auto closeHandleCalledBefore = SysCalls::closeHandleCalled;

    EXPECT_EQ(&sectionAllocation, memoryManager->createGraphicsAllocationFromSharedHandle(MemoryManager::OsHandleData{0x4321u}, properties, false, false, false, nullptr));
    EXPECT_EQ(1u, SysCalls::ntQueryObjectCalled);
    EXPECT_EQ(1u, memoryManager->createGraphicsAllocationFromSectionHandleCalled);
    EXPECT_EQ(closeHandleCalledBefore, SysCalls::closeHandleCalled);
}

TEST_F(WddmMemoryManagerSectionTest, givenSharedHandleWhenCreatingAllocationFromSharedHandleThenSectionIsNotQueried) {
    gdi->queryResourceInfo = reinterpret_cast<PFND3DKMT_QUERYRESOURCEINFO>(emptyQueryResourceInfo);
    SysCalls::ntQueryObjectTypeName = L"Section";
    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, mockDeviceBitfield);

    EXPECT_EQ(nullptr, memoryManager->createGraphicsAllocationFromSharedHandle(MemoryManager::OsHandleData{1u}, properties, false, false, false, nullptr));
    EXPECT_EQ(0u, SysCalls::ntQueryObjectCalled);
    EXPECT_EQ(0u, memoryManager->createGraphicsAllocationFromSectionHandleCalled);
}

TEST_F(WddmMemoryManagerSectionTest, givenNTHandleWhichIsNotSectionWhenCreatingAllocationFromSharedHandleThenItIsOpenedAsNTHandle) {
    gdi->queryResourceInfo = reinterpret_cast<PFND3DKMT_QUERYRESOURCEINFO>(failingQueryResourceInfo);
    gdi->queryResourceInfoFromNtHandle = reinterpret_cast<PFND3DKMT_QUERYRESOURCEINFOFROMNTHANDLE>(emptyQueryResourceInfoFromNtHandle);
    VariableBackup<uint32_t> queryResourceInfoFromNtHandleCalledBackup(&queryResourceInfoFromNtHandleCalled, 0u);
    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, mockDeviceBitfield);

    EXPECT_EQ(nullptr, memoryManager->createGraphicsAllocationFromSharedHandle(MemoryManager::OsHandleData{0x4321u}, properties, false, false, false, nullptr));
    EXPECT_EQ(1u, SysCalls::ntQueryObjectCalled);
    EXPECT_EQ(1u, queryResourceInfoFromNtHandleCalled);
    EXPECT_EQ(0u, memoryManager->createGraphicsAllocationFromSectionHandleCalled);
}

TEST_F(WddmMemoryManagerSectionTest, givenNTHandleFromParentProcessWhenCreatingAllocationFromSharedHandleThenSectionIsNotQueried) {
    gdi->queryResourceInfo = reinterpret_cast<PFND3DKMT_QUERYRESOURCEINFO>(failingQueryResourceInfo);
    gdi->queryResourceInfoFromNtHandle = reinterpret_cast<PFND3DKMT_QUERYRESOURCEINFOFROMNTHANDLE>(emptyQueryResourceInfoFromNtHandle);
    VariableBackup<uint32_t> queryResourceInfoFromNtHandleCalledBackup(&queryResourceInfoFromNtHandleCalled, 0u);
    SysCalls::ntQueryObjectTypeName = L"Section";
    MemoryManager::OsHandleData osHandleData{0x4321u};
    osHandleData.parentProcessId = 1234u;
    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, mockDeviceBitfield);

    EXPECT_EQ(nullptr, memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, false, nullptr));
    EXPECT_EQ(0u, SysCalls::ntQueryObjectCalled);
    EXPECT_EQ(1u, queryResourceInfoFromNtHandleCalled);
    EXPECT_EQ(0u, memoryManager->createGraphicsAllocationFromSectionHandleCalled);
}

struct WddmMemoryManagerAllocationFromSectionTest : public WddmFixture {
    void SetUp() override {
        WddmFixture::SetUp();
        view = alignedMalloc(viewSize, MemoryConstants::pageSize64k);
        memset(view, 0, viewSize);
        SysCalls::mapViewOfFileResults[0] = view;
        SysCalls::virtualQueryMemoryBasicInformation.RegionSize = viewSize;
    }

    void TearDown() override {
        alignedFree(view);
        WddmFixture::TearDown();
    }

    static constexpr size_t viewSize = 2 * MemoryConstants::pageSize;
    const HANDLE sectionHandle = reinterpret_cast<HANDLE>(0x4321);
    void *view = nullptr;
    VariableBackup<size_t> mapViewOfFileCalledBackup{&SysCalls::mapViewOfFileCalled, 0u};
    VariableBackup<LPVOID> mapViewOfFileResultBackup{&SysCalls::mapViewOfFileResults[0]};
    VariableBackup<DWORD> mapViewOfFileLastDesiredAccessBackup{&SysCalls::mapViewOfFileLastDesiredAccess, 0u};
    VariableBackup<SIZE_T> mapViewOfFileLastNumberOfBytesToMapBackup{&SysCalls::mapViewOfFileLastNumberOfBytesToMap, 0u};
    VariableBackup<size_t> unmapViewOfFileCalledBackup{&SysCalls::unmapViewOfFileCalled, 0u};
    VariableBackup<LPCVOID> unmapViewOfFileLastAddressBackup{&SysCalls::unmapViewOfFileLastAddress, nullptr};
    VariableBackup<MEMORY_BASIC_INFORMATION> virtualQueryMemoryBasicInformationBackup{&SysCalls::virtualQueryMemoryBasicInformation};
};

TEST_F(WddmMemoryManagerAllocationFromSectionTest, givenUnsupportedAllocationTypeWhenCreatingAllocationFromSectionHandleThenSectionIsNotMapped) {
    for (auto allocationType : {AllocationType::image, AllocationType::sharedBuffer}) {
        AllocationProperties properties(0, MemoryConstants::pageSize, allocationType, mockDeviceBitfield);
        EXPECT_EQ(nullptr, memoryManager->createGraphicsAllocationFromSectionHandle(sectionHandle, properties));
    }
    EXPECT_EQ(0u, SysCalls::mapViewOfFileCalled);
}

TEST_F(WddmMemoryManagerAllocationFromSectionTest, givenMappingSectionViewFailsWhenCreatingAllocationFromSectionHandleThenNullptrIsReturned) {
    SysCalls::mapViewOfFileResults[0] = nullptr;
    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::bufferHostMemory, mockDeviceBitfield);

    EXPECT_EQ(nullptr, memoryManager->createGraphicsAllocationFromSectionHandle(sectionHandle, properties));
    EXPECT_EQ(1u, SysCalls::mapViewOfFileCalled);
    EXPECT_EQ(0u, SysCalls::unmapViewOfFileCalled);
}

TEST_F(WddmMemoryManagerAllocationFromSectionTest, givenInvalidMetadataWhenCreatingAllocationFromSectionHandleThenViewIsUnmappedAndNullptrIsReturned) {
    GMM_RESCREATE_PARAMS exported = {};
    exported.ExistingSysMemSize = 0u;
    writeSharedSectionTail(view, viewSize, exported, 0u);
    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::bufferHostMemory, mockDeviceBitfield);

    EXPECT_EQ(nullptr, memoryManager->createGraphicsAllocationFromSectionHandle(sectionHandle, properties));
    EXPECT_EQ(1u, SysCalls::unmapViewOfFileCalled);
    EXPECT_EQ(view, SysCalls::unmapViewOfFileLastAddress);
}

TEST_F(WddmMemoryManagerAllocationFromSectionTest, givenBufferWhenCreatingAllocationFromSectionHandleThenWholeSectionIsMappedForReadAndWriteAndAllocationIsCreatedOverViewWhichIsUnmappedOnFree) {
    GMM_RESCREATE_PARAMS exported = {};
    exported.ExistingSysMemSize = MemoryConstants::pageSize;
    writeSharedSectionTail(view, viewSize, exported, 0u);

    for (auto allocationType : {AllocationType::buffer, AllocationType::bufferHostMemory}) {
        AllocationProperties properties(0, MemoryConstants::pageSize, allocationType, mockDeviceBitfield);
        SysCalls::mapViewOfFileCalled = 0u;
        SysCalls::unmapViewOfFileCalled = 0u;

        auto allocation = memoryManager->createGraphicsAllocationFromSectionHandle(sectionHandle, properties);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(static_cast<DWORD>(FILE_MAP_READ | FILE_MAP_WRITE), SysCalls::mapViewOfFileLastDesiredAccess);
        EXPECT_EQ(0u, SysCalls::mapViewOfFileLastNumberOfBytesToMap);
        EXPECT_EQ(view, allocation->getUnderlyingBuffer());
        EXPECT_EQ(view, static_cast<WddmAllocation *>(allocation)->getSectionView());
        EXPECT_EQ(0u, SysCalls::unmapViewOfFileCalled);

        memoryManager->freeGraphicsMemory(allocation);
        EXPECT_EQ(1u, SysCalls::unmapViewOfFileCalled);
        EXPECT_EQ(view, SysCalls::unmapViewOfFileLastAddress);
    }
}

TEST_F(WddmMemoryManagerAllocationFromSectionTest, givenImageWhenCreatingAllocationFromSectionHandleThenImageAllocationIsCreatedOverViewWhichIsUnmappedOnFree) {
    GMM_RESCREATE_PARAMS exported = {};
    exported.Type = RESOURCE_2D;
    exported.Flags.Info.Linear = 1;
    exported.ExistingSysMemSize = MemoryConstants::pageSize;
    writeSharedSectionTail(view, viewSize, exported, 0u);
    AllocationProperties properties(0, true, nullptr, AllocationType::sharedImage, mockDeviceBitfield);

    auto allocation = memoryManager->createGraphicsAllocationFromSectionHandle(sectionHandle, properties);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(AllocationType::sharedImage, allocation->getAllocationType());
    EXPECT_EQ(view, allocation->getUnderlyingBuffer());
    EXPECT_EQ(view, static_cast<WddmAllocation *>(allocation)->getSectionView());

    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(1u, SysCalls::unmapViewOfFileCalled);
    EXPECT_EQ(view, SysCalls::unmapViewOfFileLastAddress);
}

TEST_F(WddmMemoryManagerAllocationFromSectionTest, givenCreatingAllocationFailsWhenCreatingAllocationFromSectionHandleThenViewIsUnmappedAndNullptrIsReturned) {
    GMM_RESCREATE_PARAMS exported = {};
    exported.ExistingSysMemSize = MemoryConstants::pageSize;
    writeSharedSectionTail(view, viewSize, exported, 0u);
    AllocationProperties properties(0, MemoryConstants::pageSize + 1, AllocationType::bufferHostMemory, mockDeviceBitfield);

    EXPECT_EQ(nullptr, memoryManager->createGraphicsAllocationFromSectionHandle(sectionHandle, properties));
    EXPECT_EQ(1u, SysCalls::unmapViewOfFileCalled);
    EXPECT_EQ(view, SysCalls::unmapViewOfFileLastAddress);
}

struct WddmMemoryManagerPhysicalHostMemoryTest : public ::testing::Test {
    void SetUp() override {
        auto osEnvironment = new OsEnvironmentWin();
        osEnvironment->gdi.reset(new MockGdi());
        executionEnvironment.osEnvironment.reset(osEnvironment);
        executionEnvironment.prepareRootDeviceEnvironments(numRootDevices);
        for (auto &rootDeviceEnvironment : executionEnvironment.rootDeviceEnvironments) {
            rootDeviceEnvironment->setHwInfoAndInitHelpers(defaultHwInfo.get());
            rootDeviceEnvironment->initGmm();
            auto rootDeviceWddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *rootDeviceEnvironment));
            rootDeviceWddm->init();
            rootDeviceEnvironment->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(rootDeviceWddm);
        }
        executionEnvironment.initializeMemoryManager();
        memoryManager = std::make_unique<MockWddmMemoryManager>(executionEnvironment);
        wddm = static_cast<WddmMock *>(executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Wddm>());
        auto svmGpuAddress = memoryManager->getGfxPartition(0u)->getHeapMinimalAddress(HeapIndex::heapSvm) + MemoryConstants::pageSize2M;
        svmGpuRange = executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper()->canonize(svmGpuAddress);
        rootDeviceIndices.pushUnique(0u);
    }

    UniqueGraphicsAllocation allocatePhysicalHostMemory(MemoryManager::AllocationStatus &status) {
        AllocationData allocationData;
        allocationData.allFlags = 0;
        allocationData.size = physicalSize;
        allocationData.flags.allocateMemory = true;
        allocationData.flags.useSystemMemory = true;
        allocationData.flags.isUSMHostAllocation = true;
        return makeUniqueGraphicsAllocation(this->memoryManager.get(), this->memoryManager->allocatePhysicalHostMemory(allocationData, status));
    }

    static constexpr uint32_t numRootDevices = 2u;
    static constexpr size_t physicalSize = 2 * MemoryConstants::pageSize64k;
    static constexpr size_t mappingSize = MemoryConstants::pageSize64k;
    static constexpr size_t mappingOffset = MemoryConstants::pageSize64k;
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<MockWddmMemoryManager> memoryManager;
    WddmMock *wddm = nullptr;
    uint64_t svmGpuRange = 0u;
    RootDeviceIndicesContainer rootDeviceIndices;
    VariableBackup<size_t> closeHandleCalledBackup{&SysCalls::closeHandleCalled, 0u};
    VariableBackup<size_t> unmapViewOfFileCalledBackup{&SysCalls::unmapViewOfFileCalled, 0u};
    VariableBackup<LPCVOID> unmapViewOfFileLastAddressBackup{&SysCalls::unmapViewOfFileLastAddress, nullptr};
    VariableBackup<size_t> createFileMapping2CalledBackup{&SysCalls::createFileMapping2Called, 0u};
    VariableBackup<HANDLE> createFileMapping2ResultBackup{&SysCalls::createFileMapping2Result};
    VariableBackup<SysCalls::CreateFileMapping2Call> createFileMapping2LastCallBackup{&SysCalls::createFileMapping2LastCall, {}};
    VariableBackup<size_t> mapViewOfFile3CalledBackup{&SysCalls::mapViewOfFile3Called, 0u};
    VariableBackup<bool> mapViewOfFile3FailsBackup{&SysCalls::mapViewOfFile3Fails, false};
    VariableBackup<SysCalls::MapViewOfFile3Call> mapViewOfFile3LastCallBackup{&SysCalls::mapViewOfFile3LastCall, {}};
    VariableBackup<size_t> unmapViewOfFile2CalledBackup{&SysCalls::unmapViewOfFile2Called, 0u};
    VariableBackup<SysCalls::UnmapViewOfFile2Call> unmapViewOfFile2LastCallBackup{&SysCalls::unmapViewOfFile2LastCall, {}};
    VariableBackup<size_t> virtualAlloc2CalledBackup{&SysCalls::virtualAlloc2Called, 0u};
    VariableBackup<SysCalls::VirtualAlloc2Call> virtualAlloc2LastCallBackup{&SysCalls::virtualAlloc2LastCall, {}};
    VariableBackup<size_t> virtualFreeCalledBackup{&SysCalls::virtualFreeCalled, 0u};
    VariableBackup<MEMORY_BASIC_INFORMATION> virtualQueryMemoryBasicInformationBackup{&SysCalls::virtualQueryMemoryBasicInformation, {}};
};

TEST_F(WddmMemoryManagerPhysicalHostMemoryTest, givenRequiredStartAddressWhenReservingAndFreeingCpuAddressThenPlaceholderIsReservedAndReleasedAfterCoalescing) {
    const uint64_t requiredStartAddress = MemoryConstants::gigaByte;
    const size_t size = 4 * MemoryConstants::pageSize64k;

    auto addressRange = memoryManager->reserveCpuAddress(requiredStartAddress, size);
    EXPECT_EQ(requiredStartAddress, addressRange.address);
    EXPECT_EQ(size, addressRange.size);
    EXPECT_EQ(1u, SysCalls::virtualAlloc2Called);
    EXPECT_EQ(addrToPtr(requiredStartAddress), SysCalls::virtualAlloc2LastCall.baseAddress);
    EXPECT_EQ(size, SysCalls::virtualAlloc2LastCall.size);
    EXPECT_EQ(static_cast<ULONG>(MEM_RESERVE | MEM_RESERVE_PLACEHOLDER), SysCalls::virtualAlloc2LastCall.allocationType);
    EXPECT_EQ(static_cast<ULONG>(PAGE_NOACCESS), SysCalls::virtualAlloc2LastCall.pageProtection);

    memoryManager->freeCpuAddress(addressRange);
    ASSERT_EQ(2u, SysCalls::virtualFreeCalled);
    EXPECT_EQ(addrToPtr(requiredStartAddress), SysCalls::virtualFreeCalls[0].address);
    EXPECT_EQ(size, SysCalls::virtualFreeCalls[0].size);
    EXPECT_EQ(static_cast<DWORD>(MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS), SysCalls::virtualFreeCalls[0].freeType);
    EXPECT_EQ(addrToPtr(requiredStartAddress), SysCalls::virtualFreeCalls[1].address);
    EXPECT_EQ(0u, SysCalls::virtualFreeCalls[1].size);
    EXPECT_EQ(static_cast<DWORD>(MEM_RELEASE), SysCalls::virtualFreeCalls[1].freeType);
}

TEST_F(WddmMemoryManagerPhysicalHostMemoryTest, givenPhysicalHostMemoryWhenAllocatingThenKmdAllocationIsCreatedOverPagefileSectionViewWhichIsReleasedOnFree) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    auto physicalAllocation = allocatePhysicalHostMemory(status);
    ASSERT_NE(nullptr, physicalAllocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    auto wddmAllocation = static_cast<WddmAllocation *>(physicalAllocation.get());
    EXPECT_EQ(1u, SysCalls::createFileMapping2Called);
    EXPECT_EQ(static_cast<ULONG>(PAGE_READWRITE), SysCalls::createFileMapping2LastCall.pageProtection);
    EXPECT_EQ(static_cast<ULONG>(SEC_COMMIT), SysCalls::createFileMapping2LastCall.allocationAttributes);
    EXPECT_EQ(physicalSize, SysCalls::createFileMapping2LastCall.maximumSize);
    EXPECT_EQ(1u, SysCalls::mapViewOfFile3Called);
    EXPECT_EQ(nullptr, SysCalls::mapViewOfFile3LastCall.baseAddress);
    EXPECT_EQ(physicalSize, SysCalls::mapViewOfFile3LastCall.viewSize);
    EXPECT_EQ(SysCalls::createFileMapping2Result, wddmAllocation->getSectionHandle());
    EXPECT_EQ(SysCalls::mapViewOfFile3DefaultView, wddmAllocation->getSectionView());
    EXPECT_EQ(SysCalls::mapViewOfFile3DefaultView, wddmAllocation->getUnderlyingBuffer());
    EXPECT_EQ(1u, wddm->createAllocationResult.called);
    EXPECT_EQ(0u, wddm->lockResult.called);

    physicalAllocation.reset();
    EXPECT_EQ(1u, SysCalls::unmapViewOfFileCalled);
    EXPECT_EQ(SysCalls::mapViewOfFile3DefaultView, SysCalls::unmapViewOfFileLastAddress);
    EXPECT_EQ(1u, SysCalls::closeHandleCalled);
}

TEST_F(WddmMemoryManagerPhysicalHostMemoryTest, givenSectionCreationFailsWhenAllocatingPhysicalHostMemoryThenNullptrIsReturned) {
    SysCalls::createFileMapping2Result = nullptr;
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;

    EXPECT_EQ(nullptr, allocatePhysicalHostMemory(status));
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);
    EXPECT_EQ(0u, SysCalls::mapViewOfFile3Called);
    EXPECT_EQ(0u, wddm->createAllocationResult.called);
}

TEST_F(WddmMemoryManagerPhysicalHostMemoryTest, givenSectionViewMappingFailsWhenAllocatingPhysicalHostMemoryThenSectionIsClosedAndNullptrIsReturned) {
    SysCalls::mapViewOfFile3Fails = true;
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;

    EXPECT_EQ(nullptr, allocatePhysicalHostMemory(status));
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);
    EXPECT_EQ(0u, wddm->createAllocationResult.called);
    EXPECT_EQ(0u, SysCalls::unmapViewOfFileCalled);
    EXPECT_EQ(1u, SysCalls::closeHandleCalled);
}

TEST_F(WddmMemoryManagerPhysicalHostMemoryTest, givenKmdAllocationFailsWhenAllocatingPhysicalHostMemoryThenViewIsUnmappedAndSectionIsClosedAndNullptrIsReturned) {
    wddm->failCreateAllocation = true;
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;

    EXPECT_EQ(nullptr, allocatePhysicalHostMemory(status));
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);
    EXPECT_EQ(1u, SysCalls::unmapViewOfFileCalled);
    EXPECT_EQ(SysCalls::mapViewOfFile3DefaultView, SysCalls::unmapViewOfFileLastAddress);
    EXPECT_EQ(1u, SysCalls::closeHandleCalled);
}

TEST_F(WddmMemoryManagerPhysicalHostMemoryTest, givenSvmReservationOnTwoRootDevicesWhenMappingPhysicalHostMemoryThenFirstMappingOwnsSectionViewWhichReplacesPlaceholderAndIsUnmappedPreservingPlaceholder) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    auto physicalAllocation = allocatePhysicalHostMemory(status);
    ASSERT_NE(nullptr, physicalAllocation);
    rootDeviceIndices.pushUnique(1u);
    MultiGraphicsAllocation multiGraphicsAllocation{numRootDevices - 1};

    EXPECT_TRUE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation.get(), svmGpuRange, mappingSize, mappingOffset, true));
    EXPECT_EQ(2u, SysCalls::virtualFreeCalled);
    EXPECT_EQ(addrToPtr(svmGpuRange), SysCalls::virtualFreeCalls[1].address);
    EXPECT_EQ(mappingSize, SysCalls::virtualFreeCalls[1].size);
    EXPECT_EQ(static_cast<DWORD>(MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER), SysCalls::virtualFreeCalls[1].freeType);
    EXPECT_EQ(2u, SysCalls::mapViewOfFile3Called);
    EXPECT_EQ(SysCalls::createFileMapping2Result, SysCalls::mapViewOfFile3LastCall.fileMapping);
    EXPECT_EQ(addrToPtr(svmGpuRange), SysCalls::mapViewOfFile3LastCall.baseAddress);
    EXPECT_EQ(mappingOffset, SysCalls::mapViewOfFile3LastCall.offset);
    EXPECT_EQ(mappingSize, SysCalls::mapViewOfFile3LastCall.viewSize);
    EXPECT_EQ(static_cast<ULONG>(MEM_REPLACE_PLACEHOLDER), SysCalls::mapViewOfFile3LastCall.allocationType);
    auto firstMapping = static_cast<WddmAllocation *>(multiGraphicsAllocation.getGraphicsAllocation(0u));
    auto secondMapping = static_cast<WddmAllocation *>(multiGraphicsAllocation.getGraphicsAllocation(1u));
    EXPECT_TRUE(firstMapping && firstMapping->isPlaceholderViewMapped());
    EXPECT_TRUE(secondMapping && !secondMapping->isPlaceholderViewMapped());

    EXPECT_TRUE(memoryManager->unMapPhysicalHostMemoryFromVirtualMemory(multiGraphicsAllocation, physicalAllocation.get(), svmGpuRange, mappingSize, true));
    EXPECT_EQ(1u, SysCalls::unmapViewOfFile2Called);
    EXPECT_EQ(addrToPtr(svmGpuRange), SysCalls::unmapViewOfFile2LastCall.baseAddress);
    EXPECT_EQ(static_cast<ULONG>(MEM_PRESERVE_PLACEHOLDER), SysCalls::unmapViewOfFile2LastCall.unmapFlags);
}

TEST_F(WddmMemoryManagerPhysicalHostMemoryTest, givenRangeSplitIntoPlaceholdersByEarlierMappingsWhenMappingPhysicalHostMemoryThenOverlappedPlaceholdersAreMergedBeforeRangeIsSplitOff) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    auto physicalAllocation = allocatePhysicalHostMemory(status);
    ASSERT_NE(nullptr, physicalAllocation);
    MultiGraphicsAllocation multiGraphicsAllocation{0u};
    const auto firstPlaceholderBase = addrToPtr(svmGpuRange - MemoryConstants::pageSize64k);
    SysCalls::virtualQueryMemoryBasicInformation.AllocationBase = firstPlaceholderBase;
    SysCalls::virtualQueryMemoryBasicInformation.BaseAddress = addrToPtr(svmGpuRange + MemoryConstants::pageSize64k);
    SysCalls::virtualQueryMemoryBasicInformation.RegionSize = 2 * MemoryConstants::pageSize64k;

    EXPECT_TRUE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation.get(), svmGpuRange, mappingSize, mappingOffset, true));
    ASSERT_EQ(2u, SysCalls::virtualFreeCalled);
    EXPECT_EQ(firstPlaceholderBase, SysCalls::virtualFreeCalls[0].address);
    EXPECT_EQ(4 * MemoryConstants::pageSize64k, SysCalls::virtualFreeCalls[0].size);
    EXPECT_EQ(static_cast<DWORD>(MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS), SysCalls::virtualFreeCalls[0].freeType);

    EXPECT_TRUE(memoryManager->unMapPhysicalHostMemoryFromVirtualMemory(multiGraphicsAllocation, physicalAllocation.get(), svmGpuRange, mappingSize, true));
}

TEST_F(WddmMemoryManagerPhysicalHostMemoryTest, givenGpuOnlyReservationWhenMappingPhysicalHostMemoryThenNoCpuViewIsMapped) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    auto physicalAllocation = allocatePhysicalHostMemory(status);
    ASSERT_NE(nullptr, physicalAllocation);
    MultiGraphicsAllocation multiGraphicsAllocation{0u};

    EXPECT_TRUE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation.get(), svmGpuRange, mappingSize, mappingOffset, false));
    EXPECT_EQ(0u, SysCalls::virtualFreeCalled);
    EXPECT_EQ(1u, SysCalls::mapViewOfFile3Called);
    auto mapping = static_cast<WddmAllocation *>(multiGraphicsAllocation.getGraphicsAllocation(0u));
    EXPECT_TRUE(mapping && !mapping->isPlaceholderViewMapped());

    EXPECT_TRUE(memoryManager->unMapPhysicalHostMemoryFromVirtualMemory(multiGraphicsAllocation, physicalAllocation.get(), svmGpuRange, mappingSize, false));
    EXPECT_EQ(0u, SysCalls::unmapViewOfFile2Called);
}

TEST_F(WddmMemoryManagerPhysicalHostMemoryTest, givenMappingCpuViewFailsWhenMappingPhysicalHostMemoryToSvmReservationThenGpuMappingIsReleasedAndMapFails) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    auto physicalAllocation = allocatePhysicalHostMemory(status);
    ASSERT_NE(nullptr, physicalAllocation);
    MultiGraphicsAllocation multiGraphicsAllocation{0u};
    auto gmmClientContext = static_cast<MockGmmClientContextBase *>(executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper()->getClientContext());
    const auto gmmFreeCallsBeforeMap = gmmClientContext->freeGpuVirtualAddressCalled;
    SysCalls::mapViewOfFile3Fails = true;

    EXPECT_FALSE(memoryManager->mapPhysicalHostMemoryToVirtualMemory(rootDeviceIndices, multiGraphicsAllocation, physicalAllocation.get(), svmGpuRange, mappingSize, mappingOffset, true));
    EXPECT_EQ(nullptr, multiGraphicsAllocation.getGraphicsAllocation(0u));
    EXPECT_EQ(gmmFreeCallsBeforeMap + 1, gmmClientContext->freeGpuVirtualAddressCalled);
    EXPECT_EQ(0u, SysCalls::unmapViewOfFile2Called);
}

namespace {
MEMORY_BASIC_INFORMATION hostMemoryRegions[2] = {};
size_t hostMemoryRegionQueries = 0u;
SIZE_T queryHostMemoryRegion(LPCVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer, SIZE_T dwLength) {
    hostMemoryRegionQueries++;
    const auto address = reinterpret_cast<uintptr_t>(lpAddress);
    for (const auto &region : hostMemoryRegions) {
        const auto regionStart = reinterpret_cast<uintptr_t>(region.BaseAddress);
        if (address >= regionStart && address < regionStart + region.RegionSize) {
            *lpBuffer = region;
            return sizeof(MEMORY_BASIC_INFORMATION);
        }
    }
    return 0;
}
} // namespace

using WddmMemoryManagerHostMemoryMappableTest = WddmFixture;

TEST_F(WddmMemoryManagerHostMemoryMappableTest, givenProtectionOfHostMemoryWhenCheckingIfHostMemoryIsMappableThenOnlyCommittedMemoryWithRequiredAccessIsMappable) {
    VariableBackup<MEMORY_BASIC_INFORMATION> memoryInfoBackup{&SysCalls::virtualQueryMemoryBasicInformation};
    VariableBackup<SIZE_T> virtualQueryResultBackup{&SysCalls::virtualQueryResult};

    auto hostPtr = reinterpret_cast<void *>(0x10000);
    const size_t size = 2 * MemoryConstants::pageSize;
    auto &memoryInfo = SysCalls::virtualQueryMemoryBasicInformation;
    memoryInfo.BaseAddress = ptrOffset(hostPtr, -static_cast<ptrdiff_t>(MemoryConstants::pageSize));
    memoryInfo.RegionSize = 4 * MemoryConstants::pageSize;
    memoryInfo.State = MEM_COMMIT;

    memoryInfo.Protect = PAGE_READWRITE;
    EXPECT_TRUE(memoryManager->isHostMemoryMappable(hostPtr, size, true));
    EXPECT_TRUE(memoryManager->isHostMemoryMappable(hostPtr, size, false));

    memoryInfo.Protect = PAGE_READONLY;
    EXPECT_FALSE(memoryManager->isHostMemoryMappable(hostPtr, size, true));
    EXPECT_TRUE(memoryManager->isHostMemoryMappable(hostPtr, size, false));

    memoryInfo.Protect = PAGE_NOACCESS;
    EXPECT_FALSE(memoryManager->isHostMemoryMappable(hostPtr, size, false));

    memoryInfo.Protect = PAGE_READWRITE | PAGE_GUARD;
    EXPECT_FALSE(memoryManager->isHostMemoryMappable(hostPtr, size, false));

    memoryInfo.Protect = PAGE_READWRITE;
    memoryInfo.State = MEM_RESERVE;
    EXPECT_FALSE(memoryManager->isHostMemoryMappable(hostPtr, size, false));

    memoryInfo.State = MEM_COMMIT;
    SysCalls::virtualQueryResult = 0;
    EXPECT_FALSE(memoryManager->isHostMemoryMappable(hostPtr, size, false));
}

TEST_F(WddmMemoryManagerHostMemoryMappableTest, givenSizeThatOverflowsAddressSpaceWhenCheckingIfHostMemoryIsMappableThenMemoryIsNotMappable) {
    auto hostPtr = reinterpret_cast<void *>(0x10000);
    const size_t overflowingSize = std::numeric_limits<uintptr_t>::max() - reinterpret_cast<uintptr_t>(hostPtr) + 1;
    EXPECT_FALSE(memoryManager->isHostMemoryMappable(hostPtr, overflowingSize, false));
}

TEST_F(WddmMemoryManagerHostMemoryMappableTest, givenRangeSpanningTwoRegionsWhenCheckingIfHostMemoryIsMappableThenEveryRegionIsChecked) {
    VariableBackup<decltype(SysCalls::sysCallsVirtualQuery)> virtualQueryBackup{&SysCalls::sysCallsVirtualQuery, queryHostMemoryRegion};
    VariableBackup<size_t> regionQueriesBackup{&hostMemoryRegionQueries, 0u};

    auto firstRegion = reinterpret_cast<void *>(0x10000);
    hostMemoryRegions[0] = {};
    hostMemoryRegions[0].BaseAddress = firstRegion;
    hostMemoryRegions[0].RegionSize = MemoryConstants::pageSize;
    hostMemoryRegions[0].State = MEM_COMMIT;
    hostMemoryRegions[0].Protect = PAGE_READWRITE;
    hostMemoryRegions[1] = hostMemoryRegions[0];
    hostMemoryRegions[1].BaseAddress = ptrOffset(firstRegion, MemoryConstants::pageSize);
    hostMemoryRegions[1].RegionSize = 2 * MemoryConstants::pageSize;

    auto hostPtr = ptrOffset(firstRegion, 0x100);
    const size_t size = 2 * MemoryConstants::pageSize;
    EXPECT_TRUE(memoryManager->isHostMemoryMappable(hostPtr, size, true));
    EXPECT_EQ(2u, hostMemoryRegionQueries);

    hostMemoryRegions[1].Protect = PAGE_READONLY;
    EXPECT_FALSE(memoryManager->isHostMemoryMappable(hostPtr, size, true));
    EXPECT_TRUE(memoryManager->isHostMemoryMappable(hostPtr, size, false));

    hostMemoryRegions[1].Protect = PAGE_READWRITE;
    hostMemoryRegions[1].State = MEM_RESERVE;
    EXPECT_FALSE(memoryManager->isHostMemoryMappable(hostPtr, size, false));

    hostMemoryRegionQueries = 0u;
    EXPECT_TRUE(memoryManager->isHostMemoryMappable(hostPtr, MemoryConstants::pageSize - 0x100, false));
    EXPECT_EQ(1u, hostMemoryRegionQueries);
}

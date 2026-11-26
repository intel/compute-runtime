/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/memory_manager/definitions/storage_info.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/test/common/mock_gdi/mock_gdi.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm_client_context.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/os_interface/windows/wddm_fixture.h"
#include "shared/test/common/test_macros/test.h"

#include <optional>
#include <vector>

using namespace NEO;

namespace {

constexpr D3DKMT_HANDLE primaryPagingQueueHandle = 0x100;
constexpr D3DKMT_HANDLE primarySyncObjectHandle = 0x200;
constexpr D3DGPU_VIRTUAL_ADDRESS primaryMappedAddress = 0x12340000;

struct PeerPagingQueueGdiState {
    std::vector<UINT> createdPhysicalAdapterIndices;
    std::vector<D3DKMT_HANDLE> destroyedPagingQueues;
    std::vector<D3DDDI_MAPGPUVIRTUALADDRESS> mapCalls;
    std::vector<D3DKMT_HANDLE> makeResidentPagingQueues;
    std::vector<D3DKMT_HANDLE> waitedSyncObjects;
    std::vector<uint64_t> waitedFenceValues;
    std::vector<D3DKMT_HANDLE> evictedHandles;
    std::optional<size_t> failEvictCallIndex;
    uint64_t pagingFences[4] = {};
    std::optional<UINT> failCreateForPhysicalAdapterIndex;
    D3DKMT_HANDLE failMapForPagingQueue = 0u;
    D3DKMT_HANDLE remapToDifferentAddressForPagingQueue = 0u;
    D3DKMT_HANDLE failMakeResidentForPagingQueue = 0u;
    uint64_t pagingFenceValueToReturn = 0u;
    uint64_t numBytesToTrimToReturn = 0u;
};

PeerPagingQueueGdiState gdiState;

D3DKMT_HANDLE pagingQueueHandleForAdapter(UINT physicalAdapterIndex) {
    return primaryPagingQueueHandle + physicalAdapterIndex;
}

D3DKMT_HANDLE syncObjectHandleForAdapter(UINT physicalAdapterIndex) {
    return primarySyncObjectHandle + physicalAdapterIndex;
}

NTSTATUS __stdcall createPagingQueueMock(D3DKMT_CREATEPAGINGQUEUE *arg) {
    gdiState.createdPhysicalAdapterIndices.push_back(arg->PhysicalAdapterIndex);
    if (gdiState.failCreateForPhysicalAdapterIndex == arg->PhysicalAdapterIndex) {
        return STATUS_UNSUCCESSFUL;
    }
    arg->hPagingQueue = pagingQueueHandleForAdapter(arg->PhysicalAdapterIndex);
    arg->hSyncObject = syncObjectHandleForAdapter(arg->PhysicalAdapterIndex);
    arg->FenceValueCPUVirtualAddress = &gdiState.pagingFences[arg->PhysicalAdapterIndex];
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall destroyPagingQueueMock(D3DDDI_DESTROYPAGINGQUEUE *arg) {
    gdiState.destroyedPagingQueues.push_back(arg->hPagingQueue);
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall mapGpuVirtualAddressMock(D3DDDI_MAPGPUVIRTUALADDRESS *arg) {
    gdiState.mapCalls.push_back(*arg);
    if (arg->hPagingQueue == gdiState.failMapForPagingQueue) {
        return STATUS_UNSUCCESSFUL;
    }
    if (arg->hPagingQueue == primaryPagingQueueHandle) {
        arg->VirtualAddress = primaryMappedAddress;
    } else if (arg->hPagingQueue == gdiState.remapToDifferentAddressForPagingQueue) {
        arg->VirtualAddress = arg->BaseAddress + MemoryConstants::pageSize64k;
    } else {
        arg->VirtualAddress = arg->BaseAddress;
    }
    arg->PagingFenceValue = gdiState.pagingFenceValueToReturn;
    return gdiState.pagingFenceValueToReturn != 0u ? STATUS_PENDING : STATUS_SUCCESS;
}

NTSTATUS __stdcall makeResidentMock(D3DDDI_MAKERESIDENT *arg) {
    gdiState.makeResidentPagingQueues.push_back(arg->hPagingQueue);
    if (arg->hPagingQueue == gdiState.failMakeResidentForPagingQueue) {
        arg->NumBytesToTrim = gdiState.numBytesToTrimToReturn;
        return STATUS_NO_MEMORY;
    }
    arg->PagingFenceValue = gdiState.pagingFenceValueToReturn;
    return gdiState.pagingFenceValueToReturn != 0u ? STATUS_PENDING : STATUS_SUCCESS;
}

NTSTATUS __stdcall evictMock(D3DKMT_EVICT *arg) {
    gdiState.evictedHandles.push_back(arg->AllocationList[0]);
    if (gdiState.failEvictCallIndex == gdiState.evictedHandles.size() - 1u) {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_SUCCESS;
}

NTSTATUS __stdcall waitFromCpuMock(const D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU *arg) {
    gdiState.waitedSyncObjects.push_back(arg->ObjectHandleArray[0]);
    gdiState.waitedFenceValues.push_back(arg->FenceValueArray[0]);
    return STATUS_SUCCESS;
}

} // namespace

struct WddmPeerPagingQueueTest : public WddmTest {
    void SetUp() override {
        WddmTest::SetUp();
        gdiState = {};
        auto gdi = wddm->getGdi();
        gdi->createPagingQueue = &createPagingQueueMock;
        gdi->destroyPagingQueue = &destroyPagingQueueMock;
        gdi->mapGpuVirtualAddress = &mapGpuVirtualAddressMock;
        gdi->makeResident = &makeResidentMock;
        gdi->waitForSynchronizationObjectFromCpu = &waitFromCpuMock;
        gdi->evict = &evictMock;
        wddm->destroyPagingQueue();
        gdiState.destroyedPagingQueues.clear();
    }

    void TearDown() override {
        wddm->destroyPagingQueue();
        gdiState = {};
        WddmTest::TearDown();
    }

    void recreatePagingQueues(uint32_t numPhysicalAdapters) {
        wddm->numPhysicalAdapters = numPhysicalAdapters;
        ASSERT_TRUE(wddm->createPagingQueue());
        gdiState.createdPhysicalAdapterIndices.clear();
    }

    bool mapGpuVirtualAddress() {
        D3DGPU_VIRTUAL_ADDRESS gpuVa = 0u;
        return mapGpuVirtualAddress(gpuVa);
    }

    bool mapGpuVirtualAddress(D3DGPU_VIRTUAL_ADDRESS &gpuVa) {
        return mapGpuVirtualAddress(gpuVa, 0u);
    }

    bool mapGpuVirtualAddress(D3DGPU_VIRTUAL_ADDRESS &gpuVa, D3DGPU_VIRTUAL_ADDRESS preferredAddress) {
        GmmRequirements gmmRequirements{};
        gmmRequirements.allowLargePages = true;
        gmmRequirements.preferCompressed = false;
        auto gmm = std::make_unique<Gmm>(getGmmHelper(), nullptr, MemoryConstants::pageSize64k, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, StorageInfo{}, gmmRequirements);
        return wddm->mapGpuVirtualAddress(gmm.get(), ALLOCATION_HANDLE, wddm->getGfxPartition().Standard.Base, wddm->getGfxPartition().Standard.Limit, preferredAddress, gpuVa, AllocationType::unknown, nullptr, 0u);
    }

    MockGmmClientContext *getMockGmmClientContext() {
        return static_cast<MockGmmClientContext *>(rootDeviceEnvironment->getGmmClientContext());
    }

    bool makeResident(uint64_t *numberOfBytesToTrim) {
        wddm->callBaseMakeResident = true;
        wddm->currentPagingFenceValue = 0u;
        D3DKMT_HANDLE handle = ALLOCATION_HANDLE;
        return wddm->makeResident(&handle, 1u, false, numberOfBytesToTrim, MemoryConstants::pageSize64k);
    }
};

TEST_F(WddmPeerPagingQueueTest, givenSinglePhysicalAdapterWhenCreatingPagingQueueThenOnlyQueueForPhysicalAdapterZeroIsCreated) {
    for (auto numPhysicalAdapters : {0u, 1u}) {
        gdiState.createdPhysicalAdapterIndices.clear();
        wddm->numPhysicalAdapters = numPhysicalAdapters;
        EXPECT_TRUE(wddm->createPagingQueue());

        EXPECT_EQ((std::vector<UINT>{0u}), gdiState.createdPhysicalAdapterIndices);
        EXPECT_EQ(primaryPagingQueueHandle, wddm->getPagingQueue());
        EXPECT_TRUE(wddm->peerPagingQueues.empty());
        wddm->destroyPagingQueue();
    }
}

TEST_F(WddmPeerPagingQueueTest, givenMultiplePhysicalAdaptersWhenCreatingPagingQueueThenOneQueueIsCreatedPerPhysicalAdapter) {
    wddm->numPhysicalAdapters = 3u;
    EXPECT_TRUE(wddm->createPagingQueue());

    EXPECT_EQ((std::vector<UINT>{0u, 1u, 2u}), gdiState.createdPhysicalAdapterIndices);
    EXPECT_EQ(primaryPagingQueueHandle, wddm->getPagingQueue());
    EXPECT_EQ(primarySyncObjectHandle, wddm->getPagingQueueSyncObject());
    ASSERT_EQ(2u, wddm->peerPagingQueues.size());
    for (UINT physicalAdapterIndex = 1u; physicalAdapterIndex < 3u; physicalAdapterIndex++) {
        auto &peerPagingQueue = wddm->peerPagingQueues[physicalAdapterIndex - 1u];
        EXPECT_EQ(pagingQueueHandleForAdapter(physicalAdapterIndex), peerPagingQueue.handle);
        EXPECT_EQ(syncObjectHandleForAdapter(physicalAdapterIndex), peerPagingQueue.syncObject);
        EXPECT_EQ(&gdiState.pagingFences[physicalAdapterIndex], peerPagingQueue.fenceAddress);
    }
}

TEST_F(WddmPeerPagingQueueTest, givenMultiplePhysicalAdaptersWhenCreatingPeerPagingQueueFailsThenCreatePagingQueueFailsAndDestroyPagingQueueDestroysCreatedQueues) {
    gdiState.failCreateForPhysicalAdapterIndex = 2u;
    wddm->numPhysicalAdapters = 3u;
    EXPECT_FALSE(wddm->createPagingQueue());
    EXPECT_EQ((std::vector<UINT>{0u, 1u, 2u}), gdiState.createdPhysicalAdapterIndices);

    wddm->destroyPagingQueue();
    EXPECT_EQ((std::vector<D3DKMT_HANDLE>{pagingQueueHandleForAdapter(1u), primaryPagingQueueHandle}), gdiState.destroyedPagingQueues);
}

TEST_F(WddmPeerPagingQueueTest, givenPrimaryPagingQueueCreationFailsWhenCreatingPagingQueueThenPeerQueuesAreNotCreated) {
    gdiState.failCreateForPhysicalAdapterIndex = 0u;
    wddm->numPhysicalAdapters = 3u;
    EXPECT_FALSE(wddm->createPagingQueue());

    EXPECT_EQ((std::vector<UINT>{0u}), gdiState.createdPhysicalAdapterIndices);
    EXPECT_TRUE(wddm->peerPagingQueues.empty());
}

TEST_F(WddmPeerPagingQueueTest, givenMultiplePhysicalAdaptersWhenDestroyingPagingQueueThenEveryQueueIsDestroyed) {
    recreatePagingQueues(3u);

    wddm->destroyPagingQueue();

    EXPECT_EQ((std::vector<D3DKMT_HANDLE>{pagingQueueHandleForAdapter(1u), pagingQueueHandleForAdapter(2u), primaryPagingQueueHandle}), gdiState.destroyedPagingQueues);
    EXPECT_TRUE(wddm->peerPagingQueues.empty());
    EXPECT_EQ(0u, wddm->getPagingQueue());
}

TEST_F(WddmPeerPagingQueueTest, givenSinglePhysicalAdapterWhenMappingGpuVirtualAddressThenOnlyPrimaryPagingQueueIsUsed) {
    recreatePagingQueues(1u);

    EXPECT_TRUE(mapGpuVirtualAddress());

    ASSERT_EQ(1u, gdiState.mapCalls.size());
    EXPECT_EQ(primaryPagingQueueHandle, gdiState.mapCalls[0].hPagingQueue);
}

TEST_F(WddmPeerPagingQueueTest, givenMultiplePhysicalAdaptersWhenMappingGpuVirtualAddressThenAddressIsMappedOnEveryPeerAtPrimaryAddress) {
    recreatePagingQueues(3u);
    gdiState.pagingFenceValueToReturn = 7u;

    EXPECT_TRUE(mapGpuVirtualAddress());

    ASSERT_EQ(3u, gdiState.mapCalls.size());
    EXPECT_EQ(primaryPagingQueueHandle, gdiState.mapCalls[0].hPagingQueue);
    for (UINT physicalAdapterIndex = 1u; physicalAdapterIndex < 3u; physicalAdapterIndex++) {
        auto &peerMapCall = gdiState.mapCalls[physicalAdapterIndex];
        EXPECT_EQ(pagingQueueHandleForAdapter(physicalAdapterIndex), peerMapCall.hPagingQueue);
        EXPECT_EQ(primaryMappedAddress, peerMapCall.BaseAddress);
        EXPECT_EQ(gdiState.mapCalls[0].hAllocation, peerMapCall.hAllocation);
        EXPECT_EQ(gdiState.mapCalls[0].SizeInPages, peerMapCall.SizeInPages);
        EXPECT_EQ(7u, wddm->peerPagingQueues[physicalAdapterIndex - 1u].currentFenceValue.load());
    }
}

TEST_F(WddmPeerPagingQueueTest, givenMultiplePhysicalAdaptersWhenMappingOnPeerFailsThenPrimaryMappingIsFreedAndMapGpuVirtualAddressFails) {
    recreatePagingQueues(3u);
    gdiState.failMapForPagingQueue = pagingQueueHandleForAdapter(2u);
    auto freeCalledBefore = getMockGmmClientContext()->freeGpuVirtualAddressCalled;
    auto wddmFreeCalledBefore = wddm->freeGpuVirtualAddressResult.called;

    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0u;
    EXPECT_FALSE(mapGpuVirtualAddress(gpuVa));
    EXPECT_EQ(3u, gdiState.mapCalls.size());
    EXPECT_EQ(freeCalledBefore + 1u, getMockGmmClientContext()->freeGpuVirtualAddressCalled);
    EXPECT_EQ(wddmFreeCalledBefore, wddm->freeGpuVirtualAddressResult.called);
    EXPECT_EQ(0u, gpuVa);
}

TEST_F(WddmPeerPagingQueueTest, givenMultiplePhysicalAdaptersAndPreferredAddressWhenMappingOnPeerFailsThenPrimaryMappingIsNotFreedAndMapGpuVirtualAddressFails) {
    recreatePagingQueues(3u);
    gdiState.failMapForPagingQueue = pagingQueueHandleForAdapter(2u);
    auto freeCalledBefore = getMockGmmClientContext()->freeGpuVirtualAddressCalled;
    auto wddmFreeCalledBefore = wddm->freeGpuVirtualAddressResult.called;

    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0u;
    EXPECT_FALSE(mapGpuVirtualAddress(gpuVa, primaryMappedAddress));
    EXPECT_EQ(3u, gdiState.mapCalls.size());
    EXPECT_EQ(freeCalledBefore, getMockGmmClientContext()->freeGpuVirtualAddressCalled);
    EXPECT_EQ(wddmFreeCalledBefore, wddm->freeGpuVirtualAddressResult.called);
    EXPECT_NE(0u, gpuVa);
}

TEST_F(WddmPeerPagingQueueTest, givenMultiplePhysicalAdaptersWhenPeerMapsAtDifferentAddressThenBothMappingsAreFreedAndMapGpuVirtualAddressFails) {
    recreatePagingQueues(3u);
    gdiState.remapToDifferentAddressForPagingQueue = pagingQueueHandleForAdapter(1u);
    auto freeCalledBefore = getMockGmmClientContext()->freeGpuVirtualAddressCalled;
    auto wddmFreeCalledBefore = wddm->freeGpuVirtualAddressResult.called;

    D3DGPU_VIRTUAL_ADDRESS gpuVa = 0u;
    EXPECT_FALSE(mapGpuVirtualAddress(gpuVa));
    EXPECT_EQ(2u, gdiState.mapCalls.size());
    EXPECT_EQ(wddmFreeCalledBefore + 1u, wddm->freeGpuVirtualAddressResult.called);
    EXPECT_EQ(primaryMappedAddress + MemoryConstants::pageSize64k, wddm->freeGpuVirtualAddressResult.uint64ParamPassed);
    EXPECT_EQ(MemoryConstants::pageSize64k, wddm->freeGpuVirtualAddressResult.sizePassed);
    EXPECT_EQ(freeCalledBefore + 1u, getMockGmmClientContext()->freeGpuVirtualAddressCalled);
    EXPECT_EQ(0u, gpuVa);
}

TEST_F(WddmPeerPagingQueueTest, givenMultiplePhysicalAdaptersWhenMakingResidentThenAllocationsAreMadeResidentOnEveryPagingQueue) {
    recreatePagingQueues(3u);
    gdiState.pagingFenceValueToReturn = 5u;

    EXPECT_TRUE(makeResident(nullptr));
    EXPECT_TRUE(gdiState.evictedHandles.empty());

    EXPECT_EQ((std::vector<D3DKMT_HANDLE>{primaryPagingQueueHandle, pagingQueueHandleForAdapter(1u), pagingQueueHandleForAdapter(2u)}), gdiState.makeResidentPagingQueues);
    EXPECT_EQ(5u, wddm->getCurrentPagingFenceValue());
    for (auto &peerPagingQueue : wddm->peerPagingQueues) {
        EXPECT_EQ(5u, peerPagingQueue.currentFenceValue.load());
    }
}

TEST_F(WddmPeerPagingQueueTest, givenMultiplePhysicalAdaptersWhenMakeResidentSucceedsWithoutPendingThenPeerFenceValuesAreNotUpdated) {
    recreatePagingQueues(2u);

    EXPECT_TRUE(makeResident(nullptr));

    EXPECT_EQ(2u, gdiState.makeResidentPagingQueues.size());
    EXPECT_EQ(0u, wddm->peerPagingQueues[0].currentFenceValue.load());
}

TEST_F(WddmPeerPagingQueueTest, givenMultiplePhysicalAdaptersWhenMakeResidentOnPeerFailsThenHandlesAreEvictedOnceForEveryQueueThatMadeThemResidentAndBytesToTrimComeFromPeer) {
    recreatePagingQueues(3u);
    gdiState.failMakeResidentForPagingQueue = pagingQueueHandleForAdapter(2u);
    gdiState.numBytesToTrimToReturn = 0x3000u;

    uint64_t numberOfBytesToTrim = 0u;
    EXPECT_FALSE(makeResident(&numberOfBytesToTrim));
    EXPECT_EQ(0x3000u, numberOfBytesToTrim);
    EXPECT_EQ((std::vector<D3DKMT_HANDLE>{primaryPagingQueueHandle, pagingQueueHandleForAdapter(1u), pagingQueueHandleForAdapter(2u)}), gdiState.makeResidentPagingQueues);
    EXPECT_EQ((std::vector<D3DKMT_HANDLE>{ALLOCATION_HANDLE, ALLOCATION_HANDLE}), gdiState.evictedHandles);

    gdiState.evictedHandles.clear();
    EXPECT_FALSE(makeResident(nullptr));
    EXPECT_EQ(2u, gdiState.evictedHandles.size());
}

TEST_F(WddmPeerPagingQueueTest, givenMultiplePhysicalAdaptersWhenMakeResidentFailsOnPrimaryPagingQueueThenPeersAreNotUsedAndNothingIsEvicted) {
    recreatePagingQueues(3u);
    gdiState.failMakeResidentForPagingQueue = primaryPagingQueueHandle;

    EXPECT_FALSE(makeResident(nullptr));
    EXPECT_EQ((std::vector<D3DKMT_HANDLE>{primaryPagingQueueHandle}), gdiState.makeResidentPagingQueues);
    EXPECT_TRUE(gdiState.evictedHandles.empty());
}

TEST_F(WddmPeerPagingQueueTest, givenSinglePhysicalAdapterWhenEvictingThenEvictIsIssuedOnce) {
    recreatePagingQueues(1u);
    wddm->callBaseEvict = true;
    D3DKMT_HANDLE handle = ALLOCATION_HANDLE;
    uint64_t sizeToTrim = 0u;

    EXPECT_TRUE(wddm->evict(&handle, 1u, sizeToTrim, true));
    EXPECT_EQ((std::vector<D3DKMT_HANDLE>{ALLOCATION_HANDLE}), gdiState.evictedHandles);
}

TEST_F(WddmPeerPagingQueueTest, givenMultiplePhysicalAdaptersWhenOneEvictCallFailsThenEvictIsStillIssuedOncePerPagingQueueAndFails) {
    recreatePagingQueues(3u);
    wddm->callBaseEvict = true;
    gdiState.failEvictCallIndex = 0u;
    D3DKMT_HANDLE handle = ALLOCATION_HANDLE;
    uint64_t sizeToTrim = 0u;

    EXPECT_FALSE(wddm->evict(&handle, 1u, sizeToTrim, true));
    EXPECT_EQ((std::vector<D3DKMT_HANDLE>{ALLOCATION_HANDLE, ALLOCATION_HANDLE, ALLOCATION_HANDLE}), gdiState.evictedHandles);
}

TEST_F(WddmPeerPagingQueueTest, givenMultiplePhysicalAdaptersWhenEvictingThenEvictIsIssuedOncePerPagingQueue) {
    recreatePagingQueues(3u);
    wddm->callBaseEvict = true;
    D3DKMT_HANDLE handle = ALLOCATION_HANDLE;
    uint64_t sizeToTrim = 0u;

    EXPECT_TRUE(wddm->evict(&handle, 1u, sizeToTrim, true));
    EXPECT_EQ((std::vector<D3DKMT_HANDLE>{ALLOCATION_HANDLE, ALLOCATION_HANDLE, ALLOCATION_HANDLE}), gdiState.evictedHandles);
}

TEST_F(WddmPeerPagingQueueTest, givenMultiplePhysicalAdaptersWhenWaitingOnPagingFenceWithKmdWaitThenOnlyPeersWithPendingFenceAreWaited) {
    recreatePagingQueues(3u);
    wddm->currentPagingFenceValue = 0u;
    wddm->peerPagingQueues[0].currentFenceValue = 4u;
    gdiState.pagingFences[1] = 4u;
    wddm->peerPagingQueues[1].currentFenceValue = 9u;
    gdiState.pagingFences[2] = 3u;

    wddm->waitOnPagingFenceFromCpu(true);

    EXPECT_EQ((std::vector<D3DKMT_HANDLE>{syncObjectHandleForAdapter(2u)}), gdiState.waitedSyncObjects);
    EXPECT_EQ((std::vector<uint64_t>{9u}), gdiState.waitedFenceValues);
}

struct WddmMockWithAdvancingPeerPagingFence : public WddmMock {
    using WddmMock::WddmMock;

    volatile uint64_t *getPeerPagingFenceAddress(const PeerPagingQueue &peerPagingQueue) override {
        *peerPagingQueue.fenceAddress = *peerPagingQueue.fenceAddress + 1u;
        return WddmMock::getPeerPagingFenceAddress(peerPagingQueue);
    }
};

TEST(WddmPeerPagingQueueWaitTest, givenPeerPagingFencePendingWhenWaitingOnPagingFenceWithoutKmdWaitThenPeerFenceIsPolledUntilReached) {
    MockExecutionEnvironment executionEnvironment;
    WddmMockWithAdvancingPeerPagingFence wddm(*executionEnvironment.rootDeviceEnvironments[0]);
    volatile uint64_t peerPagingFence = 0u;
    wddm.peerPagingQueues = decltype(wddm.peerPagingQueues)(1);
    wddm.peerPagingQueues[0].fenceAddress = &peerPagingFence;
    wddm.peerPagingQueues[0].currentFenceValue = 3u;

    wddm.waitOnPagingFenceFromCpu(false);

    EXPECT_EQ(3u, peerPagingFence);
}

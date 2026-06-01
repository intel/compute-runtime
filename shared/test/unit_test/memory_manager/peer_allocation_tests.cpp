/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

using namespace NEO;

namespace {

class MockMultiHandleGraphicsAllocation : public MockGraphicsAllocation {
  public:
    using MockGraphicsAllocation::MockGraphicsAllocation;
    using MockGraphicsAllocation::peekInternalHandle;

    uint32_t getNumHandles() override { return numHandles; }

    int peekInternalHandle(MemoryManager *memoryManager, uint32_t handleId, uint64_t &handle, void *reservedHandleData) override {
        peekInternalHandleIndexedCalls++;
        lastRequestedHandleId = handleId;
        if (peekInternalHandleIndexedCalls > peekFailAfterCalls) {
            return -1;
        }
        handle = 0x1000ull + handleId;
        return peekInternalHandleIndexedResult;
    }

    uint32_t numHandles = 0u;
    uint32_t peekInternalHandleIndexedCalls = 0u;
    uint32_t lastRequestedHandleId = 0u;
    int peekInternalHandleIndexedResult = 0;
    uint32_t peekFailAfterCalls = std::numeric_limits<uint32_t>::max();
};

class FdTrackingMemoryManager : public MockMemoryManager {
  public:
    using MockMemoryManager::MockMemoryManager;

    void closeInternalHandle(uint64_t &handle, uint32_t handleId, GraphicsAllocation *graphicsAllocation) override {
        closeCalls.push_back({handle, handleId, graphicsAllocation});
    }

    struct CloseCall {
        uint64_t handle;
        uint32_t handleId;
        GraphicsAllocation *alloc;
    };
    std::vector<CloseCall> closeCalls;
};

struct PeerAllocationTest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = new MockExecutionEnvironment(defaultHwInfo.get(), false, numRootDevices);
        executionEnvironment->memoryManager.reset(new MockMemoryManager(*executionEnvironment));
        memoryManager = static_cast<MockMemoryManager *>(executionEnvironment->memoryManager.get());
        svmAllocsManager = std::make_unique<SVMAllocsManager>(memoryManager);
        device0.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0));
        device1.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 1));
    }

    PeerAllocationDeps makeCountingDeps(uint32_t &importFdCalls, uint32_t &importFdsCalls, uint32_t &decompressCalls,
                                        void *returnAddress = reinterpret_cast<void *>(0xBADC0FFEULL)) {
        PeerAllocationDeps deps{};
        deps.importFd = [&, returnAddress](Device *, uint64_t, AllocationType, void *,
                                           GraphicsAllocation **, SvmAllocationData &, bool) -> void * {
            ++importFdCalls;
            return returnAddress;
        };
        deps.importFds = [&, returnAddress](Device *, const std::vector<osHandle> &,
                                            void *, GraphicsAllocation **, SvmAllocationData &, bool) -> void * {
            ++importFdsCalls;
            return returnAddress;
        };
        deps.decompressP2P = [&](GraphicsAllocation *) {
            ++decompressCalls;
        };
        return deps;
    }

    static constexpr uint32_t numRootDevices = 2u;
    MockExecutionEnvironment *executionEnvironment = nullptr;
    MockMemoryManager *memoryManager = nullptr;
    std::unique_ptr<SVMAllocsManager> svmAllocsManager;
    std::unique_ptr<MockDevice> device0;
    std::unique_ptr<MockDevice> device1;
};

} // namespace

TEST_F(PeerAllocationTest, givenSameRootDeviceWhenIsRemoteResourceNeededThenReturnsFalse) {
    MockGraphicsAllocation alloc(0u, nullptr, 0u);
    SvmAllocationData allocData(0u);
    allocData.gpuAllocations.addAllocation(&alloc);

    EXPECT_FALSE(memoryManager->isRemoteResourceNeeded(&alloc, &allocData, device0.get()));
}

TEST_F(PeerAllocationTest, givenDifferentRootDeviceAndMissingPeerAllocationWhenIsRemoteResourceNeededThenReturnsTrue) {
    MockGraphicsAllocation alloc(0u, nullptr, 0u);
    SvmAllocationData allocData(0u);
    allocData.gpuAllocations.addAllocation(&alloc);

    EXPECT_TRUE(memoryManager->isRemoteResourceNeeded(&alloc, &allocData, device1.get()));
}

TEST_F(PeerAllocationTest, givenNullAllocationWhenIsRemoteResourceNeededThenReturnsTrue) {
    SvmAllocationData allocData(0u);
    EXPECT_TRUE(memoryManager->isRemoteResourceNeeded(nullptr, &allocData, device0.get()));
}

TEST_F(PeerAllocationTest, givenCachedPeerAllocationWhenGettingPeerAllocationThenImportIsSkipped) {
    SVMAllocsManager::MapBasedAllocationTracker storage;
    MockGraphicsAllocation cachedAlloc(1u, nullptr, 0u);
    cachedAlloc.gpuAddress = 0xCAFECAFEull;

    SvmAllocationData cachedData(numRootDevices);
    cachedData.gpuAllocations.addAllocation(&cachedAlloc);
    void *basePtr = reinterpret_cast<void *>(0x1000);
    storage.allocations.insert(std::make_pair(basePtr, cachedData));

    uint32_t importFdCalls = 0, importFdsCalls = 0, decompressCalls = 0;
    auto deps = makeCountingDeps(importFdCalls, importFdsCalls, decompressCalls);

    SvmAllocationData sourceData(numRootDevices);
    uintptr_t peerGpuAddress = 0;
    SvmAllocationData *peerData = nullptr;

    auto result = memoryManager->getOrImportPeerAllocation(device1.get(), svmAllocsManager.get(),
                                                           storage, &sourceData, basePtr,
                                                           &peerGpuAddress, &peerData, false, deps);

    EXPECT_EQ(&cachedAlloc, result);
    EXPECT_EQ(static_cast<uintptr_t>(0xCAFECAFEull), peerGpuAddress);
    EXPECT_EQ(0u, importFdCalls);
    EXPECT_EQ(0u, importFdsCalls);
}

TEST_F(PeerAllocationTest, givenSingleHandleAllocationWhenGettingPeerAllocationThenImportFdInvokedOnce) {
    SVMAllocsManager::MapBasedAllocationTracker storage;
    MockMultiHandleGraphicsAllocation source;
    source.numHandles = 0u;
    source.internalHandle = 77u;

    SvmAllocationData sourceData(numRootDevices);
    sourceData.gpuAllocations.addAllocation(&source);

    uint32_t importFdCalls = 0, importFdsCalls = 0, decompressCalls = 0;
    auto deps = makeCountingDeps(importFdCalls, importFdsCalls, decompressCalls);

    void *basePtr = reinterpret_cast<void *>(0x2000);
    uintptr_t peerGpuAddress = 0;
    SvmAllocationData *peerData = nullptr;

    auto result = memoryManager->getOrImportPeerAllocation(device1.get(), svmAllocsManager.get(),
                                                           storage, &sourceData, basePtr,
                                                           &peerGpuAddress, &peerData, false, deps);

    EXPECT_NE(nullptr, result);
    EXPECT_EQ(1u, importFdCalls);
    EXPECT_EQ(0u, importFdsCalls);
    EXPECT_EQ(0u, decompressCalls);
    EXPECT_EQ(1u, storage.allocations.count(basePtr));
}

TEST_F(PeerAllocationTest, givenMultiHandleAllocationWhenGettingPeerAllocationThenImportFdsInvokedWithAllHandles) {
    SVMAllocsManager::MapBasedAllocationTracker storage;
    MockMultiHandleGraphicsAllocation source;
    source.numHandles = 3u;

    SvmAllocationData sourceData(numRootDevices);
    sourceData.gpuAllocations.addAllocation(&source);

    uint32_t importFdCalls = 0, importFdsCalls = 0, decompressCalls = 0;
    std::vector<osHandle> capturedHandles;
    PeerAllocationDeps deps{};
    deps.importFd = [&](Device *, uint64_t, AllocationType, void *, GraphicsAllocation **,
                        SvmAllocationData &, bool) -> void * {
        ++importFdCalls;
        return reinterpret_cast<void *>(0xBADC0FFEULL);
    };
    deps.importFds = [&](Device *, const std::vector<osHandle> &handles, void *,
                         GraphicsAllocation **, SvmAllocationData &, bool) -> void * {
        ++importFdsCalls;
        capturedHandles = handles;
        return reinterpret_cast<void *>(0xBADC0FFEULL);
    };
    deps.decompressP2P = [&](GraphicsAllocation *) { ++decompressCalls; };

    void *basePtr = reinterpret_cast<void *>(0x3000);
    uintptr_t peerGpuAddress = 0;
    SvmAllocationData *peerData = nullptr;

    auto result = memoryManager->getOrImportPeerAllocation(device1.get(), svmAllocsManager.get(),
                                                           storage, &sourceData, basePtr,
                                                           &peerGpuAddress, &peerData, false, deps);

    EXPECT_NE(nullptr, result);
    EXPECT_EQ(0u, importFdCalls);
    EXPECT_EQ(1u, importFdsCalls);
    EXPECT_EQ(3u, capturedHandles.size());
    EXPECT_EQ(3u, source.peekInternalHandleIndexedCalls);
}

TEST_F(PeerAllocationTest, givenDecompressRequestedAndCrossRootWhenGettingPeerAllocationThenDecompressCalled) {
    SVMAllocsManager::MapBasedAllocationTracker storage;
    MockMultiHandleGraphicsAllocation source(0u, nullptr, 0u);
    source.numHandles = 0u;

    SvmAllocationData sourceData(numRootDevices);
    sourceData.gpuAllocations.addAllocation(&source);

    uint32_t importFdCalls = 0, importFdsCalls = 0, decompressCalls = 0;
    auto deps = makeCountingDeps(importFdCalls, importFdsCalls, decompressCalls);

    void *basePtr = reinterpret_cast<void *>(0x4000);
    uintptr_t peerGpuAddress = 0;
    SvmAllocationData *peerData = nullptr;

    auto result = memoryManager->getOrImportPeerAllocation(device1.get(), svmAllocsManager.get(),
                                                           storage, &sourceData, basePtr,
                                                           &peerGpuAddress, &peerData, true, deps);

    EXPECT_NE(nullptr, result);
    EXPECT_EQ(1u, decompressCalls);
}

TEST_F(PeerAllocationTest, givenDecompressRequestedButSameRootWhenGettingPeerAllocationThenDecompressSkipped) {
    SVMAllocsManager::MapBasedAllocationTracker storage;
    MockMultiHandleGraphicsAllocation source(1u, nullptr, 0u);
    source.numHandles = 0u;

    SvmAllocationData sourceData(numRootDevices);
    sourceData.gpuAllocations.addAllocation(&source);

    uint32_t importFdCalls = 0, importFdsCalls = 0, decompressCalls = 0;
    auto deps = makeCountingDeps(importFdCalls, importFdsCalls, decompressCalls);

    void *basePtr = reinterpret_cast<void *>(0x5000);
    uintptr_t peerGpuAddress = 0;
    SvmAllocationData *peerData = nullptr;

    auto result = memoryManager->getOrImportPeerAllocation(device1.get(), svmAllocsManager.get(),
                                                           storage, &sourceData, basePtr,
                                                           &peerGpuAddress, &peerData, true, deps);

    EXPECT_NE(nullptr, result);
    EXPECT_EQ(0u, decompressCalls);
}

TEST_F(PeerAllocationTest, givenPeekInternalHandleFailsWhenGettingPeerAllocationThenReturnsNullptr) {
    SVMAllocsManager::MapBasedAllocationTracker storage;
    MockMultiHandleGraphicsAllocation source;
    source.numHandles = 2u;
    source.peekInternalHandleIndexedResult = -1;

    SvmAllocationData sourceData(numRootDevices);
    sourceData.gpuAllocations.addAllocation(&source);

    uint32_t importFdCalls = 0, importFdsCalls = 0, decompressCalls = 0;
    auto deps = makeCountingDeps(importFdCalls, importFdsCalls, decompressCalls);

    void *basePtr = reinterpret_cast<void *>(0x6000);
    uintptr_t peerGpuAddress = 0;
    SvmAllocationData *peerData = nullptr;

    auto result = memoryManager->getOrImportPeerAllocation(device1.get(), svmAllocsManager.get(),
                                                           storage, &sourceData, basePtr,
                                                           &peerGpuAddress, &peerData, false, deps);

    EXPECT_EQ(nullptr, result);
    EXPECT_EQ(0u, importFdsCalls);
}

TEST_F(PeerAllocationTest, givenImportReturnsNullptrWhenGettingPeerAllocationThenReturnsNullptr) {
    SVMAllocsManager::MapBasedAllocationTracker storage;
    MockMultiHandleGraphicsAllocation source;
    source.numHandles = 0u;

    SvmAllocationData sourceData(numRootDevices);
    sourceData.gpuAllocations.addAllocation(&source);

    uint32_t importFdCalls = 0, importFdsCalls = 0, decompressCalls = 0;
    auto deps = makeCountingDeps(importFdCalls, importFdsCalls, decompressCalls, nullptr);

    void *basePtr = reinterpret_cast<void *>(0x7000);
    uintptr_t peerGpuAddress = 0;
    SvmAllocationData *peerData = nullptr;

    auto result = memoryManager->getOrImportPeerAllocation(device1.get(), svmAllocsManager.get(),
                                                           storage, &sourceData, basePtr,
                                                           &peerGpuAddress, &peerData, false, deps);

    EXPECT_EQ(nullptr, result);
    EXPECT_EQ(1u, importFdCalls);
    EXPECT_EQ(0u, storage.allocations.count(basePtr));
}

struct PeerAllocationFdTrackingTest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = new MockExecutionEnvironment(defaultHwInfo.get(), false, numRootDevices);
        executionEnvironment->memoryManager.reset(new FdTrackingMemoryManager(*executionEnvironment));
        memoryManager = static_cast<FdTrackingMemoryManager *>(executionEnvironment->memoryManager.get());
        svmAllocsManager = std::make_unique<SVMAllocsManager>(memoryManager);
        device0.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0));
        device1.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 1));
    }

    static constexpr uint32_t numRootDevices = 2u;
    MockExecutionEnvironment *executionEnvironment = nullptr;
    FdTrackingMemoryManager *memoryManager = nullptr;
    std::unique_ptr<SVMAllocsManager> svmAllocsManager;
    std::unique_ptr<MockDevice> device0;
    std::unique_ptr<MockDevice> device1;
};

TEST_F(PeerAllocationFdTrackingTest, givenMultiHandleAllocationWhenImportSucceedsThenAllImportedHandlesAreClosed) {
    SVMAllocsManager::MapBasedAllocationTracker storage;
    MockMultiHandleGraphicsAllocation source;
    source.numHandles = 3u;

    SvmAllocationData sourceData(numRootDevices);
    sourceData.gpuAllocations.addAllocation(&source);

    PeerAllocationDeps deps{};
    deps.importFd = [](Device *, uint64_t, AllocationType, void *, GraphicsAllocation **,
                       SvmAllocationData &, bool) -> void * { return nullptr; };
    deps.importFds = [](Device *, const std::vector<osHandle> &, void *,
                        GraphicsAllocation **, SvmAllocationData &, bool) -> void * {
        return reinterpret_cast<void *>(0xBADC0FFEULL);
    };
    deps.decompressP2P = [](GraphicsAllocation *) {};

    void *basePtr = reinterpret_cast<void *>(0x10000);
    uintptr_t peerGpuAddress = 0;
    SvmAllocationData *peerData = nullptr;

    auto result = memoryManager->getOrImportPeerAllocation(device1.get(), svmAllocsManager.get(),
                                                           storage, &sourceData, basePtr,
                                                           &peerGpuAddress, &peerData, false, deps);

    EXPECT_NE(nullptr, result);
    ASSERT_EQ(3u, memoryManager->closeCalls.size());
    EXPECT_EQ(0u, memoryManager->closeCalls[0].handleId);
    EXPECT_EQ(1u, memoryManager->closeCalls[1].handleId);
    EXPECT_EQ(2u, memoryManager->closeCalls[2].handleId);
    EXPECT_EQ(0x1000ull, memoryManager->closeCalls[0].handle);
    EXPECT_EQ(0x1001ull, memoryManager->closeCalls[1].handle);
    EXPECT_EQ(0x1002ull, memoryManager->closeCalls[2].handle);
    EXPECT_EQ(&source, memoryManager->closeCalls[0].alloc);
    EXPECT_EQ(&source, memoryManager->closeCalls[1].alloc);
    EXPECT_EQ(&source, memoryManager->closeCalls[2].alloc);
}

TEST_F(PeerAllocationFdTrackingTest, givenMultiHandlePeekFailsMidWayWhenGettingPeerAllocationThenOnlySuccessfullyPeekedHandlesAreClosedAndImportNotCalled) {
    SVMAllocsManager::MapBasedAllocationTracker storage;
    MockMultiHandleGraphicsAllocation source;
    source.numHandles = 4u;
    source.peekFailAfterCalls = 2u;

    SvmAllocationData sourceData(numRootDevices);
    sourceData.gpuAllocations.addAllocation(&source);

    bool importFdsCalled = false;
    PeerAllocationDeps deps{};
    deps.importFd = [](Device *, uint64_t, AllocationType, void *, GraphicsAllocation **,
                       SvmAllocationData &, bool) -> void * { return nullptr; };
    deps.importFds = [&](Device *, const std::vector<osHandle> &, void *,
                         GraphicsAllocation **, SvmAllocationData &, bool) -> void * {
        importFdsCalled = true;
        return reinterpret_cast<void *>(0xBADC0FFEULL);
    };
    deps.decompressP2P = [](GraphicsAllocation *) {};

    void *basePtr = reinterpret_cast<void *>(0x11000);
    uintptr_t peerGpuAddress = 0;
    SvmAllocationData *peerData = nullptr;

    auto result = memoryManager->getOrImportPeerAllocation(device1.get(), svmAllocsManager.get(),
                                                           storage, &sourceData, basePtr,
                                                           &peerGpuAddress, &peerData, false, deps);

    EXPECT_EQ(nullptr, result);
    EXPECT_FALSE(importFdsCalled);
    ASSERT_EQ(2u, memoryManager->closeCalls.size());
    EXPECT_EQ(0u, memoryManager->closeCalls[0].handleId);
    EXPECT_EQ(1u, memoryManager->closeCalls[1].handleId);
    EXPECT_EQ(0x1000ull, memoryManager->closeCalls[0].handle);
    EXPECT_EQ(0x1001ull, memoryManager->closeCalls[1].handle);
}

TEST_F(PeerAllocationFdTrackingTest, givenSingleHandleImportSucceedsWhenGettingPeerAllocationThenPeerSharedHandleIsClearedAndSourceFdIsClosed) {
    SVMAllocsManager::MapBasedAllocationTracker storage;
    MockMultiHandleGraphicsAllocation source;
    source.numHandles = 0u;
    source.internalHandle = 0xDEADBEEFull;

    SvmAllocationData sourceData(numRootDevices);
    sourceData.gpuAllocations.addAllocation(&source);

    MockGraphicsAllocation peerAlloc;
    peerAlloc.gpuAddress = 0xC0FFEE000ull;
    peerAlloc.setSharedHandle(0x42u);

    PeerAllocationDeps deps{};
    deps.importFd = [&peerAlloc](Device *, uint64_t, AllocationType, void *, GraphicsAllocation **pAlloc,
                                 SvmAllocationData &, bool) -> void * {
        if (pAlloc) {
            *pAlloc = &peerAlloc;
        }
        return reinterpret_cast<void *>(peerAlloc.gpuAddress);
    };
    deps.importFds = [](Device *, const std::vector<osHandle> &, void *, GraphicsAllocation **,
                        SvmAllocationData &, bool) -> void * { return nullptr; };
    deps.decompressP2P = [](GraphicsAllocation *) {};

    void *basePtr = reinterpret_cast<void *>(0x12000);
    uintptr_t peerGpuAddress = 0;
    SvmAllocationData *peerData = nullptr;

    auto result = memoryManager->getOrImportPeerAllocation(device1.get(), svmAllocsManager.get(),
                                                           storage, &sourceData, basePtr,
                                                           &peerGpuAddress, &peerData, false, deps);

    EXPECT_EQ(&peerAlloc, result);
    EXPECT_EQ(std::numeric_limits<osHandle>::max(), peerAlloc.peekSharedHandle());
    ASSERT_EQ(1u, memoryManager->closeCalls.size());
    EXPECT_EQ(0xDEADBEEFull, memoryManager->closeCalls[0].handle);
    EXPECT_EQ(0u, memoryManager->closeCalls[0].handleId);
    EXPECT_EQ(&source, memoryManager->closeCalls[0].alloc);
}

TEST_F(PeerAllocationFdTrackingTest, givenSingleHandleImportFailsWhenGettingPeerAllocationThenOriginalAllocSharedHandleIsNotMutated) {
    SVMAllocsManager::MapBasedAllocationTracker storage;
    MockMultiHandleGraphicsAllocation source;
    source.numHandles = 0u;
    source.internalHandle = 0xCAFEBABEull;
    source.setSharedHandle(0x99u);

    SvmAllocationData sourceData(numRootDevices);
    sourceData.gpuAllocations.addAllocation(&source);

    PeerAllocationDeps deps{};
    deps.importFd = [](Device *, uint64_t, AllocationType, void *, GraphicsAllocation **,
                       SvmAllocationData &, bool) -> void * {
        return nullptr;
    };
    deps.importFds = [](Device *, const std::vector<osHandle> &, void *, GraphicsAllocation **,
                        SvmAllocationData &, bool) -> void * { return nullptr; };
    deps.decompressP2P = [](GraphicsAllocation *) {};

    void *basePtr = reinterpret_cast<void *>(0x13000);
    uintptr_t peerGpuAddress = 0;
    SvmAllocationData *peerData = nullptr;

    auto result = memoryManager->getOrImportPeerAllocation(device1.get(), svmAllocsManager.get(),
                                                           storage, &sourceData, basePtr,
                                                           &peerGpuAddress, &peerData, false, deps);

    EXPECT_EQ(nullptr, result);
    EXPECT_EQ(0x99u, source.peekSharedHandle());
    ASSERT_EQ(1u, memoryManager->closeCalls.size());
    EXPECT_EQ(0xCAFEBABEull, memoryManager->closeCalls[0].handle);
}

TEST_F(PeerAllocationTest, givenCachedPeerAllocationWhenDecompressRequestedThenDecompressCalledOnSource) {
    SVMAllocsManager::MapBasedAllocationTracker storage;

    MockGraphicsAllocation source(0u, nullptr, 0u);
    SvmAllocationData sourceData(numRootDevices);
    sourceData.gpuAllocations.addAllocation(&source);

    MockGraphicsAllocation cachedAlloc(1u, nullptr, 0u);
    cachedAlloc.gpuAddress = 0xCAFECAFEull;
    SvmAllocationData cachedData(numRootDevices);
    cachedData.gpuAllocations.addAllocation(&cachedAlloc);

    void *basePtr = reinterpret_cast<void *>(0x8000);
    storage.allocations.insert(std::make_pair(basePtr, cachedData));

    uint32_t importFdCalls = 0, importFdsCalls = 0, decompressCalls = 0;
    GraphicsAllocation *decompressedAlloc = nullptr;
    auto deps = makeCountingDeps(importFdCalls, importFdsCalls, decompressCalls);
    deps.decompressP2P = [&](GraphicsAllocation *alloc) {
        ++decompressCalls;
        decompressedAlloc = alloc;
    };

    uintptr_t peerGpuAddress = 0;
    SvmAllocationData *peerData = nullptr;

    auto result = memoryManager->getOrImportPeerAllocation(device1.get(), svmAllocsManager.get(),
                                                           storage, &sourceData, basePtr,
                                                           &peerGpuAddress, &peerData, true, deps);

    EXPECT_EQ(&cachedAlloc, result);
    EXPECT_EQ(1u, decompressCalls);
    EXPECT_EQ(&source, decompressedAlloc);
    EXPECT_EQ(0u, importFdCalls);
    EXPECT_EQ(0u, importFdsCalls);
}

TEST_F(PeerAllocationTest, givenCachedPeerAllocationWhenDecompressNotRequestedThenDecompressNotCalled) {
    SVMAllocsManager::MapBasedAllocationTracker storage;

    MockGraphicsAllocation source(0u, nullptr, 0u);
    SvmAllocationData sourceData(numRootDevices);
    sourceData.gpuAllocations.addAllocation(&source);

    MockGraphicsAllocation cachedAlloc(1u, nullptr, 0u);
    cachedAlloc.gpuAddress = 0xCAFECAFEull;
    SvmAllocationData cachedData(numRootDevices);
    cachedData.gpuAllocations.addAllocation(&cachedAlloc);

    void *basePtr = reinterpret_cast<void *>(0x9000);
    storage.allocations.insert(std::make_pair(basePtr, cachedData));

    uint32_t importFdCalls = 0, importFdsCalls = 0, decompressCalls = 0;
    auto deps = makeCountingDeps(importFdCalls, importFdsCalls, decompressCalls);

    uintptr_t peerGpuAddress = 0;
    SvmAllocationData *peerData = nullptr;

    auto result = memoryManager->getOrImportPeerAllocation(device1.get(), svmAllocsManager.get(),
                                                           storage, &sourceData, basePtr,
                                                           &peerGpuAddress, &peerData, false, deps);

    EXPECT_EQ(&cachedAlloc, result);
    EXPECT_EQ(0u, decompressCalls);
}

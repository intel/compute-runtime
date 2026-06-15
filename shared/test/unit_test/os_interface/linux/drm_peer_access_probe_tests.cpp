/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/peer_access_probe.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/query_peer_access.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/test.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <cstring>

namespace NEO {
extern std::map<std::string, std::vector<std::string>> directoryFilesMap;
} // namespace NEO

using namespace NEO;

namespace {

class ProbeMockGraphicsAllocation : public MockGraphicsAllocation {
  public:
    using MockGraphicsAllocation::MockGraphicsAllocation;

    int peekInternalHandle(MemoryManager *memoryManager, uint64_t &handle, void *reservedHandleData) override {
        peekCalls++;
        if (peekResult < 0) {
            return peekResult;
        }
        handle = internalHandle;
        return peekResult;
    }

    uint32_t peekCalls = 0u;
    int peekResult = 0;
};

class ProbeMockMemoryManager : public MockMemoryManager {
  public:
    using MockMemoryManager::freeGraphicsMemory;
    using MockMemoryManager::MockMemoryManager;

    GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) override {
        if (properties.flags.shareable == 0u) {
            return MockMemoryManager::allocateGraphicsMemoryWithProperties(properties);
        }
        allocateCalls++;
        lastAllocationShareable = (properties.flags.shareable == 1u);
        if (failAllocate) {
            return nullptr;
        }
        auto allocation = std::make_unique<ProbeMockGraphicsAllocation>(properties.rootDeviceIndex, nullptr, MemoryConstants::pageSize);
        allocation->internalHandle = exportedHandle;
        allocation->peekResult = peekResult;
        lastProbeAllocation = allocation.get();
        mockAllocations.push_back(std::move(allocation));
        return lastProbeAllocation;
    }

    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override {
        importCalls++;
        lastImportedHandle = osHandleData.handle;
        lastImportRootDeviceIndex = properties.rootDeviceIndex;
        if (failImport) {
            return nullptr;
        }
        auto allocation = std::make_unique<ProbeMockGraphicsAllocation>(properties.rootDeviceIndex, nullptr, MemoryConstants::pageSize);
        lastImportedAllocation = allocation.get();
        mockAllocations.push_back(std::move(allocation));
        return lastImportedAllocation;
    }

    void freeGraphicsMemory(GraphicsAllocation *gfxAllocation, bool isImportedAllocation) override {
        if (isMockAllocation(gfxAllocation)) {
            freedAllocations.push_back({gfxAllocation, isImportedAllocation});
            return;
        }
        MockMemoryManager::freeGraphicsMemory(gfxAllocation, isImportedAllocation);
    }

    void closeInternalHandle(uint64_t &handle, uint32_t handleId, GraphicsAllocation *graphicsAllocation) override {
        closeCalls.push_back({handle, handleId, graphicsAllocation});
    }

    bool isMockAllocation(GraphicsAllocation *allocation) const {
        for (auto &mockAllocation : mockAllocations) {
            if (mockAllocation.get() == allocation) {
                return true;
            }
        }
        return false;
    }

    uint32_t countFrees(GraphicsAllocation *allocation) const {
        uint32_t count = 0;
        for (auto &freedAllocation : freedAllocations) {
            if (freedAllocation.allocation == allocation) {
                count++;
            }
        }
        return count;
    }

    struct FreeCall {
        GraphicsAllocation *allocation;
        bool isImportedAllocation;
    };
    struct CloseCall {
        uint64_t handle;
        uint32_t handleId;
        GraphicsAllocation *allocation;
    };

    std::vector<std::unique_ptr<ProbeMockGraphicsAllocation>> mockAllocations;
    std::vector<FreeCall> freedAllocations;
    std::vector<CloseCall> closeCalls;

    ProbeMockGraphicsAllocation *lastProbeAllocation = nullptr;
    ProbeMockGraphicsAllocation *lastImportedAllocation = nullptr;

    uint32_t allocateCalls = 0u;
    uint32_t importCalls = 0u;
    uint64_t lastImportedHandle = 0u;
    uint32_t lastImportRootDeviceIndex = std::numeric_limits<uint32_t>::max();
    uint64_t exportedHandle = 0x42u;
    int peekResult = 0;
    bool failAllocate = false;
    bool failImport = false;
    bool lastAllocationShareable = false;
};

class ConfigurableMakeResidentMemoryOps : public MockMemoryOperations {
  public:
    MemoryOperationsStatus makeResident(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations,
                                        bool isDummyExecNeeded, const bool forcePagingFence) override {
        MockMemoryOperations::makeResident(device, gfxAllocations, isDummyExecNeeded, forcePagingFence);
        lastDeviceArg = device;
        lastAllocs.assign(gfxAllocations.begin(), gfxAllocations.end());
        return makeResidentResult;
    }

    MemoryOperationsStatus makeResidentResult = MemoryOperationsStatus::success;
    Device *lastDeviceArg = nullptr;
    std::vector<GraphicsAllocation *> lastAllocs;
};

struct PeerAccessProbeTest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = new MockExecutionEnvironment(defaultHwInfo.get(), false, numRootDevices);
        executionEnvironment->memoryManager.reset(new ProbeMockMemoryManager(*executionEnvironment));
        memoryManager = static_cast<ProbeMockMemoryManager *>(executionEnvironment->memoryManager.get());
        for (uint32_t i = 0; i < numRootDevices; ++i) {
            executionEnvironment->rootDeviceEnvironments[i]->osInterface.reset(new OSInterface());
            auto drm = new DrmMock{*executionEnvironment->rootDeviceEnvironments[i]};
            executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<DriverModel>{drm});
            auto memoryOperations = std::make_unique<ConfigurableMakeResidentMemoryOps>();
            memoryOperationsHandlers[i] = memoryOperations.get();
            executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface = std::move(memoryOperations);
        }

        device0.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0));
        device1.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 1));
    }

    static constexpr uint32_t numRootDevices = 2u;
    MockExecutionEnvironment *executionEnvironment = nullptr;
    ProbeMockMemoryManager *memoryManager = nullptr;
    ConfigurableMakeResidentMemoryOps *memoryOperationsHandlers[numRootDevices] = {};
    std::unique_ptr<MockDevice> device0;
    std::unique_ptr<MockDevice> device1;
};

} // namespace

TEST_F(PeerAccessProbeTest, givenNoFabricPathAndSuccessfulDmaBufProbeWhenQueryingPeerAccessThenReturnsTrueAndImportedAllocationFreed) {
    GraphicsAllocation *probeAllocation = nullptr;
    uint64_t handle = std::numeric_limits<uint64_t>::max();
    EXPECT_TRUE(queryPeerAccessDrm(*device0, *device1, &probeAllocation, &handle));

    EXPECT_EQ(1u, memoryManager->allocateCalls);
    EXPECT_TRUE(memoryManager->lastAllocationShareable);
    EXPECT_EQ(memoryManager->lastProbeAllocation, probeAllocation);
    EXPECT_EQ(memoryManager->exportedHandle, handle);

    EXPECT_EQ(1u, memoryManager->importCalls);
    EXPECT_EQ(memoryManager->exportedHandle, memoryManager->lastImportedHandle);
    EXPECT_EQ(device1->getRootDeviceIndex(), memoryManager->lastImportRootDeviceIndex);

    ASSERT_EQ(1u, memoryManager->freedAllocations.size());
    EXPECT_EQ(memoryManager->lastImportedAllocation, memoryManager->freedAllocations[0].allocation);
    EXPECT_TRUE(memoryManager->freedAllocations[0].isImportedAllocation);
    EXPECT_EQ(0u, memoryManager->countFrees(probeAllocation));
}

TEST_F(PeerAccessProbeTest, givenAllocateFailsWhenQueryingPeerAccessThenReturnsFalseWithoutImport) {
    memoryManager->failAllocate = true;

    GraphicsAllocation *probeAllocation = nullptr;
    uint64_t handle = std::numeric_limits<uint64_t>::max();
    EXPECT_FALSE(queryPeerAccessDrm(*device0, *device1, &probeAllocation, &handle));

    EXPECT_EQ(1u, memoryManager->allocateCalls);
    EXPECT_EQ(0u, memoryManager->importCalls);
    EXPECT_EQ(nullptr, probeAllocation);
}

TEST_F(PeerAccessProbeTest, givenPeekInternalHandleFailsWhenQueryingPeerAccessThenReturnsFalseAndProbeAllocationIsReleased) {
    memoryManager->peekResult = -1;

    GraphicsAllocation *probeAllocation = nullptr;
    uint64_t handle = std::numeric_limits<uint64_t>::max();
    EXPECT_FALSE(queryPeerAccessDrm(*device0, *device1, &probeAllocation, &handle));

    EXPECT_EQ(1u, memoryManager->allocateCalls);
    EXPECT_EQ(0u, memoryManager->importCalls);
    EXPECT_EQ(nullptr, probeAllocation);
    EXPECT_EQ(std::numeric_limits<uint64_t>::max(), handle);
    ASSERT_EQ(1u, memoryManager->freedAllocations.size());
    EXPECT_EQ(memoryManager->lastProbeAllocation, memoryManager->freedAllocations[0].allocation);
    EXPECT_FALSE(memoryManager->freedAllocations[0].isImportedAllocation);
}

TEST_F(PeerAccessProbeTest, givenPeekInternalHandleFailedOnceWhenQueryingPeerAccessAgainThenProbeAllocationIsReallocatedAndProbeSucceeds) {
    memoryManager->peekResult = -1;

    GraphicsAllocation *probeAllocation = nullptr;
    uint64_t handle = std::numeric_limits<uint64_t>::max();
    EXPECT_FALSE(queryPeerAccessDrm(*device0, *device1, &probeAllocation, &handle));
    EXPECT_EQ(nullptr, probeAllocation);

    memoryManager->peekResult = 0;
    EXPECT_TRUE(queryPeerAccessDrm(*device0, *device1, &probeAllocation, &handle));

    EXPECT_EQ(2u, memoryManager->allocateCalls);
    EXPECT_EQ(memoryManager->lastProbeAllocation, probeAllocation);
    EXPECT_EQ(memoryManager->exportedHandle, handle);
}

TEST_F(PeerAccessProbeTest, givenImportFailsWhenQueryingPeerAccessThenReturnsFalseAndProbeAllocationIsKeptForCaller) {
    memoryManager->failImport = true;

    GraphicsAllocation *probeAllocation = nullptr;
    uint64_t handle = std::numeric_limits<uint64_t>::max();
    EXPECT_FALSE(queryPeerAccessDrm(*device0, *device1, &probeAllocation, &handle));

    EXPECT_EQ(1u, memoryManager->allocateCalls);
    EXPECT_EQ(1u, memoryManager->importCalls);
    EXPECT_EQ(memoryManager->lastProbeAllocation, probeAllocation);
    EXPECT_EQ(0u, memoryManager->freedAllocations.size());
}

TEST_F(PeerAccessProbeTest, givenCachedProbeAllocationWhenQueryingPeerAccessThenAllocateIsSkipped) {
    ProbeMockGraphicsAllocation cachedAllocation(0u, nullptr, MemoryConstants::pageSize);
    GraphicsAllocation *probeAllocation = &cachedAllocation;
    uint64_t handle = 99u;

    EXPECT_TRUE(queryPeerAccessDrm(*device0, *device1, &probeAllocation, &handle));

    EXPECT_EQ(0u, memoryManager->allocateCalls);
    EXPECT_EQ(0u, cachedAllocation.peekCalls);
    EXPECT_EQ(1u, memoryManager->importCalls);
    EXPECT_EQ(99u, memoryManager->lastImportedHandle);
    EXPECT_EQ(&cachedAllocation, probeAllocation);
    EXPECT_EQ(99u, handle);
}

TEST_F(PeerAccessProbeTest, givenMakeResidentReturnsSuccessWhenQueryingPeerAccessThenReturnsTrueAndMakeResidentCalledWithPeerDeviceAndImportedAlloc) {
    auto peerOps = memoryOperationsHandlers[device1->getRootDeviceIndex()];
    peerOps->makeResidentResult = MemoryOperationsStatus::success;

    GraphicsAllocation *probeAllocation = nullptr;
    uint64_t handle = std::numeric_limits<uint64_t>::max();
    EXPECT_TRUE(queryPeerAccessDrm(*device0, *device1, &probeAllocation, &handle));

    EXPECT_EQ(1, peerOps->makeResidentCalledCount.load());
    EXPECT_EQ(device1.get(), peerOps->lastDeviceArg);
    ASSERT_EQ(1u, peerOps->lastAllocs.size());
    EXPECT_EQ(memoryManager->lastImportedAllocation, peerOps->lastAllocs[0]);
    EXPECT_EQ(1u, memoryManager->countFrees(memoryManager->lastImportedAllocation));
}

TEST_F(PeerAccessProbeTest, givenMakeResidentReturnsFailedWhenQueryingPeerAccessThenReturnsFalseAndImportedAllocationFreedExactlyOnce) {
    auto peerOps = memoryOperationsHandlers[device1->getRootDeviceIndex()];
    peerOps->makeResidentResult = MemoryOperationsStatus::failed;

    GraphicsAllocation *probeAllocation = nullptr;
    uint64_t handle = std::numeric_limits<uint64_t>::max();
    EXPECT_FALSE(queryPeerAccessDrm(*device0, *device1, &probeAllocation, &handle));

    EXPECT_EQ(1, peerOps->makeResidentCalledCount.load());
    ASSERT_EQ(1u, memoryManager->freedAllocations.size());
    EXPECT_EQ(memoryManager->lastImportedAllocation, memoryManager->freedAllocations[0].allocation);
    EXPECT_TRUE(memoryManager->freedAllocations[0].isImportedAllocation);
}

TEST_F(PeerAccessProbeTest, givenMakeResidentReturnsDeviceUninitializedWhenQueryingPeerAccessThenReturnsFalse) {
    auto peerOps = memoryOperationsHandlers[device1->getRootDeviceIndex()];
    peerOps->makeResidentResult = MemoryOperationsStatus::deviceUninitialized;

    GraphicsAllocation *probeAllocation = nullptr;
    uint64_t handle = std::numeric_limits<uint64_t>::max();
    EXPECT_FALSE(queryPeerAccessDrm(*device0, *device1, &probeAllocation, &handle));

    EXPECT_EQ(1, peerOps->makeResidentCalledCount.load());
    EXPECT_EQ(1u, memoryManager->countFrees(memoryManager->lastImportedAllocation));
}

TEST_F(PeerAccessProbeTest, givenPeerMemoryOperationsInterfaceIsNullWhenQueryingPeerAccessThenReturnsTrueAndMakeResidentNotCalled) {
    executionEnvironment->rootDeviceEnvironments[device1->getRootDeviceIndex()]->memoryOperationsInterface.reset();
    memoryOperationsHandlers[device1->getRootDeviceIndex()] = nullptr;

    GraphicsAllocation *probeAllocation = nullptr;
    uint64_t handle = std::numeric_limits<uint64_t>::max();
    EXPECT_TRUE(queryPeerAccessDrm(*device0, *device1, &probeAllocation, &handle));

    EXPECT_EQ(1u, memoryManager->countFrees(memoryManager->lastImportedAllocation));
}

TEST_F(PeerAccessProbeTest, givenNoOsInterfaceWhenQueryingPeerAccessThenReturnsFalseWithoutProbing) {
    auto &osInterfacePtr = executionEnvironment->rootDeviceEnvironments[device0->getRootDeviceIndex()]->osInterface;
    auto releasedOsInterface = osInterfacePtr.release();

    GraphicsAllocation *probeAllocation = nullptr;
    uint64_t handle = std::numeric_limits<uint64_t>::max();
    EXPECT_FALSE(NEO::queryPeerAccess(*device0, *device1, &probeAllocation, &handle));

    EXPECT_EQ(0u, memoryManager->allocateCalls);
    EXPECT_EQ(0u, memoryManager->importCalls);

    osInterfacePtr.reset(releasedOsInterface);
}

TEST_F(PeerAccessProbeTest, givenNonDrmDriverModelWhenQueryingPeerAccessThenReturnsFalseWithoutProbing) {
    auto drm = static_cast<DrmMock *>(executionEnvironment->rootDeviceEnvironments[device0->getRootDeviceIndex()]->osInterface->getDriverModel());
    drm->getDriverModelTypeCallBase = false;
    drm->getDriverModelTypeResult = DriverModelType::unknown;

    GraphicsAllocation *probeAllocation = nullptr;
    uint64_t handle = std::numeric_limits<uint64_t>::max();
    EXPECT_FALSE(NEO::queryPeerAccess(*device0, *device1, &probeAllocation, &handle));

    EXPECT_EQ(0u, memoryManager->allocateCalls);
    EXPECT_EQ(0u, memoryManager->importCalls);
}

TEST_F(PeerAccessProbeTest, givenPeerDeviceWithoutOsInterfaceWhenQueryingPeerAccessThenReturnsFalseWithoutProbing) {
    auto &osInterfacePtr = executionEnvironment->rootDeviceEnvironments[device1->getRootDeviceIndex()]->osInterface;
    auto releasedOsInterface = osInterfacePtr.release();

    GraphicsAllocation *probeAllocation = nullptr;
    uint64_t handle = std::numeric_limits<uint64_t>::max();
    EXPECT_FALSE(queryPeerAccessDrm(*device0, *device1, &probeAllocation, &handle));

    EXPECT_EQ(0u, memoryManager->allocateCalls);
    EXPECT_EQ(0u, memoryManager->importCalls);
    EXPECT_EQ(nullptr, probeAllocation);

    osInterfacePtr.reset(releasedOsInterface);
}

TEST_F(PeerAccessProbeTest, givenPeerDeviceWithNonDrmDriverModelWhenQueryingPeerAccessThenReturnsFalseWithoutProbing) {
    auto drm = static_cast<DrmMock *>(executionEnvironment->rootDeviceEnvironments[device1->getRootDeviceIndex()]->osInterface->getDriverModel());
    drm->getDriverModelTypeCallBase = false;
    drm->getDriverModelTypeResult = DriverModelType::unknown;

    GraphicsAllocation *probeAllocation = nullptr;
    uint64_t handle = std::numeric_limits<uint64_t>::max();
    EXPECT_FALSE(queryPeerAccessDrm(*device0, *device1, &probeAllocation, &handle));

    EXPECT_EQ(0u, memoryManager->allocateCalls);
    EXPECT_EQ(0u, memoryManager->importCalls);
}

TEST_F(PeerAccessProbeTest, givenPeerDeviceWithoutOsInterfaceWhenQueryingFabricStatsThenReturnsFalse) {
    auto &osInterfacePtr = executionEnvironment->rootDeviceEnvironments[device1->getRootDeviceIndex()]->osInterface;
    auto releasedOsInterface = osInterfacePtr.release();

    uint32_t latency = 0;
    uint32_t bandwidth = 0;
    EXPECT_FALSE(queryFabricStatsDrm(*device0, *device1, latency, bandwidth));

    osInterfacePtr.reset(releasedOsInterface);
}

TEST_F(PeerAccessProbeTest, givenDrmDriverModelWhenQueryingPeerAccessThenProbeRuns) {
    GraphicsAllocation *probeAllocation = nullptr;
    uint64_t handle = std::numeric_limits<uint64_t>::max();
    EXPECT_TRUE(NEO::queryPeerAccess(*device0, *device1, &probeAllocation, &handle));

    EXPECT_EQ(1u, memoryManager->allocateCalls);
    EXPECT_EQ(1u, memoryManager->importCalls);
    EXPECT_EQ(memoryManager->lastProbeAllocation, probeAllocation);
}

TEST_F(PeerAccessProbeTest, givenTwoRootDevicesWhenCanAccessPeerIsCalledThenReturnsTrueAndProbeAllocationFreedWithExportedFdClosed) {
    EXPECT_TRUE(device0->canAccessPeer(device1.get()));

    EXPECT_EQ(1u, memoryManager->allocateCalls);
    auto probeAllocation = memoryManager->lastProbeAllocation;
    EXPECT_EQ(1u, probeAllocation->peekCalls);

    ASSERT_EQ(1u, memoryManager->closeCalls.size());
    EXPECT_EQ(memoryManager->exportedHandle, memoryManager->closeCalls[0].handle);
    EXPECT_EQ(0u, memoryManager->closeCalls[0].handleId);
    EXPECT_EQ(probeAllocation, memoryManager->closeCalls[0].allocation);

    EXPECT_EQ(1u, memoryManager->countFrees(probeAllocation));
    EXPECT_EQ(1u, memoryManager->countFrees(memoryManager->lastImportedAllocation));
}

TEST_F(PeerAccessProbeTest, givenCanAccessPeerCalledTwiceThenReturnsTheSameValueAndProbeRunsOnlyOnce) {
    bool firstAccess = device0->canAccessPeer(device1.get());
    bool secondAccess = device0->canAccessPeer(device1.get());

    EXPECT_EQ(firstAccess, secondAccess);
    EXPECT_EQ(1u, memoryManager->allocateCalls);
    EXPECT_EQ(1u, memoryManager->importCalls);
    EXPECT_EQ(1u, memoryManager->closeCalls.size());
}

TEST_F(PeerAccessProbeTest, givenPeekInternalHandleFailsWhenCanAccessPeerIsCalledThenReturnsFalseAndNoFdIsClosedButProbeAllocationFreed) {
    memoryManager->peekResult = -1;

    EXPECT_FALSE(device0->canAccessPeer(device1.get()));

    EXPECT_EQ(1u, memoryManager->allocateCalls);
    EXPECT_EQ(0u, memoryManager->closeCalls.size());
    EXPECT_EQ(1u, memoryManager->countFrees(memoryManager->lastProbeAllocation));
}

TEST_F(PeerAccessProbeTest, givenImportFailsWhenCanAccessPeerIsCalledThenReturnsFalseAndExportedFdIsClosedAndProbeAllocationFreed) {
    memoryManager->failImport = true;

    EXPECT_FALSE(device0->canAccessPeer(device1.get()));

    ASSERT_EQ(1u, memoryManager->closeCalls.size());
    EXPECT_EQ(memoryManager->exportedHandle, memoryManager->closeCalls[0].handle);
    EXPECT_EQ(1u, memoryManager->countFrees(memoryManager->lastProbeAllocation));
}

TEST_F(PeerAccessProbeTest, givenNoIafFabricDirectoryWhenQueryingFabricStatsThenReturnsFalse) {
    uint32_t latency = 0;
    uint32_t bandwidth = 0;
    EXPECT_FALSE(queryFabricStatsDrm(*device0, *device1, latency, bandwidth));
}

namespace {

class MockIoctlHelperIafTest : public IoctlHelperPrelim20 {
  public:
    using IoctlHelperPrelim20::IoctlHelperPrelim20;
    bool getFabricLatency(uint32_t fabricId, uint32_t &latency, uint32_t &bandwidth) override {
        latency = 1;
        bandwidth = 10;
        return true;
    }
};

class MockIoctlHelperIafFailing : public IoctlHelperPrelim20 {
  public:
    using IoctlHelperPrelim20::IoctlHelperPrelim20;
    bool getFabricLatency(uint32_t, uint32_t &, uint32_t &) override {
        return false;
    }
};

ssize_t mockPreadReturnValue = -1;
const char *mockPreadFabricIdContent = nullptr;

struct PeerAccessProbeFabricStatsTest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = new MockExecutionEnvironment(defaultHwInfo.get(), false, 1u);
        executionEnvironment->memoryManager.reset(new ProbeMockMemoryManager(*executionEnvironment));
        memoryManager = static_cast<ProbeMockMemoryManager *>(executionEnvironment->memoryManager.get());
        executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new OSInterface());
        drmMock = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
        drmMock->mockSysFsPciPath = "/sys/mock/card1";
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>{drmMock});
        device0.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0));

        directoryFilesMap[mockDevicePath] = {mockDevicePath + "/i915.iaf.0"};
    }

    void TearDown() override {
        directoryFilesMap.clear();
    }

    const std::string mockDevicePath = "/sys/mock/card1/device";
    MockExecutionEnvironment *executionEnvironment = nullptr;
    ProbeMockMemoryManager *memoryManager = nullptr;
    DrmMockResources *drmMock = nullptr;
    std::unique_ptr<MockDevice> device0;

    VariableBackup<ssize_t> backupPreadReturnValue{&mockPreadReturnValue, -1};
    VariableBackup<const char *> backupPreadContent{&mockPreadFabricIdContent, nullptr};
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen{&SysCalls::sysCallsOpen,
                                                              [](const char *, int) -> int { return 4; }};
    VariableBackup<decltype(SysCalls::sysCallsPread)> mockPread{&SysCalls::sysCallsPread,
                                                                [](int, void *buf, size_t bufSize, off_t) -> ssize_t {
                                                                    if (mockPreadFabricIdContent == nullptr) {
                                                                        return mockPreadReturnValue;
                                                                    }
                                                                    auto bytesToCopy = std::min(strlen(mockPreadFabricIdContent), bufSize);
                                                                    memcpy(buf, mockPreadFabricIdContent, bytesToCopy);
                                                                    return static_cast<ssize_t>(bytesToCopy);
                                                                }};
    VariableBackup<decltype(SysCalls::sysCallsClose)> mockClose{&SysCalls::sysCallsClose,
                                                                [](int) -> int { return 0; }};
};

} // namespace

TEST_F(PeerAccessProbeFabricStatsTest, givenFabricIdFileWhenPreadFailsThenQueryFabricStatsDrmReturnsFalse) {
    mockPreadReturnValue = -1;

    uint32_t latency = 0, bandwidth = 0;
    EXPECT_FALSE(queryFabricStatsDrm(*device0, *device0, latency, bandwidth));
}

TEST_F(PeerAccessProbeFabricStatsTest, givenFabricIdFileWhenPreadReturnsZeroBytesThenQueryFabricStatsDrmReturnsFalse) {
    mockPreadReturnValue = 0;

    uint32_t latency = 0, bandwidth = 0;
    EXPECT_FALSE(queryFabricStatsDrm(*device0, *device0, latency, bandwidth));
}

TEST_F(PeerAccessProbeFabricStatsTest, givenFabricIdFileWithMalformedContentWhenQueryFabricStatsDrmIsCalledThenItReturnsFalse) {
    mockPreadFabricIdContent = "not_hex";

    uint32_t latency = 0, bandwidth = 0;
    EXPECT_FALSE(queryFabricStatsDrm(*device0, *device0, latency, bandwidth));
}

TEST_F(PeerAccessProbeFabricStatsTest, givenFabricIdFileWithOutOfRangeContentWhenQueryFabricStatsDrmIsCalledThenItReturnsFalse) {
    mockPreadFabricIdContent = "0xffffffffffffffffffff";

    uint32_t latency = 0, bandwidth = 0;
    EXPECT_FALSE(queryFabricStatsDrm(*device0, *device0, latency, bandwidth));
}

TEST_F(PeerAccessProbeFabricStatsTest, givenFabricIdFileWhenPreadSucceedsThenQueryFabricStatsDrmCallsFabricLatency) {
    drmMock->ioctlHelper = std::make_unique<MockIoctlHelperIafTest>(*drmMock);
    mockPreadFabricIdContent = "0x00000001";

    uint32_t latency = 0, bandwidth = 0;
    EXPECT_TRUE(queryFabricStatsDrm(*device0, *device0, latency, bandwidth));
    EXPECT_EQ(1u, latency);
    EXPECT_EQ(10u, bandwidth);
}

TEST_F(PeerAccessProbeFabricStatsTest, givenFabricIdFileWhenGetFabricLatencyFailsThenQueryFabricStatsDrmReturnsFalse) {
    drmMock->ioctlHelper = std::make_unique<MockIoctlHelperIafFailing>(*drmMock);
    mockPreadFabricIdContent = "0x00000001";

    uint32_t latency = 0, bandwidth = 0;
    EXPECT_FALSE(queryFabricStatsDrm(*device0, *device0, latency, bandwidth));
}

TEST_F(PeerAccessProbeFabricStatsTest, givenLegacyIafDirectoryAmongNonMatchingEntriesWhenQueryFabricStatsDrmIsCalledThenLegacyEntryIsRecognized) {
    directoryFilesMap[mockDevicePath] = {mockDevicePath + "/power",
                                         mockDevicePath + "/iaf.0"};
    drmMock->ioctlHelper = std::make_unique<MockIoctlHelperIafTest>(*drmMock);
    mockPreadFabricIdContent = "0x00000002";

    uint32_t latency = 0, bandwidth = 0;
    EXPECT_TRUE(queryFabricStatsDrm(*device0, *device0, latency, bandwidth));
    EXPECT_EQ(1u, latency);
    EXPECT_EQ(10u, bandwidth);
}

TEST_F(PeerAccessProbeFabricStatsTest, givenSuccessfulFabricStatsWhenQueryingPeerAccessThenReturnsTrueWithoutDmaBufFallback) {
    drmMock->ioctlHelper = std::make_unique<MockIoctlHelperIafTest>(*drmMock);
    mockPreadFabricIdContent = "0x00000001";

    GraphicsAllocation *probeAllocation = nullptr;
    uint64_t handle = std::numeric_limits<uint64_t>::max();
    EXPECT_TRUE(queryPeerAccessDrm(*device0, *device0, &probeAllocation, &handle));

    EXPECT_EQ(0u, memoryManager->allocateCalls);
    EXPECT_EQ(0u, memoryManager->importCalls);
    EXPECT_EQ(nullptr, probeAllocation);
}

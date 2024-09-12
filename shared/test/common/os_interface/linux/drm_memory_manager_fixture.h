/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/test/common/fixtures/memory_management_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_sip.h"
#include "shared/test/common/os_interface/linux/device_command_stream_fixture.h"

#include <memory>

namespace NEO {
class MockDevice;
class TestedDrmMemoryManager;
struct UltHwConfig;

class DrmMemoryManagerBasic : public ::testing::Test {
  public:
    DrmMemoryManagerBasic() : executionEnvironment(defaultHwInfo.get(), false, numRootDevices){};
    void SetUp() override;

    const uint32_t rootDeviceIndex = 1u;
    const uint32_t numRootDevices = 2u;
    MockExecutionEnvironment executionEnvironment;
};

class DrmMemoryManagerFixture : public MemoryManagementFixture {
  public:
    DrmMockCustom *mock = nullptr;
    bool dontTestIoctlInTearDown = false;
    const uint32_t rootDeviceIndex = 1u;
    const uint32_t numRootDevices = 2u;
    TestedDrmMemoryManager *memoryManager = nullptr;
    MockDevice *device = nullptr;

    void setUp();
    void setUp(DrmMockCustom *mock, bool localMemoryEnabled);
    void tearDown();

  protected:
    VariableBackup<bool> mockSipBackup{&MockSipData::useMockSip, false};
    ExecutionEnvironment *executionEnvironment = nullptr;
    RootDeviceEnvironment *rootDeviceEnvironment = nullptr;
    DrmMockCustom::IoctlResExt ioctlResExt = {0, 0};
    AllocationData allocationData{};
    Ioctls additionalDestroyDeviceIoctls{};
    EnvironmentWithCsrWrapper environmentWrapper;
    DebugManagerStateRestore restore;
};

class DrmMemoryManagerWithLocalMemoryFixture : public DrmMemoryManagerFixture {
  public:
    void setUp();
    void tearDown();
    std::unique_ptr<VariableBackup<UltHwConfig>> backup;
};

struct MockedMemoryInfo : public NEO::MemoryInfo {
    using NEO::MemoryInfo::MemoryInfo;
    ~MockedMemoryInfo() override = default;

    size_t getMemoryRegionSize(uint32_t memoryBank) const override {
        return 1024u;
    }
    int createGemExt(const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle, uint64_t patIndex, std::optional<uint32_t> vmId, int32_t pairHandle, bool isChunked, uint32_t numOfChunks, bool isUSMHostAllocation) override {
        if (allocSize == 0) {
            return EINVAL;
        }
        handle = 1u;
        return 0;
    }
    int createGemExtWithSingleRegion(DeviceBitfield memoryBanks, size_t allocSize, uint32_t &handle, uint64_t patIndex, int32_t pairHandle, bool isUSMHostAllocation) override {
        if (allocSize == 0) {
            return EINVAL;
        }
        handle = 1u;
        pairHandlePassed = pairHandle;
        return 0;
    }
    int createGemExtWithMultipleRegions(DeviceBitfield memoryBanks, size_t allocSize, uint32_t &handle, uint64_t patIndex, bool isUSMHostAllocation) override {
        if (allocSize == 0) {
            return EINVAL;
        }
        handle = 1u;
        banks = static_cast<uint32_t>(memoryBanks.to_ulong());
        return 0;
    }
    int createGemExtWithMultipleRegions(DeviceBitfield memoryBanks, size_t allocSize, uint32_t &handle, uint64_t patIndex, int32_t pairHandle, bool isChunked, uint32_t numOfChunks, bool isUSMHostAllocation) override {
        if (allocSize == 0) {
            return EINVAL;
        }
        if (failOnCreateGemExtWithMultipleRegions == true) {
            return -1;
        }
        handle = 1u;
        banks = static_cast<uint32_t>(memoryBanks.to_ulong());
        isChunkedUsed = isChunked;
        return 0;
    }

    uint32_t banks = 0;
    int32_t pairHandlePassed = -1;
    bool isChunkedUsed = false;
    bool failOnCreateGemExtWithMultipleRegions = false;
};

class DrmMemoryManagerFixtureWithoutQuietIoctlExpectation {
  public:
    DrmMemoryManagerFixtureWithoutQuietIoctlExpectation();
    DrmMemoryManagerFixtureWithoutQuietIoctlExpectation(uint32_t numRootDevices, uint32_t rootIndex);
    TestedDrmMemoryManager *memoryManager = nullptr;
    DrmMockCustom *mock;

    void setUp();
    void setUp(bool enableLocalMem);
    void tearDown();

  protected:
    ExecutionEnvironment *executionEnvironment = nullptr;
    std::unique_ptr<MockDevice> device;
    DrmMockCustom::IoctlResExt ioctlResExt = {0, 0};
    DebugManagerStateRestore restore;
    const uint32_t rootDeviceIndex = 1u;
    const uint32_t numRootDevices = 3u;
};

class DrmMemoryManagerFixtureWithLocalMemoryAndWithoutQuietIoctlExpectation : public DrmMemoryManagerFixtureWithoutQuietIoctlExpectation {
  public:
    void setUp();
    void tearDown();
};
} // namespace NEO

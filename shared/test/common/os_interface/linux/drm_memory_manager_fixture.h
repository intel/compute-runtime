/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/fixtures/memory_management_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/device_command_stream_fixture.h"

#include <memory>

namespace NEO {
class MockDevice;
class TestedDrmMemoryManager;
struct UltHwConfig;

extern std::vector<void *> mmapVector;

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

    void SetUp() override;
    void SetUp(DrmMockCustom *mock, bool localMemoryEnabled);
    void TearDown() override;

  protected:
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
    void SetUp() override;
    void TearDown() override;
    std::unique_ptr<VariableBackup<UltHwConfig>> backup;
};

struct MockedMemoryInfo : public NEO::MemoryInfo {
    MockedMemoryInfo(const std::vector<MemoryRegion> &regionInfo) : MemoryInfo(regionInfo) {}
    ~MockedMemoryInfo() override{};

    size_t getMemoryRegionSize(uint32_t memoryBank) override {
        return 1024u;
    }
    uint32_t createGemExt(Drm *drm, const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle, std::optional<uint32_t> vmId) override {
        if (allocSize == 0) {
            return EINVAL;
        }
        handle = 1u;
        return 0u;
    }
    uint32_t createGemExtWithSingleRegion(Drm *drm, uint32_t memoryBanks, size_t allocSize, uint32_t &handle) override {
        if (allocSize == 0) {
            return EINVAL;
        }
        handle = 1u;
        return 0u;
    }
    uint32_t createGemExtWithMultipleRegions(Drm *drm, uint32_t memoryBanks, size_t allocSize, uint32_t &handle) override {
        if (allocSize == 0) {
            return EINVAL;
        }
        handle = 1u;
        banks = memoryBanks;
        return 0u;
    }
    uint32_t banks = 0;
};

class DrmMemoryManagerFixtureWithoutQuietIoctlExpectation {
  public:
    std::unique_ptr<TestedDrmMemoryManager> memoryManager;
    DrmMockCustom *mock;

    void SetUp();                    // NOLINT(readability-identifier-naming)
    void SetUp(bool enableLocalMem); // NOLINT(readability-identifier-naming)
    void TearDown();                 // NOLINT(readability-identifier-naming)

  protected:
    ExecutionEnvironment *executionEnvironment = nullptr;
    std::unique_ptr<MockDevice> device;
    DrmMockCustom::IoctlResExt ioctlResExt = {0, 0};
    DebugManagerStateRestore restore;
    const uint32_t rootDeviceIndex = 1u;
    const uint32_t numRootDevices = 2u;
};

class DrmMemoryManagerFixtureWithLocalMemoryAndWithoutQuietIoctlExpectation : public DrmMemoryManagerFixtureWithoutQuietIoctlExpectation {
  public:
    void SetUp();
    void TearDown();
};
} // namespace NEO

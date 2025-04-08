/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/test/common/fixtures/memory_management_fixture.h"
#include "shared/test/common/mocks/linux/mock_drm_command_stream_receiver.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/os_interface/linux/drm_mock_memory_info.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "hw_cmds_default.h"

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

    template <typename GfxFamily>
    void setUpT() {
        MemoryManagementFixture::setUp();

        executionEnvironment = MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), numRootDevices - 1);
        setUpT<GfxFamily>(DrmMockCustom::create(*executionEnvironment->rootDeviceEnvironments[0]).release(), false);
    } // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    template <typename GfxFamily>
    void setUpT(DrmMockCustom *mock, bool localMemoryEnabled) {
        ASSERT_NE(nullptr, executionEnvironment);
        executionEnvironment->incRefInternal();
        debugManager.flags.DeferOsContextInitialization.set(0);
        debugManager.flags.SetAmountOfReusableAllocations.set(0);

        environmentWrapper.setCsrType<TestedDrmCommandStreamReceiver<GfxFamily>>();
        allocationData.rootDeviceIndex = rootDeviceIndex;
        this->mock = mock;
        for (auto i = 0u; i < numRootDevices; i++) {
            auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[i].get();
            rootDeviceEnvironment->setHwInfoAndInitHelpers(defaultHwInfo.get());
            UnitTestSetter::setCcsExposure(*executionEnvironment->rootDeviceEnvironments[i]);
            UnitTestSetter::setRcsExposure(*executionEnvironment->rootDeviceEnvironments[i]);

            rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
            rootDeviceEnvironment->osInterface->setDriverModel(DrmMockCustom::create(*rootDeviceEnvironment));
            rootDeviceEnvironment->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*rootDeviceEnvironment->osInterface->getDriverModel()->as<Drm>(), i, false);
            MockRootDeviceEnvironment::resetBuiltins(rootDeviceEnvironment, new MockBuiltins);
            rootDeviceEnvironment->initGmm();
        }

        executionEnvironment->calculateMaxOsContextCount();
        rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex].get();
        rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));

        memoryManager = new (std::nothrow) TestedDrmMemoryManager(localMemoryEnabled, false, false, *executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);
        // assert we have memory manager
        ASSERT_NE(nullptr, memoryManager);
        if (memoryManager->getgemCloseWorker()) {
            memoryManager->getgemCloseWorker()->close(true);
        }
        device = MockDevice::create<MockDevice>(executionEnvironment, rootDeviceIndex);
        mock->reset();
    }

    template <typename GfxFamily>
    void tearDownT() {
        mock->testIoctls();
        mock->reset();

        int enginesCount = 0;

        for (auto &engineContainer : memoryManager->getRegisteredEngines()) {
            enginesCount += engineContainer.size();
        }

        for (auto &engineContainer : memoryManager->secondaryEngines) {
            enginesCount -= engineContainer.size();
        }

        mock->ioctlExpected.contextDestroy = enginesCount;
        mock->ioctlExpected.gemClose = enginesCount;
        mock->ioctlExpected.gemWait = enginesCount;

        auto csr = static_cast<TestedDrmCommandStreamReceiver<GfxFamily> *>(device->getDefaultEngine().commandStreamReceiver);
        if (csr->globalFenceAllocation) {
            mock->ioctlExpected.gemClose += enginesCount;
            mock->ioctlExpected.gemWait += enginesCount;
        }
        if (csr->getPreemptionAllocation()) {
            mock->ioctlExpected.gemClose += enginesCount;
            mock->ioctlExpected.gemWait += enginesCount;
        }

        auto &compilerProductHelper = device->getCompilerProductHelper();
        auto isHeapless = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);
        auto isHeaplessStateInit = compilerProductHelper.isHeaplessStateInitEnabled(isHeapless);
        if (isHeaplessStateInit) {
            if (device->getRTMemoryBackedBuffer() != nullptr) {
                mock->ioctlExpected.gemClose += 1;
                mock->ioctlExpected.gemWait += 1;
            }
            mock->ioctlExpected.gemClose += 1;
            mock->ioctlExpected.gemWait += 1;
        }

        mock->ioctlExpected.gemWait += additionalDestroyDeviceIoctls.gemWait.load();
        mock->ioctlExpected.gemClose += additionalDestroyDeviceIoctls.gemClose.load();
        delete device;
        if (dontTestIoctlInTearDown) {
            mock->reset();
        }
        mock->testIoctls();
        executionEnvironment->decRefInternal();
        MemoryManagementFixture::tearDown();
        SysCalls::mmapVector.clear();
    }

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
    template <typename GfxFamily>
    void setUpT() {
        backup = std::make_unique<VariableBackup<UltHwConfig>>(&ultHwConfig);
        ultHwConfig.csrBaseCallCreatePreemption = true;

        MemoryManagementFixture::setUp();

        executionEnvironment = MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), numRootDevices - 1);

        executionEnvironment->calculateMaxOsContextCount();
        auto drmMock = DrmMockCustom::create(*executionEnvironment->rootDeviceEnvironments[1]).release();
        drmMock->memoryInfo.reset(new MockMemoryInfo{*drmMock});
        DrmMemoryManagerFixture::setUpT<GfxFamily>(drmMock, true);
        drmMock->reset();
    } // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    template <typename GfxFamily>
    void tearDownT() {
        DrmMemoryManagerFixture::tearDownT<GfxFamily>();
    }
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

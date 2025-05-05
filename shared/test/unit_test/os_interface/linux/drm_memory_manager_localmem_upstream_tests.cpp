/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/heap_assigner.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/os_interface/linux/allocator_helper.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/heap_allocator.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/os_interface/linux/drm_memory_manager_fixture.h"
#include "shared/test/common/os_interface/linux/drm_mock_memory_info.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/os_interface/linux/drm_mock_impl.h"

#include "gtest/gtest.h"

namespace NEO {

class DrmMemoryManagerFixtureImpl : public DrmMemoryManagerFixture {
  public:
    DrmMockCustom *mockExp;

    template <typename GfxFamily>
    void setUpT() {
        backup = std::make_unique<VariableBackup<UltHwConfig>>(&ultHwConfig);
        ultHwConfig.csrBaseCallCreatePreemption = false;

        MemoryManagementFixture::setUp();
        executionEnvironment = MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), numRootDevices - 1);
        mockExp = DrmMockCustom::create(*executionEnvironment->rootDeviceEnvironments[0]).release();
        DrmMemoryManagerFixture::setUpT<GfxFamily>(mockExp, true);
    }

    template <typename GfxFamily>
    void tearDownT() {
        mockExp->testIoctls();
        DrmMemoryManagerFixture::tearDownT<GfxFamily>();
    }

    std::unique_ptr<VariableBackup<UltHwConfig>> backup;
};

BufferObject *createBufferObjectInMemoryRegion(Drm *drm, Gmm *gmm, AllocationType allocationType, uint64_t gpuAddress, size_t size, uint32_t memoryBanks, size_t maxOsContextCount, bool isSystemMemoryPool, bool isHostUSMAllocation);

class DrmMemoryManagerLocalMemoryTest : public ::testing::Test {
  public:
    DrmTipMock *mock;

    void SetUp() override {
        const bool localMemoryEnabled = true;
        executionEnvironment = new ExecutionEnvironment;
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        mock = new DrmTipMock(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
        mock->memoryInfo.reset(new MockMemoryInfo(*mock));
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u, false);

        device.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, rootDeviceIndex));
        memoryManager = std::make_unique<TestedDrmMemoryManager>(localMemoryEnabled, false, false, *executionEnvironment);
    }

  protected:
    DebugManagerStateRestore restorer{};
    ExecutionEnvironment *executionEnvironment = nullptr;
    std::unique_ptr<MockDevice> device;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager;
    const uint32_t rootDeviceIndex = 0u;
};

class DrmMemoryManagerLocalMemoryWithCustomMockTest : public ::testing::Test {
  public:
    DrmMockCustom *mock;

    void SetUp() override {
        const bool localMemoryEnabled = true;
        executionEnvironment = new ExecutionEnvironment;
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        mock = DrmMockCustom::create(*executionEnvironment->rootDeviceEnvironments[0]).release();
        executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));

        device.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0));
        memoryManager = std::make_unique<TestedDrmMemoryManager>(localMemoryEnabled, false, false, *executionEnvironment);
    }

  protected:
    ExecutionEnvironment *executionEnvironment = nullptr;
    std::unique_ptr<MockDevice> device;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager;
};

HWTEST2_F(DrmMemoryManagerLocalMemoryTest, givenDrmMemoryManagerWhenCreateBufferObjectInMemoryRegionIsCalledThenBufferObjectWithAGivenGpuAddressAndSizeIsCreatedAndAllocatedInASpecifiedMemoryRegion, NonDefaultIoctlsSupported) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);
    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->ioctlCallsCount = 0;

    auto gpuAddress = 0x1234u;
    auto size = MemoryConstants::pageSize64k;

    auto bo = std::unique_ptr<BufferObject>(memoryManager->createBufferObjectInMemoryRegion(rootDeviceIndex,
                                                                                            nullptr,
                                                                                            AllocationType::buffer,
                                                                                            gpuAddress,
                                                                                            size,
                                                                                            (1 << (MemoryBanks::getBankForLocalMemory(0) - 1)),
                                                                                            1,
                                                                                            -1,
                                                                                            false,
                                                                                            false));
    ASSERT_NE(nullptr, bo);
    EXPECT_EQ(1u, mock->ioctlCallsCount);
    EXPECT_EQ(1u, mock->createExt.handle);
    EXPECT_EQ(size, mock->createExt.size);

    EXPECT_EQ(1u, mock->numRegions);
    auto memRegions = mock->memRegions;
    EXPECT_EQ(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, memRegions.memoryClass);
    EXPECT_EQ(0u, memRegions.memoryInstance);

    EXPECT_EQ(gpuAddress, bo->peekAddress());
    EXPECT_EQ(size, bo->peekSize());
}

HWTEST2_F(DrmMemoryManagerLocalMemoryTest, givenMultiRootDeviceEnvironmentAndMemoryInfoWhenCreateMultiGraphicsAllocationThenImportAndExportIoctlAreUsed, NonDefaultIoctlsSupported) {
    uint32_t rootDevicesNumber = 3u;
    MultiGraphicsAllocation multiGraphics(rootDevicesNumber);
    RootDeviceIndicesContainer rootDeviceIndices;
    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.release();

    executionEnvironment->prepareRootDeviceEnvironments(rootDevicesNumber);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        auto mock = new DrmTipMock(*executionEnvironment->rootDeviceEnvironments[i]);

        std::vector<MemoryRegion> regionInfo(2);
        regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
        regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};

        mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
        mock->ioctlCallsCount = 0;
        executionEnvironment->rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u, false);
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();

        rootDeviceIndices.pushUnique(i);
    }
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(true, false, false, *executionEnvironment);

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, {});

    static_cast<DrmTipMock *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>())->outputFd = 7;

    auto ptr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, properties, multiGraphics);

    EXPECT_NE(ptr, nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(multiGraphics.getDefaultGraphicsAllocation())->getMmapPtr(), nullptr);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        if (i != 0) {
            EXPECT_EQ(static_cast<DrmTipMock *>(executionEnvironment->rootDeviceEnvironments[i]->osInterface->getDriverModel()->as<Drm>())->inputFd, 7);
        }
        EXPECT_NE(multiGraphics.getGraphicsAllocation(i), nullptr);
        memoryManager->freeGraphicsMemory(multiGraphics.getGraphicsAllocation(i));
    }

    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMultiRootDeviceEnvironmentAndMemoryInfoWhenCreateMultiGraphicsAllocationAndImportFailsThenNullptrIsReturned) {
    uint32_t rootDevicesNumber = 3u;
    MultiGraphicsAllocation multiGraphics(rootDevicesNumber);
    RootDeviceIndicesContainer rootDeviceIndices;
    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.release();

    executionEnvironment->prepareRootDeviceEnvironments(rootDevicesNumber);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        auto mock = new DrmTipMock(*executionEnvironment->rootDeviceEnvironments[i]);

        std::vector<MemoryRegion> regionInfo(2);
        regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
        regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};

        mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
        mock->ioctlCallsCount = 0;
        mock->fdToHandleRetVal = -1;
        executionEnvironment->rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u, false);
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();

        rootDeviceIndices.pushUnique(i);
    }
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(true, false, false, *executionEnvironment);

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, {});

    auto ptr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, properties, multiGraphics);

    EXPECT_EQ(ptr, nullptr);

    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
}

using DrmMemoryManagerUsmSharedHandleTest = DrmMemoryManagerLocalMemoryTest;

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenDrmMemoryManagerAndOsHandleWhenCreateIsCalledWithBufferHostMemoryAllocationTypeThenGraphicsAllocationIsReturned) {
    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    this->mock->outputHandle = 2u;
    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::bufferHostMemory, false, {});

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, true, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_EQ(this->mock->inputFd, (int)osHandleData.handle);
    EXPECT_EQ(this->mock->setTilingHandle, 0u);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    auto bo = drmAllocation->getBO();
    EXPECT_EQ(bo->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_EQ(1u, bo->getRefCount());
    EXPECT_EQ(size, bo->peekSize());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenMultiRootDeviceEnvironmentAndMemoryInfoWhenCreateMultiGraphicsAllocationAndImportFailsThenNullptrIsReturned) {
    uint32_t rootDevicesNumber = 1u;
    uint32_t rootDeviceIndex = 0u;
    MultiGraphicsAllocation multiGraphics(rootDevicesNumber);
    RootDeviceIndicesContainer rootDeviceIndices;
    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.release();

    executionEnvironment->prepareRootDeviceEnvironments(rootDevicesNumber);
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    auto mock = new DrmTipMock(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->ioctlCallsCount = 0;
    mock->fdToHandleRetVal = -1;
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u, false);

    rootDeviceIndices.pushUnique(rootDeviceIndex);

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(true, false, false, *executionEnvironment);

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, {});

    auto ptr = memoryManager->createUSMHostAllocationFromSharedHandle(1, properties, nullptr, true);

    const char *systemErrorDescription = nullptr;
    executionEnvironment->getErrorDescription(&systemErrorDescription);

    char expectedErrorDescription[256];
    snprintf(expectedErrorDescription, 256, "ioctl(PRIME_FD_TO_HANDLE) failed with %d. errno=%d(%s)\n", mock->fdToHandleRetVal, mock->getErrno(), strerror(mock->getErrno()));

    EXPECT_FALSE(strncmp(expectedErrorDescription, systemErrorDescription, 256));

    EXPECT_EQ(ptr, nullptr);

    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMultiRootDeviceEnvironmentAndNoMemoryInfoWhenCreateMultiGraphicsAllocationThenOldPathIsUsed) {
    uint32_t rootDevicesNumber = 3u;
    MultiGraphicsAllocation multiGraphics(rootDevicesNumber);
    RootDeviceIndicesContainer rootDeviceIndices;
    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.release();
    auto hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrLocalMemory = true;

    executionEnvironment->prepareRootDeviceEnvironments(rootDevicesNumber);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(&hwInfo);
        auto mock = new DrmTipMock(*executionEnvironment->rootDeviceEnvironments[i], &hwInfo);

        mock->memoryInfo.reset(nullptr);
        mock->ioctlCallsCount = 0;
        mock->fdToHandleRetVal = -1;
        executionEnvironment->rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u, false);
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();

        rootDeviceIndices.pushUnique(i);
    }
    auto isLocalMemorySupported = executionEnvironment->rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>().getEnableLocalMemory(hwInfo);
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(isLocalMemorySupported, false, false, *executionEnvironment);

    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        EXPECT_EQ(memoryManager->localMemBanksCount[i], (isLocalMemorySupported ? 1u : 0u));
    }
    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, {});

    auto ptr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, properties, multiGraphics);

    EXPECT_NE(ptr, nullptr);

    EXPECT_EQ(static_cast<DrmAllocation *>(multiGraphics.getDefaultGraphicsAllocation())->getMmapPtr(), nullptr);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        EXPECT_NE(multiGraphics.getGraphicsAllocation(i), nullptr);
        memoryManager->freeGraphicsMemory(multiGraphics.getGraphicsAllocation(i));
    }

    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
}

HWTEST2_F(DrmMemoryManagerLocalMemoryTest, givenMemoryInfoWhenAllocateWithAlignmentThenGemCreateExtIsUsed, NonDefaultIoctlsSupported) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBOMmapCreate.set(-1);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->ioctlCallsCount = 0;

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize64k;

    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithAlignment(allocationData));

    EXPECT_NE(allocation, nullptr);
    EXPECT_NE(allocation->getMmapPtr(), nullptr);
    EXPECT_NE(allocation->getMmapSize(), 0u);
    EXPECT_EQ(allocation->getAllocationOffset(), 0u);
    EXPECT_EQ(1u, mock->createExt.handle);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMemoryInfoAndNotUseObjectMmapPropertyWhenAllocateWithAlignmentThenUserptrIsUsed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBOMmapCreate.set(0);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->mmapOffsetRetVal = -1;

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize64k;
    allocationData.useMmapObject = false;

    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithAlignment(allocationData));

    EXPECT_NE(allocation, nullptr);
    EXPECT_EQ(static_cast<int>(mock->returnHandle), allocation->getBO()->peekHandle() + 1);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMemoryInfoAndFailedMmapOffsetWhenAllocateWithAlignmentThenNullptr) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBOMmapCreate.set(-1);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->mmapOffsetRetVal = -1;

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize64k;

    auto allocation = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);

    EXPECT_EQ(allocation, nullptr);
    mock->mmapOffsetRetVal = 0;
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMemoryInfoAndDisabledMmapBOCreationtWhenAllocateWithAlignmentThenUserptrIsUsed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBOMmapCreate.set(0);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->mmapOffsetRetVal = -1;

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize64k;

    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithAlignment(allocationData));

    EXPECT_NE(allocation, nullptr);
    EXPECT_EQ(static_cast<int>(mock->returnHandle), allocation->getBO()->peekHandle() + 1);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMemoryInfoAndFailedGemCreateExtWhenAllocateWithAlignmentThenNullptr) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBOMmapCreate.set(-1);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->gemCreateExtRetVal = -1;

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize64k;

    auto allocation = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);

    EXPECT_EQ(allocation, nullptr);
    mock->gemCreateExtRetVal = 0;
}

class DrmMemoryManagerLocalMemoryMemoryBankMock : public TestedDrmMemoryManager {
  public:
    DrmMemoryManagerLocalMemoryMemoryBankMock(bool enableLocalMemory,
                                              bool allowForcePin,
                                              bool validateHostPtrMemory,
                                              ExecutionEnvironment &executionEnvironment) : TestedDrmMemoryManager(enableLocalMemory, allowForcePin, validateHostPtrMemory, executionEnvironment) {
    }

    BufferObject *createBufferObjectInMemoryRegion(uint32_t rootDeviceIndex,
                                                   Gmm *gmm,
                                                   AllocationType allocationType,
                                                   uint64_t gpuAddress,
                                                   size_t size,
                                                   DeviceBitfield memoryBanks,
                                                   size_t maxOsContextCount,
                                                   int32_t pairHandle,
                                                   bool isSystemMemoryPool, bool isUSMHostAllocation) override {
        memoryBankIsOne = (memoryBanks.to_ulong() == 1u) ? true : false;
        return nullptr;
    }

    bool memoryBankIsOne = false;
};

class DrmMemoryManagerLocalMemoryMemoryBankTest : public ::testing::Test {
  public:
    DrmTipMock *mock;

    void SetUp() override {
        const bool localMemoryEnabled = true;
        executionEnvironment = new ExecutionEnvironment;
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        mock = new DrmTipMock(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
        mock->memoryInfo.reset(new MockMemoryInfo(*mock));
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u, false);

        device.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(),
                                                                            executionEnvironment,
                                                                            rootDeviceIndex));
        memoryManager = std::make_unique<DrmMemoryManagerLocalMemoryMemoryBankMock>(localMemoryEnabled,
                                                                                    false,
                                                                                    false,
                                                                                    *executionEnvironment);
    }

  protected:
    ExecutionEnvironment *executionEnvironment = nullptr;
    std::unique_ptr<MockDevice> device;
    std::unique_ptr<DrmMemoryManagerLocalMemoryMemoryBankMock> memoryManager;
    const uint32_t rootDeviceIndex = 0u;
};

TEST_F(DrmMemoryManagerLocalMemoryMemoryBankTest, givenDeviceMemoryWhenGraphicsAllocationInDevicePoolIsAllocatedThenMemoryBankIsSetToOne) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.useSystemMemory = false;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 1u;
    memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_TRUE(memoryManager->memoryBankIsOne);
}

HWTEST2_F(DrmMemoryManagerLocalMemoryTest, givenCpuAccessRequiredWhenAllocatingInDevicePoolThenAllocationIsLocked, NonDefaultIoctlsSupported) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.requiresCpuAccess = true;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_TRUE(allocation->isLocked());
    EXPECT_NE(nullptr, allocation->getLockedPtr());
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_NE(0u, allocation->getGpuAddress());

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST2_F(DrmMemoryManagerLocalMemoryTest, givenWriteCombinedAllocationWhenAllocatingInDevicePoolThenAllocationIsLockedAndLockedPtrIsUsedAsGpuAddress, NonDefaultIoctlsSupported) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData{};
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::writeCombined;
    allocData.rootDeviceIndex = rootDeviceIndex;
    auto sizeAligned = alignUp(allocData.size + MemoryConstants::pageSize64k, 2 * MemoryConstants::megaByte) + 2 * MemoryConstants::megaByte;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_TRUE(allocation->isLocked());
    EXPECT_NE(nullptr, allocation->getLockedPtr());

    EXPECT_EQ(allocation->getLockedPtr(), allocation->getUnderlyingBuffer());
    EXPECT_EQ(allocation->getLockedPtr(), reinterpret_cast<void *>(allocation->getGpuAddress()));
    EXPECT_EQ(sizeAligned, allocation->getUnderlyingBufferSize());

    EXPECT_EQ(0u, allocation->getReservedAddressSize());

    auto cpuAddress = allocation->getLockedPtr();
    auto alignedCpuAddress = alignDown(cpuAddress, 2 * MemoryConstants::megaByte);
    auto offset = ptrDiff(cpuAddress, alignedCpuAddress);
    EXPECT_EQ(offset, allocation->getAllocationOffset());

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    auto bo = drmAllocation->getBO();
    EXPECT_NE(nullptr, bo);
    EXPECT_EQ(reinterpret_cast<uint64_t>(cpuAddress), bo->peekAddress());
    EXPECT_EQ(sizeAligned, bo->peekSize());

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST2_F(DrmMemoryManagerLocalMemoryTest, givenSupportedTypeWhenAllocatingInDevicePoolThenSuccessStatusAndNonNullPtrIsReturned, NonDefaultIoctlsSupported) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.rootDeviceIndex = rootDeviceIndex;
    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::image2D;
    imgDesc.imageWidth = MemoryConstants::pageSize;
    imgDesc.imageHeight = MemoryConstants::pageSize;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    bool resource48Bit[] = {true, false};
    AllocationType supportedTypes[] = {AllocationType::buffer,
                                       AllocationType::image,
                                       AllocationType::commandBuffer,
                                       AllocationType::linearStream,
                                       AllocationType::indirectObjectHeap,
                                       AllocationType::timestampPacketTagBuffer,
                                       AllocationType::internalHeap,
                                       AllocationType::kernelIsa,
                                       AllocationType::svmGpu};
    for (auto res48bit : resource48Bit) {
        for (auto supportedType : supportedTypes) {
            allocData.type = supportedType;
            allocData.imgInfo = (AllocationType::image == supportedType) ? &imgInfo : nullptr;
            allocData.hostPtr = (AllocationType::svmGpu == supportedType) ? ::alignedMalloc(allocData.size, 4096) : nullptr;

            switch (supportedType) {
            case AllocationType::image:
            case AllocationType::indirectObjectHeap:
            case AllocationType::internalHeap:
            case AllocationType::kernelIsa:
                allocData.flags.resource48Bit = true;
                break;
            default:
                allocData.flags.resource48Bit = res48bit;
            }

            auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
            ASSERT_NE(nullptr, allocation);
            EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

            auto gpuAddress = allocation->getGpuAddress();
            auto gmmHelper = device->getGmmHelper();
            if (allocation->getAllocationType() == AllocationType::svmGpu) {
                if (!memoryManager->isLimitedRange(0)) {
                    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapSvm)), gpuAddress);
                    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapSvm)), gpuAddress);
                }
            } else if (memoryManager->heapAssigners[rootDeviceIndex]->useInternal32BitHeap(allocation->getAllocationType())) {
                EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapInternalDeviceMemory)), gpuAddress);
                EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapInternalDeviceMemory)), gpuAddress);
            } else {
                const bool prefer2MBAlignment = allocation->getUnderlyingBufferSize() >= 2 * MemoryConstants::megaByte;

                auto heap = HeapIndex::heapStandard64KB;
                if (prefer2MBAlignment) {
                    heap = HeapIndex::heapStandard2MB;
                } else if (memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapExtended) > 0 && !allocData.flags.resource48Bit) {
                    heap = HeapIndex::heapExtended;
                }

                EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapBase(heap)), gpuAddress);
                EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapLimit(heap)), gpuAddress);
            }

            memoryManager->freeGraphicsMemory(allocation);
            if (AllocationType::svmGpu == supportedType) {
                ::alignedFree(const_cast<void *>(allocData.hostPtr));
            }
        }
    }
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenDrmMemoryManagerWithLocalMemoryWhenLockResourceIsCalledOnNullBufferObjectThenReturnNullPtr) {
    auto ptr = memoryManager->lockBufferObject(nullptr);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockBufferObject(nullptr);
}

TEST_F(DrmMemoryManagerLocalMemoryWithCustomMockTest, givenDrmMemoryManagerWithLocalMemoryWhenLockResourceIsCalledOnBufferObjectThenReturnPtr) {
    BufferObject bo(0, mock, 3, 1, 1024, 0);

    DrmAllocation drmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, &bo, nullptr, 0u, 0u, MemoryPool::localMemory);
    EXPECT_EQ(&bo, drmAllocation.getBO());

    auto ptr = memoryManager->lockBufferObject(&bo);
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(ptr, bo.peekLockedAddress());

    memoryManager->unlockBufferObject(&bo);
    EXPECT_EQ(nullptr, bo.peekLockedAddress());
}

using DrmMemoryManagerFailInjectionTest = Test<DrmMemoryManagerFixtureImpl>;

HWTEST2_TEMPLATED_F(DrmMemoryManagerFailInjectionTest, givenEnabledLocalMemoryWhenNewFailsThenAllocateInDevicePoolReturnsStatusErrorAndNullallocation, NonDefaultIoctlsSupported) {
    mock->ioctlExpected.total = -1; // don't care
    class MockGfxPartition : public GfxPartition {
      public:
        MockGfxPartition() : GfxPartition(reservedCpuAddressRange) {
            init(defaultHwInfo->capabilityTable.gpuAddressSpace, getSizeToReserve(), 0, 1, false, 0u, defaultHwInfo->capabilityTable.gpuAddressSpace + 1);
        }
        ~MockGfxPartition() override {
            for (const auto &heap : heaps) {
                auto mockHeap = static_cast<const MockHeap *>(&heap);
                if (defaultHwInfo->capabilityTable.gpuAddressSpace != MemoryConstants::max36BitAddress && mockHeap->getSize() > 0) {
                    EXPECT_EQ(0u, mockHeap->alloc->getUsedSize());
                }
            }
        }
        struct MockHeap : Heap {
            using Heap::alloc;
        };
        OSMemory::ReservedCpuAddressRange reservedCpuAddressRange;
    };
    TestedDrmMemoryManager testedMemoryManager(true, false, true, *executionEnvironment);
    testedMemoryManager.overrideGfxPartition(new MockGfxPartition);

    InjectedFunction method = [&](size_t failureIndex) {
        MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
        AllocationData allocData;
        allocData.allFlags = 0;
        allocData.size = MemoryConstants::pageSize;
        allocData.flags.allocateMemory = true;
        allocData.type = AllocationType::buffer;
        allocData.rootDeviceIndex = rootDeviceIndex;

        auto allocation = testedMemoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);

        if (MemoryManagement::nonfailingAllocation != failureIndex) {
            EXPECT_EQ(nullptr, allocation);
            EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);
        } else {
            EXPECT_NE(nullptr, allocation);
            EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
            testedMemoryManager.freeGraphicsMemory(allocation);
        }
    };

    mock->memoryInfo.reset(new MockMemoryInfo(*mock));
    injectFailures(method);
}

using DrmMemoryManagerCopyMemoryToAllocationTest = DrmMemoryManagerLocalMemoryTest;

struct DrmMemoryManagerToTestCopyMemoryToAllocation : public DrmMemoryManager {
    using DrmMemoryManager::allocateGraphicsMemoryInDevicePool;
    DrmMemoryManagerToTestCopyMemoryToAllocation(ExecutionEnvironment &executionEnvironment, bool localMemoryEnabled, size_t lockableLocalMemorySize)
        : DrmMemoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment) {
        std::fill(this->localMemorySupported.begin(), this->localMemorySupported.end(), localMemoryEnabled);
        lockedLocalMemorySize = lockableLocalMemorySize;
    }
    void *lockResourceImpl(GraphicsAllocation &graphicsAllocation) override {
        if (lockedLocalMemorySize > 0) {
            lockedLocalMemory.reset(new uint8_t[lockedLocalMemorySize]);
            return lockedLocalMemory.get();
        }
        return nullptr;
    }
    void *lockBufferObject(BufferObject *bo) override {
        if (lockedLocalMemorySize > 0) {
            lockedLocalMemory.reset(new uint8_t[lockedLocalMemorySize]);
            return lockedLocalMemory.get();
        }
        return nullptr;
    }
    void unlockBufferObject(BufferObject *bo) override {
    }
    void unlockResourceImpl(GraphicsAllocation &graphicsAllocation) override {
    }
    std::unique_ptr<uint8_t[]> lockedLocalMemory;
    size_t lockedLocalMemorySize = 0;
};

HWTEST2_F(DrmMemoryManagerCopyMemoryToAllocationTest, givenDrmMemoryManagerWhenCopyMemoryToAllocationReturnsSuccessThenAllocationIsFilledWithCorrectData, NonDefaultIoctlsSupported) {
    size_t offset = 3;
    size_t sourceAllocationSize = MemoryConstants::pageSize;
    size_t destinationAllocationSize = sourceAllocationSize + offset;

    DrmMemoryManagerToTestCopyMemoryToAllocation drmMemoryManger(*executionEnvironment, true, destinationAllocationSize);
    std::vector<uint8_t> dataToCopy(sourceAllocationSize, 1u);

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = destinationAllocationSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::kernelIsa;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks.set(0, true);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    auto allocation = drmMemoryManger.allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);

    auto ret = drmMemoryManger.copyMemoryToAllocation(allocation, offset, dataToCopy.data(), dataToCopy.size());
    EXPECT_TRUE(ret);

    EXPECT_EQ(0, memcmp(ptrOffset(drmMemoryManger.lockedLocalMemory.get(), offset), dataToCopy.data(), dataToCopy.size()));

    drmMemoryManger.freeGraphicsMemory(allocation);
}

HWTEST2_F(DrmMemoryManagerCopyMemoryToAllocationTest, givenDrmMemoryManagerWhenCopyMemoryToAllocationFailsToLockResourceThenItReturnsFalse, NonDefaultIoctlsSupported) {
    DrmMemoryManagerToTestCopyMemoryToAllocation drmMemoryManger(*executionEnvironment, true, 0);
    std::vector<uint8_t> dataToCopy(MemoryConstants::pageSize, 1u);

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = dataToCopy.size();
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::kernelIsa;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks.set(0, true);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    auto allocation = drmMemoryManger.allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);

    auto ret = drmMemoryManger.copyMemoryToAllocation(allocation, 0, dataToCopy.data(), dataToCopy.size());
    EXPECT_FALSE(ret);

    drmMemoryManger.freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerCopyMemoryToAllocationTest, givenDrmMemoryManagerWhenCopyMemoryToAllocationWithCpuPtrThenAllocationIsFilledWithCorrectData) {
    size_t offset = 3;
    size_t sourceAllocationSize = MemoryConstants::pageSize;
    size_t destinationAllocationSize = sourceAllocationSize + offset;

    DrmMemoryManagerToTestCopyMemoryToAllocation drmMemoryManger(*executionEnvironment, false, 0);
    std::vector<uint8_t> dataToCopy(sourceAllocationSize, 1u);

    auto allocation = drmMemoryManger.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, destinationAllocationSize, AllocationType::kernelIsa, mockDeviceBitfield});
    ASSERT_NE(nullptr, allocation);

    auto ret = drmMemoryManger.copyMemoryToAllocation(allocation, offset, dataToCopy.data(), dataToCopy.size());
    EXPECT_TRUE(ret);

    EXPECT_EQ(0, memcmp(ptrOffset(allocation->getUnderlyingBuffer(), offset), dataToCopy.data(), dataToCopy.size()));

    drmMemoryManger.freeGraphicsMemory(allocation);
}

using DrmMemoryManagerTestImpl = Test<DrmMemoryManagerFixtureImpl>;

HWTEST2_TEMPLATED_F(DrmMemoryManagerTestImpl, givenDrmMemoryManagerWhenLockUnlockIsCalledOnAllocationInLocalMemoryThenCallIoctlGemMapOffsetAndReturnLockedPtr, NonDefaultIoctlsSupported) {
    mockExp->ioctlExpected.gemCreateExt = 1;
    mockExp->ioctlExpected.gemWait = 1;
    mockExp->ioctlExpected.gemClose = 1;
    mockExp->ioctlExpected.gemMmapOffset = 1;
    mockExp->memoryInfo.reset(new MockMemoryInfo(*mockExp));

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::internalHeap;
    allocData.rootDeviceIndex = rootDeviceIndex;
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());

    auto ptr = memoryManager->lockResource(allocation);
    EXPECT_NE(nullptr, ptr);

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    EXPECT_NE(nullptr, drmAllocation->getBO()->peekLockedAddress());

    EXPECT_EQ(static_cast<uint32_t>(drmAllocation->getBO()->peekHandle()), mockExp->mmapOffsetHandle);
    EXPECT_EQ(0u, mockExp->mmapOffsetPad);
    EXPECT_EQ(0u, mockExp->mmapOffsetExpected);
    EXPECT_EQ(4u, mockExp->mmapOffsetFlags);

    memoryManager->unlockResource(allocation);
    EXPECT_EQ(nullptr, drmAllocation->getBO()->peekLockedAddress());

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, givenDrmMemoryManagerWhenLockUnlockIsCalledOnAllocationInLocalMemoryButFailsOnMmapThenReturnNullPtr) {
    mockExp->ioctlExpected.gemMmapOffset = 2;
    this->ioctlResExt = {mockExp->ioctlCnt.total, -1};
    mockExp->ioctlResExt = &ioctlResExt;

    BufferObject bo(0, mockExp, 3, 1, 0, 0);
    DrmAllocation drmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, &bo, nullptr, 0u, 0u, MemoryPool::localMemory);
    EXPECT_NE(nullptr, drmAllocation.getBO());

    auto ptr = memoryManager->lockResource(&drmAllocation);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockResource(&drmAllocation);
    mockExp->ioctlResExt = &mockExp->none;
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, givenDrmMemoryManagerWhenLockUnlockIsCalledOnAllocationInLocalMemoryButFailsOnIoctlMmapFunctionOffsetThenReturnNullPtr) {
    mockExp->ioctlExpected.gemMmapOffset = 2;
    mockExp->returnIoctlExtraErrorValue = true;
    mockExp->failOnMmapOffset = true;

    BufferObject bo(0, mockExp, 3, 1, 0, 0);
    DrmAllocation drmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, &bo, nullptr, 0u, 0u, MemoryPool::localMemory);
    EXPECT_NE(nullptr, drmAllocation.getBO());

    auto ptr = memoryManager->lockResource(&drmAllocation);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockResource(&drmAllocation);
    mockExp->ioctlResExt = &mockExp->none;
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, givenDrmMemoryManagerWhenLockUnlockIsCalledOnAllocationInLocalMemoryButBufferObjectIsNullThenReturnNullPtr) {
    DrmAllocation drmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, nullptr, 0u, 0u, MemoryPool::localMemory);

    auto ptr = memoryManager->lockResource(&drmAllocation);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockResource(&drmAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, givenDrmMemoryManagerWhenGetLocalMemorySizeIsCalledForMemoryInfoThenReturnMemoryRegionSize) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = new DrmMock(*executionEnvironment.rootDeviceEnvironments[0]);
    drm->memoryInfo.reset(new MockMemoryInfo(*drm));
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    TestedDrmMemoryManager memoryManager(executionEnvironment);

    auto memoryInfo = drm->getMemoryInfo();
    ASSERT_NE(nullptr, memoryInfo);
    EXPECT_EQ(memoryInfo->getMemoryRegionSize(MemoryBanks::getBankForLocalMemory(0)), memoryManager.getLocalMemorySize(0u, 0xF));
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, givenDrmMemoryManagerWhenGetLocalMemorySizeIsCalledForMemoryInfoAndInvalidDeviceBitfieldThenReturnZero) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = new DrmMock(*executionEnvironment.rootDeviceEnvironments[0]);
    drm->memoryInfo.reset(new MockMemoryInfo(*drm));
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    TestedDrmMemoryManager memoryManager(executionEnvironment);

    auto memoryInfo = drm->getMemoryInfo();
    ASSERT_NE(nullptr, memoryInfo);
    EXPECT_EQ(0u, memoryManager.getLocalMemorySize(0u, 0u));
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, givenDrmMemoryManagerWhenGetLocalMemorySizeIsCalledButMemoryInfoIsNotAvailableThenSizeZeroIsReturned) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = new DrmMock(*executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    TestedDrmMemoryManager memoryManager(executionEnvironment);

    EXPECT_EQ(0u, memoryManager.getLocalMemorySize(0u, 0xF));
}

HWTEST2_F(DrmMemoryManagerLocalMemoryTest, givenGraphicsAllocationInDevicePoolIsAllocatedForImage1DWhenTheSizeReturnedFromGmmIsUnalignedThenCreateBufferObjectWithSizeAlignedTo64KB, NonDefaultIoctlsSupported) {
    ImageDescriptor imgDesc = {};
    imgDesc.imageType = ImageType::image1D;
    imgDesc.imageWidth = 100;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::image;
    allocData.flags.resource48Bit = true;
    allocData.imgInfo = &imgInfo;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_TRUE(allocData.imgInfo->useLocalMemory);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());

    auto gmm = allocation->getDefaultGmm();
    EXPECT_NE(nullptr, gmm);
    EXPECT_EQ(0u, gmm->resourceParams.Flags.Info.NonLocalOnly);

    auto gpuAddress = allocation->getGpuAddress();
    auto sizeAlignedTo64KB = alignUp(allocData.imgInfo->size, MemoryConstants::pageSize64k);
    EXPECT_NE(0u, gpuAddress);
    EXPECT_EQ(sizeAlignedTo64KB, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(gpuAddress, reinterpret_cast<uint64_t>(allocation->getReservedAddressPtr()));
    EXPECT_EQ(sizeAlignedTo64KB, allocation->getReservedAddressSize());

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    auto bo = drmAllocation->getBO();
    EXPECT_NE(nullptr, bo);
    EXPECT_EQ(gpuAddress, bo->peekAddress());
    EXPECT_EQ(sizeAlignedTo64KB, bo->peekSize());

    memoryManager->freeGraphicsMemory(allocation);
}

[[maybe_unused]] static uint32_t munmapCalledCount = 0u;

HWTEST2_F(DrmMemoryManagerLocalMemoryTest, givenAlignmentAndSizeWhenMmapReturnsUnalignedPointerThenCreateAllocWithAlignmentUnmapTwoUnalignedPart, NonDefaultIoctlsSupported) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBOMmapCreate.set(-1);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->ioctlCallsCount = 0;

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize64k;

    memoryManager->mmapFunction = [](void *addr, size_t len, int prot,
                                     int flags, int fd, off_t offset) throw() {
        if (addr == 0) {
            return reinterpret_cast<void *>(0x12345678);
        } else {
            return addr;
        }
    };

    memoryManager->munmapFunction = [](void *addr, size_t len) throw() {
        munmapCalledCount++;
        return 0;
    };

    munmapCalledCount = 0;
    auto allocation = memoryManager->createAllocWithAlignment(allocationData, MemoryConstants::pageSize, MemoryConstants::pageSize64k, MemoryConstants::pageSize64k, 0u);

    EXPECT_EQ(alignUp(reinterpret_cast<void *>(0x12345678), MemoryConstants::pageSize64k), allocation->getMmapPtr());
    EXPECT_EQ(1u, munmapCalledCount);
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(3u, munmapCalledCount);
    munmapCalledCount = 0u;
}

HWTEST2_F(DrmMemoryManagerLocalMemoryTest, givenAlignmentAndSizeWhenMmapReturnsAlignedThenCreateAllocWithAlignmentUnmapOneUnalignedPart, NonDefaultIoctlsSupported) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBOMmapCreate.set(-1);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->ioctlCallsCount = 0;

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize64k;

    memoryManager->mmapFunction = [](void *addr, size_t len, int prot,
                                     int flags, int fd, off_t offset) throw() {
        if (addr == 0) {
            return reinterpret_cast<void *>(0x12345678);
        } else {
            return addr;
        }
    };

    memoryManager->munmapFunction = [](void *addr, size_t len) throw() {
        munmapCalledCount++;
        return 0;
    };

    munmapCalledCount = 0u;
    auto allocation = memoryManager->createAllocWithAlignment(allocationData, MemoryConstants::pageSize, 4u, MemoryConstants::pageSize64k, 0u);

    EXPECT_EQ(reinterpret_cast<void *>(0x12345678), allocation->getMmapPtr());
    EXPECT_EQ(1u, munmapCalledCount);
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(2u, munmapCalledCount);
    munmapCalledCount = 0u;
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenAllocationWithInvalidCacheRegionWhenAllocatingInDevicePoolThenReturnNullptr) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 18 * MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.storageInfo.memoryBanks = maxNBitValue(MemoryBanks::getBankForLocalMemory(3));
    allocData.storageInfo.multiStorage = true;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.cacheRegion = 0xFFFF;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenAllocationWithUnifiedMemoryAllocationThenReturnNullptr) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 18 * MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::unifiedSharedMemory;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);
    memoryManager->freeGraphicsMemory(allocation);
}
} // namespace NEO

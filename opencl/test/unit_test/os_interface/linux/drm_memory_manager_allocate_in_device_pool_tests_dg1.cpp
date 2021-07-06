/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/heap_assigner.h"
#include "shared/source/os_interface/linux/allocator_helper.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "opencl/test/unit_test/mocks/mock_gmm.h"
#include "opencl/test/unit_test/os_interface/linux/drm_memory_manager_tests_dg1.h"
#include "opencl/test/unit_test/os_interface/linux/drm_mock_dg1.h"
#include "opencl/test/unit_test/os_interface/linux/drm_mock_memory_info.h"
#include "test.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(DrmMemoryManagerSimpleTest, givenDrmMemoryManagerWhenAllocateInDevicePoolIsCalledThenNullptrAndStatusRetryIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u);
    TestedDrmMemoryManager memoryManager(executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.useSystemMemory = true;
    allocData.flags.allocateMemory = true;

    auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
}

TEST(DrmMemoryManagerSimpleTest, givenDrmMemoryManagerWhenLockResourceIsCalledOnNullBufferObjectThenReturnNullPtr) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u);
    TestedDrmMemoryManager memoryManager(executionEnvironment);
    DrmAllocation drmAllocation(0, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, nullptr, 0u, 0u, MemoryPool::LocalMemory);

    auto ptr = memoryManager.lockResourceInLocalMemoryImpl(drmAllocation.getBO());
    EXPECT_EQ(nullptr, ptr);

    memoryManager.unlockResourceInLocalMemoryImpl(drmAllocation.getBO());
}

TEST(DrmMemoryManagerSimpleTest, givenDrmMemoryManagerWhenFreeGraphicsMemoryIsCalledOnAllocationWithNullBufferObjectThenEarlyReturn) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u);
    TestedDrmMemoryManager memoryManager(executionEnvironment);

    auto drmAllocation = new DrmAllocation(0, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, nullptr, 0u, 0u, MemoryPool::LocalMemory);
    EXPECT_NE(nullptr, drmAllocation);

    memoryManager.freeGraphicsMemoryImpl(drmAllocation);
}

using DrmMemoryManagerWithLocalMemoryTest = Test<DrmMemoryManagerWithLocalMemoryFixture>;

TEST_F(DrmMemoryManagerWithLocalMemoryTest, givenDrmMemoryManagerWithLocalMemoryWhenLockResourceIsCalledOnAllocationInLocalMemoryThenReturnNullPtr) {
    DrmAllocation drmAllocation(rootDeviceIndex, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, nullptr, 0u, 0u, MemoryPool::LocalMemory);

    auto ptr = memoryManager->lockResource(&drmAllocation);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockResource(&drmAllocation);
}

using DrmMemoryManagerTest = Test<DrmMemoryManagerFixture>;

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenCopyMemoryToAllocationThenAllocationIsFilledWithCorrectData) {
    mock->ioctl_expected.gemUserptr = 1;
    mock->ioctl_expected.gemWait = 1;
    mock->ioctl_expected.gemClose = 1;

    std::vector<uint8_t> dataToCopy(MemoryConstants::pageSize, 1u);

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties({rootDeviceIndex, dataToCopy.size(), GraphicsAllocation::AllocationType::BUFFER, device->getDeviceBitfield()});
    ASSERT_NE(nullptr, allocation);

    auto ret = memoryManager->copyMemoryToAllocation(allocation, 0, dataToCopy.data(), dataToCopy.size());
    EXPECT_TRUE(ret);

    EXPECT_EQ(0, memcmp(allocation->getUnderlyingBuffer(), dataToCopy.data(), dataToCopy.size()));

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenGetLocalMemoryIsCalledThenSizeOfLocalMemoryIsReturned) {
    EXPECT_EQ(0 * GB, memoryManager->getLocalMemorySize(rootDeviceIndex, 0xF));
}

namespace NEO {

BufferObject *createBufferObjectInMemoryRegion(Drm *drm, uint64_t gpuAddress, size_t size, uint32_t memoryBanks, size_t maxOsContextCount);

class DrmMemoryManagerLocalMemoryTest : public ::testing::Test {
  public:
    DrmMockDg1 *mock;

    void SetUp() override {
        const bool localMemoryEnabled = true;
        executionEnvironment = new ExecutionEnvironment;
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setHwInfo(defaultHwInfo.get());
        mock = new DrmMockDg1(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
        mock->memoryInfo.reset(new MockMemoryInfo());
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u);

        device.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, rootDeviceIndex));
        memoryManager = std::make_unique<TestedDrmMemoryManager>(localMemoryEnabled, false, false, *executionEnvironment);
    }

    bool isAllocationWithinHeap(const GraphicsAllocation &allocation, HeapIndex heap) {
        const auto allocationStart = allocation.getGpuAddress();
        const auto allocationEnd = allocationStart + allocation.getUnderlyingBufferSize();
        const auto heapStart = GmmHelper::canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(heap));
        const auto heapEnd = GmmHelper::canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(heap));
        return heapStart <= allocationStart && allocationEnd <= heapEnd;
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
    DrmMockCustomDg1 *mock;

    void SetUp() override {
        const bool localMemoryEnabled = true;
        executionEnvironment = new ExecutionEnvironment;
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
        mock = new DrmMockCustomDg1();
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

TEST_F(DrmMemoryManagerLocalMemoryTest, givenDrmMemoryManagerWhenCreateBufferObjectInMemoryRegionIsCalledThenBufferObjectWithAGivenGpuAddressAndSizeIsCreatedAndAllocatedInASpecifiedMemoryRegion) {
    drm_i915_memory_region_info regionInfo[2] = {};
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, 0};

    mock->memoryInfo.reset(new MemoryInfoImpl(regionInfo, 2));
    mock->ioctlCallsCount = 0;

    auto gpuAddress = 0x1234u;
    auto size = MemoryConstants::pageSize64k;

    auto bo = std::unique_ptr<BufferObject>(memoryManager->createBufferObjectInMemoryRegion(&memoryManager->getDrm(0),
                                                                                            gpuAddress,
                                                                                            size,
                                                                                            (1 << (MemoryBanks::getBankForLocalMemory(0) - 1)),
                                                                                            1));
    ASSERT_NE(nullptr, bo);

    EXPECT_EQ(1u, mock->ioctlCallsCount);

    EXPECT_EQ(1u, mock->createExt.handle);
    EXPECT_EQ(size, mock->createExt.size);

    EXPECT_EQ(I915_GEM_CREATE_EXT_SETPARAM, mock->setparamRegion.base.name);

    auto regionParam = mock->setparamRegion.param;
    EXPECT_EQ(0u, regionParam.handle);
    EXPECT_EQ(1u, regionParam.size);
    EXPECT_EQ(I915_OBJECT_PARAM | I915_PARAM_MEMORY_REGIONS, regionParam.param);

    auto memRegions = mock->memRegions;
    EXPECT_EQ(I915_MEMORY_CLASS_DEVICE, memRegions.memory_class);
    EXPECT_EQ(0u, memRegions.memory_instance);

    EXPECT_EQ(gpuAddress, bo->peekAddress());
    EXPECT_EQ(size, bo->peekSize());
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenDrmMemoryManagerWhenCreateBufferObjectInMemoryRegionIsCalledWithoutMemoryInfoThenNullBufferObjectIsReturned) {
    mock->memoryInfo.reset(nullptr);

    auto gpuAddress = 0x1234u;
    auto size = MemoryConstants::pageSize;

    auto bo = std::unique_ptr<BufferObject>(memoryManager->createBufferObjectInMemoryRegion(&memoryManager->getDrm(0),
                                                                                            gpuAddress,
                                                                                            size,
                                                                                            MemoryBanks::MainBank,
                                                                                            1));
    EXPECT_EQ(nullptr, bo);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenDrmMemoryManagerWhenCreateBufferObjectInMemoryRegionIsCalledWithIncorrectRegionInfoThenNullBufferObjectIsReturned) {
    drm_i915_memory_region_info regionInfo = {};

    mock->memoryInfo.reset(new MemoryInfoImpl(&regionInfo, 0));

    auto gpuAddress = 0x1234u;
    auto size = MemoryConstants::pageSize;

    auto bo = std::unique_ptr<BufferObject>(memoryManager->createBufferObjectInMemoryRegion(&memoryManager->getDrm(0),
                                                                                            gpuAddress,
                                                                                            size,
                                                                                            MemoryBanks::MainBank,
                                                                                            1));
    EXPECT_EQ(nullptr, bo);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenDrmMemoryManagerWhenCreateBufferObjectInMemoryRegionIsCalledWithZeroSizeThenNullBufferObjectIsReturned) {
    mock->memoryInfo.reset(new MockMemoryInfo());

    auto gpuAddress = 0x1234u;
    auto size = 0u;

    auto bo = std::unique_ptr<BufferObject>(memoryManager->createBufferObjectInMemoryRegion(&memoryManager->getDrm(0),
                                                                                            gpuAddress,
                                                                                            size,
                                                                                            MemoryBanks::MainBank,
                                                                                            1));
    EXPECT_EQ(nullptr, bo);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMultiRootDeviceEnvironmentAndMemoryInfoWhenCreateMultiGraphicsAllocationThenImportAndExportIoctlAreUsed) {
    uint32_t rootDevicesNumber = 3u;
    MultiGraphicsAllocation multiGraphics(rootDevicesNumber);
    std::vector<uint32_t> rootDeviceIndices;
    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.release();

    executionEnvironment->prepareRootDeviceEnvironments(rootDevicesNumber);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
        auto mock = new DrmMockDg1(*executionEnvironment->rootDeviceEnvironments[i]);

        drm_i915_memory_region_info regionInfo[2] = {};
        regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
        regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, 0};

        mock->memoryInfo.reset(new MemoryInfoImpl(regionInfo, 2));
        mock->ioctlCallsCount = 0;
        executionEnvironment->rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u);

        rootDeviceIndices.push_back(i);
    }
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(true, false, false, *executionEnvironment);

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, true, size, GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, false, {});

    static_cast<DrmMockDg1 *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>())->outputFd = 7;

    auto ptr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, properties, multiGraphics);

    EXPECT_NE(ptr, nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(multiGraphics.getDefaultGraphicsAllocation())->getMmapPtr(), nullptr);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        if (i != 0) {
            EXPECT_EQ(static_cast<DrmMockDg1 *>(executionEnvironment->rootDeviceEnvironments[i]->osInterface->getDriverModel()->as<Drm>())->inputFd, 7);
        }
        EXPECT_NE(multiGraphics.getGraphicsAllocation(i), nullptr);
        memoryManager->freeGraphicsMemory(multiGraphics.getGraphicsAllocation(i));
    }

    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMultiRootDeviceEnvironmentAndMemoryInfoWhenCreateMultiGraphicsAllocationAndImportFailsThenNullptrIsReturned) {
    uint32_t rootDevicesNumber = 3u;
    MultiGraphicsAllocation multiGraphics(rootDevicesNumber);
    std::vector<uint32_t> rootDeviceIndices;
    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.release();

    executionEnvironment->prepareRootDeviceEnvironments(rootDevicesNumber);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
        auto mock = new DrmMockDg1(*executionEnvironment->rootDeviceEnvironments[i]);

        drm_i915_memory_region_info regionInfo[2] = {};
        regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
        regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, 0};

        mock->memoryInfo.reset(new MemoryInfoImpl(regionInfo, 2));
        mock->ioctlCallsCount = 0;
        mock->fdToHandleRetVal = -1;
        executionEnvironment->rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u);

        rootDeviceIndices.push_back(i);
    }
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(true, false, false, *executionEnvironment);

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, true, size, GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, false, {});

    auto ptr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, properties, multiGraphics);

    EXPECT_EQ(ptr, nullptr);

    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
}

using DrmMemoryManagerUsmSharedHandleTest = DrmMemoryManagerLocalMemoryTest;

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenDrmMemoryManagerAndOsHandleWhenCreateIsCalledWithBufferHostMemoryAllocationTypeThenGraphicsAllocationIsReturned) {
    osHandle handle = 1u;
    this->mock->outputHandle = 2u;
    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, false, {});

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(handle, properties, false, true);
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_EQ(this->mock->inputFd, (int)handle);
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
    std::vector<uint32_t> rootDeviceIndices;
    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.release();

    executionEnvironment->prepareRootDeviceEnvironments(rootDevicesNumber);
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setHwInfo(defaultHwInfo.get());
    auto mock = new DrmMockDg1(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    drm_i915_memory_region_info regionInfo[2] = {};
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, 0};

    mock->memoryInfo.reset(new MemoryInfoImpl(regionInfo, 2));
    mock->ioctlCallsCount = 0;
    mock->fdToHandleRetVal = -1;
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u);

    rootDeviceIndices.push_back(rootDeviceIndex);

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(true, false, false, *executionEnvironment);

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, true, size, GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, false, {});

    auto ptr = memoryManager->createUSMHostAllocationFromSharedHandle(1, properties, false);

    EXPECT_EQ(ptr, nullptr);

    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMultiRootDeviceEnvironmentAndNoMemoryInfoWhenCreateMultiGraphicsAllocationThenOldPathIsUsed) {
    uint32_t rootDevicesNumber = 3u;
    MultiGraphicsAllocation multiGraphics(rootDevicesNumber);
    std::vector<uint32_t> rootDeviceIndices;
    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.release();

    executionEnvironment->prepareRootDeviceEnvironments(rootDevicesNumber);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(defaultHwInfo.get());
        auto mock = new DrmMockDg1(*executionEnvironment->rootDeviceEnvironments[i]);

        mock->memoryInfo.reset(nullptr);
        mock->ioctlCallsCount = 0;
        mock->fdToHandleRetVal = -1;
        executionEnvironment->rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u);

        rootDeviceIndices.push_back(i);
    }
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(true, false, false, *executionEnvironment);

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, true, size, GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY, false, {});

    auto ptr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, properties, multiGraphics);

    EXPECT_NE(ptr, nullptr);

    EXPECT_EQ(static_cast<DrmAllocation *>(multiGraphics.getDefaultGraphicsAllocation())->getMmapPtr(), nullptr);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        EXPECT_NE(multiGraphics.getGraphicsAllocation(i), nullptr);
        memoryManager->freeGraphicsMemory(multiGraphics.getGraphicsAllocation(i));
    }

    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMemoryInfoWhenAllocateWithAlignmentThenGemCreateExtIsUsed) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableBOMmapCreate.set(-1);

    drm_i915_memory_region_info regionInfo[2] = {};
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, 0};

    mock->memoryInfo.reset(new MemoryInfoImpl(regionInfo, 2));
    mock->ioctlCallsCount = 0;

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize64k;

    auto allocation = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);

    EXPECT_NE(allocation, nullptr);
    EXPECT_NE(allocation->getMmapPtr(), nullptr);
    EXPECT_NE(allocation->getMmapSize(), 0u);
    EXPECT_EQ(allocation->getAllocationOffset(), 0u);
    EXPECT_EQ(1u, mock->createExt.handle);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMemoryInfoAndNotUseObjectMmapPropertyWhenAllocateWithAlignmentThenUserptrIsUsed) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableBOMmapCreate.set(0);

    drm_i915_memory_region_info regionInfo[2] = {};
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, 0};

    mock->memoryInfo.reset(new MemoryInfoImpl(regionInfo, 2));
    mock->mmapOffsetRetVal = -1;

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize64k;
    allocationData.useMmapObject = false;

    auto allocation = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);

    EXPECT_NE(allocation, nullptr);
    EXPECT_EQ(static_cast<int>(mock->returnHandle), allocation->getBO()->peekHandle() + 1);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMemoryInfoAndFailedMmapOffsetWhenAllocateWithAlignmentThenNullptr) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableBOMmapCreate.set(-1);

    drm_i915_memory_region_info regionInfo[2] = {};
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, 0};

    mock->memoryInfo.reset(new MemoryInfoImpl(regionInfo, 2));
    mock->mmapOffsetRetVal = -1;

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize64k;

    auto allocation = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);

    EXPECT_EQ(allocation, nullptr);
    mock->mmapOffsetRetVal = 0;
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMemoryInfoAndDisabledMmapBOCreationtWhenAllocateWithAlignmentThenUserptrIsUsed) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableBOMmapCreate.set(0);

    drm_i915_memory_region_info regionInfo[2] = {};
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, 0};

    mock->memoryInfo.reset(new MemoryInfoImpl(regionInfo, 2));
    mock->mmapOffsetRetVal = -1;

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize64k;

    auto allocation = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);

    EXPECT_NE(allocation, nullptr);
    EXPECT_EQ(static_cast<int>(mock->returnHandle), allocation->getBO()->peekHandle() + 1);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMemoryInfoAndFailedGemCreateExtWhenAllocateWithAlignmentThenNullptr) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableBOMmapCreate.set(-1);

    drm_i915_memory_region_info regionInfo[2] = {};
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, 0};

    mock->memoryInfo.reset(new MemoryInfoImpl(regionInfo, 2));
    mock->gemCreateExtRetVal = -1;

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize64k;

    auto allocation = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);

    EXPECT_EQ(allocation, nullptr);
    mock->gemCreateExtRetVal = 0;
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenUseSystemMemoryFlagWhenGraphicsAllocationInDevicePoolIsAllocatedThenNullptrIsReturned) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.useSystemMemory = true;
    allocData.type = GraphicsAllocation::AllocationType::BUFFER;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
}

class DrmMemoryManagerLocalMemoryMemoryBankMock : public TestedDrmMemoryManager {
  public:
    DrmMemoryManagerLocalMemoryMemoryBankMock(bool enableLocalMemory,
                                              bool allowForcePin,
                                              bool validateHostPtrMemory,
                                              ExecutionEnvironment &executionEnvironment) : TestedDrmMemoryManager(enableLocalMemory, allowForcePin, validateHostPtrMemory, executionEnvironment) {
    }

    BufferObject *createBufferObjectInMemoryRegion(Drm *drm,
                                                   uint64_t gpuAddress,
                                                   size_t size,
                                                   uint32_t memoryBanks,
                                                   size_t maxOsContextCount) override {
        memoryBankIsOne = (memoryBanks == 1) ? true : false;
        return nullptr;
    }

    bool memoryBankIsOne = false;
};

class DrmMemoryManagerLocalMemoryMemoryBankTest : public ::testing::Test {
  public:
    DrmMockDg1 *mock;

    void SetUp() override {
        const bool localMemoryEnabled = true;
        executionEnvironment = new ExecutionEnvironment;
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setHwInfo(defaultHwInfo.get());
        mock = new DrmMockDg1(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
        mock->memoryInfo.reset(new MockMemoryInfo());
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u);

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
    allocData.type = GraphicsAllocation::AllocationType::BUFFER;
    allocData.rootDeviceIndex = rootDeviceIndex;

    memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_TRUE(memoryManager->memoryBankIsOne);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenNotSetUseSystemMemoryWhenGraphicsAllocationInDevicePoolIsAllocatedForBufferThenLocalMemoryAllocationIsReturnedFromStandard64KbHeap) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.type = GraphicsAllocation::AllocationType::BUFFER;
    allocData.rootDeviceIndex = rootDeviceIndex;
    auto sizeAligned = alignUp(allocData.size, MemoryConstants::pageSize64k);

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());

    auto gmm = allocation->getDefaultGmm();
    EXPECT_NE(nullptr, gmm);
    EXPECT_FALSE(gmm->useSystemMemoryPool);

    EXPECT_EQ(RESOURCE_BUFFER, gmm->resourceParams.Type);
    EXPECT_EQ(sizeAligned, gmm->resourceParams.BaseWidth64);
    EXPECT_NE(nullptr, gmm->gmmResourceInfo->peekHandle());
    EXPECT_NE(0u, gmm->gmmResourceInfo->getHAlign());

    auto gpuAddress = allocation->getGpuAddress();
    EXPECT_NE(0u, gpuAddress);

    auto heap = HeapIndex::HEAP_STANDARD64KB;
    if (memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::HEAP_EXTENDED)) {
        heap = HeapIndex::HEAP_EXTENDED;
    }
    EXPECT_LT(GmmHelper::canonize(memoryManager->getGfxPartition(0)->getHeapBase(heap)), gpuAddress);
    EXPECT_GT(GmmHelper::canonize(memoryManager->getGfxPartition(0)->getHeapLimit(heap)), gpuAddress);
    EXPECT_EQ(0u, allocation->getGpuBaseAddress());
    EXPECT_EQ(sizeAligned, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(gpuAddress, reinterpret_cast<uint64_t>(allocation->getReservedAddressPtr()));
    EXPECT_EQ(sizeAligned, allocation->getReservedAddressSize());

    EXPECT_EQ(1u, allocation->storageInfo.getNumBanks());
    EXPECT_EQ(allocData.storageInfo.getMemoryBanks(), allocation->storageInfo.getMemoryBanks());
    EXPECT_EQ(allocData.storageInfo.multiStorage, allocation->storageInfo.multiStorage);
    EXPECT_EQ(allocData.flags.flushL3, allocation->isFlushL3Required());

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    auto bo = drmAllocation->getBO();
    EXPECT_NE(nullptr, bo);
    EXPECT_EQ(gpuAddress, bo->peekAddress());
    EXPECT_EQ(sizeAligned, bo->peekSize());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenNotSetUseSystemMemoryWhenGraphicsAllocationInDevicePoolIsAllocatedForImageThenLocalMemoryAllocationIsReturnedFromStandard64KbHeap) {
    cl_image_desc imgDesc = {};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imgDesc.image_width = 512;
    imgDesc.image_height = 512;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = GraphicsAllocation::AllocationType::IMAGE;
    allocData.flags.resource48Bit = true;
    allocData.imgInfo = &imgInfo;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_TRUE(allocData.imgInfo->useLocalMemory);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());

    auto gmm = allocation->getDefaultGmm();
    EXPECT_NE(nullptr, gmm);
    EXPECT_FALSE(gmm->useSystemMemoryPool);

    auto gpuAddress = allocation->getGpuAddress();
    auto sizeAligned = alignUp(allocData.imgInfo->size, MemoryConstants::pageSize64k);
    EXPECT_NE(0u, gpuAddress);
    EXPECT_LT(GmmHelper::canonize(memoryManager->getGfxPartition(0)->getHeapBase(HeapIndex::HEAP_STANDARD64KB)), gpuAddress);
    EXPECT_GT(GmmHelper::canonize(memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::HEAP_STANDARD64KB)), gpuAddress);
    EXPECT_EQ(0u, allocation->getGpuBaseAddress());
    EXPECT_EQ(sizeAligned, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(gpuAddress, reinterpret_cast<uint64_t>(allocation->getReservedAddressPtr()));
    EXPECT_EQ(sizeAligned, allocation->getReservedAddressSize());

    EXPECT_EQ(1u, allocation->storageInfo.getNumBanks());
    EXPECT_EQ(allocData.storageInfo.getMemoryBanks(), allocation->storageInfo.getMemoryBanks());
    EXPECT_EQ(allocData.storageInfo.multiStorage, allocation->storageInfo.multiStorage);
    EXPECT_EQ(allocData.flags.flushL3, allocation->isFlushL3Required());

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    auto bo = drmAllocation->getBO();
    EXPECT_NE(nullptr, bo);
    EXPECT_EQ(gpuAddress, bo->peekAddress());
    EXPECT_EQ(sizeAligned, bo->peekSize());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenNotSetUseSystemMemoryWhenGraphicsAllocatioInDevicePoolIsAllocatednForKernelIsaThenLocalMemoryAllocationIsReturnedFromInternalHeap) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.type = GraphicsAllocation::AllocationType::KERNEL_ISA;
    allocData.rootDeviceIndex = rootDeviceIndex;
    auto sizeAligned = alignUp(allocData.size, MemoryConstants::pageSize64k);

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());

    auto gpuAddress = allocation->getGpuAddress();
    EXPECT_LT(GmmHelper::canonize(memoryManager->getGfxPartition(0)->getHeapBase(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY)), gpuAddress);
    EXPECT_GT(GmmHelper::canonize(memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY)), gpuAddress);
    EXPECT_EQ(GmmHelper::canonize(memoryManager->getGfxPartition(0)->getHeapBase(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY)), allocation->getGpuBaseAddress());
    EXPECT_EQ(sizeAligned, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(gpuAddress, reinterpret_cast<uint64_t>(allocation->getReservedAddressPtr()));
    EXPECT_EQ(sizeAligned, allocation->getReservedAddressSize());

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    auto bo = drmAllocation->getBO();
    EXPECT_NE(nullptr, bo);
    EXPECT_EQ(gpuAddress, bo->peekAddress());
    EXPECT_EQ(sizeAligned, bo->peekSize());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenSvmGpuAllocationWhenHostPtrProvidedThenUseHostPtrAsGpuVa) {
    size_t size = 2 * MemoryConstants::megaByte;
    AllocationProperties properties{0, false, size, GraphicsAllocation::AllocationType::SVM_GPU, false, {}};
    properties.alignment = size;
    void *svmPtr = reinterpret_cast<void *>(2 * size);

    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(properties, svmPtr));
    ASSERT_NE(nullptr, allocation);

    EXPECT_EQ(size, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(nullptr, allocation->getDriverAllocatedCpuPtr());

    EXPECT_EQ(svmPtr, reinterpret_cast<void *>(allocation->getGpuAddress()));

    EXPECT_EQ(0u, allocation->getReservedAddressSize());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenAllowed32BitAndForce32BitWhenGraphicsAllocationInDevicePoolIsAllocatedThenNullptrIsReturned) {
    memoryManager->setForce32BitAllocations(true);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allow32Bit = true;
    allocData.flags.allocateMemory = true;
    allocData.type = GraphicsAllocation::AllocationType::BUFFER;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenAllowed32BitWhen32BitIsNotForcedThenGraphicsAllocationInDevicePoolReturnsLocalMemoryAllocation) {
    memoryManager->setForce32BitAllocations(false);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allow32Bit = true;
    allocData.flags.allocateMemory = true;
    allocData.type = GraphicsAllocation::AllocationType::BUFFER;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCpuAccessRequiredWhenAllocatingInDevicePoolThenAllocationIsLocked) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.requiresCpuAccess = true;
    allocData.flags.allocateMemory = true;
    allocData.type = GraphicsAllocation::AllocationType::BUFFER;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());
    EXPECT_TRUE(allocation->isLocked());
    EXPECT_NE(nullptr, allocation->getLockedPtr());
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_NE(0u, allocation->getGpuAddress());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenWriteCombinedAllocationWhenAllocatingInDevicePoolThenAllocationIsLockedAndLockedPtrIsUsedAsGpuAddress) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData{};
    allocData.size = MemoryConstants::pageSize;
    allocData.type = GraphicsAllocation::AllocationType::WRITE_COMBINED;
    allocData.rootDeviceIndex = rootDeviceIndex;
    auto sizeAligned = alignUp(allocData.size + MemoryConstants::pageSize64k, 2 * MemoryConstants::megaByte) + 2 * MemoryConstants::megaByte;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());
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

TEST_F(DrmMemoryManagerLocalMemoryTest, givenAllocationWithKernelIsaWhenAllocatingInDevicePoolOnAllMemoryBanksThenCreateFourBufferObjectsWithSameGpuVirtualAddressAndSize) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 3 * MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = GraphicsAllocation::AllocationType::KERNEL_ISA;
    allocData.storageInfo.multiStorage = false;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());
    EXPECT_NE(0u, allocation->getGpuAddress());
    EXPECT_EQ(1u, allocation->getNumGmms());

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    auto &bos = drmAllocation->getBOs();
    auto boAddress = drmAllocation->getGpuAddress();

    auto bo = bos[0];
    ASSERT_NE(nullptr, bo);
    auto boSize = allocation->getGmm(0)->gmmResourceInfo->getSizeAllocation();
    EXPECT_EQ(boAddress, bo->peekAddress());
    EXPECT_EQ(boSize, bo->peekSize());
    EXPECT_EQ(boSize, 3 * MemoryConstants::pageSize64k);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenSupportedTypeWhenAllocatingInDevicePoolThenSuccessStatusAndNonNullPtrIsReturned) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.rootDeviceIndex = rootDeviceIndex;
    cl_image_desc imgDesc = {CL_MEM_OBJECT_IMAGE2D, MemoryConstants::pageSize, MemoryConstants::pageSize, 0, 0, 0, 0, 0, 0, {nullptr}};
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    bool resource48Bit[] = {true, false};
    GraphicsAllocation::AllocationType supportedTypes[] = {GraphicsAllocation::AllocationType::BUFFER,
                                                           GraphicsAllocation::AllocationType::BUFFER_COMPRESSED,
                                                           GraphicsAllocation::AllocationType::IMAGE,
                                                           GraphicsAllocation::AllocationType::COMMAND_BUFFER,
                                                           GraphicsAllocation::AllocationType::LINEAR_STREAM,
                                                           GraphicsAllocation::AllocationType::INDIRECT_OBJECT_HEAP,
                                                           GraphicsAllocation::AllocationType::TIMESTAMP_PACKET_TAG_BUFFER,
                                                           GraphicsAllocation::AllocationType::INTERNAL_HEAP,
                                                           GraphicsAllocation::AllocationType::KERNEL_ISA,
                                                           GraphicsAllocation::AllocationType::SVM_GPU};
    for (auto res48bit : resource48Bit) {
        for (auto supportedType : supportedTypes) {
            allocData.type = supportedType;
            allocData.imgInfo = (GraphicsAllocation::AllocationType::IMAGE == supportedType) ? &imgInfo : nullptr;
            allocData.hostPtr = (GraphicsAllocation::AllocationType::SVM_GPU == supportedType) ? ::alignedMalloc(allocData.size, 4096) : nullptr;

            switch (supportedType) {
            case GraphicsAllocation::AllocationType::IMAGE:
            case GraphicsAllocation::AllocationType::INDIRECT_OBJECT_HEAP:
            case GraphicsAllocation::AllocationType::INTERNAL_HEAP:
            case GraphicsAllocation::AllocationType::KERNEL_ISA:
                allocData.flags.resource48Bit = true;
                break;
            default:
                allocData.flags.resource48Bit = res48bit;
            }

            auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
            ASSERT_NE(nullptr, allocation);
            EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

            auto gpuAddress = allocation->getGpuAddress();
            if (allocation->getAllocationType() == GraphicsAllocation::AllocationType::SVM_GPU) {
                if (!memoryManager->isLimitedRange(0)) {
                    EXPECT_LT(GmmHelper::canonize(memoryManager->getGfxPartition(0)->getHeapBase(HeapIndex::HEAP_SVM)), gpuAddress);
                    EXPECT_GT(GmmHelper::canonize(memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::HEAP_SVM)), gpuAddress);
                }
            } else if (memoryManager->heapAssigner.useInternal32BitHeap(allocation->getAllocationType())) {
                EXPECT_LT(GmmHelper::canonize(memoryManager->getGfxPartition(0)->getHeapBase(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY)), gpuAddress);
                EXPECT_GT(GmmHelper::canonize(memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY)), gpuAddress);
            } else {
                const bool prefer2MBAlignment = allocation->getUnderlyingBufferSize() >= 2 * MemoryConstants::megaByte;
                const bool prefer57bitAddressing = memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::HEAP_EXTENDED) > 0 && !allocData.flags.resource48Bit;

                auto heap = HeapIndex::HEAP_STANDARD64KB;
                if (prefer2MBAlignment) {
                    heap = HeapIndex::HEAP_STANDARD2MB;
                } else if (prefer57bitAddressing) {
                    heap = HeapIndex::HEAP_EXTENDED;
                }

                EXPECT_LT(GmmHelper::canonize(memoryManager->getGfxPartition(0)->getHeapBase(heap)), gpuAddress);
                EXPECT_GT(GmmHelper::canonize(memoryManager->getGfxPartition(0)->getHeapLimit(heap)), gpuAddress);
            }

            memoryManager->freeGraphicsMemory(allocation);
            if (GraphicsAllocation::AllocationType::SVM_GPU == supportedType) {
                ::alignedFree(const_cast<void *>(allocData.hostPtr));
            }
        }
    }
}

struct DrmMemoryManagerLocalMemoryAlignmentTest : DrmMemoryManagerLocalMemoryTest {
    std::unique_ptr<TestedDrmMemoryManager> createMemoryManager() {
        return std::make_unique<TestedDrmMemoryManager>(true, false, false, *executionEnvironment);
    }

    bool isAllocationWithinHeap(MemoryManager &memoryManager, const GraphicsAllocation &allocation, HeapIndex heap) {
        const auto allocationStart = allocation.getGpuAddress();
        const auto allocationEnd = allocationStart + allocation.getUnderlyingBufferSize();
        const auto heapStart = GmmHelper::canonize(memoryManager.getGfxPartition(rootDeviceIndex)->getHeapBase(heap));
        const auto heapEnd = GmmHelper::canonize(memoryManager.getGfxPartition(rootDeviceIndex)->getHeapLimit(heap));
        return heapStart <= allocationStart && allocationEnd <= heapEnd;
    }

    const uint32_t rootDeviceIndex = 0u;
    DebugManagerStateRestore restore{};
};

TEST_F(DrmMemoryManagerLocalMemoryAlignmentTest, given2MbAlignmentAllowedWhenAllocatingAllocationLessThen2MbThenUse64kbHeap) {
    AllocationData allocationData;
    allocationData.allFlags = 0;
    allocationData.size = MemoryConstants::pageSize;
    allocationData.flags.allocateMemory = true;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.type = GraphicsAllocation::AllocationType::BUFFER;
    allocationData.flags.resource48Bit = true;
    MemoryManager::AllocationStatus allocationStatus;

    {
        DebugManager.flags.AlignLocalMemoryVaTo2MB.set(-1);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::HEAP_STANDARD64KB));
        memoryManager->freeGraphicsMemory(allocation);
    }
    {
        DebugManager.flags.AlignLocalMemoryVaTo2MB.set(0);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::HEAP_STANDARD64KB));
        memoryManager->freeGraphicsMemory(allocation);
    }
    {
        DebugManager.flags.AlignLocalMemoryVaTo2MB.set(1);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::HEAP_STANDARD64KB));
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerLocalMemoryAlignmentTest, given2MbAlignmentAllowedWhenAllocatingAllocationBiggerThan2MbThenUse2MbHeap) {
    AllocationData allocationData;
    allocationData.allFlags = 0;
    allocationData.size = 2 * MemoryConstants::megaByte;
    allocationData.flags.allocateMemory = true;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.type = GraphicsAllocation::AllocationType::BUFFER;
    allocationData.flags.resource48Bit = true;
    MemoryManager::AllocationStatus allocationStatus;

    {
        DebugManager.flags.AlignLocalMemoryVaTo2MB.set(-1);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::HEAP_STANDARD2MB));
        memoryManager->freeGraphicsMemory(allocation);
    }
    {
        DebugManager.flags.AlignLocalMemoryVaTo2MB.set(0);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::HEAP_STANDARD64KB));
        memoryManager->freeGraphicsMemory(allocation);
    }
    {
        DebugManager.flags.AlignLocalMemoryVaTo2MB.set(1);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::HEAP_STANDARD2MB));
        EXPECT_TRUE(isAligned(allocation->getGpuAddress(), MemoryConstants::pageSize2Mb));
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerLocalMemoryAlignmentTest, givenExtendedHeapPreferredAnd2MbAlignmentAllowedWhenAllocatingAllocationBiggerThenUseExtendedHeap) {
    if (memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::HEAP_EXTENDED) == 0) {
        GTEST_SKIP();
    }

    AllocationData allocationData;
    allocationData.allFlags = 0;
    allocationData.size = 2 * MemoryConstants::megaByte;
    allocationData.flags.allocateMemory = true;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.type = GraphicsAllocation::AllocationType::BUFFER;
    allocationData.flags.resource48Bit = false;
    MemoryManager::AllocationStatus allocationStatus;

    {
        DebugManager.flags.AlignLocalMemoryVaTo2MB.set(-1);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAligned(allocation->getGpuAddress(), MemoryConstants::pageSize2Mb));
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::HEAP_EXTENDED));
        memoryManager->freeGraphicsMemory(allocation);
    }
    {
        DebugManager.flags.AlignLocalMemoryVaTo2MB.set(0);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::HEAP_EXTENDED));
        memoryManager->freeGraphicsMemory(allocation);
    }
    {
        DebugManager.flags.AlignLocalMemoryVaTo2MB.set(1);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::HEAP_EXTENDED));
        EXPECT_TRUE(isAligned(allocation->getGpuAddress(), MemoryConstants::pageSize2Mb));
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerLocalMemoryAlignmentTest, givenCustomAlignmentWhenAllocatingAllocationBiggerThanTheAlignmentThenAlignProperly) {
    AllocationData allocationData;
    allocationData.allFlags = 0;
    allocationData.flags.allocateMemory = true;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.type = GraphicsAllocation::AllocationType::BUFFER;
    allocationData.flags.resource48Bit = true;
    MemoryManager::AllocationStatus allocationStatus;

    {
        // size==2MB, use 2MB heap
        DebugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(2 * MemoryConstants::megaByte);
        allocationData.size = 2 * MemoryConstants::megaByte;
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::HEAP_STANDARD2MB));
        EXPECT_TRUE(isAligned(allocation->getGpuAddress(), 2 * MemoryConstants::megaByte));
        memoryManager->freeGraphicsMemory(allocation);
    }

    {
        // size > 2MB, use 2MB heap
        DebugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(16 * MemoryConstants::megaByte);
        allocationData.size = 16 * MemoryConstants::megaByte;
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::HEAP_STANDARD2MB));
        EXPECT_TRUE(isAligned(allocation->getGpuAddress(), 16 * MemoryConstants::megaByte));
        memoryManager->freeGraphicsMemory(allocation);
    }

    {
        // size < 2MB, use 64KB heap
        DebugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(8 * MemoryConstants::pageSize64k);
        allocationData.size = 8 * MemoryConstants::pageSize64k;
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::HEAP_STANDARD64KB));
        EXPECT_TRUE(isAligned(allocation->getGpuAddress(), 8 * MemoryConstants::pageSize64k));
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerLocalMemoryAlignmentTest, givenCustomAlignmentWhenAllocatingAllocationLessThanTheAlignmentThenIgnoreCustomAlignment) {
    AllocationData allocationData;
    allocationData.allFlags = 0;
    allocationData.size = 3 * MemoryConstants::megaByte;
    allocationData.flags.allocateMemory = true;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.type = GraphicsAllocation::AllocationType::BUFFER;
    allocationData.flags.resource48Bit = true;
    MemoryManager::AllocationStatus allocationStatus;

    {
        // Too small allocation, fallback to 64KB heap
        DebugManager.flags.AlignLocalMemoryVaTo2MB.set(0);
        DebugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(32 * MemoryConstants::megaByte);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::HEAP_STANDARD64KB));
        memoryManager->freeGraphicsMemory(allocation);
    }

    {
        // Too small allocation, fallback to 2MB heap
        DebugManager.flags.AlignLocalMemoryVaTo2MB.set(1);
        DebugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(32 * MemoryConstants::megaByte);
        auto memoryManager = createMemoryManager();
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocationData, allocationStatus);
        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::Success, allocationStatus);
        EXPECT_TRUE(isAllocationWithinHeap(*memoryManager, *allocation, HeapIndex::HEAP_STANDARD2MB));
        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenUnsupportedTypeWhenAllocatingInDevicePoolThenRetryInNonDevicePoolStatusAndNullptrIsReturned) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.rootDeviceIndex = rootDeviceIndex;

    GraphicsAllocation::AllocationType unsupportedTypes[] = {GraphicsAllocation::AllocationType::SHARED_RESOURCE_COPY};

    for (auto unsupportedType : unsupportedTypes) {
        allocData.type = unsupportedType;

        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
        ASSERT_EQ(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);

        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenOversizedAllocationWhenGraphicsAllocationInDevicePoolIsAllocatedThenAllocationAndBufferObjectHaveRequestedSize) {
    auto heap = HeapIndex::HEAP_STANDARD64KB;
    if (memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::HEAP_EXTENDED)) {
        heap = HeapIndex::HEAP_EXTENDED;
    }
    auto largerSize = 6 * MemoryConstants::megaByte;

    auto gpuAddress0 = memoryManager->getGfxPartition(0)->heapAllocateWithCustomAlignment(heap, largerSize, MemoryConstants::pageSize2Mb);
    EXPECT_NE(0u, gpuAddress0);
    EXPECT_EQ(6 * MemoryConstants::megaByte, largerSize);
    auto gpuAddress1 = memoryManager->getGfxPartition(0)->heapAllocate(heap, largerSize);
    EXPECT_NE(0u, gpuAddress1);
    EXPECT_EQ(6 * MemoryConstants::megaByte, largerSize);
    auto gpuAddress2 = memoryManager->getGfxPartition(0)->heapAllocate(heap, largerSize);
    EXPECT_NE(0u, gpuAddress2);
    EXPECT_EQ(6 * MemoryConstants::megaByte, largerSize);
    memoryManager->getGfxPartition(0)->heapFree(heap, gpuAddress1, largerSize);

    auto status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.size = 5 * MemoryConstants::megaByte;
    allocData.type = GraphicsAllocation::AllocationType::BUFFER;
    allocData.rootDeviceIndex = rootDeviceIndex;
    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    memoryManager->getGfxPartition(0)->heapFree(heap, gpuAddress2, largerSize);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(largerSize, allocation->getReservedAddressSize());
    EXPECT_EQ(allocData.size, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(allocData.size, static_cast<DrmAllocation *>(allocation)->getBO()->peekSize());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenDrmMemoryManagerWithLocalMemoryWhenLockResourceIsCalledOnNullBufferObjectThenReturnNullPtr) {
    auto ptr = memoryManager->lockResourceInLocalMemoryImpl(nullptr);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockResourceInLocalMemoryImpl(nullptr);
}

TEST_F(DrmMemoryManagerLocalMemoryWithCustomMockTest, givenDrmMemoryManagerWithLocalMemoryWhenLockResourceIsCalledOnBufferObjectThenReturnPtr) {
    BufferObject bo(mock, 1, 1024, 0);

    DrmAllocation drmAllocation(0, GraphicsAllocation::AllocationType::UNKNOWN, &bo, nullptr, 0u, 0u, MemoryPool::LocalMemory);
    EXPECT_EQ(&bo, drmAllocation.getBO());

    auto ptr = memoryManager->lockResourceInLocalMemoryImpl(&bo);
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(ptr, bo.peekLockedAddress());

    memoryManager->unlockResourceInLocalMemoryImpl(&bo);
    EXPECT_EQ(nullptr, bo.peekLockedAddress());
}

TEST_F(DrmMemoryManagerLocalMemoryWithCustomMockTest, givenDrmMemoryManagerWithLocalMemoryWhenLockResourceIsCalledOnWriteCombinedAllocationThenReturnPtrAlignedTo64Kb) {
    BufferObject bo(mock, 1, 1024, 0);

    DrmAllocation drmAllocation(0, GraphicsAllocation::AllocationType::WRITE_COMBINED, &bo, nullptr, 0u, 0u, MemoryPool::LocalMemory);
    EXPECT_EQ(&bo, drmAllocation.getBO());

    auto ptr = memoryManager->lockResourceInLocalMemoryImpl(drmAllocation);
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(ptr, bo.peekLockedAddress());
    EXPECT_TRUE(isAligned<MemoryConstants::pageSize64k>(ptr));

    memoryManager->unlockResourceInLocalMemoryImpl(&bo);
    EXPECT_EQ(nullptr, bo.peekLockedAddress());
}

using DrmMemoryManagerFailInjectionTest = Test<DrmMemoryManagerFixtureDg1>;

TEST_F(DrmMemoryManagerFailInjectionTest, givenEnabledLocalMemoryWhenNewFailsThenAllocateInDevicePoolReturnsStatusErrorAndNullallocation) {
    mock->ioctl_expected.total = -1; //don't care
    class MockGfxPartition : public GfxPartition {
      public:
        MockGfxPartition() : GfxPartition(reservedCpuAddressRange) {
            init(defaultHwInfo->capabilityTable.gpuAddressSpace, getSizeToReserve(), 0, 1);
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
        allocData.type = GraphicsAllocation::AllocationType::BUFFER;
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

    mock->memoryInfo.reset(new MockMemoryInfo());
    injectFailures(method);
}

using DrmMemoryManagerCopyMemoryToAllocationTest = DrmMemoryManagerLocalMemoryTest;

struct DrmMemoryManagerToTestCopyMemoryToAllocation : public DrmMemoryManager {
    using DrmMemoryManager::allocateGraphicsMemoryInDevicePool;
    DrmMemoryManagerToTestCopyMemoryToAllocation(ExecutionEnvironment &executionEnvironment, bool localMemoryEnabled, size_t lockableLocalMemorySize)
        : DrmMemoryManager(gemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment) {
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
    void *lockResourceInLocalMemoryImpl(BufferObject *bo) override {
        if (lockedLocalMemorySize > 0) {
            lockedLocalMemory.reset(new uint8_t[lockedLocalMemorySize]);
            return lockedLocalMemory.get();
        }
        return nullptr;
    }
    void unlockResourceInLocalMemoryImpl(BufferObject *bo) override {
    }
    void unlockResourceImpl(GraphicsAllocation &graphicsAllocation) override {
    }
    std::unique_ptr<uint8_t[]> lockedLocalMemory;
    size_t lockedLocalMemorySize = 0;
};

TEST_F(DrmMemoryManagerCopyMemoryToAllocationTest, givenDrmMemoryManagerWhenCopyMemoryToAllocationReturnsSuccessThenAllocationIsFilledWithCorrectData) {
    size_t offset = 3;
    size_t sourceAllocationSize = MemoryConstants::pageSize;
    size_t destinationAllocationSize = sourceAllocationSize + offset;

    DrmMemoryManagerToTestCopyMemoryToAllocation drmMemoryManger(*executionEnvironment, true, destinationAllocationSize);
    std::vector<uint8_t> dataToCopy(sourceAllocationSize, 1u);

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = destinationAllocationSize;
    allocData.flags.allocateMemory = true;
    allocData.type = GraphicsAllocation::AllocationType::KERNEL_ISA;
    allocData.rootDeviceIndex = rootDeviceIndex;
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    auto allocation = drmMemoryManger.allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);

    auto ret = drmMemoryManger.copyMemoryToAllocation(allocation, offset, dataToCopy.data(), dataToCopy.size());
    EXPECT_TRUE(ret);

    EXPECT_EQ(0, memcmp(ptrOffset(drmMemoryManger.lockedLocalMemory.get(), offset), dataToCopy.data(), dataToCopy.size()));

    drmMemoryManger.freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerCopyMemoryToAllocationTest, givenDrmMemoryManagerWhenCopyMemoryToAllocationFailsToLockResourceThenItReturnsFalse) {
    DrmMemoryManagerToTestCopyMemoryToAllocation drmMemoryManger(*executionEnvironment, true, 0);
    std::vector<uint8_t> dataToCopy(MemoryConstants::pageSize, 1u);

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = dataToCopy.size();
    allocData.flags.allocateMemory = true;
    allocData.type = GraphicsAllocation::AllocationType::KERNEL_ISA;
    allocData.rootDeviceIndex = rootDeviceIndex;
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

    auto allocation = drmMemoryManger.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, destinationAllocationSize, GraphicsAllocation::AllocationType::KERNEL_ISA, mockDeviceBitfield});
    ASSERT_NE(nullptr, allocation);

    auto ret = drmMemoryManger.copyMemoryToAllocation(allocation, offset, dataToCopy.data(), dataToCopy.size());
    EXPECT_TRUE(ret);

    EXPECT_EQ(0, memcmp(ptrOffset(allocation->getUnderlyingBuffer(), offset), dataToCopy.data(), dataToCopy.size()));

    drmMemoryManger.freeGraphicsMemory(allocation);
}

using DrmMemoryManagerTestDg1 = Test<DrmMemoryManagerFixtureDg1>;

TEST_F(DrmMemoryManagerTestDg1, givenDrmMemoryManagerWhenLockUnlockIsCalledOnAllocationInLocalMemoryThenCallIoctlGemMapOffsetAndReturnLockedPtr) {
    mockDg1->ioctlDg1_expected.gemCreateExt = 1;
    mockDg1->ioctl_expected.gemWait = 1;
    mockDg1->ioctl_expected.gemClose = 1;
    mockDg1->ioctlDg1_expected.gemMmapOffset = 1;
    mockDg1->memoryInfo.reset(new MockMemoryInfo());

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.type = GraphicsAllocation::AllocationType::INTERNAL_HEAP;
    allocData.rootDeviceIndex = rootDeviceIndex;
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());

    auto ptr = memoryManager->lockResource(allocation);
    EXPECT_NE(nullptr, ptr);

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    EXPECT_NE(nullptr, drmAllocation->getBO()->peekLockedAddress());

    EXPECT_EQ(static_cast<uint32_t>(drmAllocation->getBO()->peekHandle()), mockDg1->mmapOffsetHandle);
    EXPECT_EQ(0u, mockDg1->mmapOffsetPad);
    EXPECT_EQ(0u, mockDg1->mmapOffsetOffset);
    EXPECT_EQ((uint64_t)I915_MMAP_OFFSET_WC, mockDg1->mmapOffsetFlags);

    memoryManager->unlockResource(allocation);
    EXPECT_EQ(nullptr, drmAllocation->getBO()->peekLockedAddress());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerTestDg1, givenDrmMemoryManagerWhenLockUnlockIsCalledOnAllocationInLocalMemoryButFailsOnIoctlMmapOffsetThenReturnNullPtr) {
    mockDg1->ioctlDg1_expected.gemMmapOffset = 1;
    this->ioctlResExt = {mockDg1->ioctl_cnt.total, -1};
    mockDg1->ioctl_res_ext = &ioctlResExt;

    BufferObject bo(mockDg1, 1, 0, 0);
    DrmAllocation drmAllocation(0, GraphicsAllocation::AllocationType::UNKNOWN, &bo, nullptr, 0u, 0u, MemoryPool::LocalMemory);
    EXPECT_NE(nullptr, drmAllocation.getBO());

    auto ptr = memoryManager->lockResource(&drmAllocation);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockResource(&drmAllocation);
    mockDg1->ioctl_res_ext = &mockDg1->NONE;
}

TEST_F(DrmMemoryManagerTestDg1, givenDrmMemoryManagerWhenLockUnlockIsCalledOnAllocationInLocalMemoryButBufferObjectIsNullThenReturnNullPtr) {
    DrmAllocation drmAllocation(0, GraphicsAllocation::AllocationType::UNKNOWN, nullptr, nullptr, 0u, 0u, MemoryPool::LocalMemory);

    auto ptr = memoryManager->lockResource(&drmAllocation);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockResource(&drmAllocation);
}

TEST_F(DrmMemoryManagerTestDg1, givenDrmMemoryManagerWhenGetLocalMemorySizeIsCalledForMemoryInfoThenReturnMemoryRegionSize) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = new DrmMock(*executionEnvironment.rootDeviceEnvironments[0]);
    drm->memoryInfo.reset(new MockMemoryInfo());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    TestedDrmMemoryManager memoryManager(executionEnvironment);

    auto memoryInfo = static_cast<MemoryInfoImpl *>(drm->getMemoryInfo());
    ASSERT_NE(nullptr, memoryInfo);
    EXPECT_EQ(memoryInfo->getMemoryRegionSize(MemoryBanks::getBankForLocalMemory(0)), memoryManager.getLocalMemorySize(0u, 0xF));
}

TEST_F(DrmMemoryManagerTestDg1, givenDrmMemoryManagerWhenGetLocalMemorySizeIsCalledButMemoryInfoIsNotAvailableThenSizeZeroIsReturned) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = new DrmMock(*executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    TestedDrmMemoryManager memoryManager(executionEnvironment);

    EXPECT_EQ(0u, memoryManager.getLocalMemorySize(0u, 0xF));
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenGraphicsAllocationInDevicePoolIsAllocatedForImage1DWhenTheSizeReturnedFromGmmIsUnalignedThenCreateBufferObjectWithSizeAlignedTo64KB) {
    cl_image_desc imgDesc = {};
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imgDesc.image_width = 100;
    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = GraphicsAllocation::AllocationType::IMAGE;
    allocData.flags.resource48Bit = true;
    allocData.imgInfo = &imgInfo;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_TRUE(allocData.imgInfo->useLocalMemory);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());

    auto gmm = allocation->getDefaultGmm();
    EXPECT_NE(nullptr, gmm);
    EXPECT_FALSE(gmm->useSystemMemoryPool);

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

static uint32_t munmapCalledCount = 0u;

TEST_F(DrmMemoryManagerLocalMemoryTest, givenAlignmentAndSizeWhenMmapReturnsUnalignedPointerThenCreateAllocWithAlignmentUnmapTwoUnalignedPart) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableBOMmapCreate.set(-1);

    drm_i915_memory_region_info regionInfo[2] = {};
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, 0};

    mock->memoryInfo.reset(new MemoryInfoImpl(regionInfo, 2));
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
    EXPECT_EQ(munmapCalledCount, 2u);
    munmapCalledCount = 0u;
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenAlignmentAndSizeWhenMmapReturnsAlignedThenCreateAllocWithAlignmentUnmapOneUnalignedPart) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableBOMmapCreate.set(-1);

    drm_i915_memory_region_info regionInfo[2] = {};
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, 0};

    mock->memoryInfo.reset(new MemoryInfoImpl(regionInfo, 2));
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
    auto allocation = memoryManager->createAllocWithAlignment(allocationData, MemoryConstants::pageSize, 1u, MemoryConstants::pageSize64k, 0u);

    EXPECT_EQ(reinterpret_cast<void *>(0x12345678), allocation->getMmapPtr());
    EXPECT_EQ(munmapCalledCount, 1u);
    munmapCalledCount = 0u;
    memoryManager->freeGraphicsMemory(allocation);
}

} // namespace NEO

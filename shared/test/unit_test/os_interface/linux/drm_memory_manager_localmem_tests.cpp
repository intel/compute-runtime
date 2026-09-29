/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/heap_assigner.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/memory_manager/compression_selector.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/memory_manager/unified_memory_properties.h"
#include "shared/source/os_interface/linux/allocator_helper.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/utilities/heap_allocator.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/libult/linux/drm_mock_helper.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/linux/mock_ioctl_helper_with_capture.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gfx_partition.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/os_interface/linux/drm_memory_manager_fixture.h"
#include "shared/test/common/os_interface/linux/drm_mock_memory_info.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

#include <array>
#include <cstring>

namespace NEO {

struct DrmMemoryManagerToTestCopyMemoryToAllocation : public DrmMemoryManager {
    using DrmMemoryManager::allocateGraphicsMemoryInDevicePool;
    DrmMemoryManagerToTestCopyMemoryToAllocation(ExecutionEnvironment &executionEnvironment, bool localMemoryEnabled, size_t lockableLocalMemorySize)
        : DrmMemoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment) {
        std::fill(this->localMemorySupported.begin(), this->localMemorySupported.end(), localMemoryEnabled);
        lockedLocalMemorySize = lockableLocalMemorySize;
    }
    void *lockResourceImpl(GraphicsAllocation &graphicsAllocation) override {
        if (lockedLocalMemorySize > 0) {
            lockedLocalMemory[0].reset(new uint8_t[lockedLocalMemorySize]);
            return lockedLocalMemory[0].get();
        }
        return nullptr;
    }
    void *lockBufferObject(BufferObject *bo) override {
        if (lockedLocalMemorySize > 0) {
            deviceIndex = (deviceIndex < 3) ? deviceIndex + 1u : 0u;
            lockedLocalMemory[deviceIndex].reset(new uint8_t[lockedLocalMemorySize]);
            return lockedLocalMemory[deviceIndex].get();
        }
        return nullptr;
    }
    void unlockBufferObject(BufferObject *bo) override {
    }
    void unlockResourceImpl(GraphicsAllocation &graphicsAllocation) override {
    }
    bool copyMemoryToAllocationBanks(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy, DeviceBitfield handleMask) override {
        copyMemoryToAllocationBanksCalled++;
        return DrmMemoryManager::copyMemoryToAllocationBanks(graphicsAllocation, destinationOffset, memoryToCopy, sizeToCopy, handleMask);
    }
    std::array<std::unique_ptr<uint8_t[]>, 4> lockedLocalMemory;
    uint32_t deviceIndex = 3;
    size_t lockedLocalMemorySize = 0;
    uint32_t copyMemoryToAllocationBanksCalled = 0;
};

struct DrmMemoryManagerToTestMemsetAllocation : public DrmMemoryManager {
    using DrmMemoryManager::allocateGraphicsMemoryInDevicePool;
    DrmMemoryManagerToTestMemsetAllocation(ExecutionEnvironment &executionEnvironment, bool localMemoryEnabled, size_t lockableLocalMemorySize)
        : DrmMemoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment) {
        std::fill(this->localMemorySupported.begin(), this->localMemorySupported.end(), localMemoryEnabled);
        lockedLocalMemorySize = lockableLocalMemorySize;
    }
    void *lockResourceImpl(GraphicsAllocation &graphicsAllocation) override {
        if (lockedLocalMemorySize > 0) {
            lockedLocalMemory[0].reset(new uint8_t[lockedLocalMemorySize]);
            return lockedLocalMemory[0].get();
        }
        return nullptr;
    }
    void *lockBufferObject(BufferObject *bo) override {
        if (lockedLocalMemorySize > 0) {
            deviceIndex = (deviceIndex < 3) ? deviceIndex + 1u : 0u;
            lockedLocalMemory[deviceIndex].reset(new uint8_t[lockedLocalMemorySize]);
            return lockedLocalMemory[deviceIndex].get();
        }
        return nullptr;
    }
    void unlockBufferObject(BufferObject *bo) override {
    }
    void unlockResourceImpl(GraphicsAllocation &graphicsAllocation) override {
    }
    bool memsetAllocationBanks(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, int value, size_t sizeToSet, DeviceBitfield handleMask) override {
        memsetAllocationBanksCalled++;
        return DrmMemoryManager::memsetAllocationBanks(graphicsAllocation, destinationOffset, value, sizeToSet, handleMask);
    }
    std::array<std::unique_ptr<uint8_t[]>, 4> lockedLocalMemory;
    uint32_t deviceIndex = 3;
    size_t lockedLocalMemorySize = 0;
    uint32_t memsetAllocationBanksCalled = 0;
};

class DrmMemoryManagerFixtureImpl : public DrmMemoryManagerFixture {
  public:
    DrmMockCustom *mockExp;

    template <typename GfxFamily>
    void setUpT() {
        backup = std::make_unique<VariableBackup<UltHwConfig>>(&ultHwConfig);
        ultHwConfig.csrBaseCallCreatePreemption = false;

        DebugManagerStateRestore restore;
        debugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(NEO::PreemptionMode::Disabled));
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

class DrmMemoryManagerLocalMemoryTest : public ::testing::Test {
  public:
    void SetUp() override {
        debugManager.flags.EnableLocalMemory.set(1);
        executionEnvironment = new ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex];
        rootDeviceEnvironment.setHwInfoAndInitHelpers(defaultHwInfo.get());

        mock = createDrm(rootDeviceEnvironment);
        ioctlHelper = static_cast<MockIoctlHelperWithCapture *>(mock->getIoctlHelper());
        setUpMemoryInfo();

        rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
        rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<DriverModel>(mock));
        rootDeviceEnvironment.memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mock, 0u, false);

        memoryManager = new TestedDrmMemoryManager(true, false, false, *executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);

        device.reset(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, rootDeviceIndex));
        ioctlHelper->createGemExtCalls.clear();
    }

    virtual void setUpMemoryInfo() {
        mock->memoryInfo.reset(new MockMemoryInfo(*mock));
    }

    static DrmMock *createDrm(RootDeviceEnvironment &rootDeviceEnvironment) {
        auto drm = new DrmMock(rootDeviceEnvironment);
        drm->ioctlHelper = std::make_unique<MockIoctlHelperWithCapture>(*drm);
        return drm;
    }

    void setUpSingleDeviceMemoryRegion(DrmMock &drm) {
        auto drmIoctlHelper = drm.getIoctlHelper();
        std::vector<MemoryRegion> regionInfo(2);
        regionInfo[0].region = {static_cast<uint16_t>(drmIoctlHelper->getDrmParamValue(DrmParam::memoryClassSystem)), 0};
        regionInfo[1].region = {static_cast<uint16_t>(drmIoctlHelper->getDrmParamValue(DrmParam::memoryClassDevice)), 0};
        drm.memoryInfo.reset(new MemoryInfo(regionInfo, drm));
        drm.ioctlCallsCount = 0;
        static_cast<MockIoctlHelperWithCapture *>(drmIoctlHelper)->createGemExtCalls.clear();
    }

    DrmMock *createDrmForRootDevice(uint32_t index, bool withMemoryInfo) {
        auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[index];
        rootDeviceEnvironment.initGmm();
        auto drm = createDrm(rootDeviceEnvironment);
        if (withMemoryInfo) {
            setUpSingleDeviceMemoryRegion(*drm);
        } else {
            drm->memoryInfo.reset(nullptr);
            drm->ioctlCallsCount = 0;
        }
        rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
        rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
        rootDeviceEnvironment.memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
        rootDeviceEnvironment.initGmm();
        return drm;
    }

    DrmMock *getDrm(uint32_t index) {
        return static_cast<DrmMock *>(executionEnvironment->rootDeviceEnvironments[index]->osInterface->getDriverModel()->as<Drm>());
    }

    static uint32_t munmapCalledCount;

  protected:
    DebugManagerStateRestore restorer{};
    ExecutionEnvironment *executionEnvironment = nullptr;
    DrmMock *mock = nullptr;
    MockIoctlHelperWithCapture *ioctlHelper = nullptr;
    std::unique_ptr<MockDevice> device;
    TestedDrmMemoryManager *memoryManager = nullptr;
    const uint32_t rootDeviceIndex = 0u;
};

uint32_t DrmMemoryManagerLocalMemoryTest::munmapCalledCount = 0u;

class DrmMemoryManagerLocalMemoryMultiTileTest : public DrmMemoryManagerLocalMemoryTest {
  public:
    void setUpMemoryInfo() override {
        mock->memoryInfo.reset(new MockExtendedMemoryInfo(*mock));
    }
};

using DrmMemoryManagerUsmSharedHandleTest = DrmMemoryManagerLocalMemoryTest;

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMultiRootDeviceEnvironmentAndMemoryInfoWhenCreateMultiGraphicsAllocationThenImportAndExportIoctlAreUsed) {
    uint32_t rootDevicesNumber = 3u;
    MultiGraphicsAllocation multiGraphics(rootDevicesNumber);
    RootDeviceIndicesContainer rootDeviceIndices;
    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.release();

    executionEnvironment->prepareRootDeviceEnvironments(rootDevicesNumber);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        createDrmForRootDevice(i, true);
        rootDeviceIndices.pushUnique(i);
    }
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(true, false, false, *executionEnvironment);

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, {});

    getDrm(0)->outputFd = 7;

    auto ptr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, properties, multiGraphics);

    EXPECT_NE(ptr, nullptr);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        if (i != 0) {
            EXPECT_EQ(getDrm(i)->inputFd, 7);
        }
        ASSERT_NE(multiGraphics.getGraphicsAllocation(i), nullptr);
        EXPECT_NE(static_cast<DrmAllocation *>(multiGraphics.getGraphicsAllocation(i))->getMmapPtr(), nullptr);
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
        createDrmForRootDevice(i, true)->fdToHandleRetVal = -1;
        rootDeviceIndices.pushUnique(i);
    }
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(true, false, false, *executionEnvironment);

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, {});

    auto ptr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, properties, multiGraphics);

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
        createDrmForRootDevice(i, false)->fdToHandleRetVal = -1;
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
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        ASSERT_NE(multiGraphics.getGraphicsAllocation(i), nullptr);
        EXPECT_EQ(static_cast<DrmAllocation *>(multiGraphics.getGraphicsAllocation(i))->getMmapPtr(), nullptr);
        memoryManager->freeGraphicsMemory(multiGraphics.getGraphicsAllocation(i));
    }

    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenOsHandleWhenCreateIsCalledWithBufferHostMemoryAllocationTypeThenGraphicsAllocationIsReturned) {
    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    mock->outputHandle = 2u;
    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::bufferHostMemory, false, {});

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, true, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, graphicsAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(mock->inputFd, static_cast<int>(osHandleData.handle));
    EXPECT_EQ(mock->setTilingHandle, 0u);

    auto bo = static_cast<DrmAllocation *>(graphicsAllocation)->getBO();
    EXPECT_EQ(bo->peekHandle(), static_cast<int>(mock->outputHandle));
    EXPECT_NE(0llu, bo->peekAddress());
    EXPECT_EQ(1u, bo->getRefCount());
    EXPECT_EQ(size, bo->peekSize());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenSharedHandleImportFailureWhenCreatingUsmHostAllocationFromSharedHandleThenNullptrIsReturnedAndErrorDescriptionIsSet) {
    mock->fdToHandleRetVal = -1;

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, {});

    auto ptr = memoryManager->createUSMHostAllocationFromSharedHandle(1, properties, nullptr, true);
    EXPECT_EQ(ptr, nullptr);

    const char *systemErrorDescription = nullptr;
    executionEnvironment->getErrorDescription(&systemErrorDescription);

    char expectedErrorDescription[256];
    snprintf(expectedErrorDescription, 256, "ioctl(PRIME_FD_TO_HANDLE) failed with %d. errno=%d(%s)\n", mock->fdToHandleRetVal, mock->getErrno(), strerror(mock->getErrno()));
    EXPECT_FALSE(strncmp(expectedErrorDescription, systemErrorDescription, 256));
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMemoryInfoWhenAllocateWithAlignmentThenGemCreateExtIsUsed) {
    debugManager.flags.EnableBOMmapCreate.set(-1);
    setUpSingleDeviceMemoryRegion(*mock);

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize64k;

    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithAlignment(allocationData));

    ASSERT_NE(allocation, nullptr);
    EXPECT_NE(allocation->getMmapPtr(), nullptr);
    EXPECT_NE(allocation->getMmapSize(), 0u);
    EXPECT_EQ(allocation->getAllocationOffset(), 0u);
    ASSERT_EQ(1u, ioctlHelper->createGemExtCalls.size());
    EXPECT_EQ(static_cast<int>(ioctlHelper->createGemExtHandle), allocation->getBO()->peekHandle());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMemoryInfoAndNotUseObjectMmapPropertyWhenAllocateWithAlignmentThenUserptrIsUsed) {
    debugManager.flags.EnableBOMmapCreate.set(-1);
    setUpSingleDeviceMemoryRegion(*mock);
    ioctlHelper->retrieveMmapOffsetResult = false;

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize64k;
    allocationData.useMmapObject = false;

    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithAlignment(allocationData));

    ASSERT_NE(allocation, nullptr);
    EXPECT_EQ(static_cast<int>(mock->returnHandle), allocation->getBO()->peekHandle() + 1);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMemoryInfoAndFailedMmapOffsetWhenAllocateWithAlignmentThenNullptr) {
    debugManager.flags.EnableBOMmapCreate.set(-1);
    setUpSingleDeviceMemoryRegion(*mock);
    ioctlHelper->retrieveMmapOffsetResult = false;

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize64k;

    auto allocation = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);

    EXPECT_EQ(allocation, nullptr);
    ioctlHelper->retrieveMmapOffsetResult = true;
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMemoryInfoAndDisabledMmapBOCreationtWhenAllocateWithAlignmentThenUserptrIsUsed) {
    debugManager.flags.EnableBOMmapCreate.set(0);
    setUpSingleDeviceMemoryRegion(*mock);
    ioctlHelper->retrieveMmapOffsetResult = false;

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize64k;

    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithAlignment(allocationData));

    ASSERT_NE(allocation, nullptr);
    EXPECT_EQ(static_cast<int>(mock->returnHandle), allocation->getBO()->peekHandle() + 1);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMemoryInfoAndFailedGemCreateExtWhenAllocateWithAlignmentThenNullptr) {
    debugManager.flags.EnableBOMmapCreate.set(-1);
    setUpSingleDeviceMemoryRegion(*mock);
    ioctlHelper->createGemExtResult = -1;

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize64k;

    auto allocation = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);

    EXPECT_EQ(allocation, nullptr);
    ioctlHelper->createGemExtResult = 0;
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCpuAccessRequiredWhenAllocatingInDevicePoolThenAllocationIsLocked) {
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

TEST_F(DrmMemoryManagerLocalMemoryTest, givenWriteCombinedAllocationWhenAllocatingInDevicePoolThenAllocationIsLockedAndLockedPtrIsUsedAsGpuAddress) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData{};
    allocData.size = MemoryConstants::pageSize;
    allocData.type = AllocationType::writeCombined;
    allocData.rootDeviceIndex = rootDeviceIndex;
    auto sizeAligned = alignUp(allocData.size + MemoryConstants::pageSize64k, 2 * MemoryConstants::megaByte) + 2 * MemoryConstants::megaByte;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
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

    auto bo = static_cast<DrmAllocation *>(allocation)->getBO();
    ASSERT_NE(nullptr, bo);
    EXPECT_EQ(reinterpret_cast<uint64_t>(cpuAddress), bo->peekAddress());
    EXPECT_EQ(sizeAligned, bo->peekSize());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenSupportedTypeWhenAllocatingInDevicePoolThenSuccessStatusAndNonNullPtrIsReturned) {
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
                                       AllocationType::kernelIsaInternal,
                                       AllocationType::debugModuleArea,
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
            case AllocationType::kernelIsaInternal:
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
            auto gfxPartition = memoryManager->getGfxPartition(0);
            if (allocation->getAllocationType() == AllocationType::svmGpu) {
                if (!memoryManager->isLimitedRange(0)) {
                    EXPECT_LT(gmmHelper->canonize(gfxPartition->getHeapBase(HeapIndex::heapSvm)), gpuAddress);
                    EXPECT_GT(gmmHelper->canonize(gfxPartition->getHeapLimit(HeapIndex::heapSvm)), gpuAddress);
                }
            } else if (memoryManager->heapAssigners[rootDeviceIndex]->useInternal32BitHeap(allocation->getAllocationType())) {
                EXPECT_LT(gmmHelper->canonize(gfxPartition->getHeapBase(HeapIndex::heapInternalDeviceMemory)), gpuAddress);
                EXPECT_GT(gmmHelper->canonize(gfxPartition->getHeapLimit(HeapIndex::heapInternalDeviceMemory)), gpuAddress);
            } else {
                const bool prefer2MBAlignment = allocation->getUnderlyingBufferSize() >= 2 * MemoryConstants::megaByte;
                const bool prefer57bitAddressing = gfxPartition->getHeapLimit(HeapIndex::heapExtended) > 0 && !allocData.flags.resource48Bit;

                auto heap = HeapIndex::heapStandard64KB;
                if (prefer57bitAddressing) {
                    heap = HeapIndex::heapExtended;
                } else if (prefer2MBAlignment) {
                    heap = HeapIndex::heapStandard2MB;
                }

                EXPECT_LT(gmmHelper->canonize(gfxPartition->getHeapBase(heap)), gpuAddress);
                EXPECT_GT(gmmHelper->canonize(gfxPartition->getHeapLimit(heap)), gpuAddress);
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

TEST_F(DrmMemoryManagerLocalMemoryTest, givenDrmMemoryManagerWhenCopyMemoryToAllocationReturnsSuccessThenAllocationIsFilledWithCorrectData) {
    size_t offset = 3;
    size_t sourceAllocationSize = MemoryConstants::pageSize;
    size_t destinationAllocationSize = sourceAllocationSize + offset;

    DrmMemoryManagerToTestCopyMemoryToAllocation drmMemoryManager(*executionEnvironment, true, destinationAllocationSize);
    std::vector<uint8_t> dataToCopy(sourceAllocationSize, 1u);

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = destinationAllocationSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::kernelIsa;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks.set(0, true);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    auto allocation = drmMemoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);

    auto ret = drmMemoryManager.copyMemoryToAllocation(allocation, offset, dataToCopy.data(), dataToCopy.size());
    EXPECT_TRUE(ret);

    EXPECT_EQ(0, memcmp(ptrOffset(drmMemoryManager.lockedLocalMemory[0].get(), offset), dataToCopy.data(), dataToCopy.size()));

    drmMemoryManager.freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenDrmMemoryManagerWhenCopyMemoryToAllocationFailsToLockResourceThenItReturnsFalse) {
    DrmMemoryManagerToTestCopyMemoryToAllocation drmMemoryManager(*executionEnvironment, true, 0);
    std::vector<uint8_t> dataToCopy(MemoryConstants::pageSize, 1u);

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = dataToCopy.size();
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::kernelIsa;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks.set(0, true);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    auto allocation = drmMemoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);

    auto ret = drmMemoryManager.copyMemoryToAllocation(allocation, 0, dataToCopy.data(), dataToCopy.size());
    EXPECT_FALSE(ret);

    drmMemoryManager.freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenDrmMemoryManagerWhenCopyMemoryToAllocationWithCpuPtrThenAllocationIsFilledWithCorrectData) {
    size_t offset = 3;
    size_t sourceAllocationSize = MemoryConstants::pageSize;
    size_t destinationAllocationSize = sourceAllocationSize + offset;

    DrmMemoryManagerToTestCopyMemoryToAllocation drmMemoryManager(*executionEnvironment, false, 0);
    std::vector<uint8_t> dataToCopy(sourceAllocationSize, 1u);

    auto allocation = drmMemoryManager.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, destinationAllocationSize, AllocationType::kernelIsa, mockDeviceBitfield});
    ASSERT_NE(nullptr, allocation);

    auto ret = drmMemoryManager.copyMemoryToAllocation(allocation, offset, dataToCopy.data(), dataToCopy.size());
    EXPECT_TRUE(ret);

    EXPECT_EQ(0, memcmp(ptrOffset(allocation->getUnderlyingBuffer(), offset), dataToCopy.data(), dataToCopy.size()));

    drmMemoryManager.freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenGraphicsAllocationInDevicePoolIsAllocatedForImage1DWhenTheSizeReturnedFromGmmIsUnalignedThenCreateBufferObjectWithAlignedSize) {
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
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    EXPECT_TRUE(allocData.imgInfo->useLocalMemory);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());

    auto gmm = allocation->getDefaultGmm();
    ASSERT_NE(nullptr, gmm);
    auto *gmmResourceParams = reinterpret_cast<GMM_RESCREATE_PARAMS *>(gmm->resourceParamsData.data());
    EXPECT_EQ(0u, gmmResourceParams->Flags.Info.NonLocalOnly);

    auto gpuAddress = allocation->getGpuAddress();
    auto &productHelper = *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper;
    auto alignmentSize = productHelper.is2MBLocalMemAlignmentEnabled() ? MemoryConstants::pageSize2M : MemoryConstants::pageSize64k;
    auto sizeAligned = alignUp(allocData.imgInfo->size, alignmentSize);
    EXPECT_NE(0u, gpuAddress);
    EXPECT_EQ(sizeAligned, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(gpuAddress, reinterpret_cast<uint64_t>(allocation->getReservedAddressPtr()));
    EXPECT_EQ(sizeAligned, allocation->getReservedAddressSize());

    auto bo = static_cast<DrmAllocation *>(allocation)->getBO();
    ASSERT_NE(nullptr, bo);
    EXPECT_EQ(gpuAddress, bo->peekAddress());
    EXPECT_EQ(sizeAligned, bo->peekSize());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenAlignmentAndSizeWhenMmapReturnsUnalignedPointerThenCreateAllocWithAlignmentUnmapTwoUnalignedPart) {
    debugManager.flags.EnableBOMmapCreate.set(-1);
    setUpSingleDeviceMemoryRegion(*mock);

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
        DrmMemoryManagerLocalMemoryTest::munmapCalledCount++;
        return 0;
    };

    munmapCalledCount = 0u;
    auto allocation = memoryManager->createAllocWithAlignment(allocationData, MemoryConstants::pageSize, MemoryConstants::pageSize64k, MemoryConstants::pageSize64k, 0u);
    ASSERT_NE(nullptr, allocation);
    EXPECT_NE(allocation->getDefaultGmm(), nullptr);

    EXPECT_EQ(alignUp(reinterpret_cast<void *>(0x12345678), MemoryConstants::pageSize64k), allocation->getMmapPtr());
    EXPECT_EQ(1u, munmapCalledCount);
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(3u, munmapCalledCount);
    munmapCalledCount = 0u;

    memoryManager->mmapFunction = SysCalls::mmap;
    memoryManager->munmapFunction = SysCalls::munmap;
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenAlignmentAndSizeWhenMmapReturnsAlignedThenCreateAllocWithAlignmentUnmapOneUnalignedPart) {
    debugManager.flags.EnableBOMmapCreate.set(-1);
    setUpSingleDeviceMemoryRegion(*mock);

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize64k;

    memoryManager->mmapFunction = [](void *addr, size_t len, int prot,
                                     int flags, int fd, off_t offset) throw() {
        if (addr == 0) {
            return reinterpret_cast<void *>(0x10000000);
        } else {
            return addr;
        }
    };

    memoryManager->munmapFunction = [](void *addr, size_t len) throw() {
        DrmMemoryManagerLocalMemoryTest::munmapCalledCount++;
        return 0;
    };

    munmapCalledCount = 0u;
    auto allocation = memoryManager->createAllocWithAlignment(allocationData, MemoryConstants::pageSize, MemoryConstants::pageSize64k, MemoryConstants::pageSize64k, 0u);
    ASSERT_NE(nullptr, allocation);

    EXPECT_EQ(reinterpret_cast<void *>(0x10000000), allocation->getMmapPtr());
    EXPECT_EQ(1u, munmapCalledCount);
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(2u, munmapCalledCount);
    munmapCalledCount = 0u;

    memoryManager->mmapFunction = SysCalls::mmap;
    memoryManager->munmapFunction = SysCalls::munmap;
}

TEST_F(DrmMemoryManagerLocalMemoryMultiTileTest, givenAllocationWithInvalidCacheRegionWhenAllocatingInDevicePoolThenReturnNullptr) {
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
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenDrmMemoryManagerWithLocalMemoryWhenLockResourceIsCalledOnBufferObjectThenReturnPtr) {
    BufferObject bo(0, mock, 3, 1, 1024, 1);

    DrmAllocation drmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, &bo, nullptr, 0u, 0u, MemoryPool::localMemory);
    EXPECT_EQ(&bo, drmAllocation.getBO());

    auto ptr = memoryManager->lockBufferObject(&bo);
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(ptr, bo.peekLockedAddress());

    memoryManager->unlockBufferObject(&bo);
    EXPECT_EQ(nullptr, bo.peekLockedAddress());
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenDrmMemoryManagerWhenCreateBufferObjectInMemoryRegionIsCalledThenBufferObjectWithAGivenGpuAddressAndSizeIsCreatedAndAllocatedInASpecifiedMemoryRegion) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);
    setUpSingleDeviceMemoryRegion(*mock);

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
    ASSERT_EQ(1u, ioctlHelper->createGemExtCalls.size());
    EXPECT_EQ(static_cast<int>(ioctlHelper->createGemExtHandle), bo->peekHandle());
    EXPECT_EQ(size, ioctlHelper->createGemExtCalls[0].allocSize);

    ASSERT_EQ(1u, ioctlHelper->createGemExtCalls[0].memClassInstances.size());
    auto memRegion = ioctlHelper->createGemExtCalls[0].memClassInstances[0];
    EXPECT_EQ(ioctlHelper->getDrmParamValue(DrmParam::memoryClassDevice), memRegion.memoryClass);
    EXPECT_EQ(0u, memRegion.memoryInstance);

    EXPECT_EQ(gpuAddress, bo->peekAddress());
    EXPECT_EQ(size, bo->peekSize());
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenVmAdviseAtomicAttributeNotSupportedWhenAllocatingUnifiedMemoryInDevicePoolThenNullptrIsReturned) {
    ioctlHelper->vmAdviseAtomicAttribute = 0u;
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = 18 * MemoryConstants::pageSize64k;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::unifiedSharedMemory;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);
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
    DrmMock *mock;

    void SetUp() override {
        const bool localMemoryEnabled = true;
        executionEnvironment = new ExecutionEnvironment;
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        mock = DrmMemoryManagerLocalMemoryTest::createDrm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
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

using DrmMemoryManagerFailInjectionTest = Test<DrmMemoryManagerFixtureImpl>;

HWTEST_TEMPLATED_F(DrmMemoryManagerFailInjectionTest, givenEnabledLocalMemoryWhenNewFailsThenAllocateInDevicePoolReturnsStatusErrorAndNullallocation) {
    mock->ioctlExpected.total = -1; // don't care
    class MockGfxPartition : public GfxPartition {
      public:
        MockGfxPartition(const ProductHelper *productHelper) : GfxPartition(reservedCpuAddressRange) {
            init(defaultHwInfo->capabilityTable.gpuAddressSpace, getSizeToReserve(), 0, 1, false, 0u, defaultHwInfo->capabilityTable.gpuAddressSpace + 1, productHelper);
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
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getProductHelper();
    TestedDrmMemoryManager testedMemoryManager(true, false, true, *executionEnvironment);
    testedMemoryManager.overrideGfxPartition(new MockGfxPartition(&productHelper));

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

using DrmMemoryManagerTestImpl = Test<DrmMemoryManagerFixtureImpl>;

HWTEST2_TEMPLATED_F(DrmMemoryManagerTestImpl, givenDrmMemoryManagerWhenLockUnlockIsCalledOnAllocationInLocalMemoryThenCallIoctlGemMapOffsetAndReturnLockedPtr, IsAtMostXeCore) {
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

HWTEST2_TEMPLATED_F(DrmMemoryManagerTestImpl, givenDrmMemoryManagerWhenLockUnlockIsCalledOnAllocationInLocalMemoryButFailsOnMmapThenReturnNullPtr, IsAtMostXeCore) {
    mockExp->ioctlExpected.gemMmapOffset = 2;
    DrmMockCustom::IoctlResExt ioctlResExtMmapFailure = {mockExp->ioctlCnt.total, -1};
    mockExp->ioctlResExt = &ioctlResExtMmapFailure;

    BufferObject bo(0, mockExp, 3, 1, 0, 0);
    DrmAllocation drmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, &bo, nullptr, 0u, 0u, MemoryPool::localMemory);
    EXPECT_NE(nullptr, drmAllocation.getBO());

    auto ptr = memoryManager->lockResource(&drmAllocation);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockResource(&drmAllocation);
    mockExp->ioctlResExt = &mockExp->none;
}

HWTEST2_TEMPLATED_F(DrmMemoryManagerTestImpl, givenDrmMemoryManagerWhenLockUnlockIsCalledOnAllocationInLocalMemoryButFailsOnIoctlMmapFunctionOffsetThenReturnNullPtr, IsAtMostXeCore) {
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
    auto drm = DrmMemoryManagerLocalMemoryTest::createDrm(*executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    drm->memoryInfo.reset(new MockMemoryInfo(*drm));
    TestedDrmMemoryManager memoryManager(executionEnvironment);

    auto memoryInfo = drm->getMemoryInfo();
    ASSERT_NE(nullptr, memoryInfo);

    auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo();
    auto deviceMask = std::max(static_cast<uint32_t>(maxNBitValue(hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount)), 1u);

    EXPECT_EQ(memoryInfo->getMemoryRegionSize(deviceMask), memoryManager.getLocalMemorySize(0u, deviceMask));
    EXPECT_EQ(memoryInfo->getMemoryRegionSize(1), memoryManager.getLocalMemorySize(0u, 1));
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

class DrmMemoryManagerAllocation57BitTest : public DrmMemoryManagerLocalMemoryTest,
                                            public ::testing::WithParamInterface<AllocationType> {
  public:
    void SetUp() override { DrmMemoryManagerLocalMemoryTest::SetUp(); }
    void TearDown() override { DrmMemoryManagerLocalMemoryTest::TearDown(); }
};

static const AllocationType allocation57Bit[] = {
    AllocationType::buffer,
    AllocationType::constantSurface,
    AllocationType::globalSurface,
    AllocationType::printfSurface,
    AllocationType::privateSurface,
    AllocationType::sharedBuffer,
};

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenMemoryInfoWhenCreateUSMHostAllocationFromSharedHandleSucceedsThenAllocationIsReturnedAndRegisteredInSystemMemoryForProperAccounting) {
    setUpSingleDeviceMemoryRegion(*mock);

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, {});

    auto allocation = memoryManager->createUSMHostAllocationFromSharedHandle(1, properties, nullptr, true);
    ASSERT_NE(allocation, nullptr);

    auto &sysMemAllocs = memoryManager->getSysMemAllocs();
    auto it = std::find(sysMemAllocs.begin(), sysMemAllocs.end(), allocation);
    EXPECT_NE(it, sysMemAllocs.end());
    EXPECT_GE(memoryManager->getUsedSystemMemorySize(), allocation->getUnderlyingBufferSize());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenInvalidCacheRegionWhenMmapReturnsUnalignedPointerThenReleaseUnalignedPartsEarly) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBOMmapCreate.set(-1);

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

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
    allocationData.cacheRegion = static_cast<uint32_t>(CacheRegion::none);
    auto allocation = memoryManager->createAllocWithAlignment(allocationData, MemoryConstants::pageSize, MemoryConstants::pageSize64k, MemoryConstants::pageSize64k, 0u);

    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(2u, munmapCalledCount);
    munmapCalledCount = 0u;

    memoryManager->mmapFunction = SysCalls::mmap;
    memoryManager->munmapFunction = SysCalls::munmap;
}

HWTEST2_F(DrmMemoryManagerLocalMemoryTest, givenNotSetUseSystemMemoryWhenGraphicsAllocationInDevicePoolIsAllocatedForBufferAndBufferCompressedThenCreateGmmWithCorrectAuxFlags, IsAtMostXeHpgCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.RenderCompressedBuffersEnabled.set(1);
    DeviceBitfield deviceBitfield{0x0};
    AllocationProperties properties(0, MemoryConstants::pageSize,
                                    AllocationType::buffer,
                                    deviceBitfield);

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.useSystemMemory = false;
    allocData.rootDeviceIndex = rootDeviceIndex;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;

    allocData.flags.preferCompressed = CompressionSelector::preferCompressedAllocation(properties);
    auto buffer = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, buffer);
    auto *gmmResourceParams = reinterpret_cast<GMM_RESCREATE_PARAMS *>(buffer->getDefaultGmm()->resourceParamsData.data());
    EXPECT_EQ(0u, gmmResourceParams->Flags.Info.RenderCompressed);

    allocData.flags.preferCompressed = true;
    auto bufferCompressed = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, bufferCompressed);
    gmmResourceParams = reinterpret_cast<GMM_RESCREATE_PARAMS *>(bufferCompressed->getDefaultGmm()->resourceParamsData.data());
    EXPECT_EQ(1u, gmmResourceParams->Flags.Info.RenderCompressed);

    memoryManager->freeGraphicsMemory(buffer);
    memoryManager->freeGraphicsMemory(bufferCompressed);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCpuAccessRequiredWhenAllocatingInDevicePoolAndMMAPFailThenAllocationIsNullptr) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.requiresCpuAccess = true;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.rootDeviceIndex = rootDeviceIndex;

    memoryManager->mmapFunction = [](void *addr, size_t len, int prot,
                                     int flags, int fd, off_t offset) throw() {
        return MAP_FAILED;
    };

    auto allocation1 = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation1);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);

    status = MemoryManager::AllocationStatus::Success;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.requiresCpuAccess = true;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::writeCombined;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation2 = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation2);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenDebugModuleAreaTypeWhenAllocatingByDrmAndOsAgnosticMemoryManagersThenBothAllocateFromTheSameHeap) {
    auto gfxPartitionDrm = new MockGfxPartition();
    gfxPartitionDrm->callHeapAllocate = false;
    auto gfxPartitionRestore = memoryManager->gfxPartitions[rootDeviceIndex].release();
    memoryManager->gfxPartitions[rootDeviceIndex].reset(gfxPartitionDrm);

    NEO::AllocationProperties properties{device->getRootDeviceIndex(), true, MemoryConstants::pageSize64k,
                                         NEO::AllocationType::debugModuleArea,
                                         false,
                                         device->getDeviceBitfield()};

    auto moduleDebugArea = memoryManager->allocateGraphicsMemoryWithProperties(properties);
    auto drmHeapIndex = gfxPartitionDrm->heapAllocateIndex;

    MockMemoryManager osAgnosticMemoryManager(false, true, *executionEnvironment);
    auto gfxPartitionOsAgnostic = new MockGfxPartition();
    gfxPartitionOsAgnostic->callHeapAllocate = false;
    auto gfxPartitionRestore2 = osAgnosticMemoryManager.gfxPartitions[rootDeviceIndex].release();
    osAgnosticMemoryManager.gfxPartitions[rootDeviceIndex].reset(gfxPartitionOsAgnostic);

    auto moduleDebugArea2 = osAgnosticMemoryManager.allocateGraphicsMemoryWithProperties(properties);
    auto osAgnosticHeapIndex = gfxPartitionOsAgnostic->heapAllocateIndex;

    EXPECT_EQ(osAgnosticHeapIndex, drmHeapIndex);

    memoryManager->freeGraphicsMemory(moduleDebugArea);
    osAgnosticMemoryManager.freeGraphicsMemory(moduleDebugArea2);

    osAgnosticMemoryManager.gfxPartitions[rootDeviceIndex].reset(gfxPartitionRestore2);
    memoryManager->gfxPartitions[rootDeviceIndex].reset(gfxPartitionRestore);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenSipKernelTypeWhenAllocatingByDrmAndOsAgnosticMemoryManagersThenBothAllocateFromTheSameHeap) {
    auto gfxPartitionDrm = new MockGfxPartition();
    gfxPartitionDrm->callHeapAllocate = false;

    memoryManager->gfxPartitions[rootDeviceIndex].reset(gfxPartitionDrm);

    AllocationProperties properties = {rootDeviceIndex, 0x1000, NEO::AllocationType::kernelIsaInternal, device->getDeviceBitfield()};
    properties.flags.use32BitFrontWindow = true;

    auto sipKernel = memoryManager->allocateGraphicsMemoryWithProperties(properties);
    auto drmHeapIndex = gfxPartitionDrm->heapAllocateIndex;

    MockMemoryManager osAgnosticMemoryManager(false, true, *executionEnvironment);

    auto gfxPartitionOsAgnostic = new MockGfxPartition();
    gfxPartitionOsAgnostic->callHeapAllocate = false;
    osAgnosticMemoryManager.gfxPartitions[rootDeviceIndex].reset(gfxPartitionOsAgnostic);

    auto sipKernel2 = osAgnosticMemoryManager.allocateGraphicsMemoryWithProperties(properties);
    auto osAgnosticHeapIndex = gfxPartitionOsAgnostic->heapAllocateIndex;

    EXPECT_EQ(osAgnosticHeapIndex, drmHeapIndex);

    memoryManager->freeGraphicsMemory(sipKernel);
    osAgnosticMemoryManager.freeGraphicsMemory(sipKernel2);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenDebugModuleAreaTypeWhenAllocatingThenBothUseFrontWindowInternalDeviceHeaps) {
    NEO::AllocationProperties properties{device->getRootDeviceIndex(), true, MemoryConstants::pageSize64k,
                                         NEO::AllocationType::debugModuleArea,
                                         false,
                                         device->getDeviceBitfield()};
    auto moduleDebugArea = memoryManager->allocateGraphicsMemoryWithProperties(properties);
    ASSERT_NE(nullptr, moduleDebugArea);

    auto gpuAddress = moduleDebugArea->getGpuAddress();
    auto gmmHelper = device->getGmmHelper();
    EXPECT_LE(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapInternalDeviceFrontWindow)), gpuAddress);
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapInternalDeviceFrontWindow)), gpuAddress);
    EXPECT_EQ(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapInternalDeviceFrontWindow)), moduleDebugArea->getGpuBaseAddress());
    EXPECT_EQ(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapInternalDeviceMemory)), moduleDebugArea->getGpuBaseAddress());
    EXPECT_EQ(MemoryPool::localMemory, moduleDebugArea->getMemoryPool());

    memoryManager->freeGraphicsMemory(moduleDebugArea);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenSipKernelTypeWhenAllocatingThenFrontWindowInternalDeviceHeapIsUsed) {
    const auto allocType = AllocationType::kernelIsaInternal;
    AllocationProperties properties = {rootDeviceIndex, 0x1000, allocType, device->getDeviceBitfield()};
    properties.flags.use32BitFrontWindow = true;

    auto sipAllocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);
    ASSERT_NE(nullptr, sipAllocation);

    auto gpuAddress = sipAllocation->getGpuAddress();
    auto gmmHelper = device->getGmmHelper();
    EXPECT_LE(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapInternalDeviceFrontWindow)), gpuAddress);
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapInternalDeviceFrontWindow)), gpuAddress);
    EXPECT_EQ(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapInternalDeviceFrontWindow)), sipAllocation->getGpuBaseAddress());
    EXPECT_EQ(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapInternalDeviceMemory)), sipAllocation->getGpuBaseAddress());
    EXPECT_EQ(MemoryPool::localMemory, sipAllocation->getMemoryPool());

    memoryManager->freeGraphicsMemory(sipAllocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenDebugVariableSetWhenAllocatingThenForceHeapAlignment) {
    debugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(static_cast<int32_t>(MemoryConstants::megaByte * 2));

    const auto allocType = AllocationType::kernelIsaInternal;
    AllocationProperties properties = {rootDeviceIndex, 0x1000, allocType, device->getDeviceBitfield()};

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    EXPECT_TRUE(isAligned(allocation->getGpuAddress(), MemoryConstants::megaByte * 2));
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryMultiTileTest, givenDrmMemoryManagerWhenMemsetAllocationReturnsSuccessThenAllocationIsFilledWithCorrectData) {
    size_t offset = 3;
    size_t allocationSize = MemoryConstants::pageSize;
    size_t destinationAllocationSize = allocationSize + offset;

    DrmMemoryManagerToTestMemsetAllocation drmMemoryManager(*executionEnvironment, true, destinationAllocationSize);
    int value = 0x42;

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = allocationSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::kernelIsa;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks.set(0, true);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    auto allocation = drmMemoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);

    auto ret = drmMemoryManager.memsetAllocation(allocation, offset, value, allocationSize);
    EXPECT_TRUE(ret);

    auto ptr = ptrOffset(drmMemoryManager.lockedLocalMemory[0].get(), offset);
    for (size_t i = 0; i < allocationSize; i++) {
        EXPECT_EQ(value, static_cast<uint8_t *>(ptr)[i]);
    }

    drmMemoryManager.freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryMultiTileTest, givenDrmMemoryManagerWhenMemsetAllocationFailsToLockResourceThenItReturnsFalse) {
    DrmMemoryManagerToTestMemsetAllocation drmMemoryManager(*executionEnvironment, true, 0);
    size_t allocationSize = MemoryConstants::pageSize;
    int value = 0x42;

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = allocationSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::kernelIsa;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks.set(0, true);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    auto allocation = drmMemoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);

    auto ret = drmMemoryManager.memsetAllocation(allocation, 0, value, allocationSize);
    EXPECT_FALSE(ret);

    drmMemoryManager.freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryMultiTileTest, givenDrmMemoryManagerWhenMemsetAllocationWithCpuPtrThenAllocationIsFilledWithCorrectData) {
    size_t offset = 3;
    size_t allocationSize = MemoryConstants::pageSize;
    size_t destinationAllocationSize = allocationSize + offset;

    DrmMemoryManagerToTestMemsetAllocation drmMemoryManager(*executionEnvironment, false, 0);
    int value = 0x42;

    auto allocation = drmMemoryManager.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, destinationAllocationSize, AllocationType::kernelIsa, mockDeviceBitfield});
    ASSERT_NE(nullptr, allocation);

    auto ret = drmMemoryManager.memsetAllocation(allocation, offset, value, allocationSize);
    EXPECT_TRUE(ret);

    auto ptr = ptrOffset(allocation->getUnderlyingBuffer(), offset);
    for (size_t i = 0; i < allocationSize; i++) {
        EXPECT_EQ(value, static_cast<uint8_t *>(ptr)[i]);
    }

    drmMemoryManager.freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenBufferObjectCreatedInMemoryRegionWhenCreatingMultiHostDebugSurfaceAllocationThenAllocationIsReturned) {

    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b1;
    allocationData.gpuAddress = 0x1000;

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_NE(nullptr, allocation);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMmapFailureWhenCreatingMultiHostDebugSurfaceAllocationThenNullptrIsReturned) {
    memoryManager->mmapFunction = [](void *addr, size_t len, int prot, int flags, int fd, off_t offset) throw() {
        return MAP_FAILED;
    };

    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b1;
    allocationData.gpuAddress = 0x1000;

    EXPECT_EQ(nullptr, memoryManager->createMultiHostDebugSurfaceAllocation(allocationData));
    memoryManager->mmapFunction = SysCalls::mmap;
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateMultiHostDebugSurfaceAllocationWithBufferObjectCreationFailureThenNullptrReturned) {
    class MockDrmMemoryManagerWithBOFailure : public TestedDrmMemoryManager {
      public:
        MockDrmMemoryManagerWithBOFailure(ExecutionEnvironment &executionEnvironment) : TestedDrmMemoryManager(executionEnvironment) {}

        BufferObject *createBufferObjectInMemoryRegion(uint32_t rootDeviceIndex, Gmm *gmm, AllocationType allocationType,
                                                       uint64_t gpuAddress, size_t size, DeviceBitfield memoryBanks,
                                                       size_t maxOsContextCount, int32_t pairHandle, bool isSystemMemoryPool,
                                                       bool isUSMHostAllocation) override {
            if (shouldBOCreationFail) {
                return nullptr; // Simulate buffer object creation failure
            }
            return DrmMemoryManager::createBufferObjectInMemoryRegion(rootDeviceIndex, gmm, allocationType, gpuAddress,
                                                                      size, memoryBanks, maxOsContextCount, pairHandle,
                                                                      isSystemMemoryPool, isUSMHostAllocation);
        }

        bool shouldBOCreationFail = false;
    };

    MockDrmMemoryManagerWithBOFailure mockMemoryManager(*executionEnvironment);
    mockMemoryManager.shouldBOCreationFail = true;

    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b1;
    allocationData.gpuAddress = 0x1000;

    auto allocation = mockMemoryManager.createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_EQ(nullptr, allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateMultiHostDebugSurfaceAllocationWithRetrieveMmapOffsetFailureThenNullptrReturned) {
    ioctlHelper->retrieveMmapOffsetResult = false;

    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b1;
    allocationData.gpuAddress = 0x1000;

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_EQ(nullptr, allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateMultiHostDebugSurfaceAllocationWithComplexMemoryBankPatternThenCorrectBankMappingIsUsed) {
    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b10101011; // Banks 0, 1, 3, 5, 7 (include bank 0 for getBO() compatibility)
    allocationData.gpuAddress = 0x1000;

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_NE(nullptr, allocation);

    // Check that allocation has correct number of GMMs (based on num banks)
    auto numBanks = allocation->getNumGmms();
    EXPECT_GT(numBanks, 0u);

    // Check that buffer objects are created
    const auto &bufferObjects = allocation->getBOs();
    EXPECT_GE(bufferObjects.size(), numBanks); // At least as many as GMMs

    // Check that some buffer objects are valid
    uint32_t validBufferObjectCount = 0;
    for (const auto &bo : bufferObjects) {
        if (bo != nullptr) {
            validBufferObjectCount++;
        }
    }
    EXPECT_GT(validBufferObjectCount, 0u); // At least some valid buffer objects

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateMultiHostDebugSurfaceAllocationWithBufferObjectCreationFailureInSecondTileThenNullptrReturned) {
    class MockDrmMemoryManagerWithSelectiveBOFailure : public TestedDrmMemoryManager {
      public:
        MockDrmMemoryManagerWithSelectiveBOFailure(ExecutionEnvironment &executionEnvironment) : TestedDrmMemoryManager(executionEnvironment) {}

        BufferObject *createBufferObjectInMemoryRegion(uint32_t rootDeviceIndex, Gmm *gmm, AllocationType allocationType,
                                                       uint64_t gpuAddress, size_t size, DeviceBitfield memoryBanks,
                                                       size_t maxOsContextCount, int32_t pairHandle, bool isSystemMemoryPool,
                                                       bool isUSMHostAllocation) override {
            if (creationCount == 1) { // Fail on second tile
                return nullptr;
            }
            creationCount++;
            return DrmMemoryManager::createBufferObjectInMemoryRegion(rootDeviceIndex, gmm, allocationType, gpuAddress,
                                                                      size, memoryBanks, maxOsContextCount, pairHandle,
                                                                      isSystemMemoryPool, isUSMHostAllocation);
        }

        uint32_t creationCount = 0;
    };

    MockDrmMemoryManagerWithSelectiveBOFailure mockMemoryManager(*executionEnvironment);

    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b11; // Two tiles
    allocationData.gpuAddress = 0x1000;

    auto allocation = mockMemoryManager.createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_EQ(nullptr, allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateMultiHostDebugSurfaceAllocationWithMultipleMemoryBanksThenCorrectBankMappingIsUsed) {
    // Configure mock to use mock behavior for createBufferObjectInMemoryRegion
    memoryManager->createBufferObjectInMemoryRegionCallBase = false;

    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b1011; // Banks 0, 1 and 3 (include bank 0 for getBO() compatibility)
    allocationData.gpuAddress = 0x1000;

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_NE(nullptr, allocation);

    // Should have 3 buffer objects for the 3 active banks
    EXPECT_EQ(3u, allocation->getNumGmms());

    // Check that buffer objects are created
    const auto &bufferObjects = allocation->getBOs();
    EXPECT_GE(bufferObjects.size(), 3u); // At least 3 buffer objects

    // Check that some buffer objects are valid
    uint32_t validBufferObjectCount = 0;
    for (const auto &bo : bufferObjects) {
        if (bo != nullptr) {
            validBufferObjectCount++;
        }
    }
    EXPECT_GE(validBufferObjectCount, 3u); // At least 3 valid buffer objects

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateMultiHostDebugSurfaceAllocationWithSingleBankThenCorrectMmapSizeIsSet) {
    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b1; // Single bank
    allocationData.gpuAddress = 0x1000;

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_NE(nullptr, allocation);

    // For single bank, host size should equal allocation size
    EXPECT_EQ(MemoryConstants::pageSize, allocation->getMmapSize());
    EXPECT_NE(nullptr, allocation->getMmapPtr());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateMultiHostDebugSurfaceAllocationWithMultipleBanksThenCorrectMmapSizeIsSet) {
    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b11; // Two banks
    allocationData.gpuAddress = 0x1000;

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_NE(nullptr, allocation);

    // For two banks, host size should be twice the allocation size
    EXPECT_EQ(2 * MemoryConstants::pageSize, allocation->getMmapSize());
    EXPECT_NE(nullptr, allocation->getMmapPtr());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenDrmMemoryManagerWhenCopyDebugSurfaceToMultiTileAllocationThenCallCopyMemoryToAllocation) {
    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b11; // Two banks
    allocationData.gpuAddress = 0x1000;
    std::vector<uint8_t> dataToCopy(allocationData.size, 1u);

    AllocationType debugSurfaces[] = {AllocationType::debugContextSaveArea, AllocationType::debugSbaTrackingBuffer};

    for (auto type : debugSurfaces) {
        allocationData.type = type;
        auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
        EXPECT_NE(nullptr, allocation);

        // For two banks, host size should be twice the allocation size
        EXPECT_EQ(2 * MemoryConstants::pageSize, allocation->getMmapSize());
        EXPECT_NE(nullptr, allocation->getMmapPtr());

        auto ret = memoryManager->copyMemoryToAllocation(allocation, 0, dataToCopy.data(), dataToCopy.size());
        EXPECT_TRUE(ret);

        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenDrmMemoryManagerWhenMemsetMultiTileDebugSurfaceAllocationThenCallMemsetAllocation) {
    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b11; // Two banks
    allocationData.gpuAddress = 0x1000;
    int value = 0x42;

    AllocationType debugSurfaces[] = {AllocationType::debugContextSaveArea, AllocationType::debugSbaTrackingBuffer};

    for (auto type : debugSurfaces) {
        allocationData.type = type;
        auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
        EXPECT_NE(nullptr, allocation);

        // For two banks, host size should be twice the allocation size
        EXPECT_EQ(2 * MemoryConstants::pageSize, allocation->getMmapSize());
        EXPECT_NE(nullptr, allocation->getMmapPtr());

        auto ret = memoryManager->memsetAllocation(allocation, 0, value, allocationData.size);
        EXPECT_TRUE(ret);

        auto ptr = static_cast<uint8_t *>(allocation->getMmapPtr());
        for (size_t i = 0; i < allocationData.size; i++) {
            EXPECT_EQ(value, ptr[i]) << "Bank 0, byte at index " << i << " mismatch";
        }
        for (size_t i = 0; i < allocationData.size; i++) {
            EXPECT_EQ(value, ptr[allocationData.size + i]) << "Bank 1, byte at index " << i << " mismatch";
        }

        memoryManager->freeGraphicsMemory(allocation);
    }
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMoreThanOneSubDevicesWhenAllocatingDebugSbaTrackingBufferWithGpuVaThenAllTilesAreHandled) {
    // device creation resets MultiTileArchInfo; redeclare to exercise the real multi-tile path
    auto hwInfo = mock->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid = 1;
    hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount = 4;

    // Configure mock to use mock behavior for createBufferObjectInMemoryRegion
    memoryManager->createBufferObjectInMemoryRegionCallBase = false;

    AllocationProperties debugSurfaceProperties{0, true, MemoryConstants::pageSize, NEO::AllocationType::debugSbaTrackingBuffer, false, false, 0b1011};
    debugSurfaceProperties.gpuAddress = 0x12340000;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(debugSurfaceProperties);
    EXPECT_NE(nullptr, allocation);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenOneSubDeviceWhenAllocatingDebugSbaTrackingBufferWithGpuVaThenTheSingleTileIsHandled) {
    // Configure mock to use mock behavior for createBufferObjectInMemoryRegion
    memoryManager->createBufferObjectInMemoryRegionCallBase = false;

    AllocationProperties debugSurfaceProperties{0, true, MemoryConstants::pageSize, NEO::AllocationType::debugSbaTrackingBuffer, false, false, 0b1};
    debugSurfaceProperties.gpuAddress = 0x12340000;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(debugSurfaceProperties);
    EXPECT_NE(nullptr, allocation);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateMultiHostDebugSurfaceAllocationWithValidInputThenSuccessfulAllocationReturned) {
    // Configure mock to use mock behavior for createBufferObjectInMemoryRegion
    memoryManager->createBufferObjectInMemoryRegionCallBase = false;

    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b1;
    allocationData.gpuAddress = 0x1000;

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(AllocationType::debugContextSaveArea, allocation->getAllocationType());
    EXPECT_EQ(MemoryConstants::pageSize, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_TRUE(allocation->isFlushL3Required());
    EXPECT_TRUE(allocation->isUncacheable());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateMultiHostDebugSurfaceAllocationWithZeroGpuAddressThenAddressIsAcquired) {
    // Configure mock to use mock behavior for createBufferObjectInMemoryRegion
    memoryManager->createBufferObjectInMemoryRegionCallBase = false;

    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b1;
    allocationData.gpuAddress = 0; // Zero address

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_NE(0u, allocation->getGpuAddress());
    EXPECT_NE(nullptr, allocation->getReservedAddressPtr());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateMultiHostDebugSurfaceAllocationWithStorageInfoThenStorageInfoIsSet) {
    // Configure mock to use mock behavior for createBufferObjectInMemoryRegion
    memoryManager->createBufferObjectInMemoryRegionCallBase = false;
    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b1011; // Banks 0, 1 and 3 (include bank 0 for getBO() compatibility)
    allocationData.gpuAddress = 0x1000;

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(allocationData.storageInfo.memoryBanks, allocation->storageInfo.memoryBanks);
    EXPECT_EQ(allocationData.storageInfo.getNumBanks(), allocation->storageInfo.getNumBanks());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateDebugSurfaceWithUnalignedSizeCalledThenNullptrReturned) {
    AllocationProperties debugSurfaceProperties{0, true, MemoryConstants::pageSize + 101, NEO::AllocationType::debugContextSaveArea, false, false, 0b1011};
    auto debugSurface = memoryManager->allocateGraphicsMemoryWithProperties(debugSurfaceProperties);
    EXPECT_EQ(nullptr, debugSurface);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateMultiHostDebugSurfaceAllocationWithUnalignedSizeThenNullptrReturned) {
    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize + 101;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b1;

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    EXPECT_EQ(nullptr, allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateMultiHostDebugSurfaceAllocationWithNonZeroGpuAddressThenProvidedAddressIsUsed) {
    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b11;
    allocationData.gpuAddress = 0x1000;

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    ASSERT_NE(nullptr, allocation);
    auto gmmHelper = memoryManager->getGmmHelper(0);
    EXPECT_EQ(gmmHelper->canonize(0x1000), allocation->getGpuAddress());
    EXPECT_EQ(nullptr, allocation->getReservedAddressPtr());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateMultiHostDebugSurfaceAllocationWithOsContextThenOsContextIsSet) {
    AllocationData allocationData{};
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::debugContextSaveArea;
    allocationData.rootDeviceIndex = 0;
    allocationData.storageInfo.memoryBanks = 0b11;
    allocationData.gpuAddress = 0x1000;

    auto osContext = static_cast<OsContextLinux *>(device->getDefaultEngine().osContext);
    allocationData.osContext = osContext;

    auto allocation = memoryManager->createMultiHostDebugSurfaceAllocation(allocationData);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(osContext, allocation->getOsContext());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenConsumeFdTrueAndMappedPtrWhenCreateUSMHostAllocationFromSharedHandleThenFdIsClosedAndSharedHandleIsNonShared) {
    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<int> closeArgBackup(&SysCalls::closeFuncArgPassed, 0);

    int testFd = 123;
    void *testMappedPtr = reinterpret_cast<void *>(0x12345000);

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.gpuAddress = 0x1000;

    mock->outputHandle = 100;

    auto sysMemBefore = memoryManager->getUsedSystemMemorySize();

    auto allocation = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd, properties, testMappedPtr, false, true);

    ASSERT_NE(allocation, nullptr);
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(testFd, SysCalls::closeFuncArgPassed);
    EXPECT_EQ(Sharing::nonSharedResource, allocation->peekSharedHandle());
    EXPECT_GT(memoryManager->getUsedSystemMemorySize(), sysMemBefore);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenConsumeFdFalseAndMappedPtrWhenCreateUSMHostAllocationFromSharedHandleThenFdIsNotClosedAndSharedHandleIsPreserved) {
    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);

    int testFd = 123;
    void *testMappedPtr = reinterpret_cast<void *>(0x12345000);

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.gpuAddress = 0x1000;

    mock->outputHandle = 100;

    auto sysMemBefore = memoryManager->getUsedSystemMemorySize();
    auto registerBefore = memoryManager->registerSysMemAllocCalled;

    auto allocation = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd, properties, testMappedPtr, false, false);

    ASSERT_NE(allocation, nullptr);
    EXPECT_EQ(0u, SysCalls::closeFuncCalled);
    EXPECT_EQ(static_cast<unsigned int>(testFd), allocation->peekSharedHandle());
    EXPECT_EQ(memoryManager->getUsedSystemMemorySize(), sysMemBefore);
    EXPECT_EQ(registerBefore, memoryManager->registerSysMemAllocCalled);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenConsumeFdTrueAndNoBooMmapWhenCreateUSMHostAllocationFromSharedHandleThenFdIsClosedAndRegistered) {
    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<int> closeArgBackup(&SysCalls::closeFuncArgPassed, 0);

    int testFd = 123;

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.useMmapObject = false;
    properties.gpuAddress = 0x1000;

    mock->outputHandle = 100;

    auto sysMemBefore = memoryManager->getUsedSystemMemorySize();

    auto allocation = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd, properties, nullptr, false, true);

    ASSERT_NE(allocation, nullptr);
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(testFd, SysCalls::closeFuncArgPassed);
    EXPECT_EQ(Sharing::nonSharedResource, allocation->peekSharedHandle());
    EXPECT_GT(memoryManager->getUsedSystemMemorySize(), sysMemBefore);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenConsumeFdFalseAndNoBooMmapWhenCreateUSMHostAllocationFromSharedHandleThenFdIsNotClosedAndSharedHandlePreserved) {
    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);

    int testFd = 123;

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.useMmapObject = false;
    properties.gpuAddress = 0x1000;

    mock->outputHandle = 100;

    auto sysMemBefore = memoryManager->getUsedSystemMemorySize();
    auto registerBefore = memoryManager->registerSysMemAllocCalled;

    auto allocation = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd, properties, nullptr, false, false);

    ASSERT_NE(allocation, nullptr);
    EXPECT_EQ(0u, SysCalls::closeFuncCalled);
    EXPECT_EQ(static_cast<unsigned int>(testFd), allocation->peekSharedHandle());
    EXPECT_EQ(memoryManager->getUsedSystemMemorySize(), sysMemBefore);
    EXPECT_EQ(registerBefore, memoryManager->registerSysMemAllocCalled);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenConsumeFdTrueAndNewBoCreationWhenCreateUSMHostAllocationFromSharedHandleThenFdIsClosedAfterLseek) {
    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<int> closeArgBackup(&SysCalls::closeFuncArgPassed, 0);
    VariableBackup<off_t> lseekRetValBackup(&SysCalls::lseekReturn, MemoryConstants::pageSize);

    int testFd = 123;

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.useMmapObject = true;
    properties.gpuAddress = 0x1000;

    mock->outputHandle = 100;

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));

    auto sysMemBefore = memoryManager->getUsedSystemMemorySize();

    auto allocation = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd, properties, nullptr, false, true);

    ASSERT_NE(allocation, nullptr);
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(testFd, SysCalls::closeFuncArgPassed);
    EXPECT_EQ(Sharing::nonSharedResource, allocation->peekSharedHandle());
    EXPECT_GT(memoryManager->getUsedSystemMemorySize(), sysMemBefore);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenConsumeFdFalseAndNewBoCreationWhenCreateUSMHostAllocationFromSharedHandleThenFdIsNotClosed) {
    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<off_t> lseekRetValBackup(&SysCalls::lseekReturn, MemoryConstants::pageSize);

    int testFd = 123;

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.useMmapObject = true;
    properties.gpuAddress = 0x1000;

    mock->outputHandle = 100;

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));

    auto allocation = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd, properties, nullptr, false, false);

    ASSERT_NE(allocation, nullptr);
    EXPECT_EQ(0u, SysCalls::closeFuncCalled);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenConsumeFdTrueAndReuseSharedAllocationWhenCreateUSMHostAllocationFromSharedHandleThenFdIsClosedAndBoIsReused) {
    VariableBackup<off_t> lseekRetValBackup(&SysCalls::lseekReturn, MemoryConstants::pageSize);

    int testFd1 = 123;
    int testFd2 = 456;

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.useMmapObject = true;
    properties.gpuAddress = 0x1000;

    mock->outputHandle = 100;

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));

    auto allocation1 = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd1, properties, nullptr, false, false);
    ASSERT_NE(allocation1, nullptr);

    auto sysMemAfterFirst = memoryManager->getUsedSystemMemorySize();

    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<int> closeArgBackup(&SysCalls::closeFuncArgPassed, 0);

    auto allocation2 = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd2, properties, nullptr, true, true);

    ASSERT_NE(allocation2, nullptr);
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(testFd2, SysCalls::closeFuncArgPassed);
    EXPECT_EQ(Sharing::nonSharedResource, allocation2->peekSharedHandle());
    EXPECT_GT(memoryManager->getUsedSystemMemorySize(), sysMemAfterFirst);

    auto bo1 = static_cast<DrmAllocation *>(allocation1)->getBO();
    auto bo2 = static_cast<DrmAllocation *>(allocation2)->getBO();
    EXPECT_EQ(bo1, bo2);

    memoryManager->freeGraphicsMemory(allocation1);
    memoryManager->freeGraphicsMemory(allocation2);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenConsumeFdFalseAndReuseSharedAllocationWhenCreateUSMHostAllocationFromSharedHandleThenFdIsNotClosedAndBoIsReused) {
    VariableBackup<off_t> lseekRetValBackup(&SysCalls::lseekReturn, MemoryConstants::pageSize);

    int testFd1 = 123;
    int testFd2 = 456;

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.useMmapObject = true;
    properties.gpuAddress = 0x1000;

    mock->outputHandle = 100;

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));

    auto allocation1 = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd1, properties, nullptr, false, false);
    ASSERT_NE(allocation1, nullptr);

    auto sysMemAfterFirst = memoryManager->getUsedSystemMemorySize();

    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);

    auto registerBefore = memoryManager->registerSysMemAllocCalled;

    auto allocation2 = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd2, properties, nullptr, true, false);

    ASSERT_NE(allocation2, nullptr);
    EXPECT_EQ(0u, SysCalls::closeFuncCalled);
    EXPECT_EQ(static_cast<unsigned int>(testFd2), allocation2->peekSharedHandle());
    EXPECT_EQ(memoryManager->getUsedSystemMemorySize(), sysMemAfterFirst);
    EXPECT_EQ(registerBefore, memoryManager->registerSysMemAllocCalled);

    memoryManager->freeGraphicsMemory(allocation1);
    memoryManager->freeGraphicsMemory(allocation2);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenDefaultConsumeFdWhenCreateUSMHostAllocationFromSharedHandleCalledThenConsumeFdIsFalse) {
    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);

    int testFd = 123;

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.useMmapObject = false;
    properties.gpuAddress = 0x1000;

    mock->outputHandle = 100;

    auto allocation = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd, properties, nullptr, false);

    ASSERT_NE(allocation, nullptr);
    EXPECT_EQ(0u, SysCalls::closeFuncCalled);
    EXPECT_EQ(static_cast<unsigned int>(testFd), allocation->peekSharedHandle());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenConsumeFdTrueAndIoctlFailureWhenCreateUSMHostAllocationFromSharedHandleThenFdIsClosedAndNullptrReturned) {
    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<int> closeArgBackup(&SysCalls::closeFuncArgPassed, 0);

    int testFd = 123;

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.gpuAddress = 0x1000;

    mock->fdToHandleRetVal = -1;

    auto allocation = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd, properties, nullptr, false, true);

    EXPECT_EQ(allocation, nullptr);
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(testFd, SysCalls::closeFuncArgPassed);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenConsumeFdFalseAndIoctlFailureWhenCreateUSMHostAllocationFromSharedHandleThenFdIsNotClosedAndNullptrReturned) {
    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);

    int testFd = 123;

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.gpuAddress = 0x1000;

    mock->fdToHandleRetVal = -1;

    auto allocation = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd, properties, nullptr, false, false);

    EXPECT_EQ(allocation, nullptr);
    EXPECT_EQ(0u, SysCalls::closeFuncCalled);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenConsumeFdTrueAndMappedPtrWhenRegisterSysMemAllocFailsThenNullptrReturnedAndBoUnreferenced) {
    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<int> closeArgBackup(&SysCalls::closeFuncArgPassed, 0);

    int testFd = 123;
    void *testMappedPtr = reinterpret_cast<void *>(0x12345000);

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.gpuAddress = 0x1000;

    mock->outputHandle = 100;
    memoryManager->failRegisterSysMemAlloc = true;

    auto sysMemBefore = memoryManager->getUsedSystemMemorySize();
    auto unreferenceBefore = memoryManager->unreferenceCalled;

    auto allocation = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd, properties, testMappedPtr, false, true);

    EXPECT_EQ(allocation, nullptr);
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(testFd, SysCalls::closeFuncArgPassed);
    EXPECT_GT(memoryManager->unreferenceCalled, unreferenceBefore);
    EXPECT_EQ(sysMemBefore, memoryManager->getUsedSystemMemorySize());
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenConsumeFdTrueAndNoBooMmapWhenRegisterSysMemAllocFailsThenNullptrReturnedAndAccountingConsistent) {
    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<int> closeArgBackup(&SysCalls::closeFuncArgPassed, 0);

    int testFd = 123;

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.useMmapObject = false;
    properties.gpuAddress = 0x1000;

    mock->outputHandle = 100;
    memoryManager->failRegisterSysMemAlloc = true;

    auto sysMemBefore = memoryManager->getUsedSystemMemorySize();
    auto unregisterBefore = memoryManager->unregisterAllocationCalled;

    auto allocation = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd, properties, nullptr, false, true);

    EXPECT_EQ(allocation, nullptr);
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(testFd, SysCalls::closeFuncArgPassed);
    EXPECT_EQ(sysMemBefore, memoryManager->getUsedSystemMemorySize());
    EXPECT_EQ(unregisterBefore, memoryManager->unregisterAllocationCalled);
    EXPECT_EQ(0u, memoryManager->callsToCloseSharedHandle);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenNewBoCreationWhenRegisterSysMemAllocFailsThenNullptrReturnedAndAccountingConsistent) {
    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<int> closeArgBackup(&SysCalls::closeFuncArgPassed, 0);
    VariableBackup<off_t> lseekRetValBackup(&SysCalls::lseekReturn, MemoryConstants::pageSize);

    int testFd = 123;

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.useMmapObject = true;
    properties.gpuAddress = 0x1000;

    mock->outputHandle = 100;

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));

    memoryManager->failRegisterSysMemAlloc = true;

    auto sysMemBefore = memoryManager->getUsedSystemMemorySize();
    auto unregisterBefore = memoryManager->unregisterAllocationCalled;

    auto allocation = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd, properties, nullptr, false, true);

    EXPECT_EQ(allocation, nullptr);
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(testFd, SysCalls::closeFuncArgPassed);
    EXPECT_EQ(sysMemBefore, memoryManager->getUsedSystemMemorySize());
    EXPECT_EQ(unregisterBefore, memoryManager->unregisterAllocationCalled);
    EXPECT_EQ(0u, memoryManager->callsToCloseSharedHandle);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenConsumeFdTrueAndReuseSharedAllocationWhenRegisterSysMemAllocFailsThenNullptrReturnedAndAccountingConsistent) {
    VariableBackup<off_t> lseekRetValBackup(&SysCalls::lseekReturn, MemoryConstants::pageSize);

    int testFd1 = 123;
    int testFd2 = 456;

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.useMmapObject = true;
    properties.gpuAddress = 0x1000;

    mock->outputHandle = 100;

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));

    auto allocation1 = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd1, properties, nullptr, false, false);
    ASSERT_NE(allocation1, nullptr);

    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<int> closeArgBackup(&SysCalls::closeFuncArgPassed, 0);

    memoryManager->failRegisterSysMemAlloc = true;

    auto sysMemBefore = memoryManager->getUsedSystemMemorySize();
    auto unregisterBefore = memoryManager->unregisterAllocationCalled;

    auto allocation2 = memoryManager->createUSMHostAllocationFromSharedHandle(
        testFd2, properties, nullptr, true, true);

    EXPECT_EQ(allocation2, nullptr);
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(testFd2, SysCalls::closeFuncArgPassed);
    EXPECT_EQ(sysMemBefore, memoryManager->getUsedSystemMemorySize());
    EXPECT_EQ(unregisterBefore, memoryManager->unregisterAllocationCalled);
    EXPECT_EQ(0u, memoryManager->callsToCloseSharedHandle);

    memoryManager->freeGraphicsMemory(allocation1);
}

TEST_F(DrmMemoryManagerUsmSharedHandleTest, givenIsHostIpcAllocationWhenCreateGraphicsAllocationFromSharedHandleCalledThenConsumeFdIsTrue) {
    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<int> closeArgBackup(&SysCalls::closeFuncArgPassed, 0);

    int testFd = 123;

    AllocationProperties properties(0, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.useMmapObject = false;
    properties.gpuAddress = 0x1000;

    TestedDrmMemoryManager::OsHandleData osHandleData{static_cast<uint64_t>(testFd)};

    mock->outputHandle = 100;

    auto allocation = memoryManager->createGraphicsAllocationFromSharedHandle(
        osHandleData, properties, false, true, false, nullptr);

    ASSERT_NE(allocation, nullptr);
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(testFd, SysCalls::closeFuncArgPassed);
    EXPECT_EQ(Sharing::nonSharedResource, allocation->peekSharedHandle());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, whenCreateUnifiedMemoryAllocationThenGemCreateExtIsUsed) {
    ioctlHelper->vmAdviseAtomicAttribute = std::nullopt;
    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->ioctlCallsCount = 0;

    AllocationProperties gpuProperties{0u,
                                       MemoryConstants::pageSize64k,
                                       AllocationType::unifiedSharedMemory,
                                       1u};
    gpuProperties.alignment = 2 * MemoryConstants::megaByte;
    gpuProperties.usmInitialPlacement = GraphicsAllocation::UsmInitialPlacement::CPU;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties);

    ASSERT_NE(allocation, nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapPtr(), nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapSize(), 0u);
    EXPECT_EQ(allocation->getAllocationOffset(), 0u);

    ASSERT_FALSE(ioctlHelper->createGemExtCalls.empty());
    const auto &memRegions = ioctlHelper->createGemExtCalls.back().memClassInstances;
    ASSERT_EQ(memRegions.size(), 2u);
    EXPECT_EQ(memRegions[0].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM);
    EXPECT_EQ(memRegions[0].memoryInstance, 1u);
    EXPECT_EQ(memRegions[1].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[1].memoryInstance, regionInfo[1].region.memoryInstance);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenUseKmdMigrationSetWhenCreateSharedUnifiedMemoryAllocationThenKmdMigratedAllocationIsCreated) {
    ioctlHelper->vmAdviseAtomicAttribute = std::nullopt;
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->ioctlCallsCount = 0;
    mock->setBindAvailable();

    executionEnvironment->calculateMaxOsContextCount();
    SVMAllocsManager unifiedMemoryManager(memoryManager);

    UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto ptr = unifiedMemoryManager.createSharedUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties, nullptr);

    EXPECT_NE(ptr, nullptr);
    auto allocation = unifiedMemoryManager.getSVMAlloc(ptr)->gpuAllocations.getDefaultGraphicsAllocation();
    EXPECT_NE(allocation, nullptr);

    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());

    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapPtr(), nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapSize(), 0u);
    EXPECT_EQ(allocation->getAllocationOffset(), 0u);

    ASSERT_FALSE(ioctlHelper->createGemExtCalls.empty());
    const auto &memRegions = ioctlHelper->createGemExtCalls.back().memClassInstances;
    EXPECT_EQ(memRegions.size(), 2u);
    EXPECT_EQ(memRegions[0].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM);
    EXPECT_EQ(memRegions[0].memoryInstance, 1u);
    EXPECT_EQ(memRegions[1].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[1].memoryInstance, regionInfo[1].region.memoryInstance);

    unifiedMemoryManager.freeSVMAlloc(ptr);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, MmapFailWhenCreateSharedUnifiedMemoryAllocationThenNullPtrReturned) {
    ioctlHelper->vmAdviseAtomicAttribute = std::nullopt;
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->ioctlCallsCount = 0;
    mock->setBindAvailable();

    memoryManager->mmapFunction = [](void *addr, size_t len, int prot,
                                     int flags, int fd, off_t offset) throw() {
        return MAP_FAILED;
    };

    SVMAllocsManager unifiedMemoryManager(memoryManager);

    UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto ptr = unifiedMemoryManager.createSharedUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties, nullptr);

    EXPECT_EQ(ptr, nullptr);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenUseKmdMigrationSetWhenCreateSharedUnifiedMemoryAllocationFailsThenNullptrReturned) {
    ioctlHelper->vmAdviseAtomicAttribute = std::nullopt;
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};

    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    ioctlHelper->retrieveMmapOffsetResult = false;

    SVMAllocsManager unifiedMemoryManager(memoryManager);

    UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto ptr = unifiedMemoryManager.createSharedUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties, nullptr);

    EXPECT_EQ(ptr, nullptr);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMmapOffsetFailureWhenCreateUnifiedMemoryAllocationThenNullptr) {
    ioctlHelper->vmAdviseAtomicAttribute = std::nullopt;
    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    ioctlHelper->retrieveMmapOffsetResult = false;

    AllocationProperties gpuProperties{0u,
                                       MemoryConstants::pageSize64k,
                                       AllocationType::unifiedSharedMemory,
                                       1u};
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties);

    EXPECT_EQ(allocation, nullptr);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenGemCreateExtFailWhenCreateUnifiedMemoryAllocationThenNullptr) {
    ioctlHelper->vmAdviseAtomicAttribute = std::nullopt;
    std::vector<MemoryRegion> regionInfo(2);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    ioctlHelper->createGemExtResult = -1;

    AllocationProperties gpuProperties{0u,
                                       MemoryConstants::pageSize64k,
                                       AllocationType::unifiedSharedMemory,
                                       1u};
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties);

    EXPECT_EQ(allocation, nullptr);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenAllocationWithUnifiedMemoryAllocationWithoutUseMmapObjectThenReturnNullptr) {
    ioctlHelper->vmAdviseAtomicAttribute = std::nullopt;
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;

    auto &productHelper = *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper;
    auto alignmentSize = productHelper.is2MBLocalMemAlignmentEnabled() ? MemoryConstants::pageSize2M : MemoryConstants::pageSize64k;
    allocData.allFlags = 0;
    allocData.size = 18 * alignmentSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::unifiedSharedMemory;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.useMmapObject = false;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenAllocationWithUnifiedMemoryAllocationWithoutMemoryInfoSetThenReturnNullptr) {
    ioctlHelper->vmAdviseAtomicAttribute = std::nullopt;
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;

    auto &productHelper = *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper;
    auto alignmentSize = productHelper.is2MBLocalMemAlignmentEnabled() ? MemoryConstants::pageSize2M : MemoryConstants::pageSize64k;
    allocData.allFlags = 0;
    allocData.size = 18 * alignmentSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::unifiedSharedMemory;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.useMmapObject = true;

    mock->memoryInfo.reset();

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, whenCreateUnifiedMemoryAllocationWithMultiMemoryRegionsThenGemCreateExtIsUsedWithSingleLmemRegions) {
    ioctlHelper->vmAdviseAtomicAttribute = std::nullopt;
    std::vector<MemoryRegion> regionInfo(3);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(1, 0)};

    auto hwInfo = mock->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid = 1;
    hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount = 3;

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->ioctlCallsCount = 0;
    DeviceBitfield devices = 0b11;
    auto &productHelper = *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper;
    auto alignmentSize = productHelper.is2MBLocalMemAlignmentEnabled() ? MemoryConstants::pageSize2M : MemoryConstants::pageSize64k;
    AllocationProperties gpuProperties{
        0u,
        true,
        MemoryConstants::megaByte,
        AllocationType::unifiedSharedMemory,
        false,
        devices};
    gpuProperties.alignment = alignmentSize;
    gpuProperties.usmInitialPlacement = GraphicsAllocation::UsmInitialPlacement::CPU;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties);

    ASSERT_NE(allocation, nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapPtr(), nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapSize(), 0u);
    EXPECT_EQ(allocation->getAllocationOffset(), 0u);

    ASSERT_FALSE(ioctlHelper->createGemExtCalls.empty());
    const auto &memRegions = ioctlHelper->createGemExtCalls.back().memClassInstances;
    ASSERT_EQ(memRegions.size(), 2u);
    EXPECT_EQ(memRegions[0].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM);
    EXPECT_EQ(memRegions[0].memoryInstance, 1u);
    EXPECT_EQ(memRegions[1].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[1].memoryInstance, regionInfo[2].region.memoryInstance);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenUseKmdMigrationAndUsmInitialPlacementSetToGpuWhenCreateUnifiedSharedMemoryWithOverriddenMultiStoragePlacementThenKmdMigratedAllocationIsCreatedWithCorrectRegionsOrder) {
    ioctlHelper->vmAdviseAtomicAttribute = std::nullopt;
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(1);
    debugManager.flags.UsmInitialPlacement.set(1);
    debugManager.flags.OverrideMultiStoragePlacement.set(0x1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};

    std::vector<MemoryRegion> regionInfo(5);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(1, 0)};
    regionInfo[3].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(2, 0)};
    regionInfo[4].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(3, 0)};

    auto hwInfo = mock->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid = 1;
    hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount = 4;

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));

    SVMAllocsManager unifiedMemoryManager(memoryManager);

    UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device.get();

    auto ptr = unifiedMemoryManager.createSharedUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties, nullptr);
    EXPECT_NE(ptr, nullptr);

    auto allocation = unifiedMemoryManager.getSVMAlloc(ptr)->gpuAllocations.getDefaultGraphicsAllocation();
    EXPECT_NE(allocation, nullptr);

    ASSERT_FALSE(ioctlHelper->createGemExtCalls.empty());
    const auto memRegions = ioctlHelper->createGemExtCalls.back().memClassInstances;
    EXPECT_EQ(memRegions.size(), 2u);
    EXPECT_EQ(memRegions[0].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[0].memoryInstance, regionInfo[1].region.memoryInstance);
    EXPECT_EQ(memRegions[1].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM);
    EXPECT_EQ(memRegions[1].memoryInstance, 1u);

    unifiedMemoryManager.freeSVMAlloc(ptr);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenUseKmdMigrationSetWhenCreateSharedUnifiedMemoryAllocationIsCalledThenKmdMigratedAllocationWithMultipleBOsIsCreated) {
    ioctlHelper->vmAdviseAtomicAttribute = std::nullopt;
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};

    std::vector<MemoryRegion> regionInfo(3);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(1, 0)};

    auto hwInfo = mock->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid = 1;
    hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount = 2;

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));

    SVMAllocsManager unifiedMemoryManager(memoryManager);

    auto size = 2 * MemoryConstants::megaByte;
    UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto ptr = unifiedMemoryManager.createSharedUnifiedMemoryAllocation(size, unifiedMemoryProperties, nullptr);
    EXPECT_NE(ptr, nullptr);

    auto allocation = unifiedMemoryManager.getSVMAlloc(ptr)->gpuAllocations.getDefaultGraphicsAllocation();
    EXPECT_NE(allocation, nullptr);

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    EXPECT_EQ(ptr, alignUp(drmAllocation->getMmapPtr(), MemoryConstants::pageSize2M));
    EXPECT_EQ(size, drmAllocation->getMmapSize() - MemoryConstants::pageSize2M);

    auto &bos = drmAllocation->getBOs();
    EXPECT_EQ(2u, bos.size());

    auto gpuAddress = castToUint64(ptr);
    for (auto bo : bos) {
        EXPECT_NE(0, bo->getHandle());
        EXPECT_EQ(gpuAddress, bo->peekAddress());
        EXPECT_EQ(size / 2, bo->peekSize());
        gpuAddress += size / 2;
    }

    unifiedMemoryManager.freeSVMAlloc(ptr);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenCreateKmdMigratedSharedAllocationWithMultipleBOsClearedWhenCreateSharedUnifiedMemoryAllocationIsCalledThenKmdMigratedAllocationWithSingleBOsIsCreated) {
    ioctlHelper->vmAdviseAtomicAttribute = std::nullopt;
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(1);
    debugManager.flags.CreateKmdMigratedSharedAllocationWithMultipleBOs.set(0);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};

    std::vector<MemoryRegion> regionInfo(3);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(1, 0)};

    auto hwInfo = mock->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid = 1;
    hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount = 2;

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));

    SVMAllocsManager unifiedMemoryManager(memoryManager);

    auto size = 2 * MemoryConstants::megaByte;
    UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    auto ptr = unifiedMemoryManager.createSharedUnifiedMemoryAllocation(size, unifiedMemoryProperties, nullptr);
    EXPECT_NE(ptr, nullptr);

    auto allocation = unifiedMemoryManager.getSVMAlloc(ptr)->gpuAllocations.getDefaultGraphicsAllocation();
    EXPECT_NE(allocation, nullptr);

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    EXPECT_EQ(ptr, alignUp(drmAllocation->getMmapPtr(), MemoryConstants::pageSize2M));
    EXPECT_EQ(size, drmAllocation->getMmapSize() - MemoryConstants::pageSize2M);

    auto &bos = drmAllocation->getBOs();
    EXPECT_EQ(1u, bos.size());

    EXPECT_NE(0, bos[0]->getHandle());
    EXPECT_EQ(castToUint64(ptr), bos[0]->peekAddress());
    EXPECT_EQ(size, bos[0]->peekSize());

    unifiedMemoryManager.freeSVMAlloc(ptr);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenKMDSupportForCrossTileMigrationPolicyWhenCreateUnifiedMemoryAllocationWithMultiMemoryRegionsThenGemCreateExtIsUsedWithAllRegions) {
    ioctlHelper->vmAdviseAtomicAttribute = std::nullopt;
    DebugManagerStateRestore restorer;
    debugManager.flags.KMDSupportForCrossTileMigrationPolicy.set(1);

    std::vector<MemoryRegion> regionInfo(4);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(1, 0)};
    regionInfo[3].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(2, 0)};

    auto hwInfo = mock->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid = 1;
    hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount = 3;

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));
    mock->ioctlCallsCount = 0;
    DeviceBitfield devices = 0b101;
    AllocationProperties gpuProperties{
        0u,
        true,
        MemoryConstants::pageSize64k,
        AllocationType::unifiedSharedMemory,
        false,
        devices};
    gpuProperties.alignment = 2 * MemoryConstants::megaByte;
    gpuProperties.usmInitialPlacement = GraphicsAllocation::UsmInitialPlacement::CPU;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(gpuProperties);

    ASSERT_NE(allocation, nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapPtr(), nullptr);
    EXPECT_NE(static_cast<DrmAllocation *>(allocation)->getMmapSize(), 0u);
    EXPECT_EQ(allocation->getAllocationOffset(), 0u);

    ASSERT_FALSE(ioctlHelper->createGemExtCalls.empty());
    const auto &memRegions = ioctlHelper->createGemExtCalls.back().memClassInstances;
    ASSERT_EQ(memRegions.size(), 3u);
    EXPECT_EQ(memRegions[0].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM);
    EXPECT_EQ(memRegions[0].memoryInstance, 1u);
    EXPECT_EQ(memRegions[1].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[1].memoryInstance, regionInfo[1].region.memoryInstance);
    EXPECT_EQ(memRegions[2].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[2].memoryInstance, regionInfo[3].region.memoryInstance);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenKMDSupportForCrossTileMigrationPolicyAndUsmInitialPlacementSetToGpuWhenCreateSharedUnifiedMemoryAllocationOnMultiTileArchitectureThenKmdMigratedAllocationIsCreatedWithCorrectRegionsOrder) {
    ioctlHelper->vmAdviseAtomicAttribute = std::nullopt;
    DebugManagerStateRestore restorer;
    debugManager.flags.UseKmdMigration.set(1);
    debugManager.flags.KMDSupportForCrossTileMigrationPolicy.set(1);
    debugManager.flags.UsmInitialPlacement.set(1);
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};

    std::vector<MemoryRegion> regionInfo(5);
    regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 1};
    regionInfo[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(0, 0)};
    regionInfo[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(1, 0)};
    regionInfo[3].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(2, 0)};
    regionInfo[4].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, DrmMockHelper::getEngineOrMemoryInstanceValue(3, 0)};

    auto hwInfo = mock->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->gtSystemInfo.MultiTileArchInfo.IsValid = 1;
    hwInfo->gtSystemInfo.MultiTileArchInfo.TileCount = 4;

    mock->memoryInfo.reset(new MemoryInfo(regionInfo, *mock));

    SVMAllocsManager unifiedMemoryManager(memoryManager);

    UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = device.get();

    auto ptr = unifiedMemoryManager.createSharedUnifiedMemoryAllocation(MemoryConstants::pageSize64k, unifiedMemoryProperties, nullptr);
    EXPECT_NE(ptr, nullptr);

    auto allocation = unifiedMemoryManager.getSVMAlloc(ptr)->gpuAllocations.getDefaultGraphicsAllocation();
    EXPECT_NE(allocation, nullptr);

    ASSERT_FALSE(ioctlHelper->createGemExtCalls.empty());
    const auto memRegions = ioctlHelper->createGemExtCalls.back().memClassInstances;
    EXPECT_EQ(memRegions.size(), 5u);
    EXPECT_EQ(memRegions[0].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[0].memoryInstance, regionInfo[1].region.memoryInstance);
    EXPECT_EQ(memRegions[1].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[1].memoryInstance, regionInfo[2].region.memoryInstance);
    EXPECT_EQ(memRegions[2].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[2].memoryInstance, regionInfo[3].region.memoryInstance);
    EXPECT_EQ(memRegions[3].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE);
    EXPECT_EQ(memRegions[3].memoryInstance, regionInfo[4].region.memoryInstance);
    EXPECT_EQ(memRegions[4].memoryClass, drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM);
    EXPECT_EQ(memRegions[4].memoryInstance, 1u);

    unifiedMemoryManager.freeSVMAlloc(ptr);
}

TEST_F(DrmMemoryManagerLocalMemoryMultiTileTest, givenChunkSizeBasedColouringPolicyWhenAllocatingInDevicePoolOnAllMemoryBanksThenDivideAllocationIntoEqualBufferObjectsWithGivenChunkSize) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;

    auto &productHelper = *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper;
    auto alignmentSize = productHelper.is2MBLocalMemAlignmentEnabled() ? MemoryConstants::pageSize2M : MemoryConstants::pageSize64k;
    allocData.allFlags = 0;
    allocData.size = 18 * alignmentSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.storageInfo.memoryBanks = maxNBitValue(MemoryBanks::getBankForLocalMemory(3));
    allocData.storageInfo.multiStorage = true;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.colouringPolicy = ColouringPolicy::chunkSizeBased;
    allocData.storageInfo.colouringGranularity = 256 * MemoryConstants::kiloByte;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_NE(0u, allocation->getGpuAddress());
    EXPECT_EQ(allocation->storageInfo.colouringPolicy, ColouringPolicy::chunkSizeBased);
    EXPECT_EQ(allocation->storageInfo.colouringGranularity, 256 * MemoryConstants::kiloByte);

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    // Verify that allocation is divided into chunks based on coloring granularity
    // Each chunk size is aligned to alignmentSize - 64KB or 2MB depending on platform support
    // When 2MB alignment is enabled, KMD can allocate with 2MB pages, improving performance
    auto expectedNumHandles = static_cast<uint32_t>(alignUp(allocData.size, allocation->storageInfo.colouringGranularity) / allocation->storageInfo.colouringGranularity);
    auto numHandles = allocation->getNumGmms();
    // Actual handles can be >= expected because each chunk is aligned to alignmentSize
    EXPECT_GE(numHandles, expectedNumHandles);
    EXPECT_EQ(numHandles, drmAllocation->getBOs().size());

    auto &bos = drmAllocation->getBOs();
    auto boAddress = drmAllocation->getGpuAddress();
    for (auto handleId = 0u; handleId < numHandles; handleId++) {
        auto bo = bos[handleId];
        ASSERT_NE(nullptr, bo);
        // Each chunk size is multiple of coloring granularity, aligned to alignmentSize (64KB or 2MB)
        // 2MB aligned allocations enable KMD to use 2MB pages for better TLB performance
        EXPECT_EQ(boAddress, bo->peekAddress());
        EXPECT_EQ(bo->peekSize() % allocation->storageInfo.colouringGranularity, 0u);
        boAddress += bo->peekSize();
    }
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryMultiTileTest, givenMappingBasedColouringPolicyWhenAllocatingInDevicePoolOnAllMemoryBanksThenSetBindAddressesToBufferObjects) {
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;

    auto &productHelper = *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper;
    auto alignmentSize = productHelper.is2MBLocalMemAlignmentEnabled() ? MemoryConstants::pageSize2M : MemoryConstants::pageSize64k;
    allocData.allFlags = 0;
    allocData.size = 18 * alignmentSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::buffer;
    allocData.storageInfo.memoryBanks = maxNBitValue(MemoryBanks::getBankForLocalMemory(3));
    allocData.storageInfo.multiStorage = true;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.colouringPolicy = ColouringPolicy::mappingBased;
    allocData.storageInfo.colouringGranularity = alignmentSize;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(MemoryPool::localMemory, allocation->getMemoryPool());
    EXPECT_NE(0u, allocation->getGpuAddress());
    EXPECT_EQ(allocation->storageInfo.colouringPolicy, ColouringPolicy::mappingBased);
    EXPECT_EQ(allocation->storageInfo.colouringGranularity, alignmentSize);

    auto drmAllocation = static_cast<DrmAllocation *>(allocation);
    auto numHandles = 4u;
    EXPECT_EQ(numHandles, allocation->getNumGmms());
    EXPECT_EQ(numHandles, drmAllocation->getBOs().size());

    auto &bos = drmAllocation->getBOs();
    auto boAddress = drmAllocation->getGpuAddress();
    for (auto handleId = 0u; handleId < numHandles; handleId++) {
        auto bo = bos[handleId];
        ASSERT_NE(nullptr, bo);
        auto boSize = allocation->getGmm(handleId)->gmmResourceInfo->getSizeAllocation();
        EXPECT_EQ(boSize, bo->peekSize());

        auto addresses = bo->getColourAddresses();
        if (handleId < 2) {
            EXPECT_EQ(addresses.size(), 5u);
            EXPECT_EQ(boSize, 5 * alignmentSize);
        } else {
            EXPECT_EQ(addresses.size(), 4u);
            EXPECT_EQ(boSize, 4 * alignmentSize);
        }
        for (auto i = 0u; i < addresses.size(); i++) {
            EXPECT_EQ(addresses[i], boAddress + 4 * i * allocation->storageInfo.colouringGranularity + handleId * allocation->storageInfo.colouringGranularity);
        }
    }
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryMultiTileTest, givenDrmMemoryManagerWhenCopyMemoryToMultiTileAllocationThenCallCopyMemoryToAllocationBanks) {
    size_t sourceAllocationSize = MemoryConstants::pageSize;
    size_t destinationAllocationSize = sourceAllocationSize;

    DrmMemoryManagerToTestCopyMemoryToAllocation drmMemoryManager(*executionEnvironment, true, destinationAllocationSize);
    std::vector<uint8_t> dataToCopy(sourceAllocationSize, 1u);

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = dataToCopy.size();
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::constantSurface;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    auto allocation = drmMemoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    char data = 0;
    allocation->setCpuPtrAndGpuAddress(&data, allocation->getGpuAddress());

    auto ret = drmMemoryManager.copyMemoryToAllocation(allocation, 0, dataToCopy.data(), dataToCopy.size());
    EXPECT_TRUE(ret);
    EXPECT_EQ(1u, drmMemoryManager.copyMemoryToAllocationBanksCalled);
    drmMemoryManager.freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryMultiTileTest, givenDrmMemoryManagerWhenCopyMemoryToAllocationOnAllMemoryBanksReturnsSuccessThenAllocationIsFilledWithCorrectData) {
    size_t offset = 3;
    size_t sourceAllocationSize = MemoryConstants::pageSize;
    size_t destinationAllocationSize = sourceAllocationSize + offset;

    DrmMemoryManagerToTestCopyMemoryToAllocation drmMemoryManager(*executionEnvironment, true, destinationAllocationSize);
    std::vector<uint8_t> dataToCopy(sourceAllocationSize, 1u);

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = dataToCopy.size();
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::kernelIsa;
    allocData.storageInfo.memoryBanks = maxNBitValue(MemoryBanks::getBankForLocalMemory(3));
    allocData.rootDeviceIndex = rootDeviceIndex;
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    auto allocation = drmMemoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);

    auto ret = drmMemoryManager.copyMemoryToAllocation(allocation, offset, dataToCopy.data(), dataToCopy.size());
    EXPECT_TRUE(ret);

    for (auto index = 0u; index < 3; index++) {
        EXPECT_EQ(0, memcmp(ptrOffset(drmMemoryManager.lockedLocalMemory[index].get(), offset), dataToCopy.data(), dataToCopy.size()));
    }

    drmMemoryManager.freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryMultiTileTest, givenDrmMemoryManagerWhenMemsetMultiTileAllocationThenCallMemsetAllocationBanks) {
    size_t allocationSize = MemoryConstants::pageSize;

    DrmMemoryManagerToTestMemsetAllocation drmMemoryManager(*executionEnvironment, true, allocationSize);
    int value = 0x42;

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = allocationSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::constantSurface;
    allocData.rootDeviceIndex = rootDeviceIndex;
    allocData.storageInfo.memoryBanks = 0b11;

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    auto allocation = drmMemoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    char data = 0;
    allocation->setCpuPtrAndGpuAddress(&data, allocation->getGpuAddress());

    auto ret = drmMemoryManager.memsetAllocation(allocation, 0, value, allocationSize);
    EXPECT_TRUE(ret);
    EXPECT_EQ(1u, drmMemoryManager.memsetAllocationBanksCalled);
    drmMemoryManager.freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryMultiTileTest, givenDrmMemoryManagerWhenMemsetAllocationOnAllMemoryBanksReturnsSuccessThenAllocationIsFilledWithCorrectData) {
    size_t offset = 3;
    size_t allocationSize = MemoryConstants::pageSize;
    size_t destinationAllocationSize = allocationSize + offset;

    DrmMemoryManagerToTestMemsetAllocation drmMemoryManager(*executionEnvironment, true, destinationAllocationSize);
    int value = 0x42;

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = allocationSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::kernelIsa;
    allocData.storageInfo.memoryBanks = maxNBitValue(MemoryBanks::getBankForLocalMemory(3));
    allocData.rootDeviceIndex = rootDeviceIndex;
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    auto allocation = drmMemoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);

    auto ret = drmMemoryManager.memsetAllocation(allocation, offset, value, allocationSize);
    EXPECT_TRUE(ret);

    for (auto index = 0u; index < 3; index++) {
        auto ptr = ptrOffset(drmMemoryManager.lockedLocalMemory[index].get(), offset);
        for (size_t i = 0; i < allocationSize; i++) {
            EXPECT_EQ(value, static_cast<uint8_t *>(ptr)[i]);
        }
    }

    drmMemoryManager.freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryMultiTileTest, givenDrmMemoryManagerWhenMemsetAllocationOnSelectiveMemoryBanksThenOnlySelectedBanksAreFilled) {
    size_t offset = 3;
    size_t allocationSize = MemoryConstants::pageSize;
    size_t destinationAllocationSize = allocationSize + offset;

    DrmMemoryManagerToTestMemsetAllocation drmMemoryManager(*executionEnvironment, true, destinationAllocationSize);
    int value = 0x42;

    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = allocationSize;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::kernelIsa;
    allocData.storageInfo.memoryBanks = maxNBitValue(MemoryBanks::getBankForLocalMemory(3));
    allocData.rootDeviceIndex = rootDeviceIndex;
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    auto allocation = drmMemoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);

    DeviceBitfield memsetBanks = 0b0101;

    auto ret = drmMemoryManager.memsetAllocationBanks(allocation, offset, value, allocationSize, memsetBanks);
    EXPECT_TRUE(ret);

    for (auto lockedIndex = 0u; lockedIndex < 2u; lockedIndex++) {
        ASSERT_NE(nullptr, drmMemoryManager.lockedLocalMemory[lockedIndex]);
        auto ptr = ptrOffset(drmMemoryManager.lockedLocalMemory[lockedIndex].get(), offset);
        for (size_t i = 0; i < allocationSize; i++) {
            EXPECT_EQ(value, static_cast<uint8_t *>(ptr)[i]);
        }
    }

    // Banks 1 and 3 were never locked
    EXPECT_EQ(nullptr, drmMemoryManager.lockedLocalMemory[2]);
    EXPECT_EQ(nullptr, drmMemoryManager.lockedLocalMemory[3]);

    drmMemoryManager.freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMultiRootDeviceEnvironmentWhenCreateMultiGraphicsAllocationThenPrimeFdIsClosedAfterImport) {
    uint32_t rootDevicesNumber = 2u;
    MultiGraphicsAllocation multiGraphics(rootDevicesNumber);
    RootDeviceIndicesContainer rootDeviceIndices;
    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.release();

    executionEnvironment->prepareRootDeviceEnvironments(rootDevicesNumber);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        createDrmForRootDevice(i, true);
        rootDeviceIndices.pushUnique(i);
    }

    memoryManager = new TestedDrmMemoryManager(true, false, false, *executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);

    getDrm(0)->outputFd = 7;
    VariableBackup<uint32_t> closeFuncCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<int> closeFuncArgBackup(&SysCalls::closeFuncArgPassed, 0);

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, {});
    auto ptr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, properties, multiGraphics);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(7, SysCalls::closeFuncArgPassed);

    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        memoryManager->freeGraphicsMemory(multiGraphics.getGraphicsAllocation(i));
    }
    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMultiRootDeviceEnvironmentWhenCreateMultiGraphicsAllocationSucceedsThenInternalHandleCacheIsCleared) {
    uint32_t rootDevicesNumber = 2u;
    MultiGraphicsAllocation multiGraphics(rootDevicesNumber);
    RootDeviceIndicesContainer rootDeviceIndices;
    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.release();

    executionEnvironment->prepareRootDeviceEnvironments(rootDevicesNumber);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        createDrmForRootDevice(i, true);
        rootDeviceIndices.pushUnique(i);
    }

    memoryManager = new TestedDrmMemoryManager(true, false, false, *executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);

    auto drmMock0 = getDrm(0);
    drmMock0->outputFd = 7;

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, {});
    auto ptr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, properties, multiGraphics);
    ASSERT_NE(ptr, nullptr);

    auto primaryAlloc = multiGraphics.getDefaultGraphicsAllocation();
    ASSERT_NE(primaryAlloc, nullptr);

    drmMock0->outputFd = 42;

    uint64_t handleAfterSuccess = 0;
    auto ret = primaryAlloc->peekInternalHandle(memoryManager, handleAfterSuccess, nullptr);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(42u, handleAfterSuccess);

    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        memoryManager->freeGraphicsMemory(multiGraphics.getGraphicsAllocation(i));
    }
    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMultiRootDeviceEnvironmentWhenCreateMultiGraphicsAllocationThenSecondaryAllocationIsRegisteredAndHasNoSharedHandle) {
    uint32_t rootDevicesNumber = 2u;
    MultiGraphicsAllocation multiGraphics(rootDevicesNumber);
    RootDeviceIndicesContainer rootDeviceIndices;
    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.release();

    executionEnvironment->prepareRootDeviceEnvironments(rootDevicesNumber);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        createDrmForRootDevice(i, true);
        rootDeviceIndices.pushUnique(i);
    }

    memoryManager = new TestedDrmMemoryManager(true, false, false, *executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);

    getDrm(0)->outputFd = 7;

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, {});
    auto sysMemBefore = memoryManager->getUsedSystemMemorySize();

    auto ptr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, properties, multiGraphics);
    ASSERT_NE(ptr, nullptr);

    auto secondaryAlloc = multiGraphics.getGraphicsAllocation(1);
    ASSERT_NE(secondaryAlloc, nullptr);
    EXPECT_EQ(Sharing::nonSharedResource, secondaryAlloc->peekSharedHandle());
    EXPECT_GE(memoryManager->getUsedSystemMemorySize(), sysMemBefore + 2 * size);

    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        memoryManager->freeGraphicsMemory(multiGraphics.getGraphicsAllocation(i));
    }
    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMultiRootDeviceEnvironmentWhenCreateMultiGraphicsAllocationFailsOnSecondaryDeviceThenInternalHandleCacheIsClearedAndFreshFdObtained) {
    uint32_t rootDevicesNumber = 2u;
    MultiGraphicsAllocation multiGraphics(rootDevicesNumber);
    RootDeviceIndicesContainer rootDeviceIndices;
    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.release();

    executionEnvironment->prepareRootDeviceEnvironments(rootDevicesNumber);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        auto drm = createDrmForRootDevice(i, true);
        if (i == 1) {
            drm->fdToHandleRetVal = -1;
        }
        rootDeviceIndices.pushUnique(i);
    }

    memoryManager = new TestedDrmMemoryManager(true, false, false, *executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);

    auto drmMock0 = getDrm(0);
    drmMock0->outputFd = 7;

    size_t size = 4096u;
    AllocationProperties properties(0u, true, size, AllocationType::bufferHostMemory, false, {});
    properties.flags.forceSystemMemory = true;

    auto primaryAlloc = memoryManager->allocateGraphicsMemoryWithProperties(properties);
    ASSERT_NE(primaryAlloc, nullptr);
    ASSERT_NE(static_cast<DrmAllocation *>(primaryAlloc)->getMmapPtr(), nullptr);

    multiGraphics.addAllocation(primaryAlloc);

    AllocationProperties secondaryProps(1u, true, size, AllocationType::bufferHostMemory, false, {});
    secondaryProps.flags.forceSystemMemory = true;
    secondaryProps.flags.isUSMHostAllocation = true;
    secondaryProps.flags.allocateMemory = false;

    auto result = memoryManager->createGraphicsAllocationFromExistingStorage(
        secondaryProps, reinterpret_cast<void *>(primaryAlloc->getUnderlyingBuffer()), multiGraphics);
    EXPECT_EQ(result, nullptr);

    drmMock0->outputFd = 42;

    uint64_t handleAfterFailure = 0;
    auto ret = primaryAlloc->peekInternalHandle(memoryManager, handleAfterFailure, nullptr);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(42u, handleAfterFailure);

    memoryManager->freeGraphicsMemory(primaryAlloc);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
}

TEST_F(DrmMemoryManagerLocalMemoryTest, givenMultiRootDeviceEnvironmentAndMemoryInfoWhenCreateMultiGraphicsAllocationAndObtainFdFromHandleFailsThenNullptrIsReturned) {
    uint32_t rootDevicesNumber = 3u;
    MultiGraphicsAllocation multiGraphics(rootDevicesNumber);
    RootDeviceIndicesContainer rootDeviceIndices;
    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.release();

    executionEnvironment->prepareRootDeviceEnvironments(rootDevicesNumber);
    for (uint32_t i = 0; i < rootDevicesNumber; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        createDrmForRootDevice(i, true);

        rootDeviceIndices.pushUnique(i);
    }

    memoryManager = new TestedDrmMemoryManager(true, false, false, *executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, {});

    memoryManager->failOnObtainFdFromHandle = true;
    auto ptr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, properties, multiGraphics);
    EXPECT_EQ(ptr, nullptr);

    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, givenUncacheableFlagWhenCreatingAllocationFromMultipleSharedHandlesThenGmmUsageTypeIsUncached) {
    mock->ioctlExpected.primeFdToHandle = 2;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 2;

    std::vector<NEO::osHandle> handles{6, 7};
    size_t size = 65536u * 2;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::buffer, false, device->getDeviceBitfield());
    properties.flags.uncacheable = true;

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation);

    auto gmmHelper = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getGmmHelper();
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getHelper<ProductHelper>();
    auto expectedUsageType = CacheSettingsHelper::getGmmUsageType(AllocationType::buffer, true, productHelper, gmmHelper->getHardwareInfo());

    for (uint32_t i = 0; i < handles.size(); i++) {
        auto gmm = graphicsAllocation->getGmm(i);
        ASSERT_NE(nullptr, gmm);
        EXPECT_EQ(expectedUsageType, gmm->getResourceUsageType());
        EXPECT_TRUE(CacheSettingsHelper::isUncachedType(gmm->getResourceUsageType()));
    }

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, whenSettingNumHandlesThenTheyAreRetrievedCorrectly) {
    mock->ioctlExpected.primeFdToHandle = 2;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 2;

    std::vector<NEO::osHandle> handles{6, 7};
    size_t size = 65536u * 2;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, device->getDeviceBitfield());

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation);

    uint32_t numHandlesExpected = 8;
    graphicsAllocation->setNumHandles(numHandlesExpected);
    EXPECT_EQ(graphicsAllocation->getNumHandles(), numHandlesExpected);

    graphicsAllocation->setNumHandles(static_cast<uint32_t>(handles.size()));

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, whenCreatingAllocationFromMultipleSharedHandlesWithSharingResourcesThenDifferentAllocationsAreReturned) {
    mock->ioctlExpected.primeFdToHandle = 4;
    mock->ioctlExpected.gemWait = 2;
    mock->ioctlExpected.gemClose = 2;

    std::vector<NEO::osHandle> handles{6, 7};
    size_t size = 65536u * 2;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, device->getDeviceBitfield());

    memoryManager->failOnfindAndReferenceSharedBufferObject = false;

    auto graphicsAllocationFromReferencedHandle = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocationFromReferencedHandle);

    auto graphicsAllocationFromReferencedHandle2 = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocationFromReferencedHandle2);

    DrmAllocation *drmAllocationFromReferencedHandle = static_cast<DrmAllocation *>(graphicsAllocationFromReferencedHandle);
    auto boFromReferencedHandle = drmAllocationFromReferencedHandle->getBO();
    EXPECT_EQ(boFromReferencedHandle->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, boFromReferencedHandle->peekAddress());

    DrmAllocation *drmAllocationFromReferencedHandle2 = static_cast<DrmAllocation *>(graphicsAllocationFromReferencedHandle2);
    auto boFromReferencedHandle2 = drmAllocationFromReferencedHandle2->getBO();
    EXPECT_EQ(boFromReferencedHandle2->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, boFromReferencedHandle2->peekAddress());

    EXPECT_EQ(boFromReferencedHandle->peekAddress(), boFromReferencedHandle2->peekAddress());

    memoryManager->freeGraphicsMemory(graphicsAllocationFromReferencedHandle2);
    memoryManager->freeGraphicsMemory(graphicsAllocationFromReferencedHandle);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, whenCreatingAllocationFromMultipleSharedHandlesWithSharingResourcesWithBasePointerThenUnderlyingPointerIsTheSame) {
    mock->ioctlExpected.primeFdToHandle = 4;
    mock->ioctlExpected.gemWait = 2;
    mock->ioctlExpected.gemClose = 2;

    std::vector<NEO::osHandle> handles{6, 7};
    size_t size = 65536u * 2;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, device->getDeviceBitfield());

    memoryManager->failOnfindAndReferenceSharedBufferObject = false;

    auto graphicsAllocationFromReferencedHandle = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocationFromReferencedHandle);

    auto graphicsAllocationFromReferencedHandle2 = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, true, reinterpret_cast<void *>(graphicsAllocationFromReferencedHandle->getGpuAddress()));
    ASSERT_NE(nullptr, graphicsAllocationFromReferencedHandle2);

    DrmAllocation *drmAllocationFromReferencedHandle = static_cast<DrmAllocation *>(graphicsAllocationFromReferencedHandle);
    auto boFromReferencedHandle = drmAllocationFromReferencedHandle->getBO();
    EXPECT_EQ(boFromReferencedHandle->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, boFromReferencedHandle->peekAddress());

    DrmAllocation *drmAllocationFromReferencedHandle2 = static_cast<DrmAllocation *>(graphicsAllocationFromReferencedHandle2);
    auto boFromReferencedHandle2 = drmAllocationFromReferencedHandle2->getBO();
    EXPECT_EQ(boFromReferencedHandle2->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, boFromReferencedHandle2->peekAddress());

    EXPECT_EQ(boFromReferencedHandle->peekAddress(), boFromReferencedHandle2->peekAddress());

    EXPECT_EQ(graphicsAllocationFromReferencedHandle->getGpuAddress(), graphicsAllocationFromReferencedHandle2->getGpuAddress());
    memoryManager->freeGraphicsMemory(graphicsAllocationFromReferencedHandle2);
    memoryManager->freeGraphicsMemory(graphicsAllocationFromReferencedHandle);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, whenCreatingAllocationFromMultipleSharedHandlesWithNoSharingResourcesThenDifferentAllocationsAreReturned) {
    mock->ioctlExpected.primeFdToHandle = 4;
    mock->ioctlExpected.gemWait = 2;
    mock->ioctlExpected.gemClose = 2;

    std::vector<NEO::osHandle> handles{6, 7};
    size_t size = 65536u * 2;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, device->getDeviceBitfield());

    memoryManager->failOnfindAndReferenceSharedBufferObject = false;

    auto graphicsAllocationFromReferencedHandle = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, false, nullptr);
    ASSERT_NE(nullptr, graphicsAllocationFromReferencedHandle);

    auto graphicsAllocationFromReferencedHandle2 = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, false, nullptr);
    ASSERT_NE(nullptr, graphicsAllocationFromReferencedHandle2);

    DrmAllocation *drmAllocationFromReferencedHandle = static_cast<DrmAllocation *>(graphicsAllocationFromReferencedHandle);
    auto boFromReferencedHandle = drmAllocationFromReferencedHandle->getBO();
    EXPECT_EQ(boFromReferencedHandle->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, boFromReferencedHandle->peekAddress());

    DrmAllocation *drmAllocationFromReferencedHandle2 = static_cast<DrmAllocation *>(graphicsAllocationFromReferencedHandle2);
    auto boFromReferencedHandle2 = drmAllocationFromReferencedHandle2->getBO();
    EXPECT_EQ(boFromReferencedHandle2->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, boFromReferencedHandle2->peekAddress());

    EXPECT_NE(boFromReferencedHandle->peekAddress(), boFromReferencedHandle2->peekAddress());

    memoryManager->freeGraphicsMemory(graphicsAllocationFromReferencedHandle2);
    memoryManager->freeGraphicsMemory(graphicsAllocationFromReferencedHandle);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, whenCreatingAllocationFromMultipleSharedHandlesAndFindAndReferenceSharedBufferObjectReturnsNonNullThenAllocationSucceeds) {
    mock->ioctlExpected.primeFdToHandle = 4;
    mock->ioctlExpected.gemWait = 2;
    mock->ioctlExpected.gemClose = 2;

    std::vector<NEO::osHandle> handles{6, 7};
    size_t size = 65536u * 2;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, device->getDeviceBitfield());

    memoryManager->failOnfindAndReferenceSharedBufferObject = false;
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    auto bo = drmAllocation->getBO();
    EXPECT_EQ(bo->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, bo->peekAddress());

    auto graphicsAllocationFromReferencedHandle = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocationFromReferencedHandle);

    DrmAllocation *drmAllocationFromReferencedHandle = static_cast<DrmAllocation *>(graphicsAllocationFromReferencedHandle);
    auto boFromReferencedHandle = drmAllocationFromReferencedHandle->getBO();
    EXPECT_EQ(boFromReferencedHandle->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, boFromReferencedHandle->peekAddress());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
    memoryManager->freeGraphicsMemory(graphicsAllocationFromReferencedHandle);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, whenCreatingAllocationFromMultipleSharedHandlesWithOneHandleThenAllocationSucceedsAndGpuAddressIsFromTheExpectedHeap) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    std::vector<NEO::osHandle> handles{6};
    size_t size = 65536u * 2;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, device->getDeviceBitfield());

    memoryManager->failOnfindAndReferenceSharedBufferObject = false;
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation);

    const bool prefer57bitAddressing = memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapExtended) > 0;
    const auto expectedHeap = prefer57bitAddressing ? HeapIndex::heapExtended : HeapIndex::heapStandard2MB;

    auto gpuAddress = graphicsAllocation->getGpuAddress();
    auto gmmHelper = device->getGmmHelper();

    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(expectedHeap)), gpuAddress);
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(expectedHeap)), gpuAddress);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    auto bo = drmAllocation->getBO();
    EXPECT_EQ(bo->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, bo->peekAddress());
    EXPECT_EQ(1u, bo->getRefCount());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, whenCreatingAllocationFromMultipleSharedHandlesAndIoctlFailsThenNullIsReturned) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 0;
    mock->ioctlExpected.gemClose = 0;
    mock->failOnPrimeFdToHandle = true;

    std::vector<NEO::osHandle> handles{6, 7};
    size_t size = 65536u * 2;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, device->getDeviceBitfield());

    memoryManager->failOnfindAndReferenceSharedBufferObject = false;
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, true, nullptr);
    EXPECT_EQ(nullptr, graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, whenCreatingAllocationFromMultipleSharedHandlesThenAllocationSucceeds) {
    mock->ioctlExpected.primeFdToHandle = 2;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 2;

    std::vector<NEO::osHandle> handles{6, 7};
    size_t size = 65536u * 2;
    AllocationProperties properties(rootDeviceIndex, true, size, AllocationType::bufferHostMemory, false, device->getDeviceBitfield());

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    auto bo = drmAllocation->getBO();
    EXPECT_EQ(bo->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, bo->peekAddress());
    EXPECT_EQ(1u, bo->getRefCount());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, givenDrmMemoryManagerWithoutMemoryInfoThenDrmAllocationWithoutMappingIsReturned) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    mock->memoryInfo.reset();

    TestedDrmMemoryManager::OsHandleData osHandleData{99u};
    this->mock->outputHandle = osHandleData.handle;

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::bufferHostMemory, false, {});

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, true, true, nullptr);
    EXPECT_EQ(static_cast<int>(osHandleData.handle), static_cast<DrmAllocation *>(graphicsAllocation)->getBO()->peekHandle());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST2_TEMPLATED_F(DrmMemoryManagerTestImpl, givenOsHandleWhenCreatingAllocationFromSharedHandleThenPrimeGemWaitGemCloseAndMmapOffsetIoctlsAreUsed, IsAtMostXeCore) {
    mock->memoryInfo.reset(new MockMemoryInfo(*mock));
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.gemMmapOffset = 1;

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::bufferHostMemory, false, {});

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, true, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, graphicsAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(this->mock->inputFd, (int)osHandleData.handle);
    EXPECT_EQ(this->mock->setTilingHandle, 0u);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    auto bo = drmAllocation->getBO();
    EXPECT_EQ(bo->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, bo->peekAddress());
    EXPECT_EQ(1u, bo->getRefCount());
    EXPECT_EQ(size, bo->peekSize());
    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST2_TEMPLATED_F(DrmMemoryManagerTestImpl, whenPrintBOCreateDestroyResultFlagIsSetAndCallToCreateSharedAllocationForHostAllocationThenExpectedMessageIsPrinted, IsAtMostXeCore) {
    mock->memoryInfo.reset(new MockMemoryInfo(*mock));
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.gemMmapOffset = 1;

    DebugManagerStateRestore stateRestore;
    debugManager.flags.PrintBOCreateDestroyResult.set(true);

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::bufferHostMemory, false, {});

    StreamCapture capture;
    capture.captureStdout();
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, true, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, graphicsAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(this->mock->inputFd, (int)osHandleData.handle);
    EXPECT_EQ(this->mock->setTilingHandle, 0u);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    auto bo = drmAllocation->getBO();
    EXPECT_EQ(bo->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, bo->peekAddress());
    EXPECT_EQ(1u, bo->getRefCount());
    EXPECT_EQ(size, bo->peekSize());

    std::stringstream expectedOutput;
    expectedOutput << "Created BO-0 range: ";
    expectedOutput << std::hex << graphicsAllocation->getGpuAddress();
    expectedOutput << " - ";
    expectedOutput << std::hex << ptrOffset(graphicsAllocation->getGpuAddress(), MemoryConstants::pageSize);
    expectedOutput << ", size: 4096 from PRIME_FD_TO_HANDLE\nCalling gem close on handle: BO-0\n";

    memoryManager->freeGraphicsMemory(graphicsAllocation);

    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(expectedOutput.str(), output);
}

HWTEST2_TEMPLATED_F(DrmMemoryManagerTestImpl, givenDrmMemoryManagerAndOsHandleWhenCreateIsCalledForAnExistingAllocationGraphicsAllocationIsReturned, IsAtMostXeCore) {
    mock->memoryInfo.reset(new MockMemoryInfo(*mock));
    mock->ioctlExpected.primeFdToHandle = 2;
    mock->ioctlExpected.gemWait = 2;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.gemMmapOffset = 1;

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    this->mock->outputHandle = 2u;
    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::bufferHostMemory, false, {});

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, true, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(size, graphicsAllocation->getUnderlyingBufferSize());
    EXPECT_EQ(this->mock->inputFd, (int)osHandleData.handle);
    EXPECT_EQ(this->mock->setTilingHandle, 0u);

    DrmAllocation *drmAllocation = static_cast<DrmAllocation *>(graphicsAllocation);
    auto bo = drmAllocation->getBO();
    EXPECT_EQ(bo->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, bo->peekAddress());
    EXPECT_EQ(1u, bo->getRefCount());
    EXPECT_EQ(size, bo->peekSize());

    auto graphicsAllocation2 = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, true, true, nullptr);
    ASSERT_NE(nullptr, graphicsAllocation2);

    EXPECT_NE(nullptr, graphicsAllocation2->getUnderlyingBuffer());
    EXPECT_EQ(size, graphicsAllocation2->getUnderlyingBufferSize());

    DrmAllocation *drmAllocation2 = static_cast<DrmAllocation *>(graphicsAllocation2);
    auto bo2 = drmAllocation2->getBO();
    EXPECT_EQ(bo2->peekHandle(), (int)this->mock->outputHandle);
    EXPECT_NE(0llu, bo2->peekAddress());
    EXPECT_EQ(2u, bo2->getRefCount());
    EXPECT_EQ(size, bo2->peekSize());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
    memoryManager->freeGraphicsMemory(graphicsAllocation2);
}

HWTEST2_TEMPLATED_F(DrmMemoryManagerTestImpl, givenDrmMemoryManagerAndOsHandleWhenCreateIsCalledAndMmapOffsetIoctlFailsThenNullAllocationIsReturned, IsAtMostXeCore) {
    mock->memoryInfo.reset(new MockMemoryInfo(*mock));
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemMmapOffset = 2;

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::bufferHostMemory, false, {});

    mock->returnIoctlExtraErrorValue = true;
    mock->failOnMmapOffset = true;
    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, true, true, nullptr);
    EXPECT_EQ(nullptr, graphicsAllocation);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, givenDrmMemoryManagerAndUseMmapObjectSetToFalseThenDrmAllocationWithoutMappingIsReturned) {
    mock->memoryInfo.reset(new MockMemoryInfo(*mock));
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    TestedDrmMemoryManager::OsHandleData osHandleData{99u};
    this->mock->outputHandle = osHandleData.handle;

    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::bufferHostMemory, false, {});
    properties.useMmapObject = false;

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, true, true, nullptr);
    EXPECT_EQ(static_cast<int>(osHandleData.handle), static_cast<DrmAllocation *>(graphicsAllocation)->getBO()->peekHandle());

    memoryManager->freeGraphicsMemory(graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, MmapFailWhenUSMHostAllocationFromSharedHandleThenNullPtrReturned) {
    mock->memoryInfo.reset(new MockMemoryInfo(*mock));
    mock->ioctlExpected.primeFdToHandle = 1;

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    size_t size = 4096u;
    AllocationProperties properties(rootDeviceIndex, false, size, AllocationType::bufferHostMemory, false, {});

    memoryManager->mmapFunction = [](void *addr, size_t len, int prot,
                                     int flags, int fd, off_t offset) throw() {
        return MAP_FAILED;
    };

    auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, true, true, nullptr);

    ASSERT_EQ(nullptr, graphicsAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTestImpl, givenDrmMemoryManagerWhenGetLocalMemorySizeIsCalledThenReturnMemoryRegionSizeCorrectly) {
    auto hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &sysInfo = hwInfo.gtSystemInfo;

    sysInfo.MultiTileArchInfo.IsValid = true;
    sysInfo.MultiTileArchInfo.TileCount = 4;

    MockExecutionEnvironment executionEnvironment(&hwInfo, true, 4u);
    for (auto i = 0u; i < 4u; i++) {
        auto drm = std::unique_ptr<DrmMock>(DrmMemoryManagerLocalMemoryTest::createDrm(*executionEnvironment.rootDeviceEnvironments[i]));
        static_cast<MockIoctlHelperWithCapture *>(drm->getIoctlHelper())->memoryRegionsToReturn = extendedMemoryRegions;

        executionEnvironment.rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment.rootDeviceEnvironments[i]->osInterface->setDriverModel(std::move(drm));
    }
    TestedDrmMemoryManager memoryManager(executionEnvironment);
    for (auto i = 0u; i < 4u; i++) {
        auto drm = executionEnvironment.rootDeviceEnvironments[i]->osInterface->getDriverModel()->as<DrmMock>();
        drm->memoryInfoQueried = false;
        drm->queryMemoryInfo();

        auto memoryInfo = drm->getMemoryInfo();
        ASSERT_NE(nullptr, memoryInfo);

        uint64_t expectedSize = 0;
        for (uint32_t i = 0; i < sysInfo.MultiTileArchInfo.TileCount; i++) {
            expectedSize += memoryInfo->getMemoryRegionSize(1 << i);
        }

        auto deviceMask = static_cast<uint32_t>(maxNBitValue(sysInfo.MultiTileArchInfo.TileCount));
        EXPECT_EQ(expectedSize, memoryManager.getLocalMemorySize(i, deviceMask));
        EXPECT_EQ(memoryInfo->getMemoryRegionSize(1), memoryManager.getLocalMemorySize(i, 1));
    }
}

TEST(AllocationInfoLogging, givenDrmGraphicsAllocationWithMultipleBOsWhenGettingImplementationSpecificAllocationInfoThenReturnInfoStringWithAllHandles) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    BufferObject bo0(0, &drm, 3, 0, 0, 1), bo1(0, &drm, 3, 1, 0, 1),
        bo2(0, &drm, 3, 2, 0, 1), bo3(0, &drm, 3, 3, 0, 1);
    BufferObjects bos{&bo0, &bo1, &bo2, &bo3};
    DrmAllocation drmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, bos, nullptr, 0u, 0u, MemoryPool::localMemory);

    EXPECT_STREQ(drmAllocation.getAllocationInfoString().c_str(), " Handle: 0 Handle: 1 Handle: 2 Handle: 3");
}

TEST_P(DrmMemoryManagerAllocation57BitTest, givenAllocationTypeHaveToBeAllocatedAs57BitThenHeapExtendedIsUsed) {
    if (0 == memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapExtended)) {
        GTEST_SKIP();
    }

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, MemoryConstants::pageSize, GetParam(), mockDeviceBitfield});
    ASSERT_NE(nullptr, allocation);

    auto gpuAddress = allocation->getGpuAddress();
    auto gmmHelper = device->getGmmHelper();
    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapBase(HeapIndex::heapExtended)), gpuAddress);
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(0)->getHeapLimit(HeapIndex::heapExtended)), gpuAddress);

    memoryManager->freeGraphicsMemory(allocation);
}

INSTANTIATE_TEST_SUITE_P(Drm57Bit,
                         DrmMemoryManagerAllocation57BitTest,
                         ::testing::ValuesIn(allocation57Bit));

} // namespace NEO

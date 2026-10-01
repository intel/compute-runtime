/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/memory_manager/compression_selector.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/os_interface/linux/allocator_helper.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/utilities/heap_allocator.h"
#include "shared/test/common/libult/linux/drm_mock_helper.h"
#include "shared/test/common/mocks/mock_gfx_partition.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/unit_test/os_interface/linux/drm_memory_manager_local_memory_fixture.h"

#include "gtest/gtest.h"

namespace NEO {

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

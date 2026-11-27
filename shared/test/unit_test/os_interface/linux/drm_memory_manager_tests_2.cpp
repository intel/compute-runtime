/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_stream/tag_allocation_layout.h"
#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/local_memory_usage.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/linux/drm_fabric.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_bind.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/release_helpers/release_helper/release_helper.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/linux/mock_buffer_object.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/linux/mock_drm_command_stream_receiver.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/linux/mock_ioctl_helper.h"
#include "shared/test/common/mocks/linux/mock_os_context_linux.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gfx_partition.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_client_context_base.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/mocks/mock_host_ptr_manager.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/mock_release_helper.h"
#include "shared/test/common/os_interface/linux/drm_memory_manager_fixture.h"
#include "shared/test/common/os_interface/linux/drm_mock_cache_info.h"
#include "shared/test/common/os_interface/linux/drm_mock_memory_info.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/os_interface/linux/drm_memory_manager_tests.h"

#include "clos_matchers.h"
#include "gtest/gtest.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <vector>

namespace {
enum class KmdIsaFailingStep {
    gemCreateExt,
    mmapOffset,
};

MockedMemoryInfo *installMockedMemoryInfo(DrmMockCustom &drm) {
    std::vector<MemoryRegion> regionInfo(1);
    regionInfo[0].region = {static_cast<uint16_t>(drm.getIoctlHelper()->getDrmParamValue(DrmParam::memoryClassSystem)), 0};
    drm.memoryInfo.reset(new MockedMemoryInfo(regionInfo, drm));
    return static_cast<MockedMemoryInfo *>(drm.getMemoryInfo());
}

void setAnonymousMmapFunctions(TestedDrmMemoryManager &memoryManager) {
    memoryManager.mmapFunction = [](void *addr, size_t len, int prot, int flags, int, off_t) noexcept {
        return NEO::SysCalls::mmap(addr, len, prot, flags | MAP_ANONYMOUS, -1, 0);
    };
    memoryManager.munmapFunction = [](void *addr, size_t len) noexcept {
        return NEO::SysCalls::munmap(addr, len);
    };
}
} // namespace

HWTEST_TEMPLATED_F(DrmMemoryManagerUSMHostAllocationTests, givenMmapPtrWhenFreeGraphicsMemoryImplThenPtrIsDeallocated) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemClose = 1;

    const size_t size = 16 * MemoryConstants::kiloByte;
    AllocationData allocationData;
    allocationData.size = size;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    auto alloc = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);
    EXPECT_NE(nullptr, alloc);

    auto ptr = memoryManager->mmapFunction(0, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS, -1, 0);
    static_cast<DrmAllocation *>(alloc)->setMmapPtr(ptr);
    static_cast<DrmAllocation *>(alloc)->setMmapSize(size);

    memoryManager->freeGraphicsMemoryImpl(alloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerUSMHostAllocationTests,
                   givenCallToallocateGraphicsMemoryWithAlignmentWithisHostUSMAllocationSetToTrueThenTheExistingHostPointerIsUsedAndAllocationIsCreatedSuccessfully) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemClose = 1;

    AllocationData allocationData;

    size_t allocSize = 16 * MemoryConstants::kiloByte;
    void *hostPtr = alignedMalloc(allocSize, 0);

    allocationData.size = allocSize;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.isUSMHostAllocation = true;
    allocationData.hostPtr = hostPtr;
    NEO::GraphicsAllocation *alloc = memoryManager->allocateGraphicsMemory(allocationData);
    EXPECT_NE(nullptr, alloc);
    EXPECT_EQ(hostPtr, alloc->getUnderlyingBuffer());

    memoryManager->freeGraphicsMemoryImpl(alloc);
    alignedFree(hostPtr);
}

namespace {
uint64_t getDefaultUserPtrPatIndex(const Drm &drm, AllocationType allocationType) {
    return drm.getPatIndex(nullptr, allocationType, CacheRegion::defaultRegion, CachePolicy::writeBack, false, true, drm.getIoctlHelper()->isUserptrCoherencyRequired());
}

uint64_t getCachelineMisalignedUserPtrPatIndex(const Drm &drm, AllocationType allocationType) {
    const auto &rootDeviceEnvironment = drm.getRootDeviceEnvironment();
    const auto &releaseHelper = rootDeviceEnvironment.getReleaseHelper();
    const auto usageType = CacheSettingsHelper::getGmmUsageTypeForCachelineMisalignedUserPtr(*rootDeviceEnvironment.getHardwareInfo(), releaseHelper);
    if (usageType == GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER) {
        return getDefaultUserPtrPatIndex(drm, allocationType);
    }
    return releaseHelper.overrideSystemMemoryPatIndex(rootDeviceEnvironment.getGmmClientContext()->cachePolicyGetPATIndex(nullptr, usageType, false, true));
}
} // namespace

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDefaultSettingsAndPartiallyCoveredFragmentWhenPopulatingOsHandlesThenDefaultUserPtrPatIndexIsUsed) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    OsHandleStorage storage;
    storage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    storage.fragmentStorageData[0].fragmentSize = MemoryConstants::pageSize;
    storage.fragmentStorageData[0].partiallyCoveredByAllocation = true;
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, memoryManager->populateOsHandles(storage, rootDeviceIndex));

    auto bo = static_cast<OsHandleLinux *>(storage.fragmentStorageData[0].osHandleStorage)->bo;
    EXPECT_EQ(getDefaultUserPtrPatIndex(memoryManager->getDrm(rootDeviceIndex), AllocationType::externalHostPtr), bo->peekPatIndex());

    memoryManager->getHostPtrManager()->releaseHandleStorage(rootDeviceIndex, storage);
    memoryManager->cleanOsHandles(storage, rootDeviceIndex);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenPatIndexProgrammingSupportedWhenAllocatingUserptrThenPatIndexIsQueriedWithResourceInfoOfCacheableBufferGmm) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->vmBindPatIndexProgrammingSupported = true;

    auto &drm = memoryManager->getDrm(rootDeviceIndex);
    auto mockClientContext = static_cast<MockGmmClientContextBase *>(drm.getRootDeviceEnvironment().getGmmClientContext());
    mockClientContext->passedResourceInfoForGetPatIndexQuery = nullptr;

    BufferObject *bo = memoryManager->allocUserptrWithTemporaryGmm(0x1000, MemoryConstants::pageSize, AllocationType::externalHostPtr, rootDeviceIndex);
    ASSERT_NE(nullptr, bo);

    EXPECT_NE(nullptr, mockClientContext->passedResourceInfoForGetPatIndexQuery);
    EXPECT_FALSE(mockClientContext->passedCompressedSettingForGetPatIndexQuery);
    EXPECT_TRUE(mockClientContext->passedCpuCacheableSettingForGetPatIndexQuery);
    const auto patIndex = bo->peekPatIndex();
    EXPECT_EQ(getDefaultUserPtrPatIndex(drm, AllocationType::externalHostPtr), patIndex);

    memoryManager->unreference(bo, false);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenUserptrCoherencyRequiredAndUncachedUsageWhenAllocatingUserptrThenPatIndexIsQueriedWithCpuCacheableBufferGmm) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceUncachedGmmUsageType.set(1llu << (static_cast<int64_t>(AllocationType::externalHostPtr) - 1));

    auto ioctlHelper = new MockIoctlHelper(*mock);
    ioctlHelper->isUserptrCoherencyRequiredResult = true;
    mock->ioctlHelper.reset(ioctlHelper);
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->vmBindPatIndexProgrammingSupported = true;

    auto &drm = memoryManager->getDrm(rootDeviceIndex);
    auto mockClientContext = static_cast<MockGmmClientContextBase *>(drm.getRootDeviceEnvironment().getGmmClientContext());
    mockClientContext->passedResourceInfoForGetPatIndexQuery = nullptr;

    BufferObject *bo = memoryManager->allocUserptrWithTemporaryGmm(0x1000, MemoryConstants::pageSize, AllocationType::externalHostPtr, rootDeviceIndex);
    ASSERT_NE(nullptr, bo);

    EXPECT_NE(nullptr, mockClientContext->passedResourceInfoForGetPatIndexQuery);
    EXPECT_TRUE(mockClientContext->passedCpuCacheableSettingForGetPatIndexQuery);
    const auto patIndex = bo->peekPatIndex();
    EXPECT_EQ(getDefaultUserPtrPatIndex(drm, AllocationType::externalHostPtr), patIndex);

    memoryManager->unreference(bo, false);
}

struct DrmMemoryManagerUsmHostPtrFragmentsFixture : public DrmMemoryManagerFixture {
    template <typename GfxFamily>
    void setUpT() {
        debugManager.flags.EnableUsmHostPtrFragments.set(1);
        DrmMemoryManagerFixture::setUpT<GfxFamily>();
    }

    template <typename GfxFamily>
    void tearDownT() {
        DrmMemoryManagerFixture::tearDownT<GfxFamily>();
    }

    DebugManagerStateRestore restorer;
    void *const hostPtrBase = reinterpret_cast<void *>(0x100000);
};

using DrmMemoryManagerUsmHostPtrFragmentsTests = Test<DrmMemoryManagerUsmHostPtrFragmentsFixture>;

HWTEST_TEMPLATED_F(DrmMemoryManagerUSMHostAllocationTests, givenDefaultSettingsWhenAllocatingUnalignedExternalHostPtrThenSingleBufferObjectStartsAtHostPtrWithoutOffset) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemClose = 1;

    auto hostPtrBase = reinterpret_cast<void *>(0x100000);
    void *hostPtr = ptrOffset(hostPtrBase, 0x40);

    AllocationData allocationData;
    allocationData.size = 0x100;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.isUSMHostAllocation = true;
    allocationData.hostPtr = hostPtr;
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemory(allocationData));
    ASSERT_NE(nullptr, alloc);

    auto bo = alloc->getBO();
    ASSERT_NE(nullptr, bo);
    EXPECT_EQ(0u, alloc->getFragmentCount());
    EXPECT_EQ(castToUint64(hostPtr), bo->peekAddress());
    EXPECT_EQ(MemoryConstants::pageSize, bo->peekSize());
    EXPECT_EQ(0u, alloc->getAllocationOffset());
    EXPECT_EQ(castToUint64(hostPtr), alloc->getGpuAddress());
    EXPECT_EQ(MemoryConstants::pageSize, alloc->getUnderlyingBufferSize());

    memoryManager->freeGraphicsMemoryImpl(alloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerUsmHostPtrFragmentsTests, givenUnalignedExternalHostPtrWhenAllocatingUsmHostAllocationThenPageAlignedFragmentsBackItAndGpuAddressEqualsHostPtr) {
    mock->ioctlExpected.gemUserptr = 3;
    mock->ioctlExpected.gemWait = 3;
    mock->ioctlExpected.gemClose = 3;

    void *hostPtr = ptrOffset(hostPtrBase, 0x40);

    AllocationData allocationData;
    allocationData.size = 2 * MemoryConstants::pageSize;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.isUSMHostAllocation = true;
    allocationData.hostPtr = hostPtr;
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemory(allocationData));
    ASSERT_NE(nullptr, alloc);

    EXPECT_EQ(castToUint64(hostPtr), alloc->getGpuAddress());
    EXPECT_EQ(hostPtr, alloc->getUnderlyingBuffer());
    ASSERT_EQ(3u, alloc->getFragmentCount());
    for (auto fragmentId = 0u; fragmentId < 3u; fragmentId++) {
        const auto &fragment = alloc->fragmentsStorage->fragmentStorageData[fragmentId];
        auto bo = static_cast<OsHandleLinux *>(fragment.osHandleStorage)->bo;
        EXPECT_EQ(ptrOffset(hostPtrBase, fragmentId * MemoryConstants::pageSize), fragment.cpuPtr);
        EXPECT_EQ(MemoryConstants::pageSize, fragment.fragmentSize);
        EXPECT_EQ(castToUint64(fragment.cpuPtr), bo->peekAddress());
    }

    memoryManager->freeGraphicsMemoryImpl(alloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerUsmHostPtrFragmentsTests, givenTwoUsmHostAllocationsInOnePageWhenFirstIsFreedThenSharedFragmentIsReleasedOnlyWithSecond) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    AllocationData allocationData;
    allocationData.size = 0x100;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.isUSMHostAllocation = true;
    allocationData.hostPtr = ptrOffset(hostPtrBase, 0x40);
    auto firstAlloc = memoryManager->allocateGraphicsMemory(allocationData);
    allocationData.hostPtr = ptrOffset(hostPtrBase, 0x800);
    auto secondAlloc = memoryManager->allocateGraphicsMemory(allocationData);
    ASSERT_NE(nullptr, firstAlloc);
    ASSERT_NE(nullptr, secondAlloc);

    ASSERT_EQ(1u, firstAlloc->getFragmentCount());
    ASSERT_EQ(1u, secondAlloc->getFragmentCount());
    EXPECT_EQ(firstAlloc->fragmentsStorage->fragmentStorageData[0].osHandleStorage, secondAlloc->fragmentsStorage->fragmentStorageData[0].osHandleStorage);

    memoryManager->freeGraphicsMemoryImpl(firstAlloc);
    EXPECT_EQ(0, mock->ioctlCnt.gemClose.load());

    memoryManager->freeGraphicsMemoryImpl(secondAlloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerUsmHostPtrFragmentsTests, givenUsmHostAllocationsSharingFragmentWhenCollectingBufferObjectsForTwoSubmissionsThenEachSubmissionGetsEveryFragmentOnce) {
    mock->ioctlExpected.gemUserptr = 2;
    mock->ioctlExpected.gemWait = 2;
    mock->ioctlExpected.gemClose = 2;

    AllocationData allocationData;
    allocationData.size = 0x100;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.isUSMHostAllocation = true;
    allocationData.hostPtr = ptrOffset(hostPtrBase, 0x40);
    auto firstAlloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemory(allocationData));
    allocationData.size = MemoryConstants::pageSize;
    allocationData.hostPtr = ptrOffset(hostPtrBase, 0x800);
    auto secondAlloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemory(allocationData));
    ASSERT_NE(nullptr, firstAlloc);
    ASSERT_NE(nullptr, secondAlloc);
    ASSERT_EQ(1u, firstAlloc->getFragmentCount());
    ASSERT_EQ(2u, secondAlloc->getFragmentCount());
    auto sharedBo = static_cast<OsHandleLinux *>(firstAlloc->fragmentsStorage->fragmentStorageData[0].osHandleStorage)->bo;
    auto secondPageBo = static_cast<OsHandleLinux *>(secondAlloc->fragmentsStorage->fragmentStorageData[1].osHandleStorage)->bo;
    ASSERT_EQ(sharedBo, static_cast<OsHandleLinux *>(secondAlloc->fragmentsStorage->fragmentStorageData[0].osHandleStorage)->bo);

    auto osContext = device->getDefaultEngine().osContext;
    for (auto vmHandleId = 0u; vmHandleId < 2u; vmHandleId++) {
        std::vector<BufferObject *> submissionBos;
        EXPECT_EQ(0, firstAlloc->makeBOsResident(osContext, vmHandleId, &submissionBos, false, false));
        EXPECT_EQ(0, secondAlloc->makeBOsResident(osContext, vmHandleId, &submissionBos, false, false));
        EXPECT_EQ(2u, submissionBos.size());
        EXPECT_EQ(1, std::count(submissionBos.begin(), submissionBos.end(), sharedBo));
        EXPECT_EQ(1, std::count(submissionBos.begin(), submissionBos.end(), secondPageBo));
    }

    memoryManager->freeGraphicsMemoryImpl(firstAlloc);
    memoryManager->freeGraphicsMemoryImpl(secondAlloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerUsmHostPtrFragmentsTests, givenUsmHostAllocationWhoseFragmentIsBiggerThanStoredFragmentWhenAllocatingThenAllocationFails) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.hostPtr = ptrOffset(hostPtrBase, MemoryConstants::pageSize);
    auto storedAlloc = memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData);
    ASSERT_NE(nullptr, storedAlloc);
    ASSERT_EQ(1u, storedAlloc->getFragmentCount());

    allocationData.size = 4 * MemoryConstants::pageSize - 0x80;
    allocationData.flags.isUSMHostAllocation = true;
    allocationData.hostPtr = ptrOffset(hostPtrBase, 0x40);
    EXPECT_EQ(nullptr, memoryManager->allocateGraphicsMemory(allocationData));
    EXPECT_EQ(1u, static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager())->getFragmentCount());

    memoryManager->freeGraphicsMemoryImpl(storedAlloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerUsmHostPtrFragmentsTests, givenSvmCpuUsmHostAllocationWithUnalignedExternalHostPtrWhenAllocatingThenSingleBufferObjectCoversWholePagesAndAllocationOffsetPointsToHostPtr) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemClose = 1;

    void *hostPtr = ptrOffset(hostPtrBase, 0x40);

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.isUSMHostAllocation = true;
    allocationData.type = AllocationType::svmCpu;
    allocationData.hostPtr = hostPtr;
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemory(allocationData));
    ASSERT_NE(nullptr, alloc);

    auto bo = alloc->getBO();
    ASSERT_NE(nullptr, bo);
    EXPECT_EQ(0u, alloc->getFragmentCount());
    EXPECT_EQ(2 * MemoryConstants::pageSize, bo->peekSize());
    EXPECT_EQ(0x40u, alloc->getAllocationOffset());
    EXPECT_EQ(bo->peekAddress() + 0x40, alloc->getGpuAddress());
    EXPECT_EQ(hostPtr, alloc->getUnderlyingBuffer());
    EXPECT_EQ(getDefaultUserPtrPatIndex(memoryManager->getDrm(rootDeviceIndex), AllocationType::svmCpu), bo->peekPatIndex());

    memoryManager->freeGraphicsMemoryImpl(alloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerUsmHostPtrFragmentsTests, givenSvmCpuUsmHostAllocationWithCachelineMisalignedExternalHostPtrWhenAllocatingThenItsBufferObjectUsesCachelineMisalignedUserPtrPatIndex) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemClose = 1;

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.isUSMHostAllocation = true;
    allocationData.type = AllocationType::svmCpu;
    allocationData.hostPtr = ptrOffset(hostPtrBase, 0x41);
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemory(allocationData));
    ASSERT_NE(nullptr, alloc);

    auto bo = alloc->getBO();
    ASSERT_NE(nullptr, bo);
    EXPECT_EQ(0x41u, alloc->getAllocationOffset());
    EXPECT_EQ(getCachelineMisalignedUserPtrPatIndex(memoryManager->getDrm(rootDeviceIndex), AllocationType::svmCpu), bo->peekPatIndex());

    memoryManager->freeGraphicsMemoryImpl(alloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerUsmHostPtrFragmentsTests, givenSvmCpuUsmHostAllocationWithCachelineMisalignedExternalHostPtrWhenAllocatingThenAllocationKeepsCachelineMisalignedUserPtrGmm) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemClose = 1;
    for (auto &rootDeviceEnvironment : executionEnvironment->rootDeviceEnvironments) {
        rootDeviceEnvironment->getMutableHardwareInfo()->caps.misalignedUserPtr2WayCoherent = true;
    }

    AllocationData allocationData;
    allocationData.size = MemoryConstants::pageSize;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.isUSMHostAllocation = true;
    allocationData.type = AllocationType::svmCpu;
    allocationData.hostPtr = ptrOffset(hostPtrBase, 0x41);
    auto alloc = memoryManager->allocateGraphicsMemory(allocationData);
    ASSERT_NE(nullptr, alloc);

    const auto &rootDeviceEnvironment = memoryManager->getDrm(rootDeviceIndex).getRootDeviceEnvironment();
    const auto expectedUsage = CacheSettingsHelper::getGmmUsageTypeForUserPtr(true, allocationData.hostPtr, allocationData.size, *rootDeviceEnvironment.getHardwareInfo(), rootDeviceEnvironment.getReleaseHelper());
    EXPECT_NE(CacheSettingsHelper::getGmmUsageType(AllocationType::svmCpu, false, rootDeviceEnvironment.getProductHelper(), *rootDeviceEnvironment.getHardwareInfo()), expectedUsage);
    ASSERT_NE(nullptr, alloc->getDefaultGmm());
    EXPECT_EQ(expectedUsage, alloc->getDefaultGmm()->getResourceUsageType());

    memoryManager->freeGraphicsMemoryImpl(alloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerUsmHostPtrFragmentsTests, givenPartiallyCoveredFragmentWhenPopulatingOsHandlesThenPatIndexIsQueriedWithResourceInfoOfBufferGmm) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->vmBindPatIndexProgrammingSupported = true;

    for (auto &rootDeviceEnvironment : executionEnvironment->rootDeviceEnvironments) {
        rootDeviceEnvironment->getMutableHardwareInfo()->caps.misalignedUserPtr2WayCoherent = true;
    }
    const auto &rootDeviceEnvironment = memoryManager->getDrm(rootDeviceIndex).getRootDeviceEnvironment();
    const auto usageType = CacheSettingsHelper::getGmmUsageTypeForCachelineMisalignedUserPtr(*rootDeviceEnvironment.getHardwareInfo(), rootDeviceEnvironment.getReleaseHelper());
    ASSERT_NE(GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER, usageType);
    auto mockClientContext = static_cast<MockGmmClientContextBase *>(rootDeviceEnvironment.getGmmClientContext());

    OsHandleStorage storage;
    storage.fragmentStorageData[0].cpuPtr = hostPtrBase;
    storage.fragmentStorageData[0].fragmentSize = MemoryConstants::pageSize;
    storage.fragmentStorageData[0].partiallyCoveredByAllocation = true;
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, memoryManager->populateOsHandles(storage, rootDeviceIndex));

    EXPECT_NE(nullptr, mockClientContext->passedResourceInfoForGetPatIndexQuery);
    ASSERT_TRUE(mockClientContext->passedUsageTypeForGetPatIndexQuery.has_value());
    EXPECT_EQ(usageType, mockClientContext->passedUsageTypeForGetPatIndexQuery.value());
    EXPECT_FALSE(mockClientContext->passedCompressedSettingForGetPatIndexQuery);
    EXPECT_TRUE(mockClientContext->passedCpuCacheableSettingForGetPatIndexQuery);
    auto bo = static_cast<OsHandleLinux *>(storage.fragmentStorageData[0].osHandleStorage)->bo;
    EXPECT_EQ(getCachelineMisalignedUserPtrPatIndex(memoryManager->getDrm(rootDeviceIndex), AllocationType::externalHostPtr), bo->peekPatIndex());

    memoryManager->getHostPtrManager()->releaseHandleStorage(rootDeviceIndex, storage);
    memoryManager->cleanOsHandles(storage, rootDeviceIndex);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerUsmHostPtrFragmentsTests, givenUnalignedUsmHostAllocationBackedByFragmentsWhenAllocatingThenOnlyPartiallyCoveredPagesUseCachelineMisalignedUserPtrPatIndex) {
    mock->ioctlExpected.gemUserptr = 3;
    mock->ioctlExpected.gemWait = 3;
    mock->ioctlExpected.gemClose = 3;

    AllocationData allocationData;
    allocationData.size = 2 * MemoryConstants::pageSize;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.isUSMHostAllocation = true;
    allocationData.hostPtr = ptrOffset(hostPtrBase, 0x40);
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemory(allocationData));
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(3u, alloc->getFragmentCount());

    const auto &drm = memoryManager->getDrm(rootDeviceIndex);
    const uint64_t expectedPatIndices[] = {getCachelineMisalignedUserPtrPatIndex(drm, AllocationType::externalHostPtr),
                                           getDefaultUserPtrPatIndex(drm, AllocationType::externalHostPtr),
                                           getCachelineMisalignedUserPtrPatIndex(drm, AllocationType::externalHostPtr)};
    for (auto fragmentId = 0u; fragmentId < 3u; fragmentId++) {
        auto bo = static_cast<OsHandleLinux *>(alloc->fragmentsStorage->fragmentStorageData[fragmentId].osHandleStorage)->bo;
        EXPECT_EQ(expectedPatIndices[fragmentId], bo->peekPatIndex());
    }

    memoryManager->freeGraphicsMemoryImpl(alloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerUsmHostPtrFragmentsTests, givenCachelineAlignedUsmHostAllocationCoveringPartOfPageWhenAllocatingThenItsFragmentUsesCachelineMisalignedUserPtrPatIndex) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    AllocationData allocationData;
    allocationData.size = 4 * MemoryConstants::cacheLineSize;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.isUSMHostAllocation = true;
    allocationData.hostPtr = hostPtrBase;
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemory(allocationData));
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(1u, alloc->getFragmentCount());

    auto bo = static_cast<OsHandleLinux *>(alloc->fragmentsStorage->fragmentStorageData[0].osHandleStorage)->bo;
    EXPECT_EQ(getCachelineMisalignedUserPtrPatIndex(memoryManager->getDrm(rootDeviceIndex), AllocationType::externalHostPtr), bo->peekPatIndex());

    memoryManager->freeGraphicsMemoryImpl(alloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerUsmHostPtrFragmentsTests, givenUsmHostAllocationBackedByFragmentsWhenQueryingInternalHandleThenErrorIsReturned) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    AllocationData allocationData;
    allocationData.size = 0x100;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.isUSMHostAllocation = true;
    allocationData.hostPtr = hostPtrBase;
    auto alloc = memoryManager->allocateGraphicsMemory(allocationData);
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(1u, alloc->getFragmentCount());

    uint64_t handle = 0;
    EXPECT_EQ(-1, alloc->peekInternalHandle(memoryManager, handle, nullptr));

    memoryManager->freeGraphicsMemoryImpl(alloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerUsmHostPtrFragmentsTests, givenFourUsmHostAllocationsInOnePageWhenFreedInAnyOrderThenSharedFragmentIsReleasedOnlyWithTheLastOne) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    constexpr size_t offsets[] = {0x40, 0x400, 0x800, 0xc00};

    AllocationData allocationData;
    allocationData.size = 0x100;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.isUSMHostAllocation = true;
    GraphicsAllocation *allocations[4] = {};
    for (auto i = 0u; i < 4u; i++) {
        allocationData.hostPtr = ptrOffset(hostPtrBase, offsets[i]);
        allocations[i] = memoryManager->allocateGraphicsMemory(allocationData);
        ASSERT_NE(nullptr, allocations[i]);
        ASSERT_EQ(1u, allocations[i]->getFragmentCount());
        EXPECT_EQ(allocations[0]->fragmentsStorage->fragmentStorageData[0].osHandleStorage, allocations[i]->fragmentsStorage->fragmentStorageData[0].osHandleStorage);
        EXPECT_EQ(castToUint64(allocationData.hostPtr), allocations[i]->getGpuAddress());
    }

    for (auto i : {1u, 3u, 0u}) {
        memoryManager->freeGraphicsMemoryImpl(allocations[i]);
        EXPECT_EQ(0, mock->ioctlCnt.gemClose.load());
    }
    memoryManager->freeGraphicsMemoryImpl(allocations[2]);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerUsmHostPtrFragmentsTests, givenUsmHostAllocationBackedByFragmentWhenAllocatingOverlappingRangeInSamePageThenAllocationFailsUntilOverlappedAllocationIsFreed) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    AllocationData allocationData;
    allocationData.size = 0x100;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.isUSMHostAllocation = true;
    allocationData.hostPtr = ptrOffset(hostPtrBase, 0x40);
    auto firstAlloc = memoryManager->allocateGraphicsMemory(allocationData);
    ASSERT_NE(nullptr, firstAlloc);

    allocationData.hostPtr = ptrOffset(hostPtrBase, 0x100);
    EXPECT_EQ(nullptr, memoryManager->allocateGraphicsMemory(allocationData));
    allocationData.hostPtr = ptrOffset(hostPtrBase, 0x20);
    EXPECT_EQ(nullptr, memoryManager->allocateGraphicsMemory(allocationData));

    allocationData.hostPtr = ptrOffset(hostPtrBase, 0x140);
    auto adjacentAlloc = memoryManager->allocateGraphicsMemory(allocationData);
    ASSERT_NE(nullptr, adjacentAlloc);
    EXPECT_EQ(1u, static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager())->getFragmentCount());

    memoryManager->freeGraphicsMemoryImpl(firstAlloc);
    allocationData.hostPtr = ptrOffset(hostPtrBase, 0x40);
    auto reusedRangeAlloc = memoryManager->allocateGraphicsMemory(allocationData);
    ASSERT_NE(nullptr, reusedRangeAlloc);

    memoryManager->freeGraphicsMemoryImpl(reusedRangeAlloc);
    memoryManager->freeGraphicsMemoryImpl(adjacentAlloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerUsmHostPtrFragmentsTests, givenUsmHostAllocationBackedByFragmentsWhenAllocatingOverlappingRangeWithSameFragmentsThenAllocationFails) {
    mock->ioctlExpected.gemUserptr = 3;
    mock->ioctlExpected.gemWait = 3;
    mock->ioctlExpected.gemClose = 3;

    AllocationData allocationData;
    allocationData.size = 2 * MemoryConstants::pageSize;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.isUSMHostAllocation = true;
    allocationData.hostPtr = ptrOffset(hostPtrBase, 0x40);
    auto alloc = memoryManager->allocateGraphicsMemory(allocationData);
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(3u, alloc->getFragmentCount());

    allocationData.hostPtr = ptrOffset(hostPtrBase, 0x80);
    EXPECT_EQ(nullptr, memoryManager->allocateGraphicsMemory(allocationData));
    EXPECT_EQ(3u, static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager())->getFragmentCount());

    memoryManager->freeGraphicsMemoryImpl(alloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerUsmHostPtrFragmentsTests, givenUsmHostAllocationBackedByFragmentsWhenHandlingFenceCompletionWithoutVmBindThenEveryFragmentIsWaited) {
    mock->ioctlExpected.gemUserptr = 3;
    mock->ioctlExpected.gemWait = 6;
    mock->ioctlExpected.gemClose = 3;
    mock->isVmBindAvailableCall.callParent = false;
    mock->isVmBindAvailableCall.returnValue = false;

    AllocationData allocationData;
    allocationData.size = 2 * MemoryConstants::pageSize;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.isUSMHostAllocation = true;
    allocationData.hostPtr = ptrOffset(hostPtrBase, 0x40);
    auto alloc = memoryManager->allocateGraphicsMemory(allocationData);
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(3u, alloc->getFragmentCount());

    memoryManager->handleFenceCompletion(alloc);
    EXPECT_EQ(3, mock->ioctlCnt.gemWait.load());

    memoryManager->freeGraphicsMemoryImpl(alloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerUsmHostPtrFragmentsTests, givenLiveAllocationWhenAllocationReusingItsPageFailsOnAnotherFragmentThenReusedPageIsReleasedTogetherWithLiveAllocation) {
    mock->ioctlExpected.gemUserptr = 2;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    AllocationData allocationData;
    allocationData.size = 0x40;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.isUSMHostAllocation = true;
    allocationData.hostPtr = hostPtrBase;
    auto liveAlloc = memoryManager->allocateGraphicsMemory(allocationData);
    ASSERT_NE(nullptr, liveAlloc);
    ASSERT_EQ(1u, liveAlloc->getFragmentCount());

    DrmMockCustom::IoctlResExt ioctlResExt = {static_cast<int32_t>(mock->ioctlCnt.total.load()), -1};
    mock->ioctlResExt = &ioctlResExt;
    allocationData.size = MemoryConstants::pageSize;
    allocationData.hostPtr = ptrOffset(hostPtrBase, 0x40);
    EXPECT_EQ(nullptr, memoryManager->allocateGraphicsMemory(allocationData));
    mock->ioctlResExt = &mock->none;
    EXPECT_EQ(1u, static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager())->getFragmentCount());

    memoryManager->freeGraphicsMemoryImpl(liveAlloc);
    EXPECT_EQ(1, mock->ioctlCnt.gemClose.load());
    EXPECT_EQ(0u, static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager())->getFragmentCount());
}

HWTEST_TEMPLATED_F(DrmMemoryManagerUsmHostPtrFragmentsTests, givenCacheRegionRequestedForUsmHostAllocationSharingLiveFragmentWhenAllocatingThenFragmentBuffersKeepDefaultCacheRegionAndPatIndex) {
    mock->ioctlExpected.gemUserptr = 2;
    mock->ioctlExpected.gemWait = 2;
    mock->ioctlExpected.gemClose = 2;

    AllocationData allocationData;
    allocationData.size = 0x100;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.isUSMHostAllocation = true;
    allocationData.hostPtr = ptrOffset(hostPtrBase, 0x40);
    auto liveAlloc = memoryManager->allocateGraphicsMemory(allocationData);
    ASSERT_NE(nullptr, liveAlloc);
    auto sharedBo = static_cast<OsHandleLinux *>(liveAlloc->fragmentsStorage->fragmentStorageData[0].osHandleStorage)->bo;
    const auto sharedBoPatIndex = sharedBo->peekPatIndex();

    allocationData.size = MemoryConstants::pageSize;
    allocationData.hostPtr = ptrOffset(hostPtrBase, 0x800);
    allocationData.cacheRegion = static_cast<uint32_t>(CacheRegion::region1);
    auto alloc = memoryManager->allocateGraphicsMemory(allocationData);
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(2u, alloc->getFragmentCount());
    EXPECT_EQ(sharedBo, static_cast<OsHandleLinux *>(alloc->fragmentsStorage->fragmentStorageData[0].osHandleStorage)->bo);
    EXPECT_EQ(sharedBoPatIndex, sharedBo->peekPatIndex());
    for (auto fragmentId = 0u; fragmentId < alloc->getFragmentCount(); fragmentId++) {
        EXPECT_EQ(CacheRegion::defaultRegion, static_cast<OsHandleLinux *>(alloc->fragmentsStorage->fragmentStorageData[fragmentId].osHandleStorage)->bo->peekCacheRegion());
    }

    memoryManager->freeGraphicsMemoryImpl(alloc);
    memoryManager->freeGraphicsMemoryImpl(liveAlloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerUsmHostPtrFragmentsTests, givenPageAlignedExternalHostPtrAndSizeWhenAllocatingUsmHostAllocationThenSingleBufferObjectStartsAtHostPtr) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemClose = 1;

    AllocationData allocationData;
    allocationData.size = 2 * MemoryConstants::pageSize;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.isUSMHostAllocation = true;
    allocationData.hostPtr = hostPtrBase;
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemory(allocationData));
    ASSERT_NE(nullptr, alloc);

    auto bo = alloc->getBO();
    ASSERT_NE(nullptr, bo);
    EXPECT_EQ(0u, alloc->getFragmentCount());
    EXPECT_EQ(0u, static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager())->getFragmentCount());
    EXPECT_EQ(castToUint64(hostPtrBase), bo->peekAddress());
    EXPECT_EQ(2 * MemoryConstants::pageSize, bo->peekSize());
    EXPECT_EQ(castToUint64(hostPtrBase), alloc->getGpuAddress());
    EXPECT_EQ(getDefaultUserPtrPatIndex(memoryManager->getDrm(rootDeviceIndex), allocationData.type), bo->peekPatIndex());

    memoryManager->freeGraphicsMemoryImpl(alloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerUsmHostPtrFragmentsTests, givenReadOnlyUsmHostAllocationWithPageAlignedExternalHostPtrAndSizeWhenAllocatingThenReadOnlyBufferObjectStartsAtHostPtr) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::bufferHostMemory, false, mockDeviceBitfield);
    properties.flags.isUSMHostAllocation = true;
    properties.flags.readOnly = true;
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(properties, hostPtrBase));
    ASSERT_NE(nullptr, alloc);

    auto bo = alloc->getBO();
    ASSERT_NE(nullptr, bo);
    EXPECT_EQ(0u, alloc->getFragmentCount());
    EXPECT_EQ(castToUint64(hostPtrBase), bo->peekAddress());
    EXPECT_EQ(0u, alloc->getAllocationOffset());
    EXPECT_TRUE(bo->isReadOnlyGpuResource());
    EXPECT_EQ(0u, static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager())->getFragmentCount());

    memoryManager->freeGraphicsMemory(alloc);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenUsmHostPtrFragmentsEnabledAndForcePinWhenAllocatingUnalignedUsmHostAllocationAboveThresholdThenEveryFragmentIsPinned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableUsmHostPtrFragments.set(1);
    mock->ioctlExpected.gemUserptr = 4;
    mock->ioctlExpected.gemClose = 4;
    mock->ioctlExpected.execbuffer2 = 3;
    mock->ioctlExpected.gemWait = 3;

    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager->pinBBs[rootDeviceIndex]);

    auto hostPtrBase = reinterpret_cast<void *>(0x100000);
    allocationData.size = memoryManager->pinThreshold;
    allocationData.hostPtr = ptrOffset(hostPtrBase, 0x40);
    allocationData.flags.isUSMHostAllocation = true;
    allocationData.flags.forcePin = true;
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemory(allocationData));
    ASSERT_NE(nullptr, alloc);
    EXPECT_EQ(3u, alloc->getFragmentCount());

    memoryManager->freeGraphicsMemoryImpl(alloc);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenUsmHostPtrFragmentsAndHostMemoryValidationEnabledWhenAllocatingUnalignedUsmHostAllocationAboveForcePinThresholdThenFragmentsArePinnedAfterValidationOnlyWithVmBind) {
    class ValidatingPinBufferObject : public BufferObject {
      public:
        ValidatingPinBufferObject(uint32_t rootDeviceIndex, Drm *drm) : BufferObject(rootDeviceIndex, drm, 3, 1, 0, 1) {}

        int validateHostPtr(BufferObject *const boToPin[], size_t numberOfBos, OsContext *osContext, uint32_t vmHandleId, uint32_t drmContextId) override {
            numberOfBosValidated += numberOfBos;
            return 0;
        }
        size_t numberOfBosValidated = 0u;
    };

    DebugManagerStateRestore restorer;
    debugManager.flags.EnableUsmHostPtrFragments.set(1);
    mock->isVmBindAvailableCall.callParent = false;

    for (const bool vmBindAvailable : {false, true}) {
        mock->isVmBindAvailableCall.returnValue = vmBindAvailable;
        auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, true, *executionEnvironment);
        memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
        for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
            engine.osContext->incRefInternal();
        }
        auto pinBB = new ValidatingPinBufferObject(rootDeviceIndex, this->mock);
        memoryManager->injectPinBB(pinBB, rootDeviceIndex);
        memoryManager->emitPinningRequestForBoContainerCallBase = false;

        auto hostPtrBase = reinterpret_cast<void *>(0x100000);
        AllocationData allocationData;
        allocationData.size = memoryManager->pinThreshold;
        allocationData.rootDeviceIndex = rootDeviceIndex;
        allocationData.hostPtr = ptrOffset(hostPtrBase, 0x40);
        allocationData.flags.isUSMHostAllocation = true;
        allocationData.flags.forcePin = true;
        auto alloc = memoryManager->allocateGraphicsMemory(allocationData);
        ASSERT_NE(nullptr, alloc);
        EXPECT_EQ(3u, alloc->getFragmentCount());

        EXPECT_EQ(3u, pinBB->numberOfBosValidated);
        EXPECT_EQ(vmBindAvailable ? 3u : 0u, memoryManager->emitPinningRequestForBoContainerCalled);

        memoryManager->freeGraphicsMemoryImpl(alloc);
    }
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDefaultDrmMemoryManagerWhenAskedForAlignedMallocRestrictionsThenNullPtrIsReturned) {
    EXPECT_EQ(nullptr, memoryManager->getAlignedMallocRestrictions());
}

struct DeferBackingPressureDrmMemoryManager : public DrmMemoryManager {
    using DrmMemoryManager::DrmMemoryManager;
    uint64_t maxLocalMemory = 0u;
    uint32_t getLocalMemorySizeCallCount = 0u;
    uint64_t getLocalMemorySize(uint32_t rootDeviceIndex, uint32_t deviceBitfield) override {
        getLocalMemorySizeCallCount++;
        return maxLocalMemory;
    }
    void cacheMaxLocalMemorySize(uint32_t rootDeviceIndex) override {
        this->localMemorySupported[rootDeviceIndex] = true;
        this->cachedMaxLocalMemory[rootDeviceIndex] = this->getLocalMemorySize(rootDeviceIndex, 0u);
    }
    void setUsedLocalMemory(uint32_t rootDeviceIndex, size_t value) {
        this->localMemAllocsSize[rootDeviceIndex] = value;
    }
    bool getSystemMemoryUsageFromProcMeminfo(uint64_t &totalBytes, uint64_t &freeBytes) override {
        totalBytes = totalHostMemory;
        freeBytes = availableHostMemory;
        return meminfoAvailable;
    }
    using DrmMemoryManager::deferBackedLocalMemAllocsSize;
    bool meminfoAvailable = true;
    uint64_t totalHostMemory = 0u;
    uint64_t availableHostMemory = 0u;
};

TEST_F(DrmMemoryManagerBasic, givenThresholdWhenProjectedUsageCrossesThresholdThenDeferBackingDecisionFlips) {
    DeferBackingPressureDrmMemoryManager memoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment);
    memoryManager.maxLocalMemory = 1000u;
    memoryManager.cacheMaxLocalMemorySize(rootDeviceIndex);

    memoryManager.setUsedLocalMemory(rootDeviceIndex, 700u);
    EXPECT_FALSE(memoryManager.isDeferBackingMemoryPressureReached(rootDeviceIndex, 0u, 80));

    memoryManager.setUsedLocalMemory(rootDeviceIndex, 799u);
    EXPECT_FALSE(memoryManager.isDeferBackingMemoryPressureReached(rootDeviceIndex, 0u, 80));

    memoryManager.setUsedLocalMemory(rootDeviceIndex, 800u);
    EXPECT_TRUE(memoryManager.isDeferBackingMemoryPressureReached(rootDeviceIndex, 0u, 80));

    memoryManager.commonCleanup();
}

TEST_F(DrmMemoryManagerBasic, givenAllocationSizeWhenProjectedUsageCrossesThresholdThenDeferBackingIsEnabled) {
    DeferBackingPressureDrmMemoryManager memoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment);
    memoryManager.maxLocalMemory = 1000u;
    memoryManager.cacheMaxLocalMemorySize(rootDeviceIndex);

    memoryManager.setUsedLocalMemory(rootDeviceIndex, 700u);
    EXPECT_FALSE(memoryManager.isDeferBackingMemoryPressureReached(rootDeviceIndex, 99u, 80));
    EXPECT_TRUE(memoryManager.isDeferBackingMemoryPressureReached(rootDeviceIndex, 100u, 80));

    memoryManager.commonCleanup();
}

TEST_F(DrmMemoryManagerBasic, givenThresholdZeroThenDeferBackingAlwaysTriggeredAndAtHundredOnlyAtFull) {
    DeferBackingPressureDrmMemoryManager memoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment);
    memoryManager.maxLocalMemory = 1000u;
    memoryManager.cacheMaxLocalMemorySize(rootDeviceIndex);

    memoryManager.setUsedLocalMemory(rootDeviceIndex, 0u);
    EXPECT_TRUE(memoryManager.isDeferBackingMemoryPressureReached(rootDeviceIndex, 0u, 0));

    memoryManager.setUsedLocalMemory(rootDeviceIndex, 999u);
    EXPECT_FALSE(memoryManager.isDeferBackingMemoryPressureReached(rootDeviceIndex, 0u, 100));
    EXPECT_TRUE(memoryManager.isDeferBackingMemoryPressureReached(rootDeviceIndex, 1u, 100));

    memoryManager.setUsedLocalMemory(rootDeviceIndex, 1000u);
    EXPECT_TRUE(memoryManager.isDeferBackingMemoryPressureReached(rootDeviceIndex, 0u, 100));

    memoryManager.commonCleanup();
}

TEST_F(DrmMemoryManagerBasic, givenZeroMaxLocalMemoryWhenCheckingDeferBackingPressureThenReturnsFalse) {
    DeferBackingPressureDrmMemoryManager memoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment);
    memoryManager.maxLocalMemory = 0u;
    memoryManager.cacheMaxLocalMemorySize(rootDeviceIndex);

    memoryManager.setUsedLocalMemory(rootDeviceIndex, 0u);
    EXPECT_FALSE(memoryManager.isDeferBackingMemoryPressureReached(rootDeviceIndex, 4096u, 50));

    memoryManager.commonCleanup();
}

TEST_F(DrmMemoryManagerBasic, givenUnspecifiedRootDeviceIndexWhenCheckingDeferBackingPressureThenReturnsFalse) {
    DeferBackingPressureDrmMemoryManager memoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment);
    memoryManager.maxLocalMemory = 1000u;

    EXPECT_FALSE(memoryManager.isDeferBackingMemoryPressureReached(CommonConstants::unspecifiedDeviceIndex, 0u, 80));

    memoryManager.commonCleanup();
}

TEST_F(DrmMemoryManagerBasic, givenMaxLocalMemoryCachedAtInitWhenCheckedMultipleTimesThenItIsNotQueriedAgain) {
    DeferBackingPressureDrmMemoryManager memoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment);
    memoryManager.maxLocalMemory = 1000u;
    memoryManager.cacheMaxLocalMemorySize(rootDeviceIndex);
    EXPECT_EQ(1u, memoryManager.getLocalMemorySizeCallCount);

    memoryManager.setUsedLocalMemory(rootDeviceIndex, 900u);
    EXPECT_TRUE(memoryManager.isDeferBackingMemoryPressureReached(rootDeviceIndex, 0u, 80));
    EXPECT_EQ(1u, memoryManager.getLocalMemorySizeCallCount);

    memoryManager.maxLocalMemory = 5000u;
    EXPECT_TRUE(memoryManager.isDeferBackingMemoryPressureReached(rootDeviceIndex, 0u, 80));
    EXPECT_EQ(1u, memoryManager.getLocalMemorySizeCallCount);

    memoryManager.commonCleanup();
}

TEST_F(DrmMemoryManagerBasic, givenPrintDeferBackingLogsEnabledWhenCheckingDeferBackingPressureThenDecisionIsPrinted) {
    DebugManagerStateRestore restorer;
    debugManager.flags.PrintDeferBackingLogs.set(true);

    DeferBackingPressureDrmMemoryManager memoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment);
    memoryManager.maxLocalMemory = 1000u;
    memoryManager.cacheMaxLocalMemorySize(rootDeviceIndex);
    memoryManager.setUsedLocalMemory(rootDeviceIndex, 900u);

    StreamCapture capture;
    capture.captureStderr();
    EXPECT_TRUE(memoryManager.isDeferBackingMemoryPressureReached(rootDeviceIndex, 0u, 80));
    const std::string output = capture.getCapturedStderr();

    EXPECT_NE(std::string::npos, output.find("[DeferBackingPressure]"));

    memoryManager.commonCleanup();
}

TEST_F(DrmMemoryManagerBasic, givenHostThresholdWhenProjectedHostUsageCrossesThresholdThenHostPressureIsReached) {
    DeferBackingPressureDrmMemoryManager memoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment);
    memoryManager.totalHostMemory = 1000u;
    memoryManager.availableHostMemory = 500u;

    EXPECT_FALSE(memoryManager.isDeferBackingHostMemoryPressureReached(99u, 60));
    EXPECT_TRUE(memoryManager.isDeferBackingHostMemoryPressureReached(100u, 60));

    memoryManager.commonCleanup();
}

TEST_F(DrmMemoryManagerBasic, givenOutstandingDeferBackedLocalMemoryWhenCheckingHostPressureThenItCountsTowardProjectedUsage) {
    DeferBackingPressureDrmMemoryManager memoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment);
    memoryManager.totalHostMemory = 1000u;
    memoryManager.availableHostMemory = 1000u;

    memoryManager.deferBackedLocalMemAllocsSize = 599u;
    EXPECT_FALSE(memoryManager.isDeferBackingHostMemoryPressureReached(0u, 60));

    memoryManager.deferBackedLocalMemAllocsSize = 600u;
    EXPECT_TRUE(memoryManager.isDeferBackingHostMemoryPressureReached(0u, 60));

    memoryManager.deferBackedLocalMemAllocsSize = 0u;
    memoryManager.commonCleanup();
}

TEST_F(DrmMemoryManagerBasic, givenAvailableHostMemoryAboveTotalWhenCheckingHostPressureThenUsedHostMemoryIsZero) {
    DeferBackingPressureDrmMemoryManager memoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment);
    memoryManager.totalHostMemory = 1000u;
    memoryManager.availableHostMemory = 2000u;

    EXPECT_FALSE(memoryManager.isDeferBackingHostMemoryPressureReached(599u, 60));
    EXPECT_TRUE(memoryManager.isDeferBackingHostMemoryPressureReached(600u, 60));

    memoryManager.commonCleanup();
}

TEST_F(DrmMemoryManagerBasic, givenHostMemoryUsageUnknownWhenCheckingHostPressureThenReturnsFalse) {
    DeferBackingPressureDrmMemoryManager memoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment);
    memoryManager.meminfoAvailable = false;
    memoryManager.totalHostMemory = 1000u;
    EXPECT_FALSE(memoryManager.isDeferBackingHostMemoryPressureReached(1000u, 0));

    memoryManager.meminfoAvailable = true;
    memoryManager.totalHostMemory = 0u;
    EXPECT_FALSE(memoryManager.isDeferBackingHostMemoryPressureReached(1000u, 0));

    memoryManager.commonCleanup();
}

TEST_F(DrmMemoryManagerBasic, givenPrintDeferBackingLogsEnabledWhenCheckingHostPressureThenDecisionIsPrinted) {
    DebugManagerStateRestore restorer;
    debugManager.flags.PrintDeferBackingLogs.set(true);

    DeferBackingPressureDrmMemoryManager memoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment);
    memoryManager.totalHostMemory = 1000u;
    memoryManager.availableHostMemory = 1000u;

    StreamCapture capture;
    capture.captureStderr();
    EXPECT_FALSE(memoryManager.isDeferBackingHostMemoryPressureReached(0u, 60));
    EXPECT_TRUE(memoryManager.isDeferBackingHostMemoryPressureReached(600u, 60));
    const std::string output = capture.getCapturedStderr();

    EXPECT_NE(std::string::npos, output.find("[DeferBackingHostPressure]"));
    EXPECT_NE(std::string::npos, output.find("deferBacking=ON"));
    EXPECT_NE(std::string::npos, output.find("deferBacking=off"));

    memoryManager.commonCleanup();
}

TEST_F(DrmMemoryManagerBasic, givenLocalMemoryAllocationWhenRegisteredAndUnregisteredThenOnlyDeferBackedSizeIsTracked) {
    DeferBackingPressureDrmMemoryManager memoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment);
    auto drm = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel()->as<Drm>();

    MockBufferObject deferredBo(rootDeviceIndex, drm, 3, 0, 1000u, 1);
    deferredBo.setDeferBackingUsed(true);
    MockDrmAllocation deferredAllocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);
    deferredAllocation.bufferObjects[0] = &deferredBo;
    deferredAllocation.setSize(1000u);

    MockBufferObject backedBo(rootDeviceIndex, drm, 3, 0, 500u, 1);
    MockDrmAllocation backedAllocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);
    backedAllocation.bufferObjects[0] = &backedBo;
    backedAllocation.setSize(500u);

    EXPECT_EQ(MemoryManager::AllocationStatus::Success, memoryManager.registerLocalMemAlloc(&deferredAllocation, rootDeviceIndex));
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, memoryManager.registerLocalMemAlloc(&backedAllocation, rootDeviceIndex));
    EXPECT_EQ(1000u, memoryManager.deferBackedLocalMemAllocsSize);
    EXPECT_EQ(1500u, memoryManager.getUsedLocalMemorySize(rootDeviceIndex));

    memoryManager.unregisterAllocation(&backedAllocation);
    EXPECT_EQ(1000u, memoryManager.deferBackedLocalMemAllocsSize);

    memoryManager.unregisterAllocation(&deferredAllocation);
    EXPECT_EQ(0u, memoryManager.deferBackedLocalMemAllocsSize);
    EXPECT_EQ(0u, memoryManager.getUsedLocalMemorySize(rootDeviceIndex));

    memoryManager.commonCleanup();
}

TEST_F(DrmMemoryManagerBasic, givenMultiBankLocalMemoryAllocationWhenRegisteredAndUnregisteredThenSizesOfAllDeferBackedBufferObjectsAreTracked) {
    DeferBackingPressureDrmMemoryManager memoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, executionEnvironment);
    auto drm = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel()->as<Drm>();

    MockBufferObject deferredBo1(rootDeviceIndex, drm, 3, 0, 1000u, 1);
    deferredBo1.setDeferBackingUsed(true);
    MockBufferObject deferredBo2(rootDeviceIndex, drm, 3, 0, 1000u, 1);
    deferredBo2.setDeferBackingUsed(true);
    MockBufferObject backedBo(rootDeviceIndex, drm, 3, 0, 1000u, 1);

    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects.resize(4);
    allocation.bufferObjects[0] = nullptr;
    allocation.bufferObjects[1] = &deferredBo1;
    allocation.bufferObjects[2] = &deferredBo2;
    allocation.bufferObjects[3] = &backedBo;
    allocation.setSize(1000u);

    EXPECT_EQ(MemoryManager::AllocationStatus::Success, memoryManager.registerLocalMemAlloc(&allocation, rootDeviceIndex));
    EXPECT_EQ(2000u, memoryManager.deferBackedLocalMemAllocsSize);

    memoryManager.unregisterAllocation(&allocation);
    EXPECT_EQ(0u, memoryManager.deferBackedLocalMemAllocsSize);

    memoryManager.commonCleanup();
}

TEST_F(DrmMemoryManagerBasic, givenDefaultMemoryManagerWhenItIsCreatedThenAsyncDeleterEnabledIsTrue) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useGemCloseWorker = true;
    DrmMemoryManager memoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, true, executionEnvironment);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    memoryManager.commonCleanup();
}

TEST_F(DrmMemoryManagerBasic, givenDisabledGemCloseWorkerWhenMemoryManagerIsCreatedThenNoGemCloseWorker) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useGemCloseWorker = true;
    DebugManagerStateRestore dbgStateRestore;
    debugManager.flags.EnableGemCloseWorker.set(0u);

    TestedDrmMemoryManager memoryManager(true, true, true, executionEnvironment);

    EXPECT_EQ(memoryManager.peekGemCloseWorker(), nullptr);
}

TEST_F(DrmMemoryManagerBasic, givenEnabledAsyncDeleterFlagWhenMemoryManagerIsCreatedThenAsyncDeleterEnabledIsFalseAndDeleterIsNullptr) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useGemCloseWorker = true;
    DebugManagerStateRestore dbgStateRestore;
    debugManager.flags.EnableDeferredDeleter.set(1);
    DrmMemoryManager memoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, true, executionEnvironment);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    memoryManager.commonCleanup();
}

TEST_F(DrmMemoryManagerBasic, givenDisabledAsyncDeleterFlagWhenMemoryManagerIsCreatedThenAsyncDeleterEnabledIsFalseAndDeleterIsNullptr) {
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useGemCloseWorker = true;
    DebugManagerStateRestore dbgStateRestore;
    debugManager.flags.EnableDeferredDeleter.set(0);
    DrmMemoryManager memoryManager(GemCloseWorkerMode::gemCloseWorkerInactive, false, true, executionEnvironment);
    EXPECT_FALSE(memoryManager.isAsyncDeleterEnabled());
    EXPECT_EQ(nullptr, memoryManager.getDeferredDeleter());
    memoryManager.commonCleanup();
}

TEST_F(DrmMemoryManagerBasic, givenDefaultDrmMemoryManagerWhenItIsQueriedForInternalHeapBaseThenInternalHeapBaseIsReturned) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    true,
                                                                                                    true,
                                                                                                    executionEnvironment));
    auto heapBase = memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapInternalDeviceMemory);
    EXPECT_EQ(heapBase, memoryManager->getInternalHeapBaseAddress(rootDeviceIndex, true));
}

TEST_F(DrmMemoryManagerBasic, givenMemoryManagerWithEnabledHostMemoryValidationWhenFeatureIsQueriedThenTrueIsReturned) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    executionEnvironment));
    ASSERT_NE(nullptr, memoryManager.get());
    EXPECT_TRUE(memoryManager->isValidateHostMemoryEnabled());
}

TEST_F(DrmMemoryManagerBasic, givenMemoryManagerWithDisabledHostMemoryValidationWhenFeatureIsQueriedThenFalseIsReturned) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    false,
                                                                                                    executionEnvironment));
    ASSERT_NE(nullptr, memoryManager.get());
    EXPECT_FALSE(memoryManager->isValidateHostMemoryEnabled());
}

TEST_F(DrmMemoryManagerBasic, givenEnabledHostMemoryValidationWhenMemoryManagerIsCreatedThenPinBBIsCreated) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    executionEnvironment));
    ASSERT_NE(nullptr, memoryManager.get());
    ASSERT_NE(nullptr, memoryManager->pinBBs[rootDeviceIndex]);
}

TEST_F(DrmMemoryManagerBasic, givenEnabledHostMemoryValidationAndForcePinWhenMemoryManagerIsCreatedThenPinBBIsCreated) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    true,
                                                                                                    true,
                                                                                                    executionEnvironment));
    ASSERT_NE(nullptr, memoryManager.get());
    ASSERT_NE(nullptr, memoryManager->pinBBs[rootDeviceIndex]);
}

TEST_F(DrmMemoryManagerBasic, givenMemoryManagerWhenAllocateGraphicsMemoryIsCalledThenMemoryPoolIsSystem4KBPages) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(
        false,
        false,
        true,
        executionEnvironment));

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, MemoryConstants::pageSize, false));
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::system4KBPages, allocation->getMemoryPool());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenMemoryManagerWhenAllocateGraphicsMemoryWithPtrIsCalledThenMemoryPoolIsSystem4KBPages) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    *executionEnvironment));
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    void *ptr = reinterpret_cast<void *>(0x1001);
    auto size = 4096u;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), false, size}, ptr);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::system4KBPages, allocation->getMemoryPool());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerBasic, givenMemoryManagerWhenCreateAllocationFromHandleIsCalledThenMemoryPoolIsSystemCpuInaccessible) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    executionEnvironment));
    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, {});
    auto allocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryPool::systemCpuInaccessible, allocation->getMemoryPool());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerBasic, givenMemoryManagerWhenCreateAllocationFromHandleIsCalledThenAllocationIsNotRegisteredNorUnregistered) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    executionEnvironment));
    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, {});

    auto allocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    EXPECT_NE(nullptr, allocation);
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(0u, memoryManager->registerSysMemAllocCalled);
    EXPECT_EQ(0u, memoryManager->registerLocalMemAllocCalled);
    EXPECT_EQ(0u, memoryManager->unregisterAllocationCalled);
}

struct DrmMemoryManagerFdImportTest : DrmMemoryManagerWithExplicitExpectationsTest {
    static constexpr int callerFd = 7;
    static constexpr int firstDuplicateFd = 20;
    static inline int nextDuplicateFd = firstDuplicateFd;

    static int dupCallerFd(int fd) {
        return (fd == callerFd) ? nextDuplicateFd++ : -1;
    }

    VariableBackup<int (*)(int)> dupBackup{&SysCalls::sysCallsDup, dupCallerFd};
    VariableBackup<int> nextDuplicateFdBackup{&nextDuplicateFd, int{firstDuplicateFd}};
    VariableBackup<int> dupCalledBackup{&SysCalls::dupCalled, 0};
    VariableBackup<uint32_t> closeCalledBackup{&SysCalls::closeFuncCalled, 0u};
    VariableBackup<int> closeArgBackup{&SysCalls::closeFuncArgPassed, 0};
};

TEST_F(DrmMemoryManagerFdImportTest, givenSharedHandleWhenCreatingMultiGraphicsAllocationThenAllocationOwnsDuplicateAndCallerFdIsNotClosed) {
    const std::array<AllocationProperties, 1> properties{MockAllocationProperties{0u, false, MemoryConstants::pageSize, AllocationType::sharedBuffer}};

    auto multiGraphicsAllocation = memoryManager->createMultiGraphicsAllocationFromSharedHandle(MemoryManager::OsHandleData{static_cast<osHandle>(callerFd)}, properties, true);
    ASSERT_NE(nullptr, multiGraphicsAllocation);

    EXPECT_EQ(1, SysCalls::dupCalled);
    EXPECT_EQ(firstDuplicateFd, static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[0u]->osInterface->getDriverModel()->as<Drm>())->inputFd);
    auto graphicsAllocation = multiGraphicsAllocation->getGraphicsAllocation(0u);
    ASSERT_NE(nullptr, graphicsAllocation);
    EXPECT_EQ(static_cast<osHandle>(firstDuplicateFd), graphicsAllocation->peekSharedHandle());
    EXPECT_EQ(0u, SysCalls::closeFuncCalled);

    memoryManager->freeGraphicsMemory(graphicsAllocation);
    EXPECT_EQ(1, closeCalledCount);
    EXPECT_EQ(firstDuplicateFd, closeInputFd);
}

TEST_F(DrmMemoryManagerFdImportTest, givenPropertiesForEachRootDeviceWhenCreatingMultiGraphicsAllocationThenEachRootDeviceImportsItsOwnDuplicate) {
    const std::array<AllocationProperties, 2> properties{MockAllocationProperties{0u, false, MemoryConstants::pageSize, AllocationType::sharedBuffer},
                                                         MockAllocationProperties{1u, false, MemoryConstants::pageSize, AllocationType::sharedBuffer}};

    auto multiGraphicsAllocation = memoryManager->createMultiGraphicsAllocationFromSharedHandle(MemoryManager::OsHandleData{static_cast<osHandle>(callerFd)}, properties, true);
    ASSERT_NE(nullptr, multiGraphicsAllocation);

    EXPECT_EQ(2, SysCalls::dupCalled);
    for (uint32_t rootDeviceIndex : {0u, 1u}) {
        const int expectedDuplicateFd = firstDuplicateFd + static_cast<int>(rootDeviceIndex);
        EXPECT_EQ(expectedDuplicateFd, static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel()->as<Drm>())->inputFd);
        auto graphicsAllocation = multiGraphicsAllocation->getGraphicsAllocation(rootDeviceIndex);
        ASSERT_NE(nullptr, graphicsAllocation);
        EXPECT_EQ(rootDeviceIndex, graphicsAllocation->getRootDeviceIndex());
        EXPECT_EQ(static_cast<osHandle>(expectedDuplicateFd), graphicsAllocation->peekSharedHandle());
        memoryManager->freeGraphicsMemory(graphicsAllocation);
        EXPECT_EQ(expectedDuplicateFd, closeInputFd);
    }
    EXPECT_EQ(2, closeCalledCount);
    EXPECT_EQ(0u, SysCalls::closeFuncCalled);
}

TEST_F(DrmMemoryManagerFdImportTest, givenUnorderedPropertiesForSubsetOfRootDevicesWhenCreatingMultiGraphicsAllocationThenOnlyThoseRootDevicesImportAndItIsSizedToHighestRootDeviceIndex) {
    const std::array<AllocationProperties, 2> properties{MockAllocationProperties{2u, false, MemoryConstants::pageSize, AllocationType::sharedBuffer},
                                                         MockAllocationProperties{0u, false, MemoryConstants::pageSize, AllocationType::sharedBuffer}};

    auto multiGraphicsAllocation = memoryManager->createMultiGraphicsAllocationFromSharedHandle(MemoryManager::OsHandleData{static_cast<osHandle>(callerFd)}, properties, true);
    ASSERT_NE(nullptr, multiGraphicsAllocation);

    EXPECT_EQ(3u, multiGraphicsAllocation->getGraphicsAllocations().size());
    EXPECT_EQ(nullptr, multiGraphicsAllocation->getGraphicsAllocation(1u));
    EXPECT_EQ(0, static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[1u]->osInterface->getDriverModel()->as<Drm>())->ioctlCnt.primeFdToHandle.load());
    for (uint32_t rootDeviceIndex : {0u, 2u}) {
        auto graphicsAllocation = multiGraphicsAllocation->getGraphicsAllocation(rootDeviceIndex);
        ASSERT_NE(nullptr, graphicsAllocation);
        EXPECT_EQ(rootDeviceIndex, graphicsAllocation->getRootDeviceIndex());
        memoryManager->freeGraphicsMemory(graphicsAllocation);
    }
}

TEST_F(DrmMemoryManagerFdImportTest, givenNoPropertiesWhenCreatingMultiGraphicsAllocationThenNullptrIsReturnedAndCallerFdIsNotDuplicated) {
    EXPECT_EQ(nullptr, memoryManager->createMultiGraphicsAllocationFromSharedHandle(MemoryManager::OsHandleData{static_cast<osHandle>(callerFd)}, {}, true));
    EXPECT_EQ(0, SysCalls::dupCalled);
    EXPECT_EQ(0u, SysCalls::closeFuncCalled);
}

TEST_F(DrmMemoryManagerFdImportTest, givenCallerFdThatCannotBeDuplicatedWhenCreatingMultiGraphicsAllocationThenNothingIsImportedOrClosed) {
    VariableBackup<int (*)(int)> failingDup{&SysCalls::sysCallsDup, [](int) { return -1; }};
    const std::array<AllocationProperties, 1> properties{MockAllocationProperties{0u, false, MemoryConstants::pageSize, AllocationType::sharedBuffer}};

    EXPECT_EQ(nullptr, memoryManager->createMultiGraphicsAllocationFromSharedHandle(MemoryManager::OsHandleData{static_cast<osHandle>(callerFd)}, properties, true));
    EXPECT_EQ(0, static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[0u]->osInterface->getDriverModel()->as<Drm>())->ioctlCnt.primeFdToHandle.load());
    EXPECT_EQ(0u, SysCalls::closeFuncCalled);
}

TEST_F(DrmMemoryManagerFdImportTest, givenDuplicateThatCannotBeImportedWhenCreatingMultiGraphicsAllocationThenOnlyDuplicateIsClosed) {
    static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[0u]->osInterface->getDriverModel()->as<Drm>())->failOnPrimeFdToHandle = true;
    const std::array<AllocationProperties, 1> properties{MockAllocationProperties{0u, false, MemoryConstants::pageSize, AllocationType::sharedBuffer}};

    EXPECT_EQ(nullptr, memoryManager->createMultiGraphicsAllocationFromSharedHandle(MemoryManager::OsHandleData{static_cast<osHandle>(callerFd)}, properties, true));
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(firstDuplicateFd, SysCalls::closeFuncArgPassed);
}

TEST_F(DrmMemoryManagerFdImportTest, givenOneRootDeviceFailingToImportWhenCreatingMultiGraphicsAllocationThenCreatedAllocationsAreFreedAndFailedDuplicateIsClosed) {
    static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[1u]->osInterface->getDriverModel()->as<Drm>())->failOnPrimeFdToHandle = true;
    const std::array<AllocationProperties, 2> properties{MockAllocationProperties{0u, false, MemoryConstants::pageSize, AllocationType::sharedBuffer},
                                                         MockAllocationProperties{1u, false, MemoryConstants::pageSize, AllocationType::sharedBuffer}};

    EXPECT_EQ(nullptr, memoryManager->createMultiGraphicsAllocationFromSharedHandle(MemoryManager::OsHandleData{static_cast<osHandle>(callerFd)}, properties, true));
    EXPECT_EQ(1, closeCalledCount);
    EXPECT_EQ(firstDuplicateFd, closeInputFd);
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(firstDuplicateFd + 1, SysCalls::closeFuncArgPassed);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDisabledForcePinAndEnabledValidateHostMemoryWhenPinBBAllocationFailsThenUnrecoverableIsCalled) {
    this->mock = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());
    this->mock->reset();
    this->mock->ioctlRes = -1;
    this->mock->ioctlExpected.gemUserptr = 1;
    EXPECT_THROW(
        {
            std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(false,
                                                                                             false,
                                                                                             true,
                                                                                             *executionEnvironment));
            EXPECT_NE(nullptr, memoryManager.get());
        },
        std::exception);
    this->mock->ioctlRes = 0;
    this->mock->ioctlExpected.contextDestroy = 0;
    this->mock->testIoctls();
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDisabledForcePinAndEnabledValidateHostMemoryWhenPopulateOsHandlesIsCalledThenHostMemoryIsValidated) {

    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(false, false, true, *executionEnvironment));
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager.get());
    ASSERT_NE(nullptr, memoryManager->pinBBs[device->getRootDeviceIndex()]);

    mock->reset();
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.execbuffer2 = 1; // for pinning - host memory validation

    OsHandleStorage handleStorage;
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 4096;
    auto result = memoryManager->populateOsHandles(handleStorage, rootDeviceIndex);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, result);

    mock->testIoctls();

    EXPECT_NE(nullptr, handleStorage.fragmentStorageData[0].osHandleStorage);
    handleStorage.fragmentStorageData[0].freeTheFragment = true;

    memoryManager->cleanOsHandles(handleStorage, rootDeviceIndex);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDisabledForcePinAndEnabledValidateHostMemoryWhenPopulateOsHandlesIsCalledWithFirstFragmentAlreadyAllocatedThenNewBosAreValidated) {

    class PinBufferObject : public BufferObject {
      public:
        PinBufferObject(uint32_t rootDeviceIndex, Drm *drm) : BufferObject(rootDeviceIndex, drm, 3, 1, 0, 1) {
        }

        int validateHostPtr(BufferObject *const boToPin[], size_t numberOfBos, OsContext *osContext, uint32_t vmHandleId, uint32_t drmContextId) override {
            for (size_t i = 0; i < numberOfBos; i++) {
                pinnedBoArray[i] = boToPin[i];
            }
            numberOfBosPinned = numberOfBos;
            return 0;
        }
        BufferObject *pinnedBoArray[5];
        size_t numberOfBosPinned;
    };

    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(false, false, true, *executionEnvironment));
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager.get());
    ASSERT_NE(nullptr, memoryManager->pinBBs[device->getRootDeviceIndex()]);

    PinBufferObject *pinBB = new PinBufferObject(rootDeviceIndex, this->mock);
    memoryManager->injectPinBB(pinBB, rootDeviceIndex);

    mock->reset();
    mock->ioctlExpected.gemUserptr = 2;
    mock->ioctlExpected.execbuffer2 = 0; // pinning for host memory validation is mocked

    OsHandleStorage handleStorage;
    OsHandleLinux handle1;
    handleStorage.fragmentStorageData[0].osHandleStorage = &handle1;
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 4096;

    handleStorage.fragmentStorageData[1].osHandleStorage = nullptr;
    handleStorage.fragmentStorageData[1].cpuPtr = reinterpret_cast<void *>(0x2000);
    handleStorage.fragmentStorageData[1].fragmentSize = 8192;

    handleStorage.fragmentStorageData[2].osHandleStorage = nullptr;
    handleStorage.fragmentStorageData[2].cpuPtr = reinterpret_cast<void *>(0x4000);
    handleStorage.fragmentStorageData[2].fragmentSize = 4096;

    auto result = memoryManager->populateOsHandles(handleStorage, rootDeviceIndex);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, result);

    mock->testIoctls();

    EXPECT_NE(nullptr, handleStorage.fragmentStorageData[0].osHandleStorage);
    EXPECT_NE(nullptr, handleStorage.fragmentStorageData[1].osHandleStorage);
    EXPECT_NE(nullptr, handleStorage.fragmentStorageData[2].osHandleStorage);

    EXPECT_EQ(static_cast<OsHandleLinux *>(handleStorage.fragmentStorageData[1].osHandleStorage)->bo, pinBB->pinnedBoArray[0]);
    EXPECT_EQ(static_cast<OsHandleLinux *>(handleStorage.fragmentStorageData[2].osHandleStorage)->bo, pinBB->pinnedBoArray[1]);

    handleStorage.fragmentStorageData[0].freeTheFragment = false;
    handleStorage.fragmentStorageData[1].freeTheFragment = true;
    handleStorage.fragmentStorageData[2].freeTheFragment = true;

    memoryManager->cleanOsHandles(handleStorage, rootDeviceIndex);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenValidateHostPtrMemoryEnabledWhenHostPtrAllocationIsCreatedWithoutForcingPinThenBufferObjectIsPinned) {
    mock->ioctlExpected.gemUserptr = 2;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 2;

    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, true, true, *executionEnvironment));
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager->pinBBs[device->getRootDeviceIndex()]);

    size_t size = 10 * MemoryConstants::megaByte;
    void *ptr = ::alignedMalloc(size, 4096);
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), false, size}, ptr));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
    ::alignedFree(ptr);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledHostMemoryValidationWhenValidHostPointerIsPassedToPopulateThenSuccessIsReturned) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false,
                                                                                                    false,
                                                                                                    true,
                                                                                                    *executionEnvironment));

    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    OsHandleStorage storage;
    storage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    storage.fragmentStorageData[0].fragmentSize = 1;
    auto result = memoryManager->populateOsHandles(storage, rootDeviceIndex);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, result);

    EXPECT_NE(nullptr, storage.fragmentStorageData[0].osHandleStorage);
    storage.fragmentStorageData[0].freeTheFragment = true;
    memoryManager->cleanOsHandles(storage, rootDeviceIndex);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenForcePinAndHostMemoryValidationEnabledWhenSmallAllocationIsCreatedThenBufferObjectIsPinned) {
    mock->ioctlExpected.gemUserptr = 2;  // 1 pinBB, 1 small allocation
    mock->ioctlExpected.execbuffer2 = 1; // pinning
    mock->ioctlExpected.gemWait = 1;     // in freeGraphicsAllocation
    mock->ioctlExpected.gemClose = 2;    // 1 pinBB, 1 small allocation

    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, true, true, *executionEnvironment));
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager->pinBBs[rootDeviceIndex]);

    // one page is too small for early pinning but pinning is used for host memory validation
    allocationData.size = 4 * MemoryConstants::kiloByte;
    allocationData.hostPtr = ::alignedMalloc(allocationData.size, 4096);
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
    ::alignedFree(const_cast<void *>(allocationData.hostPtr));
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenForcePinAndHostMemoryValidationEnabledThenPinnedBufferObjectGpuAddressWithinDeviceGpuAddressSpace) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemClose = 1;

    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, true, true, *executionEnvironment));

    auto bo = memoryManager->pinBBs[rootDeviceIndex];

    ASSERT_NE(nullptr, bo);

    EXPECT_LT(bo->peekAddress(), defaultHwInfo->capabilityTable.gpuAddressSpace);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenForcePinAndHostMemoryValidationEnabledThenPinnedBufferObjectWrittenWithMIBBENDAndNOOP) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemClose = 1;

    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, true, true, *executionEnvironment));

    EXPECT_NE(0ul, memoryManager->memoryForPinBBs.size());
    ASSERT_NE(nullptr, memoryManager->memoryForPinBBs[rootDeviceIndex]);

    uint32_t *buffer = reinterpret_cast<uint32_t *>(memoryManager->memoryForPinBBs[rootDeviceIndex]);
    uint32_t bbEnd = 0x05000000;
    EXPECT_EQ(bbEnd, buffer[0]);
    EXPECT_EQ(0ul, buffer[1]);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenForcePinAllowedAndNoPinBBInMemoryManagerWhenAllocationWithForcePinFlagTrueIsCreatedThenAllocationIsNotPinned) {
    mock->ioctlExpected.gemUserptr = 2;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlRes = -1;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, true, false, *executionEnvironment));
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    EXPECT_EQ(nullptr, memoryManager->pinBBs[rootDeviceIndex]);
    mock->ioctlRes = 0;

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(createAllocationProperties(rootDeviceIndex, MemoryConstants::pageSize, true));
    EXPECT_NE(nullptr, allocation);
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenNullptrOrZeroSizeWhenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledThenAllocationIsNotCreated) {
    allocationData.size = 0;
    allocationData.hostPtr = nullptr;
    EXPECT_FALSE(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));

    allocationData.size = 100;
    allocationData.hostPtr = nullptr;
    EXPECT_FALSE(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));

    allocationData.size = 0;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x12345);
    EXPECT_FALSE(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));
}

TEST_F(DrmMemoryManagerBasic, givenDrmMemoryManagerWhenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledWithNotAlignedPtrIsPassedThenAllocationIsCreated) {
    AllocationData allocationData;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    allocationData.size = 13;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x5001);
    allocationData.rootDeviceIndex = rootDeviceIndex;
    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);

    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(0x5001u, reinterpret_cast<uint64_t>(allocation->getUnderlyingBuffer()));
    EXPECT_EQ(13u, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(1u, allocation->getAllocationOffset());

    memoryManager->freeGraphicsMemory(allocation);
}
TEST_F(DrmMemoryManagerBasic, givenDrmMemoryManagerWhenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledThenCreateBosCalled) {
    AllocationData allocationData;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    allocationData.size = 13;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x5001);
    allocationData.rootDeviceIndex = rootDeviceIndex;
    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);

    EXPECT_EQ(memoryManager->createBufferObjectsForNonSvmHostPtrCallCount, 1u);

    memoryManager->freeGraphicsMemory(allocation);
}

namespace {
void *madviseCollapseCapturedAddress = nullptr;
size_t madviseCollapseCapturedSize = 0u;

int captureMadviseCollapse(void *addr, size_t size) {
    madviseCollapseCapturedAddress = addr;
    madviseCollapseCapturedSize = size;
    return 0;
}

uint32_t allocateNonSvmHostPtrAndCountCollapses(TestedDrmMemoryManager &memoryManager, AllocationData &allocationData) {
    VariableBackup<decltype(SysCalls::sysCallsMadviseCollapse)> madviseCollapseBackup(&SysCalls::sysCallsMadviseCollapse, captureMadviseCollapse);
    VariableBackup<uint32_t> madviseCollapseCalledBackup(&SysCalls::madviseCollapseFuncCalled, 0u);
    auto allocation = memoryManager.allocateGraphicsMemoryForNonSvmHostPtr(allocationData);
    EXPECT_NE(nullptr, allocation);
    memoryManager.freeGraphicsMemory(allocation);
    return SysCalls::madviseCollapseFuncCalled;
}
} // namespace

TEST_F(DrmMemoryManagerBasic, givenImportedHostPtrWith2MBAlignedStartWhenAllocatingNonSvmHostPtrThenWhole2MBPagesAreCollapsed) {
    AllocationData allocationData;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));
    allocationData.size = 5 * MemoryConstants::megaByte + MemoryConstants::pageSize;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x40000000);
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.isImportedHostPtr = true;

    EXPECT_EQ(1u, allocateNonSvmHostPtrAndCountCollapses(*memoryManager, allocationData));
    EXPECT_EQ(allocationData.hostPtr, madviseCollapseCapturedAddress);
    EXPECT_EQ(4 * MemoryConstants::megaByte, madviseCollapseCapturedSize);
}

TEST_F(DrmMemoryManagerBasic, givenHugePageCollapseFailsWhenAllocatingImportedNonSvmHostPtrThenAllocationIsCreated) {
    VariableBackup<decltype(SysCalls::sysCallsMadviseCollapse)> madviseCollapseBackup(&SysCalls::sysCallsMadviseCollapse, [](void *, size_t) -> int { return -1; });
    AllocationData allocationData;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));
    allocationData.size = 8 * MemoryConstants::megaByte;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x40000000);
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.isImportedHostPtr = true;

    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);
    EXPECT_NE(nullptr, allocation);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerBasic, givenImportedHostPtrWithStartNot2MBAlignedWhenAllocatingNonSvmHostPtrThenNothingIsCollapsed) {
    AllocationData allocationData;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));
    allocationData.size = 8 * MemoryConstants::megaByte;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x40001000);
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.isImportedHostPtr = true;

    EXPECT_EQ(0u, allocateNonSvmHostPtrAndCountCollapses(*memoryManager, allocationData));
}

TEST_F(DrmMemoryManagerBasic, givenImportedHostPtrSmallerThan2MBWhenAllocatingNonSvmHostPtrThenNothingIsCollapsed) {
    AllocationData allocationData;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));
    allocationData.size = MemoryConstants::megaByte;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x40000000);
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.isImportedHostPtr = true;

    EXPECT_EQ(0u, allocateNonSvmHostPtrAndCountCollapses(*memoryManager, allocationData));
}

TEST_F(DrmMemoryManagerBasic, givenHostPtrNotImportedWhenAllocatingNonSvmHostPtrThenNothingIsCollapsed) {
    AllocationData allocationData;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));
    allocationData.size = 8 * MemoryConstants::megaByte;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x40000000);
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.isImportedHostPtr = false;

    EXPECT_EQ(0u, allocateNonSvmHostPtrAndCountCollapses(*memoryManager, allocationData));
}

TEST_F(DrmMemoryManagerBasic, givenHugePageCollapseDisabledWhenAllocatingImportedNonSvmHostPtrThenNothingIsCollapsed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableImportedHostPtrHugePageCollapse.set(0);
    AllocationData allocationData;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));
    allocationData.size = 8 * MemoryConstants::megaByte;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x40000000);
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.isImportedHostPtr = true;

    EXPECT_EQ(0u, allocateNonSvmHostPtrAndCountCollapses(*memoryManager, allocationData));
}

TEST_F(DrmMemoryManagerBasic, givenDrmMemoryManagerWhenAllocatingGreaterThan2GBThenCreateMoreThanOneBo) {
    AllocationData allocationData;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    allocationData.size = MemoryConstants::gigaByte * 3;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x5001);
    allocationData.rootDeviceIndex = rootDeviceIndex;
    size_t realAllocationSize = MemoryConstants::gigaByte * 3;
    const void *alignedPtr = alignDown(allocationData.hostPtr, MemoryConstants::pageSize);
    uint64_t gpuVirtualAddress = reinterpret_cast<uint64_t>(alignedPtr);
    uint32_t rootDeviceIndex = 0;
    uint8_t patIndex = 0;
    size_t alignedSize = MemoryConstants::gigaByte * 3;
    auto bos = memoryManager->createBufferObjectsForNonSvmHostPtr(realAllocationSize, alignedPtr, gpuVirtualAddress, allocationData, rootDeviceIndex, patIndex, alignedSize);
    EXPECT_EQ(bos.size(), 2u);
    for (auto &bo : bos) {
        delete bo;
    }
}

TEST_F(DrmMemoryManagerBasic, givenDrmMemoryManagerWhenAllocatingSmallerThan2GBThenCreateOneBo) {
    AllocationData allocationData;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    allocationData.size = MemoryConstants::gigaByte;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x5001);
    allocationData.rootDeviceIndex = rootDeviceIndex;
    size_t realAllocationSize = allocationData.size;
    const void *alignedPtr = alignDown(allocationData.hostPtr, MemoryConstants::pageSize);
    uint64_t gpuVirtualAddress = reinterpret_cast<uint64_t>(alignedPtr);
    uint32_t rootDeviceIndex = 0;
    uint8_t patIndex = 0;
    size_t alignedSize = allocationData.size;
    auto bos = memoryManager->createBufferObjectsForNonSvmHostPtr(realAllocationSize, alignedPtr, gpuVirtualAddress, allocationData, rootDeviceIndex, patIndex, alignedSize);
    EXPECT_EQ(bos.size(), 1u);
    for (auto &bo : bos) {
        delete bo;
    }
}
TEST_F(DrmMemoryManagerBasic, givenDrmMemoryManagerWhenAllocatingGreaterThan2GBAndDoNotUseChunkedBosForHugeHostPtrAllocsEnabledThenCreateOneBo) {
    DebugManagerStateRestore dbgState;
    debugManager.flags.DoNotUseChunkedBosForHugeHostPtrAllocs.set(1);
    AllocationData allocationData;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    allocationData.size = MemoryConstants::gigaByte * 3;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x5001);
    allocationData.rootDeviceIndex = rootDeviceIndex;
    size_t realAllocationSize = MemoryConstants::gigaByte * 3;
    const void *alignedPtr = alignDown(allocationData.hostPtr, MemoryConstants::pageSize);
    uint64_t gpuVirtualAddress = reinterpret_cast<uint64_t>(alignedPtr);
    uint32_t rootDeviceIndex = 0;
    uint8_t patIndex = 0;
    size_t alignedSize = MemoryConstants::gigaByte * 3;
    auto bos = memoryManager->createBufferObjectsForNonSvmHostPtr(realAllocationSize, alignedPtr, gpuVirtualAddress, allocationData, rootDeviceIndex, patIndex, alignedSize);
    EXPECT_EQ(bos.size(), 1u);
    for (auto &bo : bos) {
        delete bo;
    }
}
TEST_F(DrmMemoryManagerBasic, givenDrmMemoryManagerWhenAllocateGraphicsMemoryForNonSvmHostPtrThenObjectAlignedSizeIsUsedByAllocUserPtrWhenBiggerSizeAllocatedInHeap) {
    AllocationData allocationData;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    allocationData.size = 4 * MemoryConstants::megaByte + 16 * MemoryConstants::kiloByte;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x10000000);
    auto allocation0 = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));

    allocationData.hostPtr = reinterpret_cast<const void *>(0x20000000);
    auto allocation1 = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);

    memoryManager->freeGraphicsMemory(allocation0);

    allocationData.size = 4 * MemoryConstants::megaByte + 12 * MemoryConstants::kiloByte;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x30000000);
    allocation0 = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));

    EXPECT_EQ(static_cast<uint64_t>(allocation0->getBO()->peekSize()), 4 * MemoryConstants::megaByte + 12 * MemoryConstants::kiloByte);

    memoryManager->freeGraphicsMemory(allocation0);
    memoryManager->freeGraphicsMemory(allocation1);
}

TEST_F(DrmMemoryManagerBasic, givenDrmMemoryManagerWhenAllocateGraphicsMemoryForNonSvmHostPtrThenGpuRangeWith2MBAlignmentIsAcquired) {
    AllocationData allocationData;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    allocationData.size = 2 * MemoryConstants::kiloByte;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x1234);
    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));

    EXPECT_EQ(memoryManager->acquireGpuRangeWithCustomAlignmenCalledTimes, 1u);
    EXPECT_EQ(memoryManager->acquireGpuRangeWithCustomAlignmenPassedAlignment, MemoryConstants::pageSize2M);
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerBasic, givenDrmMemoryManagerWhenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledButAllocationFailedThenNullPtrReturned) {
    AllocationData allocationData;
    allocationData.rootDeviceIndex = rootDeviceIndex;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));
    memoryManager->getGfxPartition(rootDeviceIndex)->heapInit(HeapIndex::heapStandard, 0x0, 0x10000);

    allocationData.size = 64 * MemoryConstants::gigaByte;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x100000000000);
    EXPECT_FALSE(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));
}

TEST_F(DrmMemoryManagerBasic, givenDrmMemoryManagerWhenAllocateGraphicsMemoryForNonSvmHostPtrFailsThenNullPtrReturnedAndAllocationIsNotRegistered) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));
    memoryManager->getGfxPartition(rootDeviceIndex)->heapInit(HeapIndex::heapStandard, 0x0, 0x10000);
    MockAllocationProperties properties(0u, 64 * MemoryConstants::gigaByte);

    auto ptr = reinterpret_cast<const void *>(0x100000000000);
    EXPECT_FALSE(memoryManager->allocateGraphicsMemoryInPreferredPool(properties, ptr));
    EXPECT_EQ(memoryManager->getSysMemAllocs().size(), 0u);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenAllocateGraphicsMemoryForNonSvmHostPtrIsCalledWithHostPtrIsPassedAndWhenAllocUserptrFailsThenFails) {
    mock->ioctlExpected.gemUserptr = 1;
    this->ioctlResExt = {mock->ioctlCnt.total, -1};
    mock->ioctlResExt = &ioctlResExt;

    allocationData.size = 10;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x1000);
    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);

    EXPECT_EQ(nullptr, allocation);
    mock->ioctlResExt = &mock->none;
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenForcePinNotAllowedAndHostMemoryValidationEnabledWhenAllocationIsCreatedThenBufferObjectIsPinnedOnlyOnce) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(false, false, true, *executionEnvironment));
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    mock->reset();
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.execbuffer2 = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.gemWait = 1;

    AllocationData allocationData;
    allocationData.size = 4 * MemoryConstants::kiloByte;
    allocationData.hostPtr = ::alignedMalloc(allocationData.size, 4096);
    allocationData.flags.forcePin = true;
    allocationData.rootDeviceIndex = device->getRootDeviceIndex();
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
    mock->testIoctls();

    ::alignedFree(const_cast<void *>(allocationData.hostPtr));
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenForcePinNotAllowedAndHostMemoryValidationDisabledWhenAllocationIsCreatedThenBufferObjectIsNotPinned) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(false, false, false, *executionEnvironment));
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    mock->reset();
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->ioctlExpected.gemWait = 1;

    AllocationData allocationData;
    allocationData.size = 10 * MemoryConstants::megaByte; // bigger than threshold
    allocationData.hostPtr = ::alignedMalloc(allocationData.size, 4096);
    allocationData.flags.forcePin = true;
    allocationData.rootDeviceIndex = device->getRootDeviceIndex();
    auto alloc = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData));
    ASSERT_NE(nullptr, alloc);
    EXPECT_NE(nullptr, alloc->getBO());

    memoryManager->freeGraphicsMemory(alloc);
    mock->testIoctls();

    ::alignedFree(const_cast<void *>(allocationData.hostPtr));
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledValidateHostMemoryWhenReadOnlyPointerCausesPinningFailWithEfaultThenPopulateOsHandlesMarksFragmentsToFree) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(false, false, true, *executionEnvironment));
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager.get());
    ASSERT_NE(nullptr, memoryManager->pinBBs[device->getRootDeviceIndex()]);

    mock->reset();

    DrmMockCustom::IoctlResExt ioctlResExt = {2, -1};
    ioctlResExt.no.push_back(3);
    ioctlResExt.no.push_back(4);
    mock->ioctlResExt = &ioctlResExt;
    mock->errnoValue = EFAULT;
    mock->ioctlExpected.gemUserptr = 2;
    mock->ioctlExpected.execbuffer2 = 3;

    OsHandleStorage handleStorage;
    OsHandleLinux handle1;
    handleStorage.fragmentStorageData[0].osHandleStorage = &handle1;
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 4096;

    handleStorage.fragmentStorageData[1].osHandleStorage = nullptr;
    handleStorage.fragmentStorageData[1].cpuPtr = reinterpret_cast<void *>(0x2000);
    handleStorage.fragmentStorageData[1].fragmentSize = 8192;

    handleStorage.fragmentStorageData[2].osHandleStorage = nullptr;
    handleStorage.fragmentStorageData[2].cpuPtr = reinterpret_cast<void *>(0x4000);
    handleStorage.fragmentStorageData[2].fragmentSize = 4096;

    auto result = memoryManager->populateOsHandles(handleStorage, rootDeviceIndex);
    EXPECT_EQ(MemoryManager::AllocationStatus::InvalidHostPointer, result);

    mock->testIoctls();

    EXPECT_NE(nullptr, handleStorage.fragmentStorageData[0].osHandleStorage);
    EXPECT_NE(nullptr, handleStorage.fragmentStorageData[1].osHandleStorage);
    EXPECT_NE(nullptr, handleStorage.fragmentStorageData[2].osHandleStorage);

    EXPECT_TRUE(handleStorage.fragmentStorageData[1].freeTheFragment);
    EXPECT_TRUE(handleStorage.fragmentStorageData[2].freeTheFragment);

    handleStorage.fragmentStorageData[0].freeTheFragment = false;
    handleStorage.fragmentStorageData[1].freeTheFragment = true;
    handleStorage.fragmentStorageData[2].freeTheFragment = true;

    memoryManager->cleanOsHandles(handleStorage, rootDeviceIndex);
    mock->ioctlResExt = &mock->none;
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledValidateHostMemoryWhenReadOnlyPointerCausesPinningFailWithEfaultThenPopulateOsHandlesDoesNotStoreTheFragments) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(false, false, true, *executionEnvironment));
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager->pinBBs[device->getRootDeviceIndex()]);

    mock->reset();

    DrmMockCustom::IoctlResExt ioctlResExt = {2, -1};
    ioctlResExt.no.push_back(3);
    ioctlResExt.no.push_back(4);
    mock->ioctlResExt = &ioctlResExt;
    mock->errnoValue = EFAULT;
    mock->ioctlExpected.gemUserptr = 2;
    mock->ioctlExpected.execbuffer2 = 3;

    OsHandleStorage handleStorage;
    OsHandleLinux handle1;
    handleStorage.fragmentStorageData[0].osHandleStorage = &handle1;
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 4096;

    handleStorage.fragmentStorageData[1].osHandleStorage = nullptr;
    handleStorage.fragmentStorageData[1].cpuPtr = reinterpret_cast<void *>(0x2000);
    handleStorage.fragmentStorageData[1].fragmentSize = 8192;

    handleStorage.fragmentStorageData[2].osHandleStorage = nullptr;
    handleStorage.fragmentStorageData[2].cpuPtr = reinterpret_cast<void *>(0x4000);
    handleStorage.fragmentStorageData[2].fragmentSize = 4096;

    auto result = memoryManager->populateOsHandles(handleStorage, rootDeviceIndex);
    EXPECT_EQ(MemoryManager::AllocationStatus::InvalidHostPointer, result);

    mock->testIoctls();

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());

    EXPECT_EQ(0u, hostPtrManager->getFragmentCount());
    EXPECT_EQ(nullptr, hostPtrManager->getFragment({handleStorage.fragmentStorageData[1].cpuPtr, rootDeviceIndex}));
    EXPECT_EQ(nullptr, hostPtrManager->getFragment({handleStorage.fragmentStorageData[2].cpuPtr, rootDeviceIndex}));

    handleStorage.fragmentStorageData[0].freeTheFragment = false;
    handleStorage.fragmentStorageData[1].freeTheFragment = true;
    handleStorage.fragmentStorageData[2].freeTheFragment = true;

    memoryManager->cleanOsHandles(handleStorage, rootDeviceIndex);
    mock->ioctlResExt = &mock->none;
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenEnabledValidateHostMemoryWhenPopulateOsHandlesSucceedsThenFragmentIsStoredInHostPtrManager) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(false, false, true, *executionEnvironment));
    memoryManager->allRegisteredEngines[this->device->getRootDeviceIndex()] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : memoryManager->getRegisteredEngines(this->device->getRootDeviceIndex())) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, memoryManager->pinBBs[device->getRootDeviceIndex()]);

    mock->reset();
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.execbuffer2 = 1;

    OsHandleStorage handleStorage;
    handleStorage.fragmentStorageData[0].osHandleStorage = nullptr;
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 4096;

    auto result = memoryManager->populateOsHandles(handleStorage, rootDeviceIndex);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, result);

    mock->testIoctls();

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());
    EXPECT_EQ(1u, hostPtrManager->getFragmentCount());
    EXPECT_NE(nullptr, hostPtrManager->getFragment({handleStorage.fragmentStorageData[0].cpuPtr, device->getRootDeviceIndex()}));

    handleStorage.fragmentStorageData[0].freeTheFragment = true;
    memoryManager->cleanOsHandles(handleStorage, rootDeviceIndex);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenDrmMemoryManagerWhenCleanOsHandlesDeletesHandleDataThenOsHandleStorageIsSetToNullptr) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new TestedDrmMemoryManager(false, false, true, *executionEnvironment));
    ASSERT_NE(nullptr, memoryManager->pinBBs[device->getRootDeviceIndex()]);
    OsHandleStorage handleStorage;
    handleStorage.fragmentStorageData[0].osHandleStorage = new OsHandleLinux();
    handleStorage.fragmentStorageData[0].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[0].fragmentSize = 4096;

    handleStorage.fragmentStorageData[1].osHandleStorage = new OsHandleLinux();
    handleStorage.fragmentStorageData[1].cpuPtr = reinterpret_cast<void *>(0x1000);
    handleStorage.fragmentStorageData[1].fragmentSize = 4096;

    handleStorage.fragmentStorageData[0].freeTheFragment = true;
    handleStorage.fragmentStorageData[1].freeTheFragment = true;

    memoryManager->cleanOsHandles(handleStorage, rootDeviceIndex);

    for (uint32_t i = 0; i < 2; i++) {
        EXPECT_EQ(nullptr, handleStorage.fragmentStorageData[i].osHandleStorage);
    }
}

TEST_F(DrmMemoryManagerBasic, givenSpecificAddressSpaceWhenInitializingMemoryManagerThenSetCorrectHeaps) {
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getMutableHardwareInfo()->capabilityTable.gpuAddressSpace = maxNBitValue(48);
    TestedDrmMemoryManager memoryManager(false, false, false, executionEnvironment);

    auto gfxPartition = memoryManager.getGfxPartition(rootDeviceIndex);
    auto limit = gfxPartition->getHeapLimit(HeapIndex::heapSvm);

    EXPECT_EQ(maxNBitValue(48 - 1), limit);
}

TEST_F(DrmMemoryManagerBasic, givenUnalignedHostPtrWithFlushL3RequiredWhenAllocateGraphicsMemoryThenSetCorrectPatIndex) {
    AllocationData allocationData;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    memoryManager->reinitGfxPartition(MemoryConstants::max48BitAddress);

    allocationData.size = 13;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x5001);
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.flushL3 = true;
    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));

    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(0x5001u, reinterpret_cast<uint64_t>(allocation->getUnderlyingBuffer()));
    EXPECT_EQ(13u, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(1u, allocation->getAllocationOffset());
    const auto &hwInfo = *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
    auto &releaseHelper = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getReleaseHelper();

    auto expectedUsage = GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER;
    if (releaseHelper.isAppTransientCoherentPatRequired()) {
        expectedUsage = GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED;
    } else if (hwInfo.caps.misalignedUserPtr2WayCoherent) {
        expectedUsage = GMM_RESOURCE_USAGE_HW_CONTEXT;
    }
    auto mockClientContext = static_cast<MockGmmClientContextBase *>(executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getGmmClientContext());
    const auto passedUsage = mockClientContext->passedUsageTypeForGetPatIndexQuery;
    ASSERT_TRUE(passedUsage.has_value());
    EXPECT_EQ(expectedUsage, passedUsage.value());
    const auto gmmPatIndex = mockClientContext->cachePolicyGetPATIndex(nullptr, expectedUsage, false, true);
    EXPECT_EQ(releaseHelper.overrideSystemMemoryPatIndex(gmmPatIndex),
              allocation->getBO()->peekPatIndex());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerBasic, givenUnalignedHostPtrWithFlushL3RequiredWhenAllocatingNonSvmHostPtrThenPatIndexIsQueriedWithResourceInfoOfCacheableBufferGmm) {
    AllocationData allocationData;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    memoryManager->reinitGfxPartition(MemoryConstants::max48BitAddress);
    memoryManager->createBufferObjectsForNonSvmHostPtrCallBase = false;
    static_cast<DrmMock &>(memoryManager->getDrm(rootDeviceIndex)).vmBindPatIndexProgrammingSupported = true;

    auto mockClientContext = static_cast<MockGmmClientContextBase *>(executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getGmmClientContext());
    mockClientContext->passedResourceInfoForGetPatIndexQuery = nullptr;

    allocationData.size = 13;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x5001);
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.flushL3 = true;
    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);
    ASSERT_NE(nullptr, allocation);

    EXPECT_NE(nullptr, mockClientContext->passedResourceInfoForGetPatIndexQuery);
    EXPECT_FALSE(mockClientContext->passedCompressedSettingForGetPatIndexQuery);
    EXPECT_TRUE(mockClientContext->passedCpuCacheableSettingForGetPatIndexQuery);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerBasic, givenPatIndexProgrammingNotSupportedWhenAllocatingNonSvmHostPtrThenPatIndexIsQueriedWithResourceInfoOfAllocationGmm) {
    AllocationData allocationData;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    memoryManager->reinitGfxPartition(MemoryConstants::max48BitAddress);
    memoryManager->createBufferObjectsForNonSvmHostPtrCallBase = false;
    static_cast<DrmMock &>(memoryManager->getDrm(rootDeviceIndex)).vmBindPatIndexProgrammingSupported = false;

    auto mockClientContext = static_cast<MockGmmClientContextBase *>(executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getGmmClientContext());
    mockClientContext->passedResourceInfoForGetPatIndexQuery = reinterpret_cast<GMM_RESOURCE_INFO *>(0x1234);
    const auto queriesBefore = mockClientContext->getPatIndexQueryCalled;

    allocationData.size = 13;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x5001);
    allocationData.rootDeviceIndex = rootDeviceIndex;
    auto allocation = memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData);
    ASSERT_NE(nullptr, allocation);

    EXPECT_EQ(queriesBefore + 1u, mockClientContext->getPatIndexQueryCalled);
    ASSERT_NE(nullptr, allocation->getDefaultGmm());
    EXPECT_EQ(allocation->getDefaultGmm()->gmmResourceInfo->peekGmmResourceInfo(), mockClientContext->passedResourceInfoForGetPatIndexQuery);

    memoryManager->freeGraphicsMemory(allocation);
}

class DrmMemoryManagerUserptrGmmTests : public DrmMemoryManagerBasic {
  public:
    void SetUp() override {
        DrmMemoryManagerBasic::SetUp();
        memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, executionEnvironment);
        memoryManager->reinitGfxPartition(MemoryConstants::max48BitAddress);
        static_cast<DrmMock &>(memoryManager->getDrm(rootDeviceIndex)).vmBindPatIndexProgrammingSupported = true;
        mockClientContext = static_cast<MockGmmClientContextBase *>(executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getGmmClientContext());
        mockClientContext->passedResourceInfoForGetPatIndexQuery = nullptr;
        allocationData.rootDeviceIndex = rootDeviceIndex;
        allocationData.size = MemoryConstants::pageSize;
    }

    void TearDown() override {
        memoryManager.reset();
        DrmMemoryManagerBasic::TearDown();
    }

    void expectBufferGmm(GraphicsAllocation &allocation) const {
        auto gmm = allocation.getDefaultGmm();
        ASSERT_NE(nullptr, gmm);
        const auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex];
        EXPECT_EQ(CacheSettingsHelper::getGmmUsageType(allocation.getAllocationType(), false, rootDeviceEnvironment.getProductHelper(), *rootDeviceEnvironment.getHardwareInfo()), gmm->getResourceUsageType());
    }

    void expectBufferGmmUsedForPatIndex(GraphicsAllocation &allocation) const {
        expectBufferGmm(allocation);
        ASSERT_NE(nullptr, allocation.getDefaultGmm());
        EXPECT_EQ(allocation.getDefaultGmm()->gmmResourceInfo->peekGmmResourceInfo(), mockClientContext->passedResourceInfoForGetPatIndexQuery);
    }

    std::unique_ptr<TestedDrmMemoryManager> memoryManager;
    MockGmmClientContextBase *mockClientContext = nullptr;
    AllocationData allocationData;
};

TEST_F(DrmMemoryManagerUserptrGmmTests, givenBoMmapCreateDisabledWhenAllocatingWithAlignmentThenAllocationKeepsBufferGmmUsedForPatIndex) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableBOMmapCreate.set(0);
    allocationData.type = AllocationType::buffer;

    auto allocation = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);
    ASSERT_NE(nullptr, allocation);
    expectBufferGmmUsedForPatIndex(*allocation);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerUserptrGmmTests, givenHostPtrWhenAllocatingUsmHostMemoryThenAllocationKeepsBufferGmmUsedForPatIndex) {
    auto hostPtr = alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize);
    allocationData.type = AllocationType::bufferHostMemory;
    allocationData.hostPtr = hostPtr;
    allocationData.flags.isUSMHostAllocation = true;

    auto allocation = memoryManager->allocateGraphicsMemory(allocationData);
    ASSERT_NE(nullptr, allocation);
    expectBufferGmmUsedForPatIndex(*allocation);

    memoryManager->freeGraphicsMemory(allocation);
    alignedFree(hostPtr);
}

TEST_F(DrmMemoryManagerUserptrGmmTests, givenGpuVaWhenAllocatingThenAllocationKeepsBufferGmmUsedForPatIndex) {
    allocationData.type = AllocationType::buffer;
    allocationData.gpuAddress = 0x2000;

    auto allocation = memoryManager->allocateGraphicsMemory(allocationData);
    ASSERT_NE(nullptr, allocation);
    expectBufferGmmUsedForPatIndex(*allocation);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerUserptrGmmTests, givenNoHostPtrWhenAllocating32BitMemoryThenAllocationKeepsBufferGmmUsedForPatIndex) {
    allocationData.type = AllocationType::internalHeap;

    auto allocation = memoryManager->allocateGraphicsMemory(allocationData);
    ASSERT_NE(nullptr, allocation);
    expectBufferGmmUsedForPatIndex(*allocation);

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerUserptrGmmTests, givenHostPtrWhenAllocating32BitMemoryThenAllocationKeepsBufferGmmUsedForPatIndex) {
    auto hostPtr = alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize);
    allocationData.type = AllocationType::internalHeap;
    allocationData.hostPtr = hostPtr;

    auto allocation = memoryManager->allocateGraphicsMemory(allocationData);
    ASSERT_NE(nullptr, allocation);
    expectBufferGmmUsedForPatIndex(*allocation);

    memoryManager->freeGraphicsMemory(allocation);
    alignedFree(hostPtr);
}

TEST_F(DrmMemoryManagerUserptrGmmTests, givenOsHandleStorageWhenCreatingAllocationThenAllocationKeepsBufferGmm) {
    OsHandleStorage handleStorage;
    allocationData.type = AllocationType::externalHostPtr;
    allocationData.hostPtr = reinterpret_cast<void *>(0x1000);

    std::unique_ptr<GraphicsAllocation> allocation(memoryManager->createGraphicsAllocation(handleStorage, allocationData));
    ASSERT_NE(nullptr, allocation);
    expectBufferGmm(*allocation);
}

TEST_F(DrmMemoryManagerBasic, givenUnalignedHostPtrWithFlushL3RequiredAndDebugFlagWhenAllocateGraphicsMemoryThenSetCorrectPatIndex) {
    DebugManagerStateRestore restorer;
    debugManager.flags.Disable2WayCoherencyOverride.set(true);
    AllocationData allocationData;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    memoryManager->reinitGfxPartition(MemoryConstants::max48BitAddress);

    allocationData.size = 13;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x5001);
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.flushL3 = true;
    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));

    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(0x5001u, reinterpret_cast<uint64_t>(allocation->getUnderlyingBuffer()));
    EXPECT_EQ(13u, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(1u, allocation->getAllocationOffset());
    auto &releaseHelper = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getReleaseHelper();
    EXPECT_EQ(releaseHelper.overrideSystemMemoryPatIndex(MockGmmClientContextBase::MockPatIndex::cached), allocation->getBO()->peekPatIndex());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerBasic, givenUnalignedHostPtrWithFlushL3NotRequiredWhenAllocateGraphicsMemoryThenSetCorrectPatIndex) {
    AllocationData allocationData;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    memoryManager->reinitGfxPartition(MemoryConstants::max48BitAddress);

    allocationData.size = 13;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x5001);
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.flushL3 = false;
    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));
    EXPECT_NE(nullptr, allocation);

    auto &releaseHelper = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getReleaseHelper();
    EXPECT_EQ(releaseHelper.overrideSystemMemoryPatIndex(MockGmmClientContextBase::MockPatIndex::cached), allocation->getBO()->peekPatIndex());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerBasic, givenAlignedHostPtrWhenAllocateGraphicsMemoryThenSetCorrectPatIndex) {
    AllocationData allocationData;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    memoryManager->reinitGfxPartition(MemoryConstants::max48BitAddress);

    allocationData.size = MemoryConstants::cacheLineSize;
    allocationData.hostPtr = reinterpret_cast<const void *>(MemoryConstants::pageSize);
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.flushL3 = true;
    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));

    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryConstants::pageSize, reinterpret_cast<uint64_t>(allocation->getUnderlyingBuffer()));
    EXPECT_EQ(MemoryConstants::cacheLineSize, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(0u, allocation->getAllocationOffset());

    auto &releaseHelper = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getReleaseHelper();
    EXPECT_EQ(releaseHelper.overrideSystemMemoryPatIndex(MockGmmClientContextBase::MockPatIndex::cached), allocation->getBO()->peekPatIndex());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerBasic, givenNonSvmHostPtrWhenAllocateGraphicsMemoryThenPatIndexIsObtainedFromGmm) {
    AllocationData allocationData;
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    memoryManager->reinitGfxPartition(MemoryConstants::max48BitAddress);

    allocationData.size = 13;
    allocationData.hostPtr = reinterpret_cast<const void *>(0x5001);
    allocationData.rootDeviceIndex = rootDeviceIndex;
    allocationData.flags.flushL3 = false;
    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryForNonSvmHostPtr(allocationData));
    ASSERT_NE(nullptr, allocation);

    auto &releaseHelper = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getReleaseHelper();
    EXPECT_EQ(releaseHelper.overrideSystemMemoryPatIndex(MockGmmClientContextBase::MockPatIndex::cached), allocation->getBO()->peekPatIndex());

    memoryManager->freeGraphicsMemory(allocation);
}

TEST_F(DrmMemoryManagerBasic, givenImageOrSharedResourceCopyWhenGraphicsAllocationInDevicePoolIsAllocatedThenNullptrIsReturned) {
    std::unique_ptr<TestedDrmMemoryManager> memoryManager(new (std::nothrow) TestedDrmMemoryManager(false, false, false, executionEnvironment));

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;
    allocData.rootDeviceIndex = rootDeviceIndex;

    AllocationType types[] = {AllocationType::image,
                              AllocationType::sharedResourceCopy};

    for (auto type : types) {
        allocData.type = type;
        auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
        EXPECT_EQ(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
    }
}

TEST_F(DrmMemoryManagerBasic, givenLocalMemoryDisabledWhenAllocateInDevicePoolIsCalledThenNullptrAndStatusRetryIsReturned) {
    const bool localMemoryEnabled = false;
    TestedDrmMemoryManager memoryManager(localMemoryEnabled, false, false, executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.useSystemMemory = false;
    allocData.flags.allocateMemory = true;
    allocData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDebugModuleAreaTypeWhenCreatingAllocationThen32BitDrmAllocationWithFrontWindowGpuVaIsReturned) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    const auto size = MemoryConstants::pageSize64k;

    NEO::AllocationProperties properties{device->getRootDeviceIndex(), true, size,
                                         NEO::AllocationType::debugModuleArea,
                                         false,
                                         device->getDeviceBitfield()};

    auto moduleDebugArea = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    EXPECT_NE(nullptr, moduleDebugArea);
    EXPECT_NE(nullptr, moduleDebugArea->getUnderlyingBuffer());
    EXPECT_GE(moduleDebugArea->getUnderlyingBufferSize(), size);

    auto address64bit = moduleDebugArea->getGpuAddressToPatch();
    EXPECT_LT(address64bit, MemoryConstants::max32BitAddress);
    EXPECT_TRUE(moduleDebugArea->is32BitAllocation());

    HeapIndex heap = HeapAssigner::mapInternalWindowIndex(memoryManager->selectInternalHeap(moduleDebugArea->isAllocatedInLocalMemoryPool()));
    EXPECT_TRUE(heap == HeapIndex::heapInternalDeviceFrontWindow || heap == HeapIndex::heapInternalFrontWindow);

    auto gmmHelper = device->getGmmHelper();
    auto frontWindowBase = gmmHelper->canonize(memoryManager->getGfxPartition(moduleDebugArea->getRootDeviceIndex())->getHeapBase(heap));
    EXPECT_EQ(frontWindowBase, moduleDebugArea->getGpuBaseAddress());
    EXPECT_EQ(frontWindowBase, moduleDebugArea->getGpuAddress());

    auto internalHeapBase = gmmHelper->canonize(memoryManager->getGfxPartition(moduleDebugArea->getRootDeviceIndex())->getHeapBase(memoryManager->selectInternalHeap(moduleDebugArea->isAllocatedInLocalMemoryPool())));
    EXPECT_EQ(internalHeapBase, moduleDebugArea->getGpuBaseAddress());

    memoryManager->freeGraphicsMemory(moduleDebugArea);
}

struct DrmAllocationTests : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    }

    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
};

TEST_F(DrmAllocationTests, givenAllocationTypeWhenPassedToDrmAllocationConstructorThenAllocationTypeIsStored) {
    DrmAllocation allocation{0, 1u /*num gmms*/, AllocationType::commandBuffer, nullptr, nullptr, static_cast<size_t>(0), 0u, MemoryPool::memoryNull};
    EXPECT_EQ(AllocationType::commandBuffer, allocation.getAllocationType());

    DrmAllocation allocation2{0, 1u /*num gmms*/, AllocationType::unknown, nullptr, nullptr, 0ULL, static_cast<size_t>(0), MemoryPool::memoryNull};
    EXPECT_EQ(AllocationType::unknown, allocation2.getAllocationType());
}

TEST_F(DrmAllocationTests, givenAllocationWhenGettingBufferObjectForVmHandleThenBankBufferObjectIsReturnedOnlyForNotChunkedMultiBankAllocation) {
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);
    MockBufferObject firstBankBo(0, &drm, 3, 0, 0, 1);
    MockBufferObject secondBankBo(0, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(0u, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &firstBankBo;
    allocation.bufferObjects[1] = &secondBankBo;

    allocation.storageInfo.memoryBanks = 0b1;
    EXPECT_EQ(&firstBankBo, allocation.getBOForVmHandle(1u));

    allocation.storageInfo.memoryBanks = 0b11;
    EXPECT_EQ(&firstBankBo, allocation.getBOForVmHandle(0u));
    EXPECT_EQ(&secondBankBo, allocation.getBOForVmHandle(1u));

    allocation.storageInfo.isChunked = true;
    EXPECT_EQ(&firstBankBo, allocation.getBOForVmHandle(1u));
}

TEST_F(DrmAllocationTests, givenMemoryPoolWhenPassedToDrmAllocationConstructorThenMemoryPoolIsStored) {
    DrmAllocation allocation{0, 1u /*num gmms*/, AllocationType::commandBuffer, nullptr, nullptr, static_cast<size_t>(0), 0u, MemoryPool::system64KBPages};
    EXPECT_EQ(MemoryPool::system64KBPages, allocation.getMemoryPool());

    DrmAllocation allocation2{0, 1u /*num gmms*/, AllocationType::unknown, nullptr, nullptr, 0ULL, static_cast<size_t>(0), MemoryPool::systemCpuInaccessible};
    EXPECT_EQ(MemoryPool::systemCpuInaccessible, allocation2.getMemoryPool());
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, whenReservingAddressRangeThenExpectProperAddressAndReleaseWhenFreeing) {
    constexpr size_t size = 0x1000;
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), size});
    ASSERT_NE(nullptr, allocation);
    void *reserve = memoryManager->reserveCpuAddressRange(size, 0u);
    EXPECT_EQ(nullptr, reserve);
    allocation->setReservedAddressRange(reserve, size);
    EXPECT_EQ(reserve, allocation->getReservedAddressPtr());
    EXPECT_EQ(size, allocation->getReservedAddressSize());
    memoryManager->freeGraphicsMemory(allocation);
}

TEST(DrmMemoryManagerWithExplicitExpectationsTest2, whenObtainFdFromHandleIsCalledThenProperFdHandleIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(4u);
    for (auto i = 0u; i < 4u; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(DrmMockCustom::create(*executionEnvironment->rootDeviceEnvironments[0]));
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);
    for (auto i = 0u; i < 4u; i++) {
        auto mock = executionEnvironment->rootDeviceEnvironments[i]->osInterface->getDriverModel()->as<DrmMockCustom>();

        int boHandle = 3;
        mock->outputFd = 1337;
        mock->ioctlExpected.handleToPrimeFd = 1;
        auto fdHandle = memoryManager->obtainFdFromHandle(boHandle, i);
        EXPECT_EQ(mock->inputHandle, static_cast<uint32_t>(boHandle));
        EXPECT_EQ(mock->inputFlags, DRM_CLOEXEC | DRM_RDWR);
        EXPECT_EQ(1337, fdHandle);
    }
}

TEST(DrmMemoryManagerWithExplicitExpectationsTest2, whenFailingToObtainFdFromHandleThenErrorIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(4u);
    for (auto i = 0u; i < 4u; i++) {
        executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[i]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(DrmMockCustom::create(*executionEnvironment->rootDeviceEnvironments[0]));
        executionEnvironment->rootDeviceEnvironments[i]->initGmm();
    }
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);
    for (auto i = 0u; i < 4u; i++) {
        auto mock = executionEnvironment->rootDeviceEnvironments[i]->osInterface->getDriverModel()->as<DrmMockCustom>();

        int boHandle = 3;
        mock->outputFd = -1;
        mock->ioctlRes = -1;
        mock->ioctlExpected.handleToPrimeFd = 1;
        auto fdHandle = memoryManager->obtainFdFromHandle(boHandle, i);
        EXPECT_EQ(-1, fdHandle);
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenSvmCpuAllocationWhenSizeAndAlignmentProvidedThenAllocateMemoryAndReserveGpuVa) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    AllocationData allocationData;
    allocationData.size = 2 * MemoryConstants::megaByte;
    allocationData.alignment = 2 * MemoryConstants::megaByte;
    allocationData.type = AllocationType::svmCpu;
    allocationData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithAlignment(allocationData));
    ASSERT_NE(nullptr, allocation);

    EXPECT_EQ(AllocationType::svmCpu, allocation->getAllocationType());

    EXPECT_EQ(allocationData.size, allocation->getUnderlyingBufferSize());
    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(allocation->getUnderlyingBuffer(), allocation->getDriverAllocatedCpuPtr());

    EXPECT_NE(0llu, allocation->getGpuAddress());
    EXPECT_NE(reinterpret_cast<uint64_t>(allocation->getUnderlyingBuffer()), allocation->getGpuAddress());

    auto bo = allocation->getBO();
    ASSERT_NE(nullptr, bo);

    EXPECT_NE(0llu, bo->peekAddress());

    auto gmmHelper = device->getGmmHelper();
    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapStandard)), bo->peekAddress());
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapStandard)), bo->peekAddress());

    EXPECT_EQ(reinterpret_cast<void *>(allocation->getGpuAddress()), alignUp(allocation->getReservedAddressPtr(), allocationData.alignment));
    EXPECT_EQ(alignUp(allocationData.size, allocationData.alignment) + allocationData.alignment, allocation->getReservedAddressSize());

    EXPECT_GT(allocation->getReservedAddressSize(), bo->peekSize());

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenSvmCpuAllocationWhenSizeAndAlignmentProvidedButFailsToReserveGpuVaThenNullAllocationIsReturned) {
    mock->ioctlExpected.gemUserptr = 0;
    mock->ioctlExpected.gemWait = 0;
    mock->ioctlExpected.gemClose = 0;

    memoryManager->getGfxPartition(rootDeviceIndex)->heapInit(HeapIndex::heapStandard, 0, 0);

    AllocationData allocationData;
    allocationData.size = 2 * MemoryConstants::megaByte;
    allocationData.alignment = 2 * MemoryConstants::megaByte;
    allocationData.type = AllocationType::svmCpu;
    allocationData.rootDeviceIndex = rootDeviceIndex;

    auto allocation = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);
    EXPECT_EQ(nullptr, allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerAndReleaseGpuRangeIsCalledThenGpuAddressIsDecanonized) {
    constexpr size_t reservedCpuAddressRangeSize = is64bit ? (6 * 4 * MemoryConstants::gigaByte) : 0;
    auto hwInfo = defaultHwInfo.get();
    auto mockGfxPartition = std::make_unique<MockGfxPartition>();
    mockGfxPartition->init(hwInfo->capabilityTable.gpuAddressSpace, reservedCpuAddressRangeSize, 0, 1, false, 0u, hwInfo->capabilityTable.gpuAddressSpace + 1, nullptr);
    auto size = 2 * MemoryConstants::megaByte;
    auto gpuAddress = mockGfxPartition->heapAllocate(HeapIndex::heapStandard, size);
    auto gmmHelper = device->getGmmHelper();
    auto gpuAddressCanonized = gmmHelper->canonize(gpuAddress);
    EXPECT_LE(gpuAddress, gpuAddressCanonized);

    memoryManager->overrideGfxPartition(mockGfxPartition.release());
    memoryManager->releaseGpuRange(reinterpret_cast<void *>(gpuAddressCanonized), size, 0);

    auto mockGfxPartitionBasic = std::make_unique<MockGfxPartitionBasic>();
    memoryManager->overrideGfxPartition(mockGfxPartitionBasic.release());
}

TEST(DrmMemoryManagerFreeGraphicsMemoryCallSequenceTest, givenDrmMemoryManagerAndFreeGraphicsMemoryIsCalledThenUnreferenceBufferObjectIsCalledFirstWithSynchronousDestroySetToTrue) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    executionEnvironment.rootDeviceEnvironments[0]->initGmm();
    TestedDrmMemoryManager memoryManager(executionEnvironment);

    AllocationProperties properties{mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::buffer, mockDeviceBitfield};
    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties(properties);
    ASSERT_NE(allocation, nullptr);

    memoryManager.freeGraphicsMemory(allocation);

    EXPECT_EQ(EngineLimits::maxHandleCount, memoryManager.unreferenceCalled);
    for (size_t i = 0; i < EngineLimits::maxHandleCount; ++i) {
        EXPECT_TRUE(memoryManager.unreferenceParamsPassed[i].synchronousDestroy);
    }
    EXPECT_EQ(1u, memoryManager.releaseGpuRangeCalled);
    EXPECT_EQ(1u, memoryManager.alignedFreeWrapperCalled);
}

TEST(DrmMemoryManagerFreeGraphicsMemoryUnreferenceTest,
     givenCallToCreateSharedAllocationWithNoReuseSharedAllocationThenAllocationsSuccedAndAddressesAreDifferent) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    const uint32_t rootDeviceIndex = 0u;
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]);
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    executionEnvironment.rootDeviceEnvironments[0]->initGmm();
    TestedDrmMemoryManager memoryManager(executionEnvironment);

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, {});
    auto allocation = memoryManager.createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, false, nullptr);
    ASSERT_NE(nullptr, allocation);

    auto allocation2 = memoryManager.createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, false, nullptr);
    ASSERT_NE(nullptr, allocation2);

    EXPECT_NE(allocation->getGpuAddress(), allocation2->getGpuAddress());

    memoryManager.freeGraphicsMemory(allocation2);
    memoryManager.freeGraphicsMemory(allocation);
}

TEST(DrmMemoryManagerFreeGraphicsMemoryUnreferenceTest,
     givenPhysicalOffsetWhenCreateGraphicsAllocationFromSharedHandleThenBufferObjectMappingOffsetAndSizeAreSet) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    const uint32_t rootDeviceIndex = 0u;
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]);
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    executionEnvironment.rootDeviceEnvironments[0]->initGmm();
    TestedDrmMemoryManager memoryManager(executionEnvironment);

    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(2 * MemoryConstants::pageSize));
    const uint64_t physicalOffset = MemoryConstants::pageSize;
    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    osHandleData.physicalOffset = physicalOffset;
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, {});
    auto allocation = memoryManager.createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, false, nullptr);
    ASSERT_NE(nullptr, allocation);

    auto bo = static_cast<DrmAllocation *>(allocation)->getBO();
    ASSERT_NE(nullptr, bo);
    EXPECT_EQ(physicalOffset, bo->getPhysicalMemoryOffset());
    EXPECT_EQ(static_cast<size_t>(2 * MemoryConstants::pageSize) - physicalOffset, bo->getVirtualMappingSize());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(DrmMemoryManagerFreeGraphicsMemoryUnreferenceTest,
     givenReuseSharedAllocationAndPhysicalOffsetWhenCreateGraphicsAllocationFromSharedHandleThenNullptrIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    const uint32_t rootDeviceIndex = 0u;
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]);
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    executionEnvironment.rootDeviceEnvironments[0]->initGmm();
    TestedDrmMemoryManager memoryManager(executionEnvironment);

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    osHandleData.physicalOffset = MemoryConstants::pageSize;
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, {});
    auto allocation = memoryManager.createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    EXPECT_EQ(nullptr, allocation);
}

TEST(DrmMemoryManagerFreeGraphicsMemoryUnreferenceTest,
     whenPrintBOCreateDestroyResultFlagIsSetAndCallToCreateSharedAllocationThenExpectedMessageIsPrinted) {
    DebugManagerStateRestore stateRestore;

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    const uint32_t rootDeviceIndex = 0u;
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]);
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    executionEnvironment.rootDeviceEnvironments[0]->initGmm();
    TestedDrmMemoryManager memoryManager(executionEnvironment);

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, {});

    debugManager.flags.PrintBOCreateDestroyResult.set(true);
    StreamCapture capture;
    capture.captureStdout();
    auto allocation = memoryManager.createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, false, nullptr);
    ASSERT_NE(nullptr, allocation);

    std::stringstream expectedOutput;
    expectedOutput << "Created BO-0 range: ";
    expectedOutput << std::hex << allocation->getGpuAddress();
    expectedOutput << " - ";
    expectedOutput << std::hex << ptrOffset(allocation->getGpuAddress(), MemoryConstants::pageSize);
    expectedOutput << ", size: 4096 from PRIME_FD_TO_HANDLE\nCalling gem close on handle: BO-0\n";

    memoryManager.freeGraphicsMemory(allocation);

    std::string output = capture.getCapturedStdout();

    EXPECT_EQ(expectedOutput.str(), output);
}

struct DrmMemoryManagerWithHostIpcAllocationParamTest : public DrmMemoryManagerFixture, ::testing::TestWithParam<bool> {
    template <typename GfxFamily>
    void setUpT() {
        DrmMemoryManagerFixture::setUpT<GfxFamily>();
    }

    template <typename GfxFamily>
    void tearDownT() {
        DrmMemoryManagerFixture::tearDownT<GfxFamily>();
    }
};

HWTEST_TEMPLATED_P(DrmMemoryManagerWithHostIpcAllocationParamTest,
                   givenIPCBoHandleAndSharedAllocationReuseEnabledWhenAllocationCreatedThenBoHandleSharingIsNotUsed) {
    const bool reuseSharedAllocation = true;

    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->outputHandle = 88u;
    bool isHostIpcAllocation = GetParam();
    AllocationProperties properties(rootDeviceIndex, false, 4096u, AllocationType::sharedBuffer, false, {});

    TestedDrmMemoryManager::OsHandleData osHandleData{11u};
    auto gfxAllocation1 = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, isHostIpcAllocation, reuseSharedAllocation, nullptr);
    DrmAllocation *drmAllocation1 = static_cast<DrmAllocation *>(gfxAllocation1);
    ASSERT_NE(nullptr, drmAllocation1);

    // BoHandle not registered as shared at all
    auto bo1 = drmAllocation1->getBO();
    EXPECT_NE(bo1, nullptr);
    EXPECT_EQ(static_cast<uint32_t>(bo1->getHandle()), mock->outputHandle);
    EXPECT_FALSE(bo1->isBoHandleShared());
    auto boHandleWrapperIt1 = memoryManager->sharedBoHandles.find(std::make_pair(mock->outputHandle, rootDeviceIndex));
    EXPECT_EQ(boHandleWrapperIt1, std::end(memoryManager->sharedBoHandles));

    memoryManager->freeGraphicsMemory(gfxAllocation1);
}

HWTEST_TEMPLATED_P(DrmMemoryManagerWithHostIpcAllocationParamTest,
                   givenIPCBoHandleAndSharedAllocationReuseEnabledWhenAllocationCreatedFromMultipleHandlesThenBoHandleSharingIsNotUsed) {
    const bool reuseSharedAllocation = true;

    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->outputHandle = 88u;
    bool isHostIpcAllocation = GetParam();
    AllocationProperties properties(rootDeviceIndex, true, 4096u, AllocationType::sharedBuffer, false, {0b0010});

    std::vector<osHandle> handles = {11u};
    auto gfxAllocation1 = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, isHostIpcAllocation, reuseSharedAllocation, nullptr);
    DrmAllocation *drmAllocation1 = static_cast<DrmAllocation *>(gfxAllocation1);
    ASSERT_NE(nullptr, drmAllocation1);

    // BoHandle not registered as shared at all
    auto bo1 = drmAllocation1->getBO();
    EXPECT_NE(bo1, nullptr);
    EXPECT_EQ(static_cast<uint32_t>(bo1->getHandle()), mock->outputHandle);
    EXPECT_FALSE(bo1->isBoHandleShared());
    auto boHandleWrapperIt1 = memoryManager->sharedBoHandles.find(std::make_pair(mock->outputHandle, rootDeviceIndex));
    EXPECT_EQ(boHandleWrapperIt1, std::end(memoryManager->sharedBoHandles));

    memoryManager->freeGraphicsMemory(gfxAllocation1);
}

HWTEST_TEMPLATED_P(DrmMemoryManagerWithHostIpcAllocationParamTest,
                   givenIPCBoHandleAndSharedAllocationReuseDisabledWhenMultipleAllocationsCreatedFromSingleProcessThenBoHandleIsClosedOnlyOnce) {
    const bool reuseSharedAllocation = false;

    mock->ioctlExpected.primeFdToHandle = 2;
    mock->ioctlExpected.gemWait = 2;
    mock->ioctlExpected.gemClose = 1;
    mock->outputHandle = 88u;
    bool isHostIpcAllocation = GetParam();
    AllocationProperties properties(rootDeviceIndex, false, 4096u, AllocationType::sharedBuffer, false, {});

    TestedDrmMemoryManager::OsHandleData osHandleData1{11u};
    auto gfxAllocation1 = memoryManager->createGraphicsAllocationFromSharedHandle(&osHandleData1, properties, false, isHostIpcAllocation, reuseSharedAllocation, nullptr);
    DrmAllocation *drmAllocation1 = static_cast<DrmAllocation *>(gfxAllocation1);
    ASSERT_NE(nullptr, drmAllocation1);

    // BoHandle registered as shared but with WEAK ownership - GEM_CLOSE can be called on it
    auto bo1 = drmAllocation1->getBO();
    EXPECT_NE(bo1, nullptr);
    EXPECT_EQ(static_cast<uint32_t>(bo1->getHandle()), mock->outputHandle);
    EXPECT_TRUE(bo1->isBoHandleShared());
    auto boHandleWrapperIt1 = memoryManager->sharedBoHandles.find(std::make_pair(mock->outputHandle, rootDeviceIndex));
    EXPECT_NE(boHandleWrapperIt1, std::end(memoryManager->sharedBoHandles));
    EXPECT_TRUE(boHandleWrapperIt1->second.canCloseBoHandle());

    TestedDrmMemoryManager::OsHandleData osHandleData2{12u};
    auto gfxAllocation2 = memoryManager->createGraphicsAllocationFromSharedHandle(&osHandleData2, properties, false, isHostIpcAllocation, false, nullptr);
    DrmAllocation *drmAllocation2 = static_cast<DrmAllocation *>(gfxAllocation2);
    ASSERT_NE(nullptr, drmAllocation2);
    // BoHandle registered as shared with SHARED ownership - GEM_CLOSE cannot be called on it
    auto bo2 = drmAllocation2->getBO();
    EXPECT_NE(bo2, nullptr);
    EXPECT_EQ(static_cast<uint32_t>(bo2->getHandle()), mock->outputHandle);
    EXPECT_TRUE(bo2->isBoHandleShared());
    auto boHandleWrapperIt2 = memoryManager->sharedBoHandles.find(std::make_pair(mock->outputHandle, rootDeviceIndex));
    EXPECT_EQ(boHandleWrapperIt2, boHandleWrapperIt1);
    EXPECT_FALSE(boHandleWrapperIt2->second.canCloseBoHandle());

    memoryManager->freeGraphicsMemory(gfxAllocation2);
    // GEM_CLOSE can be called on BoHandle again
    EXPECT_TRUE(boHandleWrapperIt2->second.canCloseBoHandle());
    memoryManager->freeGraphicsMemory(gfxAllocation1);
}

HWTEST_TEMPLATED_P(DrmMemoryManagerWithHostIpcAllocationParamTest,
                   givenIPCBoHandleAndSharedAllocationReuseDisabledWhenMultipleAllocationsCreatedFromDifferentDevicesThenBoHandlesAreClosedSeparately) {
    const bool reuseSharedAllocation = false;

    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;
    mock->outputHandle = 88u;
    bool isHostIpcAllocation = GetParam();
    AllocationProperties properties(rootDeviceIndex, false, 4096u, AllocationType::sharedBuffer, false, {});

    TestedDrmMemoryManager::OsHandleData osHandleData1{11u};
    auto gfxAllocation1 = memoryManager->createGraphicsAllocationFromSharedHandle(&osHandleData1, properties, false, isHostIpcAllocation, reuseSharedAllocation, nullptr);
    DrmAllocation *drmAllocation1 = static_cast<DrmAllocation *>(gfxAllocation1);
    ASSERT_NE(nullptr, drmAllocation1);

    // BoHandle registered as shared but with WEAK ownership - GEM_CLOSE can be called on it
    auto bo1 = drmAllocation1->getBO();
    EXPECT_NE(bo1, nullptr);
    EXPECT_EQ(static_cast<uint32_t>(bo1->getHandle()), mock->outputHandle);
    EXPECT_TRUE(bo1->isBoHandleShared());
    auto boHandleWrapperIt1 = memoryManager->sharedBoHandles.find(std::make_pair(mock->outputHandle, rootDeviceIndex));
    EXPECT_NE(boHandleWrapperIt1, std::end(memoryManager->sharedBoHandles));
    EXPECT_TRUE(boHandleWrapperIt1->second.canCloseBoHandle());

    TestedDrmMemoryManager::OsHandleData osHandleData2{12u};
    // two devices in the system, rootDeviceIndex is set to 1, so set rootDeviceInext to 0
    auto rootDeviceIndex2 = 0;
    auto rootDeviceEnvironment2 = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex2].get();
    auto mock2 = static_cast<DrmMockCustom *>(rootDeviceEnvironment2->osInterface->getDriverModel()->as<Drm>());
    mock2->ioctlExpected.primeFdToHandle = 1;
    mock2->ioctlExpected.gemWait = 1;
    mock2->ioctlExpected.gemClose = 1;
    mock2->outputHandle = 88u;
    AllocationProperties properties2(rootDeviceIndex2, false, 4096u, AllocationType::sharedBuffer, false, {});
    auto gfxAllocation2 = memoryManager->createGraphicsAllocationFromSharedHandle(&osHandleData2, properties2, false, isHostIpcAllocation, false, nullptr);
    DrmAllocation *drmAllocation2 = static_cast<DrmAllocation *>(gfxAllocation2);
    ASSERT_NE(nullptr, drmAllocation2);
    // BoHandle registered as shared with WEAK ownership because it's from different root devices - GEM_CLOSE can be called on it
    auto bo2 = drmAllocation2->getBO();
    EXPECT_NE(bo2, nullptr);
    EXPECT_EQ(static_cast<uint32_t>(bo2->getHandle()), mock->outputHandle);
    EXPECT_TRUE(bo2->isBoHandleShared());
    auto boHandleWrapperIt2 = memoryManager->sharedBoHandles.find(std::make_pair(mock->outputHandle, rootDeviceIndex2));
    EXPECT_NE(boHandleWrapperIt2, boHandleWrapperIt1);
    EXPECT_TRUE(boHandleWrapperIt2->second.canCloseBoHandle());

    memoryManager->freeGraphicsMemory(gfxAllocation2);
    memoryManager->freeGraphicsMemory(gfxAllocation1);
    mock2->testIoctls();
    mock2->reset();
}

HWTEST_TEMPLATED_P(DrmMemoryManagerWithHostIpcAllocationParamTest,
                   givenIPCBoHandleAndSharedAllocationReuseDisabledWhenMultipleAllocationsCreatedFromMultipleSharedHandlesFromSingleProcessThenBoHandleIsClosedOnlyOnce) {
    const bool reuseSharedAllocation = false;

    mock->ioctlExpected.primeFdToHandle = 2;
    mock->ioctlExpected.gemWait = 2;
    mock->ioctlExpected.gemClose = 1;
    mock->outputHandle = 88u;
    bool isHostIpcAllocation = GetParam();
    AllocationProperties properties(rootDeviceIndex, true, 4096u, AllocationType::sharedBuffer, false, {});

    std::vector<osHandle> handles1 = {11u};
    auto gfxAllocation1 = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles1, properties, false, isHostIpcAllocation, reuseSharedAllocation, nullptr);
    DrmAllocation *drmAllocation1 = static_cast<DrmAllocation *>(gfxAllocation1);
    ASSERT_NE(nullptr, drmAllocation1);

    // BoHandle registered as shared but with WEAK ownership - GEM_CLOSE can be called on it
    auto bo1 = drmAllocation1->getBO();
    EXPECT_NE(bo1, nullptr);
    EXPECT_EQ(static_cast<uint32_t>(bo1->getHandle()), mock->outputHandle);
    EXPECT_TRUE(bo1->isBoHandleShared());
    auto boHandleWrapperIt1 = memoryManager->sharedBoHandles.find(std::make_pair(mock->outputHandle, rootDeviceIndex));
    EXPECT_NE(boHandleWrapperIt1, std::end(memoryManager->sharedBoHandles));
    EXPECT_TRUE(boHandleWrapperIt1->second.canCloseBoHandle());

    std::vector<osHandle> handles2 = {12u};
    auto gfxAllocation2 = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles2, properties, false, isHostIpcAllocation, false, nullptr);
    DrmAllocation *drmAllocation2 = static_cast<DrmAllocation *>(gfxAllocation2);
    ASSERT_NE(nullptr, drmAllocation2);
    // BoHandle registered as shared with SHARED ownership - GEM_CLOSE cannot be called on it
    auto bo2 = drmAllocation2->getBO();
    EXPECT_NE(bo2, nullptr);
    EXPECT_EQ(static_cast<uint32_t>(bo2->getHandle()), mock->outputHandle);
    EXPECT_TRUE(bo2->isBoHandleShared());
    auto boHandleWrapperIt2 = memoryManager->sharedBoHandles.find(std::make_pair(mock->outputHandle, rootDeviceIndex));
    EXPECT_EQ(boHandleWrapperIt2, boHandleWrapperIt1);
    EXPECT_FALSE(boHandleWrapperIt2->second.canCloseBoHandle());

    memoryManager->freeGraphicsMemory(gfxAllocation2);
    // GEM_CLOSE can be called on BoHandle again
    EXPECT_TRUE(boHandleWrapperIt2->second.canCloseBoHandle());
    memoryManager->freeGraphicsMemory(gfxAllocation1);
}

INSTANTIATE_TEST_SUITE_P(HostIpcAllocationFlag,
                         DrmMemoryManagerWithHostIpcAllocationParamTest,
                         ::testing::Values(false, true));

struct DrmMemoryManagerMultipleSharedHandlesTest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = std::make_unique<MockExecutionEnvironment>(defaultHwInfo.get());
        auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
        rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
        drm = DrmMockCustom::create(rootDeviceEnvironment).release();
        rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
        rootDeviceEnvironment.memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
        rootDeviceEnvironment.initGmm();
        memoryManager = new TestedDrmMemoryManager(*executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);
        drm->outputHandle = 2u;
    }

    void installSystemMemoryInfo() {
        std::vector<MemoryRegion> regionInfo(1);
        regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
        drm->memoryInfo.reset(new MemoryInfo(regionInfo, *drm));
        drm->memoryInfoQueried = true;
    }

    const uint32_t rootDeviceIndex = 0u;
    DrmMockCustom *drm = nullptr;
    TestedDrmMemoryManager *memoryManager = nullptr;
    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;
};

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenPhysicalOffsetsAndVmBindWhenCreateGraphicsAllocationFromMultipleSharedHandlesThenEachBufferObjectHonorsItsOffset) {
    drm->bindAvailable = true;
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(2 * MemoryConstants::pageSize));

    const uint64_t physicalOffset = MemoryConstants::pageSize;
    std::vector<osHandle> handles = {11u, 12u};
    std::vector<uint64_t> physicalOffsets = {physicalOffset, physicalOffset};
    AllocationProperties properties(rootDeviceIndex, true, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, systemMemoryBitfield);
    properties.physicalOffsets = physicalOffsets;

    auto gfxAllocation = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, false, nullptr);
    ASSERT_NE(nullptr, gfxAllocation);
    auto drmAllocation = static_cast<DrmAllocation *>(gfxAllocation);

    for (uint32_t i = 0; i < handles.size(); i++) {
        auto bo = drmAllocation->getBufferObjectToModify(i);
        ASSERT_NE(nullptr, bo);
        EXPECT_EQ(physicalOffset, bo->getPhysicalMemoryOffset());
        EXPECT_EQ(static_cast<size_t>(2 * MemoryConstants::pageSize) - physicalOffset, bo->getVirtualMappingSize());
    }

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenNullPhysicalOffsetsWhenCreateGraphicsAllocationFromMultipleSharedHandlesThenAllocationIsCreatedWithoutOffsets) {
    drm->bindAvailable = true;
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(2 * MemoryConstants::pageSize));

    std::vector<osHandle> handles = {11u, 12u};
    AllocationProperties properties(rootDeviceIndex, true, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, systemMemoryBitfield);
    EXPECT_TRUE(properties.physicalOffsets.empty());

    auto gfxAllocation = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, false, nullptr);
    ASSERT_NE(nullptr, gfxAllocation);
    EXPECT_EQ(0u, static_cast<DrmAllocation *>(gfxAllocation)->getBufferObjectToModify(0)->getPhysicalMemoryOffset());

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenEmptyPhysicalOffsetsVectorWhenCreateGraphicsAllocationFromMultipleSharedHandlesThenAllocationIsCreatedWithoutOffsets) {
    drm->bindAvailable = true;
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(2 * MemoryConstants::pageSize));

    std::vector<osHandle> handles = {11u, 12u};
    std::vector<uint64_t> physicalOffsets = {};
    AllocationProperties properties(rootDeviceIndex, true, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, systemMemoryBitfield);
    properties.physicalOffsets = physicalOffsets;

    auto gfxAllocation = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, false, nullptr);
    ASSERT_NE(nullptr, gfxAllocation);
    EXPECT_EQ(0u, static_cast<DrmAllocation *>(gfxAllocation)->getBufferObjectToModify(0)->getPhysicalMemoryOffset());

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenLeadingZeroPhysicalOffsetAndVmBindWhenCreateGraphicsAllocationFromMultipleSharedHandlesThenAnyOffsetAccumulatesAcrossChunks) {
    drm->bindAvailable = true;
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(2 * MemoryConstants::pageSize));

    std::vector<osHandle> handles = {11u, 12u};
    std::vector<uint64_t> physicalOffsets = {0u, MemoryConstants::pageSize};
    AllocationProperties properties(rootDeviceIndex, true, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, systemMemoryBitfield);
    properties.physicalOffsets = physicalOffsets;

    auto gfxAllocation = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, false, nullptr);
    ASSERT_NE(nullptr, gfxAllocation);
    auto drmAllocation = static_cast<DrmAllocation *>(gfxAllocation);

    EXPECT_EQ(0u, drmAllocation->getBufferObjectToModify(0)->getPhysicalMemoryOffset());
    EXPECT_EQ(MemoryConstants::pageSize, drmAllocation->getBufferObjectToModify(1)->getPhysicalMemoryOffset());

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenPhysicalOffsetsAndNoVmBindAndMultipleHandlesWhenCreateGraphicsAllocationFromMultipleSharedHandlesThenNullptrIsReturned) {
    drm->bindAvailable = false;
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(2 * MemoryConstants::pageSize));

    std::vector<osHandle> handles = {11u, 12u};
    std::vector<uint64_t> physicalOffsets = {MemoryConstants::pageSize, MemoryConstants::pageSize};
    AllocationProperties properties(rootDeviceIndex, true, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, systemMemoryBitfield);
    properties.physicalOffsets = physicalOffsets;

    auto gfxAllocation = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, false, nullptr);
    EXPECT_EQ(nullptr, gfxAllocation);
    EXPECT_EQ(0, drm->ioctlCnt.primeFdToHandle);
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenPhysicalOffsetNotSmallerThanObjectSizeWhenCreateGraphicsAllocationFromMultipleSharedHandlesThenNullptrIsReturned) {
    drm->bindAvailable = true;
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(MemoryConstants::pageSize));

    std::vector<osHandle> handles = {11u};
    std::vector<uint64_t> physicalOffsets = {MemoryConstants::pageSize};
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, systemMemoryBitfield);
    properties.physicalOffsets = physicalOffsets;

    auto gfxAllocation = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, false, nullptr);
    EXPECT_EQ(nullptr, gfxAllocation);
    EXPECT_EQ(1, drm->ioctlCnt.primeFdToHandle);
    EXPECT_EQ(drm->ioctlCnt.primeFdToHandle.load(), drm->ioctlCnt.gemClose.load());
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenSecondChunkOffsetNotSmallerThanObjectSizeWhenCreateGraphicsAllocationFromMultipleSharedHandlesThenEarlierChunkIsReleased) {
    drm->bindAvailable = true;
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(2 * MemoryConstants::pageSize));

    std::vector<osHandle> handles = {11u, 12u};
    std::vector<uint64_t> physicalOffsets = {0u, 4 * MemoryConstants::pageSize};
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, systemMemoryBitfield);
    properties.physicalOffsets = physicalOffsets;

    auto gfxAllocation = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, false, nullptr);
    EXPECT_EQ(nullptr, gfxAllocation);
    EXPECT_EQ(2, drm->ioctlCnt.primeFdToHandle);
    EXPECT_EQ(drm->ioctlCnt.primeFdToHandle.load(), drm->ioctlCnt.gemClose.load());
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenReusedBufferObjectAndSecondHandleFailingWhenCreateGraphicsAllocationFromMultipleSharedHandlesThenReusedObjectIsReleasedWithoutSynchronousDestroy) {
    drm->bindAvailable = true;
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(2 * MemoryConstants::pageSize));

    std::vector<osHandle> firstHandles = {11u};
    AllocationProperties firstProperties(rootDeviceIndex, true, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, systemMemoryBitfield);

    auto firstAllocation = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(firstHandles, firstProperties, false, false, true, nullptr);
    ASSERT_NE(nullptr, firstAllocation);
    ASSERT_EQ(1u, memoryManager->peekSharedBosSize());

    memoryManager->unreferenceCalled = 0u;
    memoryManager->unreferenceParamsPassed.clear();
    drm->failOnSecondPrimeFdToHandle = true;

    std::vector<osHandle> handles = {11u, 12u};
    AllocationProperties properties(rootDeviceIndex, true, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, systemMemoryBitfield);

    auto gfxAllocation = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, true, nullptr);
    EXPECT_EQ(nullptr, gfxAllocation);
    ASSERT_EQ(1u, memoryManager->unreferenceCalled);
    EXPECT_EQ(static_cast<DrmAllocation *>(firstAllocation)->getBO(), memoryManager->unreferenceParamsPassed[0].bo);
    EXPECT_FALSE(memoryManager->unreferenceParamsPassed[0].synchronousDestroy);
    EXPECT_EQ(1u, memoryManager->peekSharedBosSize());

    memoryManager->freeGraphicsMemory(firstAllocation);
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenImportedAllocationWhenFreeingGraphicsMemoryThenAllocationIsNotUnregistered) {
    drm->bindAvailable = true;
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(2 * MemoryConstants::pageSize));

    std::vector<osHandle> handles = {11u};
    AllocationProperties properties(rootDeviceIndex, true, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, systemMemoryBitfield);

    auto gfxAllocation = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, false, nullptr);
    ASSERT_NE(nullptr, gfxAllocation);
    gfxAllocation->setIsImported();

    memoryManager->freeGraphicsMemory(gfxAllocation);

    EXPECT_EQ(0u, memoryManager->unregisterAllocationCalled);
    EXPECT_EQ(0u, memoryManager->getUsedLocalMemorySize(rootDeviceIndex));
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenWindowRelocationAndChunkWithoutOffsetWhenCreateHostAllocationFromMultipleSharedHandlesThenChunkIsMappedInPlace) {
    static uint8_t hostRangeBuffer[4 * MemoryConstants::pageSize] = {};
    drm->bindAvailable = true;
    installSystemMemoryInfo();
    enableMmapWindowRelocation(drm);
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(2 * MemoryConstants::pageSize));
    resetCapturedMremap();
    memoryManager->mremapFixedFunction = capturingMremapFixed;

    memoryManager->mmapFunction = [](void *addr, size_t, int, int, int, off_t) noexcept -> void * {
        return addr ? addr : hostRangeBuffer;
    };
    memoryManager->munmapFunction = [](void *, size_t) noexcept -> int { return 0; };

    std::vector<osHandle> handles = {11u, 12u};
    std::vector<uint64_t> physicalOffsets = {0u, MemoryConstants::pageSize};
    AllocationProperties properties(rootDeviceIndex, MemoryConstants::pageSize, AllocationType::bufferHostMemory, systemMemoryBitfield);

    auto gfxAllocation = memoryManager->createHostAllocationFromMultipleSharedHandles(handles, properties, physicalOffsets, false);
    ASSERT_NE(nullptr, gfxAllocation);

    EXPECT_EQ(1u, capturedMremapCalls);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenPatIndexProgrammingSupportedWhenCreateHostAllocationFromMultipleSharedHandlesThenPatIndexIsQueriedWithGmmOfEachBufferObject) {
    drm->vmBindPatIndexProgrammingSupported = true;
    installSystemMemoryInfo();
    constexpr size_t chunkSize = 2 * MemoryConstants::pageSize;
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(chunkSize));
    setAnonymousMmapFunctions(*memoryManager);
    auto mockClientContext = static_cast<MockGmmClientContextBase *>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getGmmClientContext());
    mockClientContext->getPatIndexQueryCalled = 0u;

    std::vector<osHandle> handles = {11u, 12u};
    std::vector<uint64_t> physicalOffsets = {};
    AllocationProperties properties(rootDeviceIndex, MemoryConstants::pageSize, AllocationType::bufferHostMemory, systemMemoryBitfield);

    auto gfxAllocation = memoryManager->createHostAllocationFromMultipleSharedHandles(handles, properties, physicalOffsets, false);
    ASSERT_NE(nullptr, gfxAllocation);

    ASSERT_EQ(handles.size(), gfxAllocation->getNumGmms());
    for (auto handleId = 0u; handleId < gfxAllocation->getNumGmms(); handleId++) {
        auto gmm = gfxAllocation->getGmm(handleId);
        ASSERT_NE(nullptr, gmm);
        EXPECT_FALSE(gmm->isCompressionEnabled());
        EXPECT_EQ(chunkSize, gmm->gmmResourceInfo->getSizeAllocation());
    }
    EXPECT_EQ(handles.size(), mockClientContext->getPatIndexQueryCalled);
    EXPECT_EQ(gfxAllocation->getGmm(1)->gmmResourceInfo->peekGmmResourceInfo(), mockClientContext->passedResourceInfoForGetPatIndexQuery);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenWholeObjectMmapFailingWhenCreateHostAllocationFromMultipleSharedHandlesWithOffsetThenNullptrIsReturned) {
    static uint8_t hostRangeBuffer[4 * MemoryConstants::pageSize] = {};
    drm->bindAvailable = true;
    installSystemMemoryInfo();
    enableMmapWindowRelocation(drm);
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(2 * MemoryConstants::pageSize));
    resetCapturedMremap();
    memoryManager->mremapFixedFunction = capturingMremapFixed;

    memoryManager->mmapFunction = [](void *addr, size_t, int prot, int, int, off_t) noexcept -> void * {
        if ((addr == nullptr) && (prot == (PROT_READ | PROT_WRITE))) {
            return MAP_FAILED;
        }
        return addr ? addr : hostRangeBuffer;
    };
    memoryManager->munmapFunction = [](void *, size_t) noexcept -> int { return 0; };

    std::vector<osHandle> handles = {11u};
    std::vector<uint64_t> physicalOffsets = {MemoryConstants::pageSize};
    AllocationProperties properties(rootDeviceIndex, MemoryConstants::pageSize, AllocationType::bufferHostMemory, systemMemoryBitfield);

    EXPECT_EQ(nullptr, memoryManager->createHostAllocationFromMultipleSharedHandles(handles, properties, physicalOffsets, false));
    EXPECT_EQ(0u, capturedMremapCalls);
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenMremapFailingWhenCreateHostAllocationFromMultipleSharedHandlesWithOffsetThenWholeObjectIsReleasedAndNullptrIsReturned) {
    static uint8_t hostRangeBuffer[4 * MemoryConstants::pageSize] = {};
    drm->bindAvailable = true;
    installSystemMemoryInfo();
    enableMmapWindowRelocation(drm);
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(2 * MemoryConstants::pageSize));
    resetCapturedMremap();
    failCapturedMremap = true;
    memoryManager->mremapFixedFunction = capturingMremapFixed;

    memoryManager->mmapFunction = [](void *addr, size_t, int, int, int, off_t) noexcept -> void * {
        return addr ? addr : hostRangeBuffer;
    };
    memoryManager->munmapFunction = [](void *, size_t) noexcept -> int { return 0; };

    std::vector<osHandle> handles = {11u};
    std::vector<uint64_t> physicalOffsets = {MemoryConstants::pageSize};
    AllocationProperties properties(rootDeviceIndex, MemoryConstants::pageSize, AllocationType::bufferHostMemory, systemMemoryBitfield);

    EXPECT_EQ(nullptr, memoryManager->createHostAllocationFromMultipleSharedHandles(handles, properties, physicalOffsets, false));
    EXPECT_EQ(1u, capturedMremapCalls);

    failCapturedMremap = false;
}

struct RangeChunkCpuMapRecorder {
    struct FileMap {
        int fd;
        off_t offset;
        size_t size;
        void *requestedAddress;
    };
    struct Remap {
        void *oldAddress;
        size_t size;
        void *newAddress;
    };
    static constexpr size_t maxEntries = 8u;
    static inline FileMap fileMaps[maxEntries] = {};
    static inline size_t fileMapCount = 0u;
    static inline Remap remaps[maxEntries] = {};
    static inline size_t remapCount = 0u;
    static inline int failingFd = -1;
    static inline uint8_t reservation[3 * MemoryConstants::pageSize] = {};
    static inline uint8_t temporaryMapping[2 * MemoryConstants::pageSize] = {};

    static void reset() {
        fileMapCount = 0u;
        remapCount = 0u;
        failingFd = -1;
    }

    static void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset) noexcept {
        if (fd < 0) {
            return reservation;
        }
        if (fileMapCount < maxEntries) {
            fileMaps[fileMapCount++] = {fd, offset, len, addr};
        }
        if (fd == failingFd) {
            return MAP_FAILED;
        }
        return addr ? addr : temporaryMapping;
    }

    static void *mremapFixed(void *oldAddress, size_t size, void *newAddress) noexcept {
        if (remapCount < maxEntries) {
            remaps[remapCount++] = {oldAddress, size, newAddress};
        }
        return newAddress;
    }

    static int munmap(void *, size_t) noexcept {
        return 0;
    }
};

template <typename DrmMockT>
void enableDmaBufMmapWithWindowRelocation(DrmMockT *drm) {
    auto ioctlHelper = std::make_unique<MockIoctlHelper>(*drm);
    ioctlHelper->isMmapWindowRelocationSupportedResult = true;
    ioctlHelper->isDmaBufMmapPreferredForImportedHostMemoryResult = true;
    drm->ioctlHelper = std::move(ioctlHelper);
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenDmaBufMmapPreferredWhenCreateHostAllocationFromMultipleSharedHandlesThenEachChunkDmaBufIsMappedAtItsOffsetAndMovedIntoTheReservation) {
    drm->bindAvailable = true;
    installSystemMemoryInfo();
    enableDmaBufMmapWithWindowRelocation(drm);
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(2 * MemoryConstants::pageSize));
    RangeChunkCpuMapRecorder::reset();
    memoryManager->mmapFunction = RangeChunkCpuMapRecorder::mmap;
    memoryManager->mremapFixedFunction = RangeChunkCpuMapRecorder::mremapFixed;
    memoryManager->munmapFunction = RangeChunkCpuMapRecorder::munmap;

    std::vector<osHandle> handles = {11u, 12u};
    std::vector<uint64_t> physicalOffsets = {0u, MemoryConstants::pageSize};
    AllocationProperties properties(rootDeviceIndex, MemoryConstants::pageSize, AllocationType::bufferHostMemory, systemMemoryBitfield);

    const auto mmapOffsetCallsBefore = drm->ioctlCnt.gemMmapOffset.load();
    auto gfxAllocation = memoryManager->createHostAllocationFromMultipleSharedHandles(handles, properties, physicalOffsets, false);
    ASSERT_NE(nullptr, gfxAllocation);
    EXPECT_EQ(mmapOffsetCallsBefore, drm->ioctlCnt.gemMmapOffset.load());

    auto reservationBase = static_cast<void *>(RangeChunkCpuMapRecorder::reservation);
    ASSERT_EQ(2u, RangeChunkCpuMapRecorder::fileMapCount);
    EXPECT_EQ(11, RangeChunkCpuMapRecorder::fileMaps[0].fd);
    EXPECT_EQ(0, RangeChunkCpuMapRecorder::fileMaps[0].offset);
    EXPECT_EQ(2 * MemoryConstants::pageSize, RangeChunkCpuMapRecorder::fileMaps[0].size);
    EXPECT_EQ(nullptr, RangeChunkCpuMapRecorder::fileMaps[0].requestedAddress);
    EXPECT_EQ(12, RangeChunkCpuMapRecorder::fileMaps[1].fd);
    EXPECT_EQ(static_cast<off_t>(MemoryConstants::pageSize), RangeChunkCpuMapRecorder::fileMaps[1].offset);
    EXPECT_EQ(MemoryConstants::pageSize, RangeChunkCpuMapRecorder::fileMaps[1].size);
    EXPECT_EQ(nullptr, RangeChunkCpuMapRecorder::fileMaps[1].requestedAddress);
    ASSERT_EQ(2u, RangeChunkCpuMapRecorder::remapCount);
    EXPECT_EQ(reservationBase, RangeChunkCpuMapRecorder::remaps[0].newAddress);
    EXPECT_EQ(2 * MemoryConstants::pageSize, RangeChunkCpuMapRecorder::remaps[0].size);
    EXPECT_EQ(ptrOffset(reservationBase, 2 * MemoryConstants::pageSize), RangeChunkCpuMapRecorder::remaps[1].newAddress);
    EXPECT_EQ(MemoryConstants::pageSize, RangeChunkCpuMapRecorder::remaps[1].size);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenDmaBufMmapFailingForChunkWithOffsetWhenCreateHostAllocationFromMultipleSharedHandlesThenThatChunkFallsBackToDeviceMmapOffsetWindow) {
    drm->bindAvailable = true;
    installSystemMemoryInfo();
    enableDmaBufMmapWithWindowRelocation(drm);
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(2 * MemoryConstants::pageSize));
    RangeChunkCpuMapRecorder::reset();
    RangeChunkCpuMapRecorder::failingFd = 12;
    memoryManager->mmapFunction = RangeChunkCpuMapRecorder::mmap;
    memoryManager->mremapFixedFunction = RangeChunkCpuMapRecorder::mremapFixed;
    memoryManager->munmapFunction = RangeChunkCpuMapRecorder::munmap;

    std::vector<osHandle> handles = {11u, 12u};
    std::vector<uint64_t> physicalOffsets = {0u, MemoryConstants::pageSize};
    AllocationProperties properties(rootDeviceIndex, MemoryConstants::pageSize, AllocationType::bufferHostMemory, systemMemoryBitfield);

    const auto mmapOffsetCallsBefore = drm->ioctlCnt.gemMmapOffset.load();
    auto gfxAllocation = memoryManager->createHostAllocationFromMultipleSharedHandles(handles, properties, physicalOffsets, false);
    ASSERT_NE(nullptr, gfxAllocation);
    EXPECT_EQ(mmapOffsetCallsBefore + 1, drm->ioctlCnt.gemMmapOffset.load());

    ASSERT_EQ(3u, RangeChunkCpuMapRecorder::fileMapCount);
    EXPECT_EQ(12, RangeChunkCpuMapRecorder::fileMaps[1].fd);
    EXPECT_EQ(drm->getFileDescriptor(), RangeChunkCpuMapRecorder::fileMaps[2].fd);
    EXPECT_EQ(2 * MemoryConstants::pageSize, RangeChunkCpuMapRecorder::fileMaps[2].size);
    EXPECT_EQ(nullptr, RangeChunkCpuMapRecorder::fileMaps[2].requestedAddress);
    ASSERT_EQ(2u, RangeChunkCpuMapRecorder::remapCount);
    EXPECT_EQ(ptrOffset(static_cast<void *>(RangeChunkCpuMapRecorder::temporaryMapping), MemoryConstants::pageSize), RangeChunkCpuMapRecorder::remaps[1].oldAddress);
    EXPECT_EQ(ptrOffset(static_cast<void *>(RangeChunkCpuMapRecorder::reservation), 2 * MemoryConstants::pageSize), RangeChunkCpuMapRecorder::remaps[1].newAddress);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenDmaBufMmapFailingAndDeviceMmapOffsetRefusedWhenCreateHostAllocationFromMultipleSharedHandlesThenNullptrIsReturned) {
    drm->bindAvailable = true;
    installSystemMemoryInfo();
    enableDmaBufMmapWithWindowRelocation(drm);
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(2 * MemoryConstants::pageSize));
    RangeChunkCpuMapRecorder::reset();
    RangeChunkCpuMapRecorder::failingFd = 11;
    drm->failOnMmapOffset = true;
    memoryManager->mmapFunction = RangeChunkCpuMapRecorder::mmap;
    memoryManager->mremapFixedFunction = RangeChunkCpuMapRecorder::mremapFixed;
    memoryManager->munmapFunction = RangeChunkCpuMapRecorder::munmap;

    std::vector<osHandle> handles = {11u};
    std::vector<uint64_t> physicalOffsets = {0u};
    AllocationProperties properties(rootDeviceIndex, MemoryConstants::pageSize, AllocationType::bufferHostMemory, systemMemoryBitfield);

    EXPECT_EQ(nullptr, memoryManager->createHostAllocationFromMultipleSharedHandles(handles, properties, physicalOffsets, false));
    ASSERT_EQ(1u, RangeChunkCpuMapRecorder::fileMapCount);
    EXPECT_EQ(11, RangeChunkCpuMapRecorder::fileMaps[0].fd);
    EXPECT_EQ(0u, RangeChunkCpuMapRecorder::remapCount);
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenPhysicalOffsetsAndVmBindWhenCreateHostAllocationFromMultipleSharedHandlesThenAllocationIsCreatedWithPerObjectOffset) {
    static uint8_t hostRangeBuffer[3 * MemoryConstants::pageSize] = {};
    drm->bindAvailable = true;
    installSystemMemoryInfo();
    enableMmapWindowRelocation(drm);
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(2 * MemoryConstants::pageSize));

    memoryManager->mmapFunction = [](void *addr, size_t, int, int, int, off_t) noexcept -> void * {
        return addr ? addr : hostRangeBuffer;
    };
    memoryManager->munmapFunction = [](void *, size_t) noexcept -> int { return 0; };

    const uint64_t physicalOffset = MemoryConstants::pageSize;
    std::vector<osHandle> handles = {11u, 12u};
    std::vector<uint64_t> physicalOffsets = {physicalOffset, physicalOffset};
    AllocationProperties properties(rootDeviceIndex, MemoryConstants::pageSize, AllocationType::bufferHostMemory, systemMemoryBitfield);

    auto gfxAllocation = memoryManager->createHostAllocationFromMultipleSharedHandles(handles, properties, physicalOffsets, false);
    ASSERT_NE(nullptr, gfxAllocation);
    auto drmAllocation = static_cast<DrmAllocation *>(gfxAllocation);
    EXPECT_TRUE(drmAllocation->isUsmHostAllocation());
    auto bo0 = drmAllocation->getBufferObjectToModify(0);
    ASSERT_NE(nullptr, bo0);
    EXPECT_EQ(physicalOffset, bo0->getPhysicalMemoryOffset());
    EXPECT_EQ(static_cast<size_t>(2 * MemoryConstants::pageSize) - static_cast<size_t>(physicalOffset), bo0->getVirtualMappingSize());

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenRetrieveMmapOffsetFailingWhenCreateHostAllocationFromMultipleSharedHandlesThenNullptrIsReturned) {
    static uint8_t hostRangeBuffer[3 * MemoryConstants::pageSize] = {};
    drm->bindAvailable = true;
    installSystemMemoryInfo();
    drm->failOnMmapOffset = true;
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(2 * MemoryConstants::pageSize));

    memoryManager->mmapFunction = [](void *addr, size_t, int, int, int, off_t) noexcept -> void * {
        return addr ? addr : hostRangeBuffer;
    };
    memoryManager->munmapFunction = [](void *, size_t) noexcept -> int { return 0; };

    std::vector<osHandle> handles = {11u};
    std::vector<uint64_t> physicalOffsets = {0u};
    AllocationProperties properties(rootDeviceIndex, MemoryConstants::pageSize, AllocationType::bufferHostMemory, systemMemoryBitfield);

    auto gfxAllocation = memoryManager->createHostAllocationFromMultipleSharedHandles(handles, properties, physicalOffsets, false);
    EXPECT_EQ(nullptr, gfxAllocation);
    EXPECT_EQ(1, drm->ioctlCnt.primeFdToHandle);
    EXPECT_EQ(drm->ioctlCnt.primeFdToHandle.load(), drm->ioctlCnt.gemClose.load());
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenPhysicalOffsetsAndNoVmBindAndMultipleHandlesWhenCreateHostAllocationFromMultipleSharedHandlesThenNullptrIsReturned) {
    drm->bindAvailable = false;
    installSystemMemoryInfo();
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(2 * MemoryConstants::pageSize));

    std::vector<osHandle> handles = {11u, 12u};
    std::vector<uint64_t> physicalOffsets = {MemoryConstants::pageSize, MemoryConstants::pageSize};
    AllocationProperties properties(rootDeviceIndex, MemoryConstants::pageSize, AllocationType::bufferHostMemory, systemMemoryBitfield);

    auto gfxAllocation = memoryManager->createHostAllocationFromMultipleSharedHandles(handles, properties, physicalOffsets, false);
    EXPECT_EQ(nullptr, gfxAllocation);
    EXPECT_EQ(0, drm->ioctlCnt.primeFdToHandle);
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenNoMemoryInfoWhenCreateHostAllocationFromMultipleSharedHandlesThenNullptrIsReturned) {
    drm->memoryInfo.reset(nullptr);

    std::vector<osHandle> handles = {11u};
    std::vector<uint64_t> physicalOffsets = {0u};
    AllocationProperties properties(rootDeviceIndex, MemoryConstants::pageSize, AllocationType::bufferHostMemory, systemMemoryBitfield);

    auto gfxAllocation = memoryManager->createHostAllocationFromMultipleSharedHandles(handles, properties, physicalOffsets, false);
    EXPECT_EQ(nullptr, gfxAllocation);
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenEmptyHandlesWhenCreateHostAllocationFromMultipleSharedHandlesThenNullptrIsReturned) {
    std::vector<osHandle> handles = {};
    std::vector<uint64_t> physicalOffsets = {};
    AllocationProperties properties(rootDeviceIndex, MemoryConstants::pageSize, AllocationType::bufferHostMemory, systemMemoryBitfield);

    auto gfxAllocation = memoryManager->createHostAllocationFromMultipleSharedHandles(handles, properties, physicalOffsets, false);
    EXPECT_EQ(nullptr, gfxAllocation);
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenFewerPhysicalOffsetsThanHandlesWhenCreateHostAllocationFromMultipleSharedHandlesThenRemainingHandlesUseZeroOffset) {
    drm->bindAvailable = true;
    installSystemMemoryInfo();
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(2 * MemoryConstants::pageSize));

    std::vector<osHandle> handles = {11u, 12u};
    std::vector<uint64_t> physicalOffsets = {0u};
    AllocationProperties properties(rootDeviceIndex, MemoryConstants::pageSize, AllocationType::bufferHostMemory, systemMemoryBitfield);

    auto gfxAllocation = memoryManager->createHostAllocationFromMultipleSharedHandles(handles, properties, physicalOffsets, false);
    ASSERT_NE(nullptr, gfxAllocation);
    memoryManager->freeGraphicsMemory(gfxAllocation);
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenMmapFailingWhenCreateHostAllocationFromMultipleSharedHandlesThenNullptrIsReturned) {
    drm->bindAvailable = true;
    installSystemMemoryInfo();
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(2 * MemoryConstants::pageSize));
    memoryManager->mmapFunction = [](void *addr, size_t, int, int, int, off_t) noexcept -> void * {
        return MAP_FAILED;
    };

    std::vector<osHandle> handles = {11u, 12u};
    std::vector<uint64_t> physicalOffsets = {0u, 0u};
    AllocationProperties properties(rootDeviceIndex, MemoryConstants::pageSize, AllocationType::bufferHostMemory, systemMemoryBitfield);

    auto gfxAllocation = memoryManager->createHostAllocationFromMultipleSharedHandles(handles, properties, physicalOffsets, false);
    EXPECT_EQ(nullptr, gfxAllocation);
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenSecondChunkMappingFailsWhenCreateHostAllocationFromMultipleSharedHandlesThenCleanupReleasesFirstChunkWithoutDeadlockAndNullptrIsReturned) {
    static uint8_t hostRangeBuffer[4 * MemoryConstants::pageSize] = {};
    drm->bindAvailable = true;
    installSystemMemoryInfo();
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(2 * MemoryConstants::pageSize));

    static uint32_t mappedChunkCalls = 0u;
    mappedChunkCalls = 0u;
    memoryManager->mmapFunction = [](void *addr, size_t, int, int, int, off_t) noexcept -> void * {
        if (addr == nullptr) {
            return hostRangeBuffer;
        }
        if (++mappedChunkCalls == 2u) {
            return reinterpret_cast<void *>(~reinterpret_cast<uintptr_t>(addr));
        }
        return addr;
    };
    memoryManager->munmapFunction = [](void *, size_t) noexcept -> int { return 0; };

    std::vector<osHandle> handles = {11u, 12u};
    std::vector<uint64_t> physicalOffsets = {0u, 0u};
    AllocationProperties properties(rootDeviceIndex, MemoryConstants::pageSize, AllocationType::bufferHostMemory, systemMemoryBitfield);

    auto gfxAllocation = memoryManager->createHostAllocationFromMultipleSharedHandles(handles, properties, physicalOffsets, false);
    EXPECT_EQ(nullptr, gfxAllocation);
    EXPECT_EQ(2u, mappedChunkCalls);
    EXPECT_EQ(2, drm->ioctlCnt.primeFdToHandle);
    EXPECT_EQ(drm->ioctlCnt.primeFdToHandle.load(), drm->ioctlCnt.gemClose.load());
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenPrimeFdToHandleFailingWhenCreateHostAllocationFromMultipleSharedHandlesThenNullptrIsReturned) {
    installSystemMemoryInfo();
    drm->failOnPrimeFdToHandle = true;

    std::vector<osHandle> handles = {11u};
    std::vector<uint64_t> physicalOffsets = {0u};
    AllocationProperties properties(rootDeviceIndex, MemoryConstants::pageSize, AllocationType::bufferHostMemory, systemMemoryBitfield);

    auto gfxAllocation = memoryManager->createHostAllocationFromMultipleSharedHandles(handles, properties, physicalOffsets, false);
    EXPECT_EQ(nullptr, gfxAllocation);
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenLseekReturningInvalidSizeWhenCreateHostAllocationFromMultipleSharedHandlesThenNullptrIsReturned) {
    installSystemMemoryInfo();
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(-1));

    std::vector<osHandle> handles = {11u};
    std::vector<uint64_t> physicalOffsets = {0u};
    AllocationProperties properties(rootDeviceIndex, MemoryConstants::pageSize, AllocationType::bufferHostMemory, systemMemoryBitfield);

    auto gfxAllocation = memoryManager->createHostAllocationFromMultipleSharedHandles(handles, properties, physicalOffsets, false);
    EXPECT_EQ(nullptr, gfxAllocation);
    EXPECT_EQ(0, drm->ioctlCnt.primeFdToHandle);
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenPhysicalOffsetNotSmallerThanObjectSizeWhenCreateHostAllocationFromMultipleSharedHandlesThenNullptrIsReturned) {
    drm->bindAvailable = true;
    installSystemMemoryInfo();
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(MemoryConstants::pageSize));

    std::vector<osHandle> handles = {11u};
    std::vector<uint64_t> physicalOffsets = {MemoryConstants::pageSize};
    AllocationProperties properties(rootDeviceIndex, MemoryConstants::pageSize, AllocationType::bufferHostMemory, systemMemoryBitfield);

    auto gfxAllocation = memoryManager->createHostAllocationFromMultipleSharedHandles(handles, properties, physicalOffsets, false);
    EXPECT_EQ(nullptr, gfxAllocation);
    EXPECT_EQ(0, drm->ioctlCnt.primeFdToHandle);
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenNoVmBindAndSingleHandleWithOffsetWhenCreateHostAllocationFromMultipleSharedHandlesThenOffsetIsFoldedIntoReportedBase) {
    static uint8_t hostRangeBuffer[3 * MemoryConstants::pageSize] = {};
    drm->bindAvailable = false;
    installSystemMemoryInfo();
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(2 * MemoryConstants::pageSize));

    memoryManager->mmapFunction = [](void *addr, size_t, int, int, int, off_t) noexcept -> void * {
        return addr ? addr : hostRangeBuffer;
    };
    memoryManager->munmapFunction = [](void *, size_t) noexcept -> int { return 0; };

    const uint64_t physicalOffset = MemoryConstants::pageSize;
    std::vector<osHandle> handles = {11u};
    std::vector<uint64_t> physicalOffsets = {physicalOffset};
    AllocationProperties properties(rootDeviceIndex, MemoryConstants::pageSize, AllocationType::bufferHostMemory, systemMemoryBitfield);

    auto gfxAllocation = memoryManager->createHostAllocationFromMultipleSharedHandles(handles, properties, physicalOffsets, false);
    ASSERT_NE(nullptr, gfxAllocation);
    EXPECT_EQ(reinterpret_cast<uint64_t>(hostRangeBuffer) + physicalOffset, gfxAllocation->getGpuAddress());
    auto bo0 = static_cast<DrmAllocation *>(gfxAllocation)->getBufferObjectToModify(0);
    ASSERT_NE(nullptr, bo0);
    EXPECT_EQ(0u, bo0->getPhysicalMemoryOffset());

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenFewerPhysicalOffsetsThanHandlesWhenCreateGraphicsAllocationFromMultipleSharedHandlesThenTrailingHandlesUseZeroOffset) {
    drm->bindAvailable = true;
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(2 * MemoryConstants::pageSize));

    const uint64_t physicalOffset = MemoryConstants::pageSize;
    std::vector<osHandle> handles = {11u, 12u};
    std::vector<uint64_t> physicalOffsets = {physicalOffset};
    AllocationProperties properties(rootDeviceIndex, true, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, systemMemoryBitfield);
    properties.physicalOffsets = physicalOffsets;

    auto gfxAllocation = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, false, nullptr);
    ASSERT_NE(nullptr, gfxAllocation);
    auto drmAllocation = static_cast<DrmAllocation *>(gfxAllocation);
    EXPECT_EQ(physicalOffset, drmAllocation->getBufferObjectToModify(0)->getPhysicalMemoryOffset());
    EXPECT_EQ(0u, drmAllocation->getBufferObjectToModify(1)->getPhysicalMemoryOffset());

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenPrimeFdToHandleFailingAndDebugMessagesEnabledWhenCreateGraphicsAllocationFromMultipleSharedHandlesThenNullptrIsReturned) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintDebugMessages.set(true);
    drm->failOnPrimeFdToHandle = true;

    std::vector<osHandle> handles = {11u};
    AllocationProperties properties(rootDeviceIndex, true, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, systemMemoryBitfield);

    StreamCapture capture;
    capture.captureStderr();
    auto gfxAllocation = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, false, nullptr);
    capture.getCapturedStderr();
    EXPECT_EQ(nullptr, gfxAllocation);
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenNoVmBindAndSingleHandleWithOffsetWhenCreateGraphicsAllocationFromMultipleSharedHandlesThenOffsetIsFoldedIntoReportedBaseAndKeptOnBufferObject) {
    drm->bindAvailable = false;
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(2 * MemoryConstants::pageSize));

    const uint64_t physicalOffset = MemoryConstants::pageSize;
    std::vector<osHandle> handles = {11u};
    std::vector<uint64_t> physicalOffsets = {physicalOffset};
    AllocationProperties properties(rootDeviceIndex, true, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, systemMemoryBitfield);
    properties.physicalOffsets = physicalOffsets;

    auto gfxAllocation = memoryManager->createGraphicsAllocationFromMultipleSharedHandles(handles, properties, false, false, false, nullptr);
    ASSERT_NE(nullptr, gfxAllocation);
    auto bo = static_cast<DrmAllocation *>(gfxAllocation)->getBufferObjectToModify(0);
    ASSERT_NE(nullptr, bo);
    EXPECT_EQ(physicalOffset, bo->getPhysicalMemoryOffset());
    EXPECT_EQ(static_cast<size_t>(2 * MemoryConstants::pageSize - physicalOffset), bo->getVirtualMappingSize());
    EXPECT_EQ(static_cast<size_t>(2 * MemoryConstants::pageSize), bo->peekUnmapSize());
    EXPECT_TRUE(gfxAllocation->isPartialObjectMapping());

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenNoVmBindAndNoOffsetWhenCreateHostAllocationFromMultipleSharedHandlesThenAllocationIsCreated) {
    static uint8_t hostRangeBuffer[3 * MemoryConstants::pageSize] = {};
    drm->bindAvailable = false;
    installSystemMemoryInfo();
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(2 * MemoryConstants::pageSize));

    memoryManager->mmapFunction = [](void *addr, size_t, int, int, int, off_t) noexcept -> void * {
        return addr ? addr : hostRangeBuffer;
    };
    memoryManager->munmapFunction = [](void *, size_t) noexcept -> int { return 0; };

    std::vector<osHandle> handles = {11u};
    std::vector<uint64_t> physicalOffsets = {0u};
    AllocationProperties properties(rootDeviceIndex, MemoryConstants::pageSize, AllocationType::bufferHostMemory, systemMemoryBitfield);

    auto gfxAllocation = memoryManager->createHostAllocationFromMultipleSharedHandles(handles, properties, physicalOffsets, false);
    ASSERT_NE(nullptr, gfxAllocation);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

TEST_F(DrmMemoryManagerMultipleSharedHandlesTest, givenReuseSharedAllocationWhenCreateHostAllocationFromMultipleSharedHandlesThenAllocationIsCreated) {
    static uint8_t hostRangeBuffer[3 * MemoryConstants::pageSize] = {};
    drm->bindAvailable = true;
    installSystemMemoryInfo();
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(2 * MemoryConstants::pageSize));

    memoryManager->mmapFunction = [](void *addr, size_t, int, int, int, off_t) noexcept -> void * {
        return addr ? addr : hostRangeBuffer;
    };
    memoryManager->munmapFunction = [](void *, size_t) noexcept -> int { return 0; };

    std::vector<osHandle> handles = {11u};
    std::vector<uint64_t> physicalOffsets = {0u};
    AllocationProperties properties(rootDeviceIndex, MemoryConstants::pageSize, AllocationType::bufferHostMemory, systemMemoryBitfield);

    auto gfxAllocation = memoryManager->createHostAllocationFromMultipleSharedHandles(handles, properties, physicalOffsets, true);
    ASSERT_NE(nullptr, gfxAllocation);

    memoryManager->freeGraphicsMemory(gfxAllocation);
}

TEST(DrmMemoryManagerFreeGraphicsMemoryUnreferenceTest,
     givenCallToCreateSharedAllocationWithReuseSharedAllocationThenAllocationsSuccedAndAddressesAreTheSame) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    const uint32_t rootDeviceIndex = 0u;
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]);
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    executionEnvironment.rootDeviceEnvironments[0]->initGmm();
    TestedDrmMemoryManager memoryManager(executionEnvironment);

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, {});
    auto allocation = memoryManager.createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, allocation);

    auto allocation2 = memoryManager.createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, allocation2);

    EXPECT_EQ(allocation->getGpuAddress(), allocation2->getGpuAddress());

    memoryManager.freeGraphicsMemory(allocation2);
    memoryManager.freeGraphicsMemory(allocation);
}

TEST(DrmMemoryManagerFreeGraphicsMemoryUnreferenceTest, givenDrmMemoryManagerAndFreeGraphicsMemoryIsCalledForSharedAllocationThenUnreferenceBufferObjectIsCalledWithSynchronousDestroySetToFalse) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    const uint32_t rootDeviceIndex = 0u;
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]);
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    executionEnvironment.rootDeviceEnvironments[0]->initGmm();
    TestedDrmMemoryManager memoryManager(executionEnvironment);

    TestedDrmMemoryManager::OsHandleData osHandleData{1u};
    AllocationProperties properties(rootDeviceIndex, false, MemoryConstants::pageSize, AllocationType::sharedBuffer, false, {});
    auto allocation = memoryManager.createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
    ASSERT_NE(nullptr, allocation);

    memoryManager.freeGraphicsMemory(allocation);

    EXPECT_EQ(1 + EngineLimits::maxHandleCount - 1, memoryManager.unreferenceCalled);
    EXPECT_FALSE(memoryManager.unreferenceParamsPassed[0].synchronousDestroy);
    for (size_t i = 1; i < EngineLimits::maxHandleCount - 1; ++i) {
        EXPECT_TRUE(memoryManager.unreferenceParamsPassed[i].synchronousDestroy);
    }
}

using DrmMemoryManagerHostIpcTrackingTest = DrmMemoryManagerWithLocalMemoryTest;

HWTEST_TEMPLATED_F(DrmMemoryManagerHostIpcTrackingTest, givenRegisteredImportedHostAllocationWhenFreedThenAccountingAndEvictionTrackingAreCleared) {
    mock->ioctlExpected.primeFdToHandle = 2;
    mock->ioctlExpected.gemMmapOffset = 2;
    mock->ioctlExpected.gemWait = 2;
    mock->ioctlExpected.gemClose = 2;

    VariableBackup<off_t> lseekRetValBackup(&SysCalls::lseekReturn, MemoryConstants::pageSize);
    SVMAllocsManager svmAllocsManager(memoryManager);
    mock->outputHandle = 100;
    const auto usedMemoryBefore = memoryManager->getUsedSystemMemorySize();
    const auto trackedBefore = memoryManager->getSysMemAllocs().size();

    for (bool importedParameter : {false, true}) {
        SvmAllocationData importedAllocData(rootDeviceIndex);
        GraphicsAllocation *allocation = nullptr;
        auto ptr = memoryManager->importFdHandle(device, &svmAllocsManager, 123, AllocationType::bufferHostMemory,
                                                 true, nullptr, &allocation, importedAllocData, false, false, 0u);
        ASSERT_NE(nullptr, ptr);
        ASSERT_NE(nullptr, allocation);
        EXPECT_TRUE(allocation->getIsImported());
        const auto &trackedAllocations = memoryManager->getSysMemAllocs();
        EXPECT_NE(trackedAllocations.end(), std::find(trackedAllocations.begin(), trackedAllocations.end(), allocation));
        EXPECT_EQ(trackedBefore + 1, trackedAllocations.size());
        EXPECT_EQ(usedMemoryBefore + allocation->getUnderlyingBufferSize(), memoryManager->getUsedSystemMemorySize());

        EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(ptr));
        EXPECT_EQ(InternalMemoryType::hostUnifiedMemory, importedAllocData.memoryType);
        EXPECT_TRUE(importedAllocData.isImportedAllocation);
        memoryManager->freeGraphicsMemory(allocation, importedParameter);

        EXPECT_EQ(trackedBefore, memoryManager->getSysMemAllocs().size());
        EXPECT_EQ(usedMemoryBefore, memoryManager->getUsedSystemMemorySize());
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerHostIpcTrackingTest, givenRegisteredHostAllocationWithoutImportedFlagWhenFreedAsImportedThenAccountingAndEvictionTrackingAreCleared) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemMmapOffset = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    VariableBackup<off_t> lseekRetValBackup(&SysCalls::lseekReturn, MemoryConstants::pageSize);
    AllocationProperties properties(rootDeviceIndex, MemoryConstants::pageSize, AllocationType::bufferHostMemory, 1);
    properties.useMmapObject = true;
    mock->outputHandle = 100;
    const auto usedMemoryBefore = memoryManager->getUsedSystemMemorySize();
    const auto trackedBefore = memoryManager->getSysMemAllocs().size();
    const auto unregisterBefore = memoryManager->unregisterAllocationCalled;

    auto allocation = memoryManager->createUSMHostAllocationFromSharedHandle(123, properties, nullptr, false, true);
    ASSERT_NE(nullptr, allocation);
    EXPECT_FALSE(allocation->getIsImported());
    EXPECT_EQ(trackedBefore + 1, memoryManager->getSysMemAllocs().size());
    EXPECT_GT(memoryManager->getUsedSystemMemorySize(), usedMemoryBefore);

    memoryManager->freeGraphicsMemory(allocation, true);

    EXPECT_EQ(unregisterBefore + 1, memoryManager->unregisterAllocationCalled);
    EXPECT_EQ(trackedBefore, memoryManager->getSysMemAllocs().size());
    EXPECT_EQ(usedMemoryBefore, memoryManager->getUsedSystemMemorySize());
}

HWTEST_TEMPLATED_F(DrmMemoryManagerHostIpcTrackingTest, givenUnregisteredImportedDeviceAllocationWhenFreedThenAccountingIsUnchanged) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    VariableBackup<off_t> lseekRetValBackup(&SysCalls::lseekReturn, MemoryConstants::pageSize);
    AllocationProperties properties(rootDeviceIndex, MemoryConstants::pageSize, AllocationType::buffer, 1);
    mock->outputHandle = 100;
    const auto usedMemoryBefore = memoryManager->getUsedSystemMemorySize();
    const auto trackedBefore = memoryManager->getSysMemAllocs().size();
    const auto unregisterBefore = memoryManager->unregisterAllocationCalled;
    MemoryManager::OsHandleData handleData{123u};
    auto allocation = memoryManager->createGraphicsAllocationFromSharedHandle(handleData, properties, false, false, false, nullptr);
    ASSERT_NE(nullptr, allocation);
    allocation->setIsImported();
    allocation->setSharedHandle(Sharing::nonSharedResource);

    memoryManager->freeGraphicsMemory(allocation, true);

    EXPECT_EQ(trackedBefore, memoryManager->getSysMemAllocs().size());
    EXPECT_EQ(usedMemoryBefore, memoryManager->getUsedSystemMemorySize());
    EXPECT_EQ(unregisterBefore, memoryManager->unregisterAllocationCalled);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerHostIpcTrackingTest, givenConsumeFdTrueAndMappedPtrWhenRegisterSysMemAllocFailsThenNullptrReturnedAndBoUnreferenced) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemClose = 1;

    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<int> closeArgBackup(&SysCalls::closeFuncArgPassed, 0);

    int testFd = 123;
    void *testMappedPtr = reinterpret_cast<void *>(0x12345000);

    AllocationProperties properties(rootDeviceIndex, MemoryConstants::pageSize, AllocationType::buffer, 1);
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

HWTEST_TEMPLATED_F(DrmMemoryManagerHostIpcTrackingTest, givenConsumeFdTrueAndNoBooMmapWhenRegisterSysMemAllocFailsThenNullptrReturnedAndAccountingConsistent) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<int> closeArgBackup(&SysCalls::closeFuncArgPassed, 0);

    int testFd = 123;

    AllocationProperties properties(rootDeviceIndex, MemoryConstants::pageSize, AllocationType::buffer, 1);
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

HWTEST_TEMPLATED_F(DrmMemoryManagerHostIpcTrackingTest, givenNewBoCreationWhenRegisterSysMemAllocFailsThenNullptrReturnedAndAccountingConsistent) {
    mock->ioctlExpected.primeFdToHandle = 1;
    mock->ioctlExpected.gemMmapOffset = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    VariableBackup<uint32_t> closeCalledBackup(&SysCalls::closeFuncCalled, 0u);
    VariableBackup<int> closeArgBackup(&SysCalls::closeFuncArgPassed, 0);
    VariableBackup<off_t> lseekRetValBackup(&SysCalls::lseekReturn, MemoryConstants::pageSize);

    int testFd = 123;

    AllocationProperties properties(rootDeviceIndex, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.useMmapObject = true;
    properties.gpuAddress = 0x1000;

    mock->outputHandle = 100;

    installMockedMemoryInfo(*mock);

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

HWTEST_TEMPLATED_F(DrmMemoryManagerHostIpcTrackingTest, givenConsumeFdTrueAndReuseSharedAllocationWhenRegisterSysMemAllocFailsThenNullptrReturnedAndAccountingConsistent) {
    mock->ioctlExpected.primeFdToHandle = 2;
    mock->ioctlExpected.gemMmapOffset = 1;
    mock->ioctlExpected.gemWait = 2;
    mock->ioctlExpected.gemClose = 1;

    VariableBackup<off_t> lseekRetValBackup(&SysCalls::lseekReturn, MemoryConstants::pageSize);

    int testFd1 = 123;
    int testFd2 = 456;

    AllocationProperties properties(rootDeviceIndex, MemoryConstants::pageSize, AllocationType::buffer, 1);
    properties.useMmapObject = true;
    properties.gpuAddress = 0x1000;

    mock->outputHandle = 100;

    installMockedMemoryInfo(*mock);

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

struct DrmMemoryManagerMultipleSharedHandlesFailureInjectionTest : public MemoryManagementFixture, public ::testing::Test {
    void SetUp() override {
        MemoryManagementFixture::setUp();
        executionEnvironment = std::make_unique<MockExecutionEnvironment>(defaultHwInfo.get());
        auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
        rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
        drm = DrmMockCustom::create(rootDeviceEnvironment).release();
        rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
        rootDeviceEnvironment.memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
        rootDeviceEnvironment.initGmm();
        memoryManager = new TestedDrmMemoryManager(*executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);
        drm->outputHandle = 2u;
        std::vector<MemoryRegion> regionInfo(1);
        regionInfo[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0};
        drm->memoryInfo.reset(new MemoryInfo(regionInfo, *drm));
        drm->memoryInfoQueried = true;
    }

    void TearDown() override {
        executionEnvironment.reset();
        MemoryManagementFixture::tearDown();
    }

    const uint32_t rootDeviceIndex = 0u;
    DrmMockCustom *drm = nullptr;
    TestedDrmMemoryManager *memoryManager = nullptr;
    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;
};

TEST_F(DrmMemoryManagerMultipleSharedHandlesFailureInjectionTest, givenBufferObjectAllocationFailingWhenCreateHostAllocationFromMultipleSharedHandlesThenNullptrIsReturned) {
    static uint8_t hostRangeBuffer[3 * MemoryConstants::pageSize] = {};
    drm->bindAvailable = true;
    VariableBackup<off_t> lseekBackup(&SysCalls::lseekReturn, static_cast<off_t>(2 * MemoryConstants::pageSize));

    memoryManager->mmapFunction = [](void *addr, size_t, int, int, int, off_t) noexcept -> void * {
        return addr ? addr : hostRangeBuffer;
    };
    memoryManager->munmapFunction = [](void *, size_t) noexcept -> int { return 0; };

    std::vector<osHandle> handles = {11u};
    std::vector<uint64_t> physicalOffsets = {0u};
    AllocationProperties properties(rootDeviceIndex, MemoryConstants::pageSize, AllocationType::bufferHostMemory, systemMemoryBitfield);

    InjectedFunction method = [&](size_t failureIndex) {
        auto allocation = memoryManager->createHostAllocationFromMultipleSharedHandles(handles, properties, physicalOffsets, false);
        if (allocation != nullptr) {
            memoryManager->freeGraphicsMemory(allocation);
        }
    };
    injectFailures(method);
}

struct MockIoctlHelperPrelimResourceRegistration : public IoctlHelperPrelim20 {
  public:
    using IoctlHelperPrelim20::classHandles;

    MockIoctlHelperPrelimResourceRegistration(Drm &drm) : IoctlHelperPrelim20(drm) {}

    ADDMETHOD_CONST_NOBASE(isDeferBackingEnabledForAllocation, bool, false, (size_t allocationSize, AllocationType allocationType));
};

TEST_F(DrmAllocationTests, givenResourceRegistrationEnabledWhenAllocationTypeShouldBeRegisteredThenBoHasBindExtHandleAdded) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(drm);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::maxSize); i++) {
        ioctlHelper->classHandles.push_back(i);
    }
    drm.ioctlHelper.reset(ioctlHelper.release());

    {
        MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
        MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugContextSaveArea, MemoryPool::system4KBPages);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(&drm);
        EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);
        EXPECT_EQ(DrmResourceClass::contextSaveArea, drm.registeredClass);
    }
    drm.registeredClass = DrmResourceClass::maxSize;

    {
        MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
        MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugSbaTrackingBuffer, MemoryPool::system4KBPages);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(&drm);
        EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);
        EXPECT_EQ(DrmResourceClass::sbaTrackingBuffer, drm.registeredClass);
    }
    drm.registeredClass = DrmResourceClass::maxSize;

    {
        MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
        MockDrmAllocation allocation(rootDeviceIndex, AllocationType::kernelIsa, MemoryPool::system4KBPages);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(&drm);
        EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);
        EXPECT_EQ(DrmResourceClass::isa, drm.registeredClass);
    }
    drm.registeredClass = DrmResourceClass::maxSize;

    {
        MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
        MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugModuleArea, MemoryPool::system4KBPages);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(&drm);
        EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);
        EXPECT_EQ(DrmResourceClass::moduleHeapDebugArea, drm.registeredClass);
    }

    drm.registeredClass = DrmResourceClass::maxSize;

    {
        MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
        MockDrmAllocation allocation(rootDeviceIndex, AllocationType::bufferHostMemory, MemoryPool::system4KBPages);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(&drm);
        EXPECT_EQ(0u, bo.bindExtHandles.size());
        EXPECT_EQ(DrmResourceClass::maxSize, drm.registeredClass);
    }
}

TEST_F(DrmAllocationTests, givenResourceRegistrationEnabledWhenAllocationTypeShouldNotBeRegisteredThenNoBindHandleCreated) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    drm.registeredClass = DrmResourceClass::maxSize;
    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(drm);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::maxSize); i++) {
        ioctlHelper->classHandles.push_back(i);
    }
    drm.ioctlHelper.reset(ioctlHelper.release());

    {
        MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
        MockDrmAllocation allocation(rootDeviceIndex, AllocationType::kernelIsaInternal, MemoryPool::system4KBPages);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(&drm);
        EXPECT_EQ(0u, bo.bindExtHandles.size());
    }
    EXPECT_EQ(DrmResourceClass::maxSize, drm.registeredClass);
}

TEST_F(DrmAllocationTests, givenResourceRegistrationNotEnabledWhenRegisteringBindExtHandleThenHandleIsNotAddedToBo) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(drm);
    EXPECT_EQ(0u, ioctlHelper->classHandles.size());
    drm.ioctlHelper.reset(ioctlHelper.release());

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugContextSaveArea, MemoryPool::system4KBPages);
    allocation.bufferObjects[0] = &bo;
    allocation.registerBOBindExtHandle(&drm);
    EXPECT_EQ(0u, bo.bindExtHandles.size());
    EXPECT_EQ(DrmResourceClass::maxSize, drm.registeredClass);
}

TEST(DrmMemoryManager, givenTrackedAllocationTypeAndDebuggingDisabledWhenAllocatingThenRegisterBoBindExtHandleIsNotCalled) {
    const uint32_t rootDeviceIndex = 0u;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->initGmm();
    executionEnvironment->setDebuggingMode(DebuggingMode::disabled);

    auto mockDrm = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);

    EXPECT_FALSE(mockDrm->resourceRegistrationEnabled());

    mockDrm->registeredDataSize = 0;

    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugContextSaveArea, MemoryPool::system4KBPages);

    memoryManager->registerAllocationInOs(&allocation);

    EXPECT_FALSE(allocation.registerBOBindExtHandleCalled);
    EXPECT_EQ(DrmResourceClass::maxSize, mockDrm->registeredClass);
}

TEST(DrmMemoryManager, givenResourceRegistrationEnabledAndAllocTypeToCaptureWhenRegisteringAllocationInOsThenItIsMarkedForCapture) {
    const uint32_t rootDeviceIndex = 0u;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->setDebuggingMode(DebuggingMode::online);

    auto mockDrm = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);

    // mock resource registration enabling by storing class handles
    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(*mockDrm);
    ioctlHelper->classHandles.push_back(1);
    mockDrm->ioctlHelper.reset(ioctlHelper.release());

    MockBufferObject bo(rootDeviceIndex, mockDrm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::scratchSurface, MemoryPool::system4KBPages);
    allocation.bufferObjects[0] = &bo;
    memoryManager->registerAllocationInOs(&allocation);

    EXPECT_TRUE(allocation.markedForCapture);

    MockDrmAllocation allocation2(rootDeviceIndex, AllocationType::buffer, MemoryPool::system4KBPages);
    allocation2.bufferObjects[0] = &bo;
    memoryManager->registerAllocationInOs(&allocation2);

    EXPECT_FALSE(allocation2.markedForCapture);
}

TEST(DrmMemoryManager, givenResourceRegistrationEnabledWhenRegisteringAllocationInOsTwiceThenVerifyIdempotentBehavior) {
    const uint32_t rootDeviceIndex = 0u;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->setDebuggingMode(DebuggingMode::online);

    auto mockDrm = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);

    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(*mockDrm);
    ioctlHelper->classHandles.push_back(1);
    mockDrm->ioctlHelper.reset(ioctlHelper.release());

    MockBufferObject bo(rootDeviceIndex, mockDrm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugContextSaveArea, MemoryPool::system4KBPages);
    allocation.bufferObjects[0] = &bo;

    memoryManager->registerAllocationInOs(&allocation);
    auto bindHandlesSize = bo.bindExtHandles.size();
    EXPECT_EQ(1u, bindHandlesSize);

    memoryManager->registerAllocationInOs(&allocation);
    EXPECT_EQ(bindHandlesSize, bo.bindExtHandles.size());
}

TEST(DrmMemoryManager, givenTrackedAllocationTypeWhenAllocatingThenAllocationIsRegistered) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->setDebuggingMode(DebuggingMode::online);

    auto mockDrm = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);

    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(*mockDrm);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::maxSize); i++) {
        ioctlHelper->classHandles.push_back(i);
    }
    mockDrm->ioctlHelper.reset(ioctlHelper.release());

    EXPECT_TRUE(mockDrm->resourceRegistrationEnabled());

    NEO::AllocationProperties properties{0, true, MemoryConstants::pageSize,
                                         NEO::AllocationType::debugSbaTrackingBuffer,
                                         false, 1};

    properties.gpuAddress = 0x20000;
    auto sbaAllocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);
    EXPECT_EQ(DrmResourceClass::sbaTrackingBuffer, mockDrm->registeredClass);

    EXPECT_EQ(sizeof(uint64_t), mockDrm->registeredDataSize);
    uint64_t *data = reinterpret_cast<uint64_t *>(mockDrm->registeredData);
    EXPECT_EQ(properties.gpuAddress, *data);

    memoryManager->freeGraphicsMemory(sbaAllocation);
}

TEST(DrmMemoryManager, givenTrackedAllocationTypeWhenFreeingThenRegisteredHandlesAreUnregistered) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->setDebuggingMode(DebuggingMode::online);

    auto mockDrm = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));
    auto memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);

    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(*mockDrm);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::maxSize); i++) {
        ioctlHelper->classHandles.push_back(i);
    }
    mockDrm->ioctlHelper.reset(ioctlHelper.release());

    EXPECT_TRUE(mockDrm->resourceRegistrationEnabled());

    NEO::AllocationProperties properties{0, true, MemoryConstants::pageSize,
                                         NEO::AllocationType::debugSbaTrackingBuffer,
                                         false, 1};

    properties.gpuAddress = 0x20000;
    auto sbaAllocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    EXPECT_EQ(0u, mockDrm->unregisterCalledCount);

    memoryManager->freeGraphicsMemory(sbaAllocation);

    EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, mockDrm->unregisteredHandle);
    EXPECT_EQ(1u, mockDrm->unregisterCalledCount);
}

TEST(DrmMemoryManager, givenEnabledResourceRegistrationWhenSshIsAllocatedThenItIsMarkedForCapture) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceRunAloneContext.set(0);
    auto executionEnvironment = new MockExecutionEnvironment();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->setDebuggingMode(DebuggingMode::online);

    auto mockDrm = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockDrm));
    executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*mockDrm, 0u, false);
    executionEnvironment->memoryManager = std::make_unique<TestedDrmMemoryManager>(false, false, false, *executionEnvironment);

    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(*mockDrm);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::maxSize); i++) {
        ioctlHelper->classHandles.push_back(i);
    }
    mockDrm->ioctlHelper.reset(ioctlHelper.release());
    EXPECT_TRUE(mockDrm->resourceRegistrationEnabled());

    auto device = std::unique_ptr<MockDevice>(MockDevice::create<MockDevice>(executionEnvironment, 0));

    CommandContainer cmdContainer;
    cmdContainer.initialize(device.get(), nullptr, HeapSize::getDefaultHeapSize(IndirectHeapType::surfaceState), true, false);

    auto *ssh = cmdContainer.getIndirectHeap(NEO::HeapType::surfaceState);
    auto bo = static_cast<DrmAllocation *>(ssh->getGraphicsAllocation())->getBO();

    ASSERT_NE(nullptr, bo);
    EXPECT_TRUE(bo->isMarkedForCapture());
}

TEST(DrmMemoryManager, givenNullBoWhenRegisteringBindExtHandleThenEarlyReturn) {
    const uint32_t rootDeviceIndex = 0u;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    auto mockDrm = std::make_unique<DrmMockResources>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(*mockDrm);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::maxSize); i++) {
        ioctlHelper->classHandles.push_back(i);
    }
    mockDrm->ioctlHelper.reset(ioctlHelper.release());

    EXPECT_TRUE(mockDrm->resourceRegistrationEnabled());

    MockDrmAllocation gfxAllocation(rootDeviceIndex, AllocationType::debugSbaTrackingBuffer, MemoryPool::memoryNull);

    gfxAllocation.registerBOBindExtHandle(mockDrm.get());
    EXPECT_EQ(1u, gfxAllocation.registeredBoBindHandles.size());
    gfxAllocation.freeRegisteredBOBindExtHandles(mockDrm.get());
}

TEST_F(DrmAllocationTests, givenResourceRegistrationEnabledWhenAllocationIsRegisteredThenBosAreMarkedForCaptureAndRequireImmediateBinding) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
    // mock resource registration enabling by storing class handles
    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(drm);

    ioctlHelper->classHandles.push_back(1);
    drm.ioctlHelper.reset(ioctlHelper.release());

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugContextSaveArea, MemoryPool::system4KBPages);
    allocation.bufferObjects[0] = &bo;
    allocation.registerBOBindExtHandle(&drm);

    EXPECT_TRUE(bo.isMarkedForCapture());
    EXPECT_TRUE(bo.isImmediateBindingRequired());
}

TEST_F(DrmAllocationTests, givenResourceRegistrationEnabledAndTileInstancedIsaWhenIsaIsRegisteredThenCookieIsAddedToBoHandle) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(drm);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::maxSize); i++) {
        ioctlHelper->classHandles.push_back(i);
    }
    drm.ioctlHelper.reset(ioctlHelper.release());
    drm.registeredClass = DrmResourceClass::maxSize;

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::kernelIsa, MemoryPool::localMemory);
    allocation.storageInfo.tileInstanced = true;
    allocation.bufferObjects[0] = &bo;
    allocation.registerBOBindExtHandle(&drm);
    EXPECT_EQ(2u, bo.bindExtHandles.size());

    EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);
    EXPECT_EQ(drm.currentCookie - 1, bo.bindExtHandles[1]);

    allocation.freeRegisteredBOBindExtHandles(&drm);
    EXPECT_EQ(2u, drm.unregisterCalledCount);
}

TEST_F(DrmAllocationTests, givenResourceRegistrationEnabledAndSingleInstanceIsaWhenIsaIsRegisteredThenCookieIsNotAddedToBoHandle) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(drm);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::maxSize); i++) {
        ioctlHelper->classHandles.push_back(i);
    }
    drm.ioctlHelper.reset(ioctlHelper.release());
    drm.registeredClass = DrmResourceClass::maxSize;

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::kernelIsa, MemoryPool::system4KBPages);
    allocation.bufferObjects[0] = &bo;
    allocation.registerBOBindExtHandle(&drm);
    EXPECT_EQ(1u, bo.bindExtHandles.size());

    EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);

    allocation.freeRegisteredBOBindExtHandles(&drm);
    EXPECT_EQ(1u, drm.unregisterCalledCount);
}

TEST_F(DrmAllocationTests, givenResourceRegistrationEnabledWhenIsaIsRegisteredThenDeviceBitfieldIsPassedAsPayload) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(drm);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::maxSize); i++) {
        ioctlHelper->classHandles.push_back(i);
    }
    drm.ioctlHelper.reset(ioctlHelper.release());
    drm.registeredClass = DrmResourceClass::maxSize;

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::kernelIsa, MemoryPool::localMemory);
    allocation.storageInfo.subDeviceBitfield = 1 << 3;
    allocation.bufferObjects[0] = &bo;
    allocation.registerBOBindExtHandle(&drm);
    EXPECT_EQ(1u, bo.bindExtHandles.size());

    EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);

    EXPECT_EQ(sizeof(uint32_t), drm.registeredDataSize);
    uint32_t *data = reinterpret_cast<uint32_t *>(drm.registeredData);
    EXPECT_EQ(static_cast<uint32_t>(allocation.storageInfo.subDeviceBitfield.to_ulong()), *data);
}

TEST_F(DrmAllocationTests, givenResourceRegistrationEnabledAndSubDeviceBitfieldSetWhenSbaTrackingBufferIsRegisteredThenDeviceContextIdIsPassedAsPayload) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(drm);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::maxSize); i++) {
        ioctlHelper->classHandles.push_back(i);
    }
    drm.ioctlHelper.reset(ioctlHelper.release());
    drm.registeredClass = DrmResourceClass::maxSize;

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugSbaTrackingBuffer, MemoryPool::localMemory);
    allocation.storageInfo.tileInstanced = false;
    allocation.storageInfo.subDeviceBitfield = 0b0010;
    allocation.bufferObjects[0] = &bo;

    MockOsContextLinux::contextIdForOfflineDump.store(0u);
    MockOsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor(DeviceBitfield{0b11}));
    allocation.setOsContext(&osContext);

    const auto processId = 0xABCEDF;
    uint64_t offlineDumpContextId = static_cast<uint64_t>(processId) << 32 | static_cast<uint64_t>(1u);

    allocation.registerBOBindExtHandle(&drm);
    EXPECT_EQ(2u, bo.bindExtHandles.size());

    EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);

    uint64_t *data = reinterpret_cast<uint64_t *>(drm.registeredData);
    EXPECT_EQ(offlineDumpContextId, *data);
    EXPECT_EQ(DrmResourceClass::contextID, drm.registeredClass);

    allocation.freeRegisteredBOBindExtHandles(&drm);
    EXPECT_EQ(2u, drm.unregisterCalledCount);
}

TEST_F(DrmAllocationTests, givenResourceRegistrationEnabledAndSubDeviceBitfieldNotSetWhenSbaTrackingBufferIsRegisteredThenContextIdIsTakenFromDevice0AndPassedAsPayload) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(drm);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::maxSize); i++) {
        ioctlHelper->classHandles.push_back(i);
    }
    drm.ioctlHelper.reset(ioctlHelper.release());
    drm.registeredClass = DrmResourceClass::maxSize;

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugSbaTrackingBuffer, MemoryPool::localMemory);
    allocation.storageInfo.tileInstanced = false;
    allocation.bufferObjects[0] = &bo;

    MockOsContextLinux::contextIdForOfflineDump.store(3u);
    MockOsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    allocation.setOsContext(&osContext);

    const auto processId = 0xABCEDF;
    uint64_t offlineDumpContextId = static_cast<uint64_t>(processId) << 32 | static_cast<uint64_t>(3u);

    allocation.registerBOBindExtHandle(&drm);
    EXPECT_EQ(2u, bo.bindExtHandles.size());

    EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);

    uint64_t *data = reinterpret_cast<uint64_t *>(drm.registeredData);
    EXPECT_EQ(offlineDumpContextId, *data);
    EXPECT_EQ(DrmResourceClass::contextID, drm.registeredClass);

    allocation.freeRegisteredBOBindExtHandles(&drm);
    EXPECT_EQ(2u, drm.unregisterCalledCount);
}

TEST_F(DrmAllocationTests, givenTwoBufferObjectsAndTileInstancedSbaAndSubDeviceBitfieldWhenSbaTrackingBufferIsRegisteredThenContextIdIsTakenFromBufferObjectIndexAndPassedAsPayload) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(drm);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::maxSize); i++) {
        ioctlHelper->classHandles.push_back(i);
    }
    drm.ioctlHelper.reset(ioctlHelper.release());
    drm.registeredClass = DrmResourceClass::maxSize;

    MockBufferObject bo0(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockBufferObject bo1(rootDeviceIndex, &drm, 3, 0, 0, 1);

    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugSbaTrackingBuffer, MemoryPool::localMemory);
    allocation.storageInfo.subDeviceBitfield = 0b0011;
    allocation.storageInfo.tileInstanced = true;
    allocation.bufferObjects[0] = &bo0;
    allocation.bufferObjects[1] = &bo1;

    MockOsContextLinux::contextIdForOfflineDump.store(5u);
    MockOsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor(DeviceBitfield{0b0011}));
    allocation.setOsContext(&osContext);

    const auto processId = 0xABCEDF;
    uint64_t offlineDumpContextIdBo1 = static_cast<uint64_t>(processId) << 32 | static_cast<uint64_t>(6u);

    allocation.registerBOBindExtHandle(&drm);
    EXPECT_EQ(2u, bo0.bindExtHandles.size());
    EXPECT_EQ(2u, bo1.bindExtHandles.size());

    EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo0.bindExtHandles[0]);
    EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo1.bindExtHandles[0]);

    uint64_t *dataBo1 = reinterpret_cast<uint64_t *>(drm.registeredData);
    EXPECT_EQ(offlineDumpContextIdBo1, *dataBo1);

    allocation.freeRegisteredBOBindExtHandles(&drm);
    EXPECT_EQ(3u, drm.unregisterCalledCount);
}

TEST_F(DrmAllocationTests, givenResourceRegistrationEnabledWhenSbaTrackingBufferIsRegisteredWithoutOsContextThenHandleIsNotAddedToBO) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    auto ioctlHelper = std::make_unique<MockIoctlHelperPrelimResourceRegistration>(drm);

    for (uint32_t i = 3; i < 3 + static_cast<uint32_t>(DrmResourceClass::maxSize); i++) {
        ioctlHelper->classHandles.push_back(i);
    }
    drm.ioctlHelper.reset(ioctlHelper.release());
    drm.registeredClass = DrmResourceClass::maxSize;

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugSbaTrackingBuffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;

    allocation.registerBOBindExtHandle(&drm);
    EXPECT_EQ(1u, bo.bindExtHandles.size());

    EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);

    allocation.freeRegisteredBOBindExtHandles(&drm);
    EXPECT_EQ(1u, drm.unregisterCalledCount);
}

TEST_F(DrmAllocationTests, givenDrmAllocationWhenSetCacheRegionIsCalledForDefaultRegionThenReturnTrue) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);

    EXPECT_TRUE(allocation.setCacheRegion(&drm, CacheRegion::defaultRegion));
}

TEST_F(DrmAllocationTests, givenDrmAllocationWhenCacheInfoIsNotAvailableThenCacheRegionIsNotSet) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);

    EXPECT_FALSE(allocation.setCacheRegion(&drm, CacheRegion::region1));
}

TEST_F(DrmAllocationTests, givenDrmAllocationWhenDefaultCacheInfoIsAvailableThenCacheRegionIsNotSet) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
    drm.setupCacheInfo(*defaultHwInfo.get());

    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);

    EXPECT_FALSE(allocation.setCacheRegion(&drm, CacheRegion::region1));
}

TEST_F(DrmAllocationTests, givenDrmAllocationWhenCacheRegionIsNotSetThenReturnFalse) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    CacheReservationParameters l2CacheParameters{};
    CacheReservationParameters l3CacheParameters{};
    l3CacheParameters.maxSize = 32 * MemoryConstants::kiloByte;
    l3CacheParameters.maxNumRegions = 2;
    l3CacheParameters.maxNumWays = 32;
    drm.cacheInfo.reset(new MockCacheInfo(*drm.getIoctlHelper(), l2CacheParameters, l3CacheParameters));

    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);

    EXPECT_FALSE(allocation.setCacheAdvice(&drm, 1024, CacheRegion::none, false));
}

TEST_F(DrmAllocationTests, givenDrmAllocationWhenCacheRegionIsSetSuccessfullyThenReturnTrue) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
    drm.queryAndSetVmBindPatIndexProgrammingSupport();

    CacheReservationParameters l2CacheParameters{};
    CacheReservationParameters l3CacheParameters{};
    l3CacheParameters.maxSize = 32 * MemoryConstants::kiloByte;
    l3CacheParameters.maxNumRegions = 2;
    l3CacheParameters.maxNumWays = 32;
    drm.cacheInfo.reset(new MockCacheInfo(*drm.getIoctlHelper(), l2CacheParameters, l3CacheParameters));

    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    const auto &hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();

    if ((hwInfo.caps.numCacheRegions == 0) &&
        productHelper.isVmBindPatIndexProgrammingSupported()) {
        EXPECT_ANY_THROW(allocation.setCacheAdvice(&drm, 1024, CacheRegion::region1, false));
    } else {
        EXPECT_TRUE(allocation.setCacheAdvice(&drm, 1024, CacheRegion::region1, false));
    }
}

TEST_F(DrmAllocationTests, givenDrmAllocationWhenCacheRegionIsSetSuccessfullyThenSetRegionInBufferObject) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
    drm.queryAndSetVmBindPatIndexProgrammingSupport();

    CacheReservationParameters l2CacheParameters{};
    CacheReservationParameters l3CacheParameters{};
    l3CacheParameters.maxSize = 32 * MemoryConstants::kiloByte;
    l3CacheParameters.maxNumRegions = 2;
    l3CacheParameters.maxNumWays = 32;
    drm.cacheInfo.reset(new MockCacheInfo(*drm.getIoctlHelper(), l2CacheParameters, l3CacheParameters));

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;

    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    const auto &hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();

    if ((hwInfo.caps.numCacheRegions == 0) &&
        productHelper.isVmBindPatIndexProgrammingSupported()) {
        EXPECT_ANY_THROW(allocation.setCacheAdvice(&drm, 1024, CacheRegion::region1, false));
    } else {
        EXPECT_TRUE(allocation.setCacheAdvice(&drm, 1024, CacheRegion::region1, false));

        for (auto bo : allocation.bufferObjects) {
            if (bo != nullptr) {
                EXPECT_EQ(CacheRegion::region1, bo->peekCacheRegion());
            }
        }
    }
}

TEST_F(DrmAllocationTests, givenDrmAllocationWhenBufferObjectIsCreatedThenApplyDefaultCachePolicy) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;

    for (auto bo : allocation.bufferObjects) {
        if (bo != nullptr) {
            EXPECT_EQ(CachePolicy::writeBack, bo->peekCachePolicy());
        }
    }
}

TEST_F(DrmAllocationTests, givenDrmAllocationWhenSetCachePolicyIsCalledThenUpdatePolicyInBufferObject) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;

    allocation.setCachePolicy(CachePolicy::uncached);

    for (auto bo : allocation.bufferObjects) {
        if (bo != nullptr) {
            EXPECT_EQ(CachePolicy::uncached, bo->peekCachePolicy());
        }
    }
}

TEST_F(DrmAllocationTests, givenDrmAllocationWhenSetMemAdviseWithCachePolicyIsCalledThenUpdatePolicyInBufferObject) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;

    EXPECT_EQ(CachePolicy::writeBack, bo.peekCachePolicy());

    MemAdviseFlags memAdviseFlags{};
    EXPECT_TRUE(memAdviseFlags.cachedMemory);

    for (auto cached : {true, false, true}) {
        memAdviseFlags.cachedMemory = cached;

        EXPECT_TRUE(allocation.setMemAdvise(&drm, memAdviseFlags));

        EXPECT_EQ(cached ? CachePolicy::writeBack : CachePolicy::uncached, bo.peekCachePolicy());

        EXPECT_EQ(memAdviseFlags.allFlags, allocation.getMemAdviseFlags().allFlags);
    }
}

TEST_F(DrmAllocationTests, givenForceCoherentTrueWhenUncachedThenCacheableIsForcedTrue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceAllResourcesUncached.set(1);

    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
    drm.vmBindPatIndexProgrammingSupported = true;

    auto mockClientContext = static_cast<MockGmmClientContextBase *>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getGmmClientContext());

    drm.getPatIndex(nullptr, AllocationType::buffer, CacheRegion::defaultRegion, CachePolicy::writeBack, false, true, true);
    EXPECT_TRUE(mockClientContext->passedCpuCacheableSettingForGetPatIndexQuery);
}

TEST_F(DrmAllocationTests, givenL3FlushAfterPostSyncSupportWhenGettingPatForCoherentUserptrThenSelectUsageAccordingTo2WayCoherencySupport) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableOverrideToPat19ForSystemMemory.set(-1);

    const uint32_t rootDeviceIndex = 0u;
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex];
    auto mockProductHelper = std::make_unique<MockProductHelper>();
    mockProductHelper->isL3FlushAfterPostSyncSupportedResult = true;
    rootDeviceEnvironment.productHelper = std::move(mockProductHelper);

    auto mockReleaseHelper = std::make_unique<MockReleaseHelper>();
    auto mockReleaseHelperPtr = mockReleaseHelper.get();
    rootDeviceEnvironment.releaseHelper = std::move(mockReleaseHelper);

    DrmMock drm(rootDeviceEnvironment);
    drm.vmBindPatIndexProgrammingSupported = true;

    auto mockClientContext = static_cast<MockGmmClientContextBase *>(rootDeviceEnvironment.getGmmClientContext());

    for (const bool is2WayCoherentPatSupported : {false, true}) {
        mockReleaseHelperPtr->isAppTransientCoherentPatRequiredResult = !is2WayCoherentPatSupported;
        mockClientContext->passedUsageTypeForGetPatIndexQuery.reset();

        const auto patIndex = drm.getPatIndex(nullptr, AllocationType::buffer, CacheRegion::defaultRegion, CachePolicy::writeBack, false, true, true);

        const auto expectedUsage = is2WayCoherentPatSupported
                                       ? GMM_RESOURCE_USAGE_FINE_GRAINED_COHERENT
                                       : GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER;
        ASSERT_TRUE(mockClientContext->passedUsageTypeForGetPatIndexQuery.has_value());
        EXPECT_EQ(expectedUsage, mockClientContext->passedUsageTypeForGetPatIndexQuery.value());
        EXPECT_EQ(is2WayCoherentPatSupported ? MockGmmClientContextBase::MockPatIndex::twoWayCoherent : MockGmmClientContextBase::MockPatIndex::cached, patIndex);
    }
}

TEST_F(DrmAllocationTests, givenBufferGmmCreatedFromPatIndexInputsWhenGettingPatIndexWithItThenQueryArgumentsAndPatIndexAreSameAsWithoutGmm) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableOverrideToPat19ForSystemMemory.set(-1);

    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
    auto mockProductHelper = std::make_unique<MockProductHelper>();
    mockProductHelper->isL3FlushAfterPostSyncSupportedResult = true;
    rootDeviceEnvironment.productHelper = std::move(mockProductHelper);
    auto mockReleaseHelper = std::make_unique<MockReleaseHelper>();
    mockReleaseHelper->isAppTransientCoherentPatRequiredResult = false;
    rootDeviceEnvironment.releaseHelper = std::move(mockReleaseHelper);

    DrmMock drm(rootDeviceEnvironment);
    drm.vmBindPatIndexProgrammingSupported = true;
    auto mockClientContext = static_cast<MockGmmClientContextBase *>(rootDeviceEnvironment.getGmmClientContext());

    for (const auto allocationType : {AllocationType::buffer, AllocationType::bufferHostMemory, AllocationType::externalHostPtr}) {
        for (const auto cachePolicy : {CachePolicy::writeBack, CachePolicy::uncached}) {
            for (const bool isSystemMemory : {false, true}) {
                const auto patIndexWithoutGmm = drm.getPatIndex(nullptr, allocationType, CacheRegion::defaultRegion, cachePolicy, false, isSystemMemory, false);
                ASSERT_TRUE(mockClientContext->passedUsageTypeForGetPatIndexQuery.has_value());
                const auto usageWithoutGmm = mockClientContext->passedUsageTypeForGetPatIndexQuery.value();
                const bool compressedWithoutGmm = mockClientContext->passedCompressedSettingForGetPatIndexQuery;
                const bool cacheableWithoutGmm = mockClientContext->passedCpuCacheableSettingForGetPatIndexQuery;

                auto gmm = drm.createBufferGmm(allocationType, cachePolicy, isSystemMemory, MemoryConstants::pageSize);
                const auto patIndexWithGmm = drm.getPatIndex(gmm.get(), allocationType, CacheRegion::defaultRegion, cachePolicy, false, isSystemMemory, false);

                EXPECT_EQ(gmm->gmmResourceInfo->peekGmmResourceInfo(), mockClientContext->passedResourceInfoForGetPatIndexQuery);
                EXPECT_EQ(usageWithoutGmm, mockClientContext->passedUsageTypeForGetPatIndexQuery.value());
                EXPECT_EQ(compressedWithoutGmm, mockClientContext->passedCompressedSettingForGetPatIndexQuery);
                EXPECT_EQ(cacheableWithoutGmm, mockClientContext->passedCpuCacheableSettingForGetPatIndexQuery);
                EXPECT_EQ(patIndexWithoutGmm, patIndexWithGmm);
            }
        }
    }
}

TEST_F(DrmAllocationTests, givenPatIndexInputsWhenCreatingBufferGmmThenResourceFlagsFollowPlacementAndAllowCpuAccess) {
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
    rootDeviceEnvironment.getMutableHardwareInfo()->featureTable.flags.ftrLocalMemory = true;
    DrmMock drm(rootDeviceEnvironment);
    const auto expectedUsage = CacheSettingsHelper::getGmmUsageType(AllocationType::buffer, false, rootDeviceEnvironment.getProductHelper(), *rootDeviceEnvironment.getHardwareInfo());

    for (const bool isSystemMemory : {false, true}) {
        auto gmm = drm.createBufferGmm(AllocationType::buffer, CachePolicy::writeBack, isSystemMemory, MemoryConstants::pageSize);

        const auto *resourceParams = reinterpret_cast<const GMM_RESCREATE_PARAMS *>(gmm->resourceParamsData.data());
        EXPECT_EQ(expectedUsage, static_cast<GmmResourceUsageType>(resourceParams->Usage));
        EXPECT_EQ(isSystemMemory, static_cast<bool>(resourceParams->Flags.Info.NonLocalOnly));
        EXPECT_FALSE(static_cast<bool>(resourceParams->Flags.Info.NotLockable));
        EXPECT_FALSE(gmm->getPreferNoCpuAccess());
        EXPECT_FALSE(gmm->isCompressionEnabled());
    }
}

TEST_F(DrmAllocationTests, givenUsageTypeAndCpuCacheableWhenCreatingBufferGmmThenResourceUsesThemInSystemMemory) {
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
    rootDeviceEnvironment.getMutableHardwareInfo()->featureTable.flags.ftrLocalMemory = true;
    DrmMock drm(rootDeviceEnvironment);

    for (const GmmResourceUsageType usageType : {GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED, GMM_RESOURCE_USAGE_HW_CONTEXT}) {
        for (const bool cpuCacheable : {false, true}) {
            auto gmm = drm.createBufferGmm(usageType, cpuCacheable, true, MemoryConstants::pageSize);

            const auto *resourceParams = reinterpret_cast<const GMM_RESCREATE_PARAMS *>(gmm->resourceParamsData.data());
            EXPECT_EQ(usageType, static_cast<GmmResourceUsageType>(resourceParams->Usage));
            EXPECT_EQ(cpuCacheable, static_cast<bool>(resourceParams->Flags.Info.Cacheable));
            EXPECT_TRUE(static_cast<bool>(resourceParams->Flags.Info.NonLocalOnly));
            EXPECT_FALSE(gmm->getPreferNoCpuAccess());
            EXPECT_FALSE(gmm->isCompressionEnabled());
        }
    }
}

TEST_F(DrmAllocationTests, givenL3FlushAfterPostSyncSupportAndAppTransientUsageWhenGettingPatForCoherentUserptrThenPreserveAppTransientUsage) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableOverrideToPat19ForSystemMemory.set(-1);

    const uint32_t rootDeviceIndex = 0u;
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex];
    auto mockProductHelper = std::make_unique<MockProductHelper>();
    mockProductHelper->isL3FlushAfterPostSyncSupportedResult = true;
    rootDeviceEnvironment.productHelper = std::move(mockProductHelper);

    auto mockReleaseHelper = std::make_unique<MockReleaseHelper>();
    mockReleaseHelper->is2WayCoherentPatSupportedResult = true;
    rootDeviceEnvironment.releaseHelper = std::move(mockReleaseHelper);

    DrmMock drm(rootDeviceEnvironment);
    drm.vmBindPatIndexProgrammingSupported = true;

    auto mockClientContext = static_cast<MockGmmClientContextBase *>(rootDeviceEnvironment.getGmmClientContext());
    mockClientContext->passedUsageTypeForGetPatIndexQuery.reset();

    const auto patIndex = drm.getPatIndex(nullptr, AllocationType::bufferHostMemory, CacheRegion::defaultRegion, CachePolicy::writeBack, false, true, true);

    ASSERT_TRUE(mockClientContext->passedUsageTypeForGetPatIndexQuery.has_value());
    EXPECT_EQ(GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER, mockClientContext->passedUsageTypeForGetPatIndexQuery.value());
    EXPECT_EQ(MockGmmClientContextBase::MockPatIndex::cached, patIndex);
}

HWTEST2_F(DrmAllocationTests, givenPat19OverrideWhenGettingPatThenOnlyCacheableSystemMemoryIsOverridden, IsAtLeastXe3pCore) {
    DebugManagerStateRestore restorer;
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
    rootDeviceEnvironment.productHelper = std::make_unique<MockProductHelper>();
    rootDeviceEnvironment.releaseHelper = std::make_unique<MockReleaseHelper>();

    DrmMock drm(rootDeviceEnvironment);
    drm.vmBindPatIndexProgrammingSupported = true;

    GmmRequirements gmmRequirements{};
    gmmRequirements.preferCompressed = false;
    auto gmm = std::make_unique<Gmm>(rootDeviceEnvironment.getGmmHelper(), nullptr, 4096, 0,
                                     GMM_RESOURCE_USAGE_OCL_BUFFER, StorageInfo{}, gmmRequirements);

    debugManager.flags.EnableOverrideToPat19ForSystemMemory.set(1);
    static_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get())->mockResourceCreateParams.Flags.Info.Cacheable = true;
    const auto getPatIndex = [&](Gmm *resource, bool isSystemMemory) {
        return drm.getPatIndex(resource, AllocationType::buffer, CacheRegion::defaultRegion,
                               CachePolicy::writeBack, false, isSystemMemory, true);
    };

    EXPECT_EQ(19u, getPatIndex(gmm.get(), true));
    EXPECT_EQ(19u, getPatIndex(nullptr, true));
    EXPECT_EQ(MockGmmClientContextBase::MockPatIndex::cached, getPatIndex(gmm.get(), false));

    static_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get())->mockResourceCreateParams.Flags.Info.Cacheable = false;
    EXPECT_EQ(MockGmmClientContextBase::MockPatIndex::cached, getPatIndex(gmm.get(), true));

    debugManager.flags.EnableOverrideToPat19ForSystemMemory.set(0);
    static_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get())->mockResourceCreateParams.Flags.Info.Cacheable = true;
    EXPECT_EQ(MockGmmClientContextBase::MockPatIndex::cached, getPatIndex(gmm.get(), true));
}

TEST_F(DrmAllocationTests, givenForceCoherentFalseWhenUncachedThenCacheableIsNotForced) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceAllResourcesUncached.set(1);

    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
    drm.vmBindPatIndexProgrammingSupported = true;

    auto mockClientContext = static_cast<MockGmmClientContextBase *>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getGmmClientContext());

    drm.getPatIndex(nullptr, AllocationType::buffer, CacheRegion::defaultRegion, CachePolicy::writeBack, false, true, false);
    EXPECT_FALSE(mockClientContext->passedCpuCacheableSettingForGetPatIndexQuery);
}

TEST_F(DrmAllocationTests, givenForceCoherentTrueWithGmmWhenUncachedThenGmmCacheableTakesPrecedence) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceAllResourcesUncached.set(1);

    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
    drm.vmBindPatIndexProgrammingSupported = true;

    auto mockClientContext = static_cast<MockGmmClientContextBase *>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getGmmClientContext());

    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::make_unique<Gmm>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getGmmHelper(),
                                     nullptr, 4096, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, StorageInfo{}, gmmRequirements);
    static_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get())->mockResourceCreateParams.Flags.Info.Cacheable = 0;

    drm.getPatIndex(gmm.get(), AllocationType::buffer, CacheRegion::defaultRegion, CachePolicy::writeBack, false, true, true);
    EXPECT_FALSE(mockClientContext->passedCpuCacheableSettingForGetPatIndexQuery);
    ASSERT_TRUE(mockClientContext->passedUsageTypeForGetPatIndexQuery.has_value());
    EXPECT_EQ(gmm->getResourceUsageType(), mockClientContext->passedUsageTypeForGetPatIndexQuery.value());
}

TEST_F(DrmAllocationTests, givenGmmWhenGettingPatIndexThenResourceInfoOfGmmIsPassedToPatIndexQuery) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
    drm.vmBindPatIndexProgrammingSupported = true;

    auto mockClientContext = static_cast<MockGmmClientContextBase *>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getGmmClientContext());

    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::make_unique<Gmm>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getGmmHelper(),
                                     nullptr, 4096, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, StorageInfo{}, gmmRequirements);

    const auto queriesBefore = mockClientContext->getPatIndexQueryCalled;
    drm.getPatIndex(gmm.get(), AllocationType::buffer, CacheRegion::defaultRegion, CachePolicy::writeBack, false, false, false);

    EXPECT_EQ(queriesBefore + 1u, mockClientContext->getPatIndexQueryCalled);
    EXPECT_EQ(gmm->gmmResourceInfo->peekGmmResourceInfo(), mockClientContext->passedResourceInfoForGetPatIndexQuery);
}

TEST_F(DrmAllocationTests, givenGmmAndMemoryPropertiesWhenGettingPatThenPreserveGmmCacheabilityAndSelectCoherentUsageForCacheableSystemMemoryOrForcedCoherencyWithoutGmm) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableOverrideToPat19ForSystemMemory.set(-1);

    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
    auto productHelper = std::make_unique<MockProductHelper>();
    productHelper->isL3FlushAfterPostSyncSupportedResult = true;
    rootDeviceEnvironment.productHelper = std::move(productHelper);

    auto releaseHelper = std::make_unique<MockReleaseHelper>();
    auto releaseHelperPtr = releaseHelper.get();
    rootDeviceEnvironment.releaseHelper = std::move(releaseHelper);

    DrmMock drm(rootDeviceEnvironment);
    drm.vmBindPatIndexProgrammingSupported = true;
    auto clientContext = static_cast<MockGmmClientContextBase *>(rootDeviceEnvironment.getGmmClientContext());

    GmmRequirements gmmRequirements{};
    gmmRequirements.preferCompressed = false;
    auto gmm = std::make_unique<Gmm>(rootDeviceEnvironment.getGmmHelper(), nullptr, 4096, 0,
                                     GMM_RESOURCE_USAGE_OCL_BUFFER, StorageInfo{}, gmmRequirements);

    for (const bool supports2WayCoherency : {false, true}) {
        releaseHelperPtr->isAppTransientCoherentPatRequiredResult = !supports2WayCoherency;
        for (const bool withGmm : {false, true}) {
            for (const bool gmmCacheable : {false, true}) {
                static_cast<MockGmmResourceInfo *>(gmm->gmmResourceInfo.get())->mockResourceCreateParams.Flags.Info.Cacheable = gmmCacheable;
                for (const bool isSystemMemory : {false, true}) {
                    for (const bool forceCoherent : {false, true}) {
                        clientContext->passedUsageTypeForGetPatIndexQuery.reset();
                        const bool cacheable = !withGmm || gmmCacheable;
                        const bool selectCoherentUsage = (isSystemMemory && cacheable) || (!withGmm && forceCoherent);
                        const auto coherentUsage = supports2WayCoherency ? GMM_RESOURCE_USAGE_FINE_GRAINED_COHERENT : GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER;
                        const auto expectedUsage = selectCoherentUsage ? coherentUsage : GMM_RESOURCE_USAGE_OCL_BUFFER;

                        const auto patIndex = drm.getPatIndex(withGmm ? gmm.get() : nullptr, AllocationType::buffer,
                                                              CacheRegion::defaultRegion, CachePolicy::writeBack, false, isSystemMemory, forceCoherent);

                        ASSERT_TRUE(clientContext->passedUsageTypeForGetPatIndexQuery.has_value());
                        EXPECT_EQ(expectedUsage, clientContext->passedUsageTypeForGetPatIndexQuery.value());
                        EXPECT_EQ(cacheable, clientContext->passedCpuCacheableSettingForGetPatIndexQuery);
                        EXPECT_EQ(selectCoherentUsage && supports2WayCoherency ? MockGmmClientContextBase::MockPatIndex::twoWayCoherent : MockGmmClientContextBase::MockPatIndex::cached, patIndex);
                        EXPECT_EQ(GMM_RESOURCE_USAGE_OCL_BUFFER, gmm->getResourceUsageType());
                        EXPECT_EQ(gmmCacheable, gmm->gmmResourceInfo->isCacheable());
                    }
                }
            }
        }
    }
}

TEST_F(DrmAllocationTests, givenUncachedCachePolicyWhenGettingPatIndexThenUncachedGmmUsageTypeIsSelected) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
    drm.vmBindPatIndexProgrammingSupported = true;

    auto mockClientContext = static_cast<MockGmmClientContextBase *>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getGmmClientContext());

    drm.getPatIndex(nullptr, AllocationType::buffer, CacheRegion::defaultRegion, CachePolicy::writeBack, false, false, false);
    EXPECT_TRUE(mockClientContext->passedCpuCacheableSettingForGetPatIndexQuery);

    drm.getPatIndex(nullptr, AllocationType::buffer, CacheRegion::defaultRegion, CachePolicy::uncached, false, false, false);
    EXPECT_FALSE(mockClientContext->passedCpuCacheableSettingForGetPatIndexQuery);
}

TEST_F(DrmAllocationTests, givenUncachedCachePolicyAndOverridesSetWhenGettingPatIndexThenUncachedOverrideIsUsed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.OverridePatIndexForUncachedTypes.set(3);
    debugManager.flags.OverridePatIndexForCachedTypes.set(1);

    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
    drm.vmBindPatIndexProgrammingSupported = true;

    EXPECT_EQ(1u, drm.getPatIndex(nullptr, AllocationType::buffer, CacheRegion::defaultRegion, CachePolicy::writeBack, false, false, false));
    EXPECT_EQ(3u, drm.getPatIndex(nullptr, AllocationType::buffer, CacheRegion::defaultRegion, CachePolicy::uncached, false, false, false));
}

TEST_F(DrmAllocationTests, givenBoWhenMarkingForCaptureThenBosAreMarked) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::scratchSurface, MemoryPool::system4KBPages);
    allocation.markForCapture();

    allocation.bufferObjects[0] = &bo;
    allocation.markForCapture();

    EXPECT_TRUE(bo.isMarkedForCapture());
}

TEST_F(DrmAllocationTests, givenUncachedTypeWhenForceOverridePatIndexForUncachedTypesThenForcedByFlagPatIndexIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceAllResourcesUncached.set(1);
    uint64_t patIndex = 8u;
    debugManager.flags.OverridePatIndexForUncachedTypes.set(static_cast<int32_t>(patIndex));

    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);

    EXPECT_EQ(patIndex, drm.getPatIndex(allocation.getDefaultGmm(), allocation.getAllocationType(), CacheRegion::defaultRegion, CachePolicy::writeBack, false, false, false));
}

TEST_F(DrmAllocationTests, givenCachedTypeWhenForceOverridePatIndexForUncachedTypesThenPatIndexIsNotOverrideByFlag) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceAllResourcesUncached.set(0);
    uint64_t patIndex = 8u;
    debugManager.flags.OverridePatIndexForUncachedTypes.set(static_cast<int32_t>(patIndex));

    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);

    EXPECT_NE(patIndex, drm.getPatIndex(allocation.getDefaultGmm(), allocation.getAllocationType(), CacheRegion::defaultRegion, CachePolicy::writeBack, false, false, false));
}

TEST_F(DrmAllocationTests, givenUncachedTypeWhenForceOverridePatIndexForCachedTypesThenPatIndexIsNotOverrideByFlag) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceAllResourcesUncached.set(1);
    uint64_t patIndex = 8u;
    debugManager.flags.OverridePatIndexForCachedTypes.set(static_cast<int32_t>(patIndex));

    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);

    EXPECT_NE(patIndex, drm.getPatIndex(allocation.getDefaultGmm(), allocation.getAllocationType(), CacheRegion::defaultRegion, CachePolicy::writeBack, false, false, false));
}

TEST_F(DrmAllocationTests, givenCachedTypeWhenForceOverridePatIndexForCachedTypesThenForcedByFlagPatIndexIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceAllResourcesUncached.set(0);
    uint64_t patIndex = 8u;
    debugManager.flags.OverridePatIndexForCachedTypes.set(static_cast<int32_t>(patIndex));

    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);

    EXPECT_EQ(patIndex, drm.getPatIndex(allocation.getDefaultGmm(), allocation.getAllocationType(), CacheRegion::defaultRegion, CachePolicy::writeBack, false, false, false));
}

TEST_F(DrmAllocationTests, givenUpstreamDebuggerWhenSetIsaDebugDataHandleCalledThenHandleIsSetAndReturnedCorrectly) {
    const uint32_t rootDeviceIndex = 0u;
    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::kernelIsa, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;
    allocation.bufferObjects[1] = nullptr;

    uint32_t isaDebugDataHandle = 0x1234;
    allocation.setIsaDebugDataHandle(isaDebugDataHandle);
    EXPECT_EQ(isaDebugDataHandle, bo.isaDebugDataHandle);

    EXPECT_EQ(isaDebugDataHandle, bo.getIsaDebugDataHandle());
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmAllocationWithHostPtrWhenItIsCreatedWithCacheRegionThenSetRegionInBufferObject) {

    const auto &hwInfo = *executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
    if (hwInfo.caps.numCacheRegions == 0) {
        GTEST_SKIP();
    }
    mock->ioctlExpected.total = -1;
    auto drm = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel()->as<Drm>());

    CacheReservationParameters l2CacheParameters{};
    CacheReservationParameters l3CacheParameters{};
    l3CacheParameters.maxSize = 32 * MemoryConstants::kiloByte;
    l3CacheParameters.maxNumRegions = 2;
    l3CacheParameters.maxNumWays = 32;
    drm->cacheInfo.reset(new MockCacheInfo(*drm->getIoctlHelper(), l2CacheParameters, l3CacheParameters));

    auto ptr = reinterpret_cast<void *>(0x1000);
    auto size = MemoryConstants::pageSize;

    OsHandleStorage storage;
    storage.fragmentStorageData[0].cpuPtr = ptr;
    storage.fragmentStorageData[0].fragmentSize = 1;
    storage.fragmentCount = 1;

    memoryManager->populateOsHandles(storage, rootDeviceIndex);

    auto allocation = std::make_unique<DrmAllocation>(rootDeviceIndex, 1u /*num gmms*/, AllocationType::bufferHostMemory,
                                                      nullptr, ptr, castToUint64(ptr), size, MemoryPool::system4KBPages);
    allocation->fragmentsStorage = std::make_unique<OsHandleStorage>(storage);

    allocation->setCacheAdvice(drm, 1024, CacheRegion::region1, false);

    for (uint32_t i = 0; i < storage.fragmentCount; i++) {
        auto bo = static_cast<OsHandleLinux *>(allocation->fragmentsStorage->fragmentStorageData[i].osHandleStorage)->bo;
        EXPECT_EQ(CacheRegion::region1, bo->peekCacheRegion());
    }

    storage.fragmentStorageData[0].freeTheFragment = true;
    memoryManager->cleanOsHandles(storage, rootDeviceIndex);
}

HWTEST2_TEMPLATED_F(DrmMemoryManagerTest, givenDrmAllocationWithHostPtrWhenItIsCreatedWithIncorrectCacheRegionThenReturnNull, IsClosUnsupported) {
    mock->ioctlExpected.total = -1;
    auto drm = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel()->as<Drm>());
    drm->setupCacheInfo(*defaultHwInfo.get());

    auto ptr = reinterpret_cast<void *>(0x1000);
    auto size = MemoryConstants::pageSize;

    allocationData.size = size;
    allocationData.hostPtr = ptr;
    allocationData.cacheRegion = 0xFFFF;

    auto allocation = std::unique_ptr<GraphicsAllocation>(memoryManager->allocateGraphicsMemoryWithHostPtr(allocationData));
    EXPECT_EQ(allocation, nullptr);
}

HWTEST2_TEMPLATED_F(DrmMemoryManagerTest, givenDrmAllocationWithWithAlignmentFromUserptrWhenItIsCreatedWithIncorrectCacheRegionThenReturnNull, IsClosUnsupported) {
    mock->ioctlExpected.total = -1;
    auto drm = static_cast<DrmMockCustom *>(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel()->as<Drm>());
    drm->setupCacheInfo(*defaultHwInfo.get());

    auto size = MemoryConstants::pageSize;
    allocationData.size = size;
    allocationData.cacheRegion = 0xFFFF;

    auto allocation = static_cast<DrmAllocation *>(memoryManager->createAllocWithAlignmentFromUserptr(allocationData, size, 0, 0, 0x1000));
    EXPECT_EQ(allocation, nullptr);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenForcePinAndInvalidCacheRegionWhenAllocatingWithAlignmentFromUserptrThenBoIsNotPinned) {
    mock->ioctlExpected.total = -1;

    auto pinningMemoryManager = std::make_unique<TestedDrmMemoryManager>(false, true, false, *executionEnvironment);
    pinningMemoryManager->allRegisteredEngines[rootDeviceIndex] = EngineControlContainer{this->device->allEngines};
    for (auto &engine : pinningMemoryManager->getRegisteredEngines(rootDeviceIndex)) {
        engine.osContext->incRefInternal();
    }
    ASSERT_NE(nullptr, pinningMemoryManager->pinBBs[rootDeviceIndex]);

    allocationData.size = pinningMemoryManager->pinThreshold;
    allocationData.flags.forcePin = true;
    allocationData.cacheRegion = 0xFFFF;

    auto allocation = pinningMemoryManager->createAllocWithAlignmentFromUserptr(allocationData, allocationData.size, MemoryConstants::pageSize, 0, 0x1000);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(0, mock->ioctlCnt.execbuffer2.load());
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenSvmCpuAllocationWhenAllocatingWithAlignmentFailsThenGpuReservationIsReleased) {
    mock->ioctlExpected.total = -1;
    allocationData.size = MemoryConstants::pageSize;
    allocationData.type = AllocationType::svmCpu;
    allocationData.cacheRegion = 0xFFFF;

    auto allocation = memoryManager->allocateGraphicsMemoryWithAlignment(allocationData);

    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(1u, memoryManager->releaseGpuRangeCalled);
}

TEST_F(DrmMemoryManagerWithExplicitExpectationsTest, givenAffinityMaskDeviceWithBitfieldIndex1SetWhenAllocatingDebugSurfaceThenSingleAllocationWithOneBoIsCreated) {
    NEO::debugManager.flags.CreateMultipleSubDevices.set(2);

    AffinityMaskHelper affinityMask;
    affinityMask.enableGenericSubDevice(1);

    executionEnvironment->rootDeviceEnvironments[0]->deviceAffinityMask = affinityMask;
    AllocationProperties debugSurfaceProperties{0, true, MemoryConstants::pageSize, NEO::AllocationType::debugContextSaveArea, false, false, 0b0010};
    auto debugSurface = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithProperties(debugSurfaceProperties));

    EXPECT_NE(nullptr, debugSurface);

    auto mem = debugSurface->getUnderlyingBuffer();
    ASSERT_NE(nullptr, mem);

    EXPECT_EQ(1u, debugSurface->getNumGmms());

    auto &bos = debugSurface->getBOs();
    auto bo = debugSurface->getBO();

    EXPECT_NE(nullptr, bos[0]);
    EXPECT_EQ(nullptr, bos[1]);
    EXPECT_EQ(bos[0], bo);

    EXPECT_EQ(debugSurface->getGpuAddress(), bos[0]->peekAddress());

    auto sipType = SipKernel::getSipKernelType(*device);
    SipKernel::initSipKernel(sipType, *device);

    auto &stateSaveAreaHeader = NEO::SipKernel::getSipKernel(*device, nullptr).getStateSaveAreaHeader();
    mem = ptrOffset(mem, stateSaveAreaHeader.size());
    auto size = debugSurface->getUnderlyingBufferSize() - stateSaveAreaHeader.size();

    EXPECT_TRUE(memoryZeroed(mem, size));

    SipKernel::freeSipKernels(&device->getRootDeviceEnvironmentRef(), device->getMemoryManager());
    memoryManager->freeGraphicsMemory(debugSurface);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, whenWddmMemoryManagerIsCreatedThenAlignmentSelectorHasExpectedAlignments) {
    std::vector<AlignmentSelector::CandidateAlignment> expectedAlignments = {
        {MemoryConstants::pageSize2M, false, AlignmentSelector::anyWastage, HeapIndex::heapStandard2MB},
        {MemoryConstants::pageSize64k, true, AlignmentSelector::anyWastage, HeapIndex::heapStandard64KB},
    };

    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    EXPECT_EQ(expectedAlignments, memoryManager.alignmentSelector.peekCandidateAlignments());
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, whenDebugFlagToNotFreeResourcesIsSpecifiedThenFreeIsNotDoingAnything) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DoNotFreeResources.set(true);
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    size_t sizeIn = 1024llu;
    uint64_t gpuAddress = 0x1337llu;
    auto drmAllocation = std::make_unique<DrmAllocation>(0u, 1u /*num gmms*/, AllocationType::buffer, nullptr, nullptr, gpuAddress, sizeIn, MemoryPool::system64KBPages);
    memoryManager.freeGraphicsMemoryImpl(drmAllocation.get());

    EXPECT_EQ(drmAllocation->getNumGmms(), 1u);
    EXPECT_EQ(drmAllocation->getRootDeviceIndex(), 0u);
    EXPECT_EQ(drmAllocation->getBOs().size(), EngineLimits::maxHandleCount);
    EXPECT_EQ(drmAllocation->getAllocationType(), AllocationType::buffer);
    EXPECT_EQ(drmAllocation->getGpuAddress(), gpuAddress);
    EXPECT_EQ(drmAllocation->getUnderlyingBufferSize(), sizeIn);
    EXPECT_EQ(drmAllocation->getMemoryPool(), MemoryPool::system64KBPages);
    EXPECT_EQ(drmAllocation->getUnderlyingBuffer(), nullptr);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, given2MbPagesDisabledWhenWddmMemoryManagerIsCreatedThenAlignmentSelectorHasExpectedAlignments) {
    DebugManagerStateRestore restore{};
    debugManager.flags.AlignLocalMemoryVaTo2MB.set(0);

    std::vector<AlignmentSelector::CandidateAlignment> expectedAlignments = {
        {MemoryConstants::pageSize64k, true, AlignmentSelector::anyWastage, HeapIndex::heapStandard64KB},
    };

    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    EXPECT_EQ(expectedAlignments, memoryManager.alignmentSelector.peekCandidateAlignments());
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenCustomAlignmentWhenWddmMemoryManagerIsCreatedThenAlignmentSelectorHasExpectedAlignments) {
    DebugManagerStateRestore restore{};

    {
        debugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(MemoryConstants::megaByte);
        std::vector<AlignmentSelector::CandidateAlignment> expectedAlignments = {
            {MemoryConstants::pageSize2M, false, AlignmentSelector::anyWastage, HeapIndex::heapStandard2MB},
            {MemoryConstants::megaByte, true, AlignmentSelector::anyWastage, HeapIndex::heapStandard64KB},
            {MemoryConstants::pageSize64k, true, AlignmentSelector::anyWastage, HeapIndex::heapStandard64KB},
        };
        TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
        EXPECT_EQ(expectedAlignments, memoryManager.alignmentSelector.peekCandidateAlignments());
    }

    {
        debugManager.flags.ExperimentalEnableCustomLocalMemoryAlignment.set(2 * MemoryConstants::pageSize2M);
        std::vector<AlignmentSelector::CandidateAlignment> expectedAlignments = {
            {2 * MemoryConstants::pageSize2M, true, AlignmentSelector::anyWastage, HeapIndex::heapStandard2MB},
            {MemoryConstants::pageSize2M, false, AlignmentSelector::anyWastage, HeapIndex::heapStandard2MB},
            {MemoryConstants::pageSize64k, true, AlignmentSelector::anyWastage, HeapIndex::heapStandard64KB},
        };
        TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
        EXPECT_EQ(expectedAlignments, memoryManager.alignmentSelector.peekCandidateAlignments());
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmManagerWithLocalMemoryWhenGettingGlobalMemoryPercentThenValueDependsOnWhetherLocalMemorySizeIsNetOfKmdReservations) {
    auto mockIoctlHelper = new MockIoctlHelper(*mock);
    mock->ioctlHelper.reset(mockIoctlHelper);

    TestedDrmMemoryManager memoryManager(true, false, false, *executionEnvironment);

    mockIoctlHelper->isLocalMemorySizeNetOfKmdReservationsResult = true;
    EXPECT_EQ(0.98, memoryManager.getPercentOfGlobalMemoryAvailable(rootDeviceIndex));

    mockIoctlHelper->isLocalMemorySizeNetOfKmdReservationsResult = false;
    EXPECT_EQ(0.95, memoryManager.getPercentOfGlobalMemoryAvailable(rootDeviceIndex));
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmManagerWithoutLocalMemoryWhenGettingGlobalMemoryPercentThenValueDoesNotDependOnWhetherLocalMemorySizeIsNetOfKmdReservations) {
    auto mockIoctlHelper = new MockIoctlHelper(*mock);
    mock->ioctlHelper.reset(mockIoctlHelper);

    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);

    for (bool localMemorySizeNetOfKmdReservations : {true, false}) {
        mockIoctlHelper->isLocalMemorySizeNetOfKmdReservationsResult = localMemorySizeNetOfKmdReservations;
        EXPECT_EQ(0.94, memoryManager.getPercentOfGlobalMemoryAvailable(rootDeviceIndex));
    }
}

struct DrmMemoryManagerToTestLockInLocalMemory : public TestedDrmMemoryManager {
    using TestedDrmMemoryManager::lockResourceImpl;
    DrmMemoryManagerToTestLockInLocalMemory(ExecutionEnvironment &executionEnvironment)
        : TestedDrmMemoryManager(true, false, false, executionEnvironment) {}

    void *lockBufferObject(BufferObject *bo) override {
        lockedLocalMemory.reset(new uint8_t[bo->peekSize()]);
        return lockedLocalMemory.get();
    }
    std::unique_ptr<uint8_t[]> lockedLocalMemory;
};

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmManagerWithLocalMemoryWhenLockResourceIsCalledOnWriteCombinedAllocationThenReturnPtrAlignedTo64Kb) {
    DrmMemoryManagerToTestLockInLocalMemory memoryManager(*executionEnvironment);
    BufferObject bo(rootDeviceIndex, mock, 3, 1, 1024, 0);

    DrmAllocation drmAllocation(0, 1u /*num gmms*/, AllocationType::writeCombined, &bo, nullptr, 0u, 0u, MemoryPool::localMemory);
    EXPECT_EQ(&bo, drmAllocation.getBO());

    auto ptr = memoryManager.lockResourceImpl(drmAllocation);
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(ptr, bo.peekLockedAddress());
    EXPECT_TRUE(isAligned<MemoryConstants::pageSize64k>(ptr));

    memoryManager.unlockBufferObject(&bo);
    EXPECT_EQ(nullptr, bo.peekLockedAddress());
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWithoutLocalMemoryWhenCopyMemoryToAllocationThenAllocationIsFilledWithCorrectData) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    std::vector<uint8_t> dataToCopy(MemoryConstants::pageSize, 1u);

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties({rootDeviceIndex, dataToCopy.size(), AllocationType::buffer, device->getDeviceBitfield()});
    ASSERT_NE(nullptr, allocation);

    auto ret = memoryManager.copyMemoryToAllocation(allocation, 0, dataToCopy.data(), dataToCopy.size());
    EXPECT_TRUE(ret);

    EXPECT_EQ(0, memcmp(allocation->getUnderlyingBuffer(), dataToCopy.data(), dataToCopy.size()));

    memoryManager.freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWithoutLocalMemoryAndCpuPtrWhenCopyMemoryToAllocationThenReturnFalse) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    std::vector<uint8_t> dataToCopy(MemoryConstants::pageSize, 1u);

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties({rootDeviceIndex, dataToCopy.size(), AllocationType::buffer, device->getDeviceBitfield()});
    ASSERT_NE(nullptr, allocation);
    allocation->setCpuPtrAndGpuAddress(nullptr, 0u);
    auto ret = memoryManager.copyMemoryToAllocation(allocation, 0, dataToCopy.data(), dataToCopy.size());
    EXPECT_FALSE(ret);

    memoryManager.freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWithoutLocalMemoryAndCpuPtrWhenMemsetAllocationThenReturnFalse) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    auto allocation = memoryManager.allocateGraphicsMemoryWithProperties({rootDeviceIndex, MemoryConstants::pageSize, AllocationType::buffer, device->getDeviceBitfield()});
    ASSERT_NE(nullptr, allocation);
    allocation->setCpuPtrAndGpuAddress(nullptr, 0u);
    auto ret = memoryManager.memsetAllocation(allocation, 0, 2, MemoryConstants::pageSize);
    EXPECT_FALSE(ret);

    memoryManager.freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenNullDefaultAllocWhenCreateGraphicsAllocationFromExistingStorageThenDoNotImportHandle) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    mock->ioctlExpected.primeFdToHandle = 0;

    MockAllocationProperties properties(0u, 1u);
    MultiGraphicsAllocation allocation(0u);
    auto alloc = memoryManager.createGraphicsAllocationFromExistingStorage(properties, nullptr, allocation);

    EXPECT_NE(alloc, nullptr);

    memoryManager.freeGraphicsMemory(alloc);
}

TEST(DrmMemoryManagerSimpleTest, givenDrmMemoryManagerWhenAllocateInDevicePoolIsCalledThenNullptrAndStatusRetryIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
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

TEST(DrmMemoryManagerSimpleTest, givenDrmMemoryManagerWhenDeviceHeapIsDepletedThenNullptrReturnedAndStatusIsError) {
    constexpr size_t reservedCpuAddressRangeSize = is64bit ? (6 * 4 * MemoryConstants::gigaByte) : 0;

    auto hwInfo = defaultHwInfo.get();
    auto executionEnvironment = MockExecutionEnvironment{hwInfo};
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);

    auto mockGfxPartition = std::make_unique<MockGfxPartition>();
    mockGfxPartition->init(hwInfo->capabilityTable.gpuAddressSpace, reservedCpuAddressRangeSize, 0, 1, false, 0u, hwInfo->capabilityTable.gpuAddressSpace + 1, nullptr);

    auto status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.size = MemoryConstants::pageSize + mockGfxPartition->heapGetLimit(NEO::HeapIndex::heapInternalDeviceMemory);
    allocData.flags.useSystemMemory = false;
    allocData.flags.allocateMemory = true;
    allocData.type = AllocationType::kernelIsa;

    auto memoryManager = TestedDrmMemoryManager{true, false, false, executionEnvironment};
    memoryManager.overrideGfxPartition(mockGfxPartition.release());
    auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);

    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);
}

TEST(DrmMemoryManagerSimpleTest, givenDrmMemoryManagerWhenAllocateInLocalDeviceMemoryIsCalledThenNullptrAndStatusRetryIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    TestedDrmMemoryManager memoryManager(executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.useSystemMemory = true;
    allocData.flags.allocateMemory = true;

    auto allocation = memoryManager.allocatePhysicalLocalDeviceMemory(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);
}

TEST(DrmMemoryManagerSimpleTest, givenDrmMemoryManagerWhenLockResourceIsCalledOnNullBufferObjectThenReturnNullPtr) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    TestedDrmMemoryManager memoryManager(executionEnvironment);
    DrmAllocation drmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, nullptr, 0u, 0u, MemoryPool::localMemory);

    auto ptr = memoryManager.lockBufferObject(drmAllocation.getBO());
    EXPECT_EQ(nullptr, ptr);

    memoryManager.unlockBufferObject(drmAllocation.getBO());
}

TEST(DrmMemoryManagerSimpleTest, givenDrmMemoryManagerWhenFreeGraphicsMemoryIsCalledOnAllocationWithNullBufferObjectThenEarlyReturn) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[0]);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));
    executionEnvironment.rootDeviceEnvironments[0]->memoryOperationsInterface = DrmMemoryOperationsHandler::create(*drm, 0u, false);
    TestedDrmMemoryManager memoryManager(executionEnvironment);

    auto drmAllocation = new DrmAllocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, nullptr, 0u, 0u, MemoryPool::localMemory);
    EXPECT_NE(nullptr, drmAllocation);

    memoryManager.freeGraphicsMemoryImpl(drmAllocation);
}

TEST(DrmMemoryManagerSimpleTest, WhenDrmIsCreatedThenQueryPageFaultSupportIsCalled) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto drm = std::unique_ptr<Drm>(Drm::create(nullptr, *executionEnvironment.rootDeviceEnvironments[0]));

    EXPECT_TRUE(static_cast<DrmMock *>(drm.get())->queryPageFaultSupportCalled);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenDrmMemoryManagerWithoutLocalMemoryWhenPreferCompressedIsSetThenLocalOnlyRequriedDeterminedByReleaseHelper) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);

    AllocationProperties properties{1, true, 4096, AllocationType::unknown, false, 0b10};
    properties.flags.preferCompressed = true;
    auto storageInfo = memoryManager.createStorageInfoFromProperties(properties);

    const auto &hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    EXPECT_EQ(storageInfo.localOnlyRequired, hwInfo.caps.localOnlyAllowed);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenDrmMemoryManagerWithLocalMemoryWhenLockResourceIsCalledOnAllocationInLocalMemoryThenReturnNullPtr) {
    DrmAllocation drmAllocation(rootDeviceIndex, 1u /*num gmms*/, AllocationType::unknown, nullptr, nullptr, 0u, 0u, MemoryPool::localMemory);

    auto ptr = memoryManager->lockResource(&drmAllocation);
    EXPECT_EQ(nullptr, ptr);

    memoryManager->unlockResource(&drmAllocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenSingleLocalMemoryWhenParticularSubdeviceIndicatedThenCorrectBankIsSelected) {
    auto *memoryInfo = static_cast<MockMemoryInfo *>(mock->memoryInfo.get());
    auto &localMemoryRegions = memoryInfo->localMemoryRegions;
    localMemoryRegions.resize(1U);
    localMemoryRegions[0].tilesMask = 0b11;

    AllocationProperties properties{1, true, 4096, AllocationType::buffer, false, {}};
    properties.subDevicesBitfield = 0b10;

    memoryManager->computeStorageInfoMemoryBanksCalled = 0U;
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);

    constexpr auto expectedMemoryBanks = 0b01;
    EXPECT_EQ(storageInfo.memoryBanks, expectedMemoryBanks);
    EXPECT_EQ(memoryManager->computeStorageInfoMemoryBanksCalled, 2UL);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenSingleLocalMemoryAndNonTileInstancedAllocationWhenAllTilesIndicatedThenCorrectBankIsSelected) {
    auto *memoryInfo = static_cast<MockMemoryInfo *>(mock->memoryInfo.get());
    auto &localMemoryRegions = memoryInfo->localMemoryRegions;
    localMemoryRegions.resize(1U);
    localMemoryRegions[0].tilesMask = 0b11;

    AllocationProperties properties{1, true, 4096, AllocationType::buffer, false, {}};
    properties.subDevicesBitfield = 0b11;

    memoryManager->computeStorageInfoMemoryBanksCalled = 0U;
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);

    constexpr auto expectedMemoryBanks = 0b01;
    EXPECT_EQ(storageInfo.memoryBanks, expectedMemoryBanks);
    EXPECT_EQ(memoryManager->computeStorageInfoMemoryBanksCalled, 2UL);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenSingleLocalMemoryWhenTileInstancedAllocationCreatedThenMemoryBanksSetToAllTilesRegardlessOfSubDevicesIndicatedInProperties) {
    auto *memoryInfo = static_cast<MockMemoryInfo *>(mock->memoryInfo.get());
    auto &localMemoryRegions = memoryInfo->localMemoryRegions;
    debugManager.flags.CreateMultipleSubDevices.set(2);
    localMemoryRegions.resize(1U);
    localMemoryRegions[0].tilesMask = 0b11;

    const auto expectedMemoryBanks = 0b11;
    for (auto subDevicesMask = 1U; subDevicesMask < 4; ++subDevicesMask) {
        AllocationProperties properties{1, 4096, AllocationType::workPartitionSurface, subDevicesMask};

        memoryManager->computeStorageInfoMemoryBanksCalled = 0U;
        auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);

        EXPECT_TRUE(storageInfo.tileInstanced);
        EXPECT_EQ(storageInfo.memoryBanks, expectedMemoryBanks);
        EXPECT_EQ(memoryManager->computeStorageInfoMemoryBanksCalled, 2UL);
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenSingleLocalMemoryAndTileInstancedAllocationTypeEvenWhenSubsetOfTilesIndicatedThenCorrectBankIsSelected) {
    auto *memoryInfo = static_cast<MockMemoryInfo *>(mock->memoryInfo.get());
    auto &localMemoryRegions = memoryInfo->localMemoryRegions;
    debugManager.flags.CreateMultipleSubDevices.set(2);

    localMemoryRegions.resize(1U);
    localMemoryRegions[0].tilesMask = 0b11;

    AllocationProperties properties{1, true, 4096, AllocationType::workPartitionSurface, false, 0b10};
    const auto expectedMemoryBanks = 0b11;

    memoryManager->computeStorageInfoMemoryBanksCalled = 0U;
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);

    EXPECT_EQ(storageInfo.memoryBanks, expectedMemoryBanks);
    EXPECT_EQ(memoryManager->computeStorageInfoMemoryBanksCalled, 2UL);
}

TEST_F(DrmMemoryManagerWithLocalMemoryAndExplicitExpectationsTest, givenSingleLocalMemoryWhenTileInstancedAllocationCreatedThenItHasCorrectBOs) {
    auto *memoryInfo = static_cast<MockMemoryInfo *>(mock->memoryInfo.get());
    auto &localMemoryRegions = memoryInfo->localMemoryRegions;
    localMemoryRegions.resize(1U);
    localMemoryRegions[0].tilesMask = 0b11;

    const DeviceBitfield subDeviceBitfield{0b11};
    const auto expectedNumHandles{subDeviceBitfield.count()};
    EXPECT_NE(expectedNumHandles, 0UL);

    AllocationProperties properties{1, 4096, AllocationType::workPartitionSurface, subDeviceBitfield};
    auto *allocation{memoryManager->allocateGraphicsMemoryInPreferredPool(properties, nullptr)};
    EXPECT_EQ(allocation->getNumHandles(), expectedNumHandles);

    const auto &bos{static_cast<DrmAllocation *>(allocation)->getBOs()};
    const auto commonBoAddress{bos[0U]->peekAddress()};
    auto numberOfValidBos{0U};
    for (const auto bo : bos) {
        if (bo == nullptr) {
            continue;
        }
        ++numberOfValidBos;
        EXPECT_EQ(bo->peekAddress(), commonBoAddress);
    }
    EXPECT_EQ(numberOfValidBos, expectedNumHandles);
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenMultipleLocalMemoryRegionsWhenParticularSubdeviceIndicatedThenItIsSelected) {
    auto *memoryInfo = static_cast<MockMemoryInfo *>(mock->memoryInfo.get());
    auto &localMemoryRegions = memoryInfo->localMemoryRegions;
    constexpr auto leastOccupiedBankBitPosition = 1u;
    debugManager.flags.OverrideLeastOccupiedBank.set(leastOccupiedBankBitPosition);

    localMemoryRegions.resize(2U);
    localMemoryRegions[0].tilesMask = 0b01;
    localMemoryRegions[1].tilesMask = 0b10;

    AllocationProperties properties{1, true, 4096, AllocationType::buffer, false, {}};
    properties.subDevicesBitfield = 0b10;
    const auto expectedMemoryBanks = properties.subDevicesBitfield;

    memoryManager->computeStorageInfoMemoryBanksCalled = 0U;
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);

    EXPECT_EQ(storageInfo.memoryBanks, expectedMemoryBanks);
    EXPECT_EQ(memoryManager->computeStorageInfoMemoryBanksCalled, 2UL);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, givenMultipleLocalMemoryRegionsWhenAllSubdevicesIndicatedThenTheLeastOccuppiedBankIsSelected) {
    auto *memoryInfo = static_cast<MockMemoryInfo *>(mock->memoryInfo.get());
    auto &localMemoryRegions = memoryInfo->localMemoryRegions;
    constexpr auto leastOccupiedBankBitPosition = 1u;
    debugManager.flags.OverrideLeastOccupiedBank.set(leastOccupiedBankBitPosition);

    localMemoryRegions.resize(2U);
    localMemoryRegions[0].tilesMask = 0b01;
    localMemoryRegions[1].tilesMask = 0b10;

    AllocationProperties properties{1, true, 4096, AllocationType::buffer, false, {}};
    properties.subDevicesBitfield = 0b11;

    const auto expectedMemoryBanks = memoryManager->createStorageInfoFromPropertiesGeneric(properties).memoryBanks;

    memoryManager->computeStorageInfoMemoryBanksCalled = 0U;
    auto storageInfo = memoryManager->createStorageInfoFromProperties(properties);

    EXPECT_EQ(storageInfo.memoryBanks, expectedMemoryBanks);
    EXPECT_EQ(memoryManager->computeStorageInfoMemoryBanksCalled, 2UL);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerWithLocalMemoryTest, Given2MBLocalMemAlignmentEnabledWhenAllocatingSvmCpuAllocLargerThan2MBThenAlignStorageSizeTo2MB) {
    auto mockProductHelper = new MockProductHelper;
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper.reset(mockProductHelper);
    mockProductHelper->is2MBLocalMemAlignmentEnabledResult = true;

    ASSERT_TRUE(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->productHelper->is2MBLocalMemAlignmentEnabled());

    mock->ioctlExpected.gemCreateExt = 1;
    mock->ioctlExpected.gemMmapOffset = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    AllocationData allocationData;
    allocationData.size = 2 * MemoryConstants::megaByte + 512 * MemoryConstants::kiloByte;
    allocationData.alignment = MemoryConstants::pageSize64k;
    allocationData.type = AllocationType::svmCpu;
    allocationData.makeGPUVaDifferentThanCPUPtr = true;
    allocationData.rootDeviceIndex = rootDeviceIndex;

    const size_t expectedAllocSize = alignUp(allocationData.size, MemoryConstants::pageSize2M);

    auto allocation = static_cast<DrmAllocation *>(memoryManager->allocateGraphicsMemoryWithAlignment(allocationData));
    ASSERT_NE(nullptr, allocation);

    EXPECT_EQ(AllocationType::svmCpu, allocation->getAllocationType());

    EXPECT_EQ(expectedAllocSize, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(expectedAllocSize, allocation->getMmapSize());

    EXPECT_NE(nullptr, allocation->getUnderlyingBuffer());
    EXPECT_EQ(allocation->getMmapPtr(), allocation->getUnderlyingBuffer());
    EXPECT_EQ(nullptr, allocation->getDriverAllocatedCpuPtr());

    EXPECT_NE(0llu, allocation->getGpuAddress());
    EXPECT_NE(reinterpret_cast<uint64_t>(allocation->getUnderlyingBuffer()), allocation->getGpuAddress());

    auto bo = allocation->getBO();
    ASSERT_NE(nullptr, bo);

    EXPECT_NE(0llu, bo->peekAddress());

    auto gmmHelper = device->getGmmHelper();
    EXPECT_LT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(HeapIndex::heapStandard)), bo->peekAddress());
    EXPECT_GT(gmmHelper->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapLimit(HeapIndex::heapStandard)), bo->peekAddress());

    EXPECT_EQ(reinterpret_cast<void *>(allocation->getGpuAddress()), alignUp(allocation->getReservedAddressPtr(), MemoryConstants::pageSize2M));
    EXPECT_EQ(expectedAllocSize + MemoryConstants::pageSize2M, allocation->getReservedAddressSize());
    EXPECT_GT(allocation->getReservedAddressSize(), bo->peekSize());

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenIsaAllocationTypeWhenAllocating32BitWithoutHostPtrThenKmdMappedBoWithNoCompressionHintIsUsedInsteadOfUserptr) {
    DebugManagerStateRestore dbgStateRestore;
    debugManager.flags.UseGemCreateExtInAllocateMemoryByKMD.set(1);
    debugManager.flags.UseKmdAllocationForIsa.set(1);

    auto memoryInfo = installMockedMemoryInfo(*mock);

    setAnonymousMmapFunctions(*memoryManager);
    mock->reset();
    mock->ioctlExpected.gemMmapOffset = 2;
    mock->ioctlExpected.gemWait = 2;
    mock->ioctlExpected.gemClose = 2;

    constexpr size_t size = MemoryConstants::pageSize;
    const auto hwInfo = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
    const auto expectedAllocationSize = alignUp(size + device->getGfxCoreHelper().getPaddingForISAAllocation(), MemoryConstants::pageSize);

    for (auto allocationType : {AllocationType::kernelIsa, AllocationType::kernelIsaInternal}) {
        const auto heapIndex = memoryManager->heapAssigners[rootDeviceIndex]->get32BitHeapIndex(allocationType, false, *hwInfo, false);
        const auto expectedGpuBaseAddress = device->getGmmHelper()->canonize(memoryManager->getGfxPartition(rootDeviceIndex)->getHeapBase(heapIndex));
        const auto gemMmapOffsetCount = mock->ioctlCnt.gemMmapOffset.load();
        memoryInfo->receivedGemCreateExtHint = GemCreateExtHint::none;

        auto allocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, size, nullptr, allocationType);

        ASSERT_NE(nullptr, allocation);
        // KMD backed ISA has to skip the CCS metadata and its blitter based clear
        EXPECT_EQ(GemCreateExtHint::noCompression, memoryInfo->receivedGemCreateExtHint);
        EXPECT_EQ(0, mock->ioctlCnt.gemCreate.load());
        EXPECT_EQ(0, mock->ioctlCnt.gemUserptr.load());
        EXPECT_EQ(gemMmapOffsetCount + 1, mock->ioctlCnt.gemMmapOffset.load());
        EXPECT_EQ(0u, allocation->getBO()->getUserptr());
        EXPECT_TRUE(allocation->is32BitAllocation());
        EXPECT_EQ(MemoryPool::system4KBPagesWith32BitGpuAddressing, allocation->getMemoryPool());
        EXPECT_EQ(expectedGpuBaseAddress, allocation->getGpuBaseAddress());
        // the ISA is uploaded by the CPU through the persistent mmap, so it has to stay cacheable and coherent
        ASSERT_NE(nullptr, allocation->getDefaultGmm());
        EXPECT_EQ(GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER, allocation->getDefaultGmm()->getResourceUsageType());
        EXPECT_FALSE(allocation->getDefaultGmm()->isCompressionEnabled());
        EXPECT_TRUE(allocation->getDefaultGmm()->gmmResourceInfo->isCacheable());
        EXPECT_TRUE(allocation->isAllocationLockable());
        const auto isCoherent = device->getProductHelper().isCoherentAllocation(allocation->getBO()->peekPatIndex());
        EXPECT_FALSE(isCoherent.has_value() && !isCoherent.value());
        EXPECT_EQ(allocation->getUnderlyingBuffer(), allocation->getMmapPtr());
        EXPECT_EQ(nullptr, allocation->getDriverAllocatedCpuPtr());
        EXPECT_EQ(expectedAllocationSize, allocation->getUnderlyingBufferSize());
        EXPECT_EQ(expectedAllocationSize, allocation->getMmapSize());
        EXPECT_EQ(expectedAllocationSize, allocation->getBO()->peekSize());
        EXPECT_GE(allocation->getReservedAddressSize(), expectedAllocationSize);
        std::array<uint8_t, 16> sourceData{};
        sourceData.fill(0xab);
        EXPECT_TRUE(memoryManager->copyMemoryToAllocation(allocation, 0, sourceData.data(), sourceData.size()));
        EXPECT_EQ(0, std::memcmp(sourceData.data(), allocation->getUnderlyingBuffer(), sourceData.size()));
        memoryManager->freeGraphicsMemory(allocation);
    }
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenKmdAllocationForIsaDisabledWhenAllocatingIsaThenUserptrIsUsed) {
    DebugManagerStateRestore dbgStateRestore;
    debugManager.flags.UseKmdAllocationForIsa.set(0);
    mock->reset();
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    auto allocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, MemoryConstants::pageSize, nullptr, AllocationType::kernelIsa);

    ASSERT_NE(nullptr, allocation);
    EXPECT_NE(0u, allocation->getBO()->getUserptr());
    EXPECT_NE(nullptr, allocation->getDriverAllocatedCpuPtr());
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenKernelIsaInternalWhenAllocatingFromForcedLargerReservedRangeThenBackingSizeRemainsRequestedSize) {
    DebugManagerStateRestore dbgStateRestore;
    debugManager.flags.UseKmdAllocationForIsa.set(1);

    struct ReservedSizeGfxPartition : public MockGfxPartition {
        uint64_t heapAllocate(HeapIndex heapIndex, size_t &size) override {
            lastHeapIndex = heapIndex;
            requestedAllocationSize = size;
            size = forcedReservedSize;
            return forcedGpuVa;
        }

        void freeGpuAddressRange(uint64_t gpuAddress, size_t size) override {
            freeGpuAddressRangeCalled++;
            freedGpuAddress = gpuAddress;
            freedSize = size;
        }

        size_t forcedReservedSize = 0;
        size_t requestedAllocationSize = 0;
        uint64_t forcedGpuVa = 0x200000;
        uint64_t freedGpuAddress = 0;
        size_t freedSize = 0;
        uint32_t freeGpuAddressRangeCalled = 0u;
        HeapIndex lastHeapIndex = HeapIndex::totalHeaps;
    };

    installMockedMemoryInfo(*mock);
    setAnonymousMmapFunctions(*memoryManager);
    mock->reset();
    mock->ioctlExpected.gemMmapOffset = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    constexpr size_t isaSize = MemoryConstants::pageSize;
    const auto hwInfo = executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
    const auto backingSize = alignUp(isaSize + device->getGfxCoreHelper().getPaddingForISAAllocation(), MemoryConstants::pageSize);
    const auto reservedSize = backingSize + MemoryConstants::pageSize;
    const auto heapIndex = memoryManager->heapAssigners[rootDeviceIndex]->get32BitHeapIndex(AllocationType::kernelIsaInternal, false, *hwInfo, false);

    auto gfxPartition = std::make_unique<ReservedSizeGfxPartition>();
    gfxPartition->forcedReservedSize = reservedSize;
    gfxPartition->initHeap(heapIndex, gfxPartition->forcedGpuVa, 0x100000, MemoryConstants::pageSize);
    auto gfxPartitionPtr = gfxPartition.get();
    NonCopyableVariableBackup<std::unique_ptr<GfxPartition>> gfxPartitionBackup(&memoryManager->gfxPartitions[rootDeviceIndex], std::move(gfxPartition));

    auto allocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, isaSize, nullptr, AllocationType::kernelIsaInternal);

    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(heapIndex, gfxPartitionPtr->lastHeapIndex);
    EXPECT_EQ(backingSize, gfxPartitionPtr->requestedAllocationSize);
    EXPECT_EQ(0, mock->ioctlCnt.gemCreate.load());
    EXPECT_EQ(0, mock->ioctlCnt.gemUserptr.load());
    EXPECT_EQ(1, mock->ioctlCnt.gemMmapOffset.load());
    EXPECT_EQ(nullptr, allocation->getDriverAllocatedCpuPtr());
    EXPECT_EQ(backingSize, allocation->getUnderlyingBufferSize());
    EXPECT_EQ(backingSize, allocation->getMmapSize());
    EXPECT_EQ(backingSize, allocation->getBO()->peekSize());
    EXPECT_EQ(0u, allocation->getBO()->getUserptr());
    EXPECT_EQ(reservedSize, allocation->getReservedAddressSize());
    EXPECT_GT(allocation->getReservedAddressSize(), allocation->getUnderlyingBufferSize());
    memoryManager->freeGraphicsMemory(allocation);
    EXPECT_EQ(1u, gfxPartitionPtr->freeGpuAddressRangeCalled);
    EXPECT_EQ(gfxPartitionPtr->forcedGpuVa, gfxPartitionPtr->freedGpuAddress);
    EXPECT_EQ(reservedSize, gfxPartitionPtr->freedSize);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenKmdMappedIsaFailingWhenAllocating32BitIsaThenGpuRangeIsReleasedAndUserptrIsUsedInstead) {
    DebugManagerStateRestore dbgStateRestore;
    debugManager.flags.UseKmdAllocationForIsa.set(1);

    struct FreeTrackingGfxPartition : public MockGfxPartition {
        FreeTrackingGfxPartition() {
            callHeapAllocate = false;
        }

        void heapFree(HeapIndex, uint64_t, size_t size) override {
            heapFreeCalled++;
            freedSize = size;
        }

        size_t freedSize = 0;
        uint32_t heapFreeCalled = 0u;
    };

    auto memoryInfo = installMockedMemoryInfo(*mock);
    setAnonymousMmapFunctions(*memoryManager);

    // the KMD backed ISA is an optimization over userptr, so a failure at any of its steps has to
    // leave a working userptr allocation behind instead of failing the whole allocation
    for (const auto failingStep : {KmdIsaFailingStep::gemCreateExt, KmdIsaFailingStep::mmapOffset}) {
        auto gfxPartition = std::make_unique<FreeTrackingGfxPartition>();
        auto gfxPartitionPtr = gfxPartition.get();
        NonCopyableVariableBackup<std::unique_ptr<GfxPartition>> gfxPartitionBackup(&memoryManager->gfxPartitions[rootDeviceIndex], std::move(gfxPartition));

        memoryInfo->failOnCreateGemExtWithSingleRegion = (failingStep == KmdIsaFailingStep::gemCreateExt);
        VariableBackup<bool> failOnMmapOffsetBackup(&mock->failOnMmapOffset, failingStep == KmdIsaFailingStep::mmapOffset);
        mock->reset();

        auto allocation = memoryManager->allocate32BitGraphicsMemory(rootDeviceIndex, MemoryConstants::pageSize, nullptr, AllocationType::kernelIsaInternal);

        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(1u, gfxPartitionPtr->heapFreeCalled);
        EXPECT_EQ(allocation->getUnderlyingBufferSize(), gfxPartitionPtr->freedSize);
        EXPECT_EQ(1, mock->ioctlCnt.gemUserptr.load());
        EXPECT_NE(0u, allocation->getBO()->getUserptr());
        EXPECT_NE(nullptr, allocation->getDriverAllocatedCpuPtr());
        EXPECT_EQ(nullptr, allocation->getMmapPtr());
        EXPECT_TRUE(allocation->is32BitAllocation());
        EXPECT_EQ(MemoryPool::system4KBPagesWith32BitGpuAddressing, allocation->getMemoryPool());
        std::array<uint8_t, 16> sourceData{};
        sourceData.fill(0xcd);
        EXPECT_TRUE(memoryManager->copyMemoryToAllocation(allocation, 0, sourceData.data(), sourceData.size()));
        EXPECT_EQ(0, std::memcmp(sourceData.data(), allocation->getUnderlyingBuffer(), sourceData.size()));
        memoryManager->freeGraphicsMemory(allocation);
    }
    mock->reset();
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenCopyMemoryToAllocationThenAllocationIsFilledWithCorrectData) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    std::vector<uint8_t> dataToCopy(MemoryConstants::pageSize, 1u);

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties({rootDeviceIndex, dataToCopy.size(), AllocationType::buffer, device->getDeviceBitfield()});
    ASSERT_NE(nullptr, allocation);

    auto ret = memoryManager->copyMemoryToAllocation(allocation, 0, dataToCopy.data(), dataToCopy.size());
    EXPECT_TRUE(ret);

    EXPECT_EQ(0, memcmp(allocation->getUnderlyingBuffer(), dataToCopy.data(), dataToCopy.size()));

    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenFreeingImportedMemoryThenCloseSharedHandleIsNotCalled) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    std::vector<uint8_t> dataToCopy(MemoryConstants::pageSize, 1u);

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties({rootDeviceIndex, dataToCopy.size(), AllocationType::buffer, device->getDeviceBitfield()});
    ASSERT_NE(nullptr, allocation);

    memoryManager->freeGraphicsMemory(allocation, true);

    EXPECT_EQ(memoryManager->callsToCloseSharedHandle, 0u);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenFreeingNonImportedMemoryThenCloseSharedHandleIsCalled) {
    mock->ioctlExpected.gemUserptr = 1;
    mock->ioctlExpected.gemWait = 1;
    mock->ioctlExpected.gemClose = 1;

    std::vector<uint8_t> dataToCopy(MemoryConstants::pageSize, 1u);

    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties({rootDeviceIndex, dataToCopy.size(), AllocationType::buffer, device->getDeviceBitfield()});
    ASSERT_NE(nullptr, allocation);

    memoryManager->freeGraphicsMemory(allocation);

    EXPECT_EQ(memoryManager->callsToCloseSharedHandle, 1u);
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenGetLocalMemoryIsCalledThenSizeOfLocalMemoryIsReturned) {
    EXPECT_EQ(0 * MemoryConstants::gigaByte, memoryManager->getLocalMemorySize(rootDeviceIndex, 0xF));
}

HWTEST_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSetMemAdviseIsCalledThenUpdateCachePolicyInBufferObject) {
    TestedDrmMemoryManager memoryManager(false, false, false, *executionEnvironment);
    BufferObject bo(rootDeviceIndex, mock, 3, 1, 1024, 0);

    DrmAllocation drmAllocation(rootDeviceIndex, 1u /*num gmms*/, AllocationType::unifiedSharedMemory, &bo, nullptr, 0u, 0u, MemoryPool::localMemory);
    EXPECT_EQ(&bo, drmAllocation.getBO());

    for (auto isCached : {false, true}) {
        MemAdviseFlags flags{};
        flags.cachedMemory = isCached;

        EXPECT_TRUE(memoryManager.setMemAdvise(&drmAllocation, flags, rootDeviceIndex));
        EXPECT_EQ(isCached ? CachePolicy::writeBack : CachePolicy::uncached, bo.peekCachePolicy());
    }
}

HWTEST2_TEMPLATED_F(DrmMemoryManagerTest, givenDrmMemoryManagerWhenSetSharedSystemMemAdviseIsCalledThenAdviceSentToIoctlHelper, IsBMG) {

    TestedDrmMemoryManager memoryManager(*executionEnvironment);

    auto device0 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 0u));
    auto device1 = std::unique_ptr<MockDevice>(Device::create<MockDevice>(executionEnvironment, 1u));

    class MyMockIoctlHelper : public MockIoctlHelper {
        using MockIoctlHelper::MockIoctlHelper;

      public:
        bool setVmSharedSystemMemAdvise(uint64_t handle, const size_t size, const uint32_t attribute, const uint64_t param, const StackVec<uint32_t, 2> &vmIds, uint32_t numSubDevices) override {
            setVmSharedSystemMemAdviseCalled++;
            numSubDevicesCalled = numSubDevices;
            return true;
        }
        uint32_t setVmSharedSystemMemAdviseCalled = 0;
        uint32_t numSubDevicesCalled = 0;
    };

    auto mockIoctlHelper = new MyMockIoctlHelper(*mock);

    std::vector<MemoryRegion> regionInfo{
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 2}, 1024, 0}};

    auto &drm0 = static_cast<DrmMockCustom &>(memoryManager.getDrm(device0->getRootDeviceIndex()));
    auto &drm1 = static_cast<DrmMockCustom &>(memoryManager.getDrm(device1->getRootDeviceIndex()));

    drm0.memoryInfo.reset(new MemoryInfo(regionInfo, drm0));
    drm1.memoryInfo.reset(new MemoryInfo(regionInfo, drm1));
    drm0.ioctlHelper.reset(mockIoctlHelper);
    drm0.ioctlExpected.total = -1;
    drm1.ioctlExpected.total = -1;

    MemAdvise memAdviseOp = MemAdvise::setPreferredLocation;
    EXPECT_TRUE(memoryManager.setSharedSystemMemAdvise(nullptr, 0u, memAdviseOp, *device0.get(), *device1.get()));
    EXPECT_EQ(1u, mockIoctlHelper->setVmSharedSystemMemAdviseCalled);
    EXPECT_EQ(1u, mockIoctlHelper->numSubDevicesCalled);
}

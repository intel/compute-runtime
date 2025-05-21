/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/fixtures/memory_manager_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_host_ptr_manager.h"
#include "shared/test/common/mocks/mock_internal_allocation_storage.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

struct HostPtrManagerTest : ::testing::Test {
    const uint32_t rootDeviceIndex = 1u;
};

TEST_F(HostPtrManagerTest, GivenAlignedPointerAndAlignedSizeWhenGettingAllocationRequirementsThenOneFragmentIsReturned) {
    auto size = MemoryConstants::pageSize * 10;
    void *ptr = (void *)0x1000;

    AllocationRequirements reqs = MockHostPtrManager::getAllocationRequirements(rootDeviceIndex, ptr, size);

    EXPECT_EQ(1u, reqs.requiredFragmentsCount);
    EXPECT_EQ(reqs.allocationFragments[0].fragmentPosition, FragmentPosition::middle);
    EXPECT_EQ(reqs.allocationFragments[1].fragmentPosition, FragmentPosition::none);
    EXPECT_EQ(reqs.allocationFragments[2].fragmentPosition, FragmentPosition::none);

    EXPECT_EQ(reqs.totalRequiredSize, size);

    EXPECT_EQ(ptr, reqs.allocationFragments[0].allocationPtr);
    EXPECT_EQ(size, reqs.allocationFragments[0].allocationSize);

    EXPECT_EQ(nullptr, reqs.allocationFragments[1].allocationPtr);
    EXPECT_EQ(nullptr, reqs.allocationFragments[2].allocationPtr);

    EXPECT_EQ(rootDeviceIndex, reqs.rootDeviceIndex);
}

TEST_F(HostPtrManagerTest, GivenAlignedPointerAndNotAlignedSizeWhenGettingAllocationRequirementsThenTwoFragmentsAreReturned) {
    auto size = MemoryConstants::pageSize * 10 - 1;
    void *ptr = (void *)0x1000;

    AllocationRequirements reqs = MockHostPtrManager::getAllocationRequirements(rootDeviceIndex, ptr, size);
    EXPECT_EQ(2u, reqs.requiredFragmentsCount);

    EXPECT_EQ(reqs.allocationFragments[0].fragmentPosition, FragmentPosition::middle);
    EXPECT_EQ(reqs.allocationFragments[1].fragmentPosition, FragmentPosition::trailing);
    EXPECT_EQ(reqs.allocationFragments[2].fragmentPosition, FragmentPosition::none);
    EXPECT_EQ(reqs.totalRequiredSize, alignUp(size, MemoryConstants::pageSize));

    EXPECT_EQ(ptr, reqs.allocationFragments[0].allocationPtr);
    EXPECT_EQ(9 * MemoryConstants::pageSize, reqs.allocationFragments[0].allocationSize);

    auto trailingPtr = alignDown(ptrOffset(ptr, size), MemoryConstants::pageSize);
    EXPECT_EQ(trailingPtr, reqs.allocationFragments[1].allocationPtr);
    EXPECT_EQ(MemoryConstants::pageSize, reqs.allocationFragments[1].allocationSize);

    EXPECT_EQ(nullptr, reqs.allocationFragments[2].allocationPtr);
    EXPECT_EQ(0u, reqs.allocationFragments[2].allocationSize);
}

TEST_F(HostPtrManagerTest, GivenNotAlignedPointerAndNotAlignedSizeWhenGettingAllocationRequirementsThenThreeFragmentsAreReturned) {
    auto size = MemoryConstants::pageSize * 10 - 1;
    void *ptr = (void *)0x1045;

    AllocationRequirements reqs = MockHostPtrManager::getAllocationRequirements(rootDeviceIndex, ptr, size);
    EXPECT_EQ(3u, reqs.requiredFragmentsCount);

    EXPECT_EQ(reqs.allocationFragments[0].fragmentPosition, FragmentPosition::leading);
    EXPECT_EQ(reqs.allocationFragments[1].fragmentPosition, FragmentPosition::middle);
    EXPECT_EQ(reqs.allocationFragments[2].fragmentPosition, FragmentPosition::trailing);

    auto leadingPtr = (void *)0x1000;
    auto middlePtr = (void *)0x2000;
    auto trailingPtr = (void *)0xb000;

    EXPECT_EQ(reqs.totalRequiredSize, 11 * MemoryConstants::pageSize);

    EXPECT_EQ(leadingPtr, reqs.allocationFragments[0].allocationPtr);
    EXPECT_EQ(MemoryConstants::pageSize, reqs.allocationFragments[0].allocationSize);

    EXPECT_EQ(middlePtr, reqs.allocationFragments[1].allocationPtr);
    EXPECT_EQ(9 * MemoryConstants::pageSize, reqs.allocationFragments[1].allocationSize);

    EXPECT_EQ(trailingPtr, reqs.allocationFragments[2].allocationPtr);
    EXPECT_EQ(MemoryConstants::pageSize, reqs.allocationFragments[2].allocationSize);
}

TEST_F(HostPtrManagerTest, GivenNotAlignedPointerAndNotAlignedSizeWithinOnePageWhenGettingAllocationRequirementsThenOneFragmentIsReturned) {
    auto size = 200;
    void *ptr = (void *)0x1045;

    AllocationRequirements reqs = MockHostPtrManager::getAllocationRequirements(rootDeviceIndex, ptr, size);
    EXPECT_EQ(1u, reqs.requiredFragmentsCount);

    EXPECT_EQ(reqs.allocationFragments[0].fragmentPosition, FragmentPosition::leading);
    EXPECT_EQ(reqs.allocationFragments[1].fragmentPosition, FragmentPosition::none);
    EXPECT_EQ(reqs.allocationFragments[2].fragmentPosition, FragmentPosition::none);

    auto leadingPtr = (void *)0x1000;

    EXPECT_EQ(reqs.totalRequiredSize, MemoryConstants::pageSize);

    EXPECT_EQ(leadingPtr, reqs.allocationFragments[0].allocationPtr);
    EXPECT_EQ(MemoryConstants::pageSize, reqs.allocationFragments[0].allocationSize);

    EXPECT_EQ(nullptr, reqs.allocationFragments[1].allocationPtr);
    EXPECT_EQ(0u, reqs.allocationFragments[1].allocationSize);

    EXPECT_EQ(nullptr, reqs.allocationFragments[2].allocationPtr);
    EXPECT_EQ(0u, reqs.allocationFragments[2].allocationSize);
}

TEST_F(HostPtrManagerTest, GivenNotAlignedPointerAndNotAlignedSizeWithinTwoPagesWhenGettingAllocationRequirementsThenTwoFragmentsAreReturned) {
    auto size = MemoryConstants::pageSize;
    void *ptr = (void *)0x1045;

    AllocationRequirements reqs = MockHostPtrManager::getAllocationRequirements(rootDeviceIndex, ptr, size);
    EXPECT_EQ(2u, reqs.requiredFragmentsCount);

    EXPECT_EQ(reqs.allocationFragments[0].fragmentPosition, FragmentPosition::leading);
    EXPECT_EQ(reqs.allocationFragments[1].fragmentPosition, FragmentPosition::trailing);
    EXPECT_EQ(reqs.allocationFragments[2].fragmentPosition, FragmentPosition::none);

    auto leadingPtr = (void *)0x1000;
    auto trailingPtr = (void *)0x2000;

    EXPECT_EQ(reqs.totalRequiredSize, 2 * MemoryConstants::pageSize);

    EXPECT_EQ(leadingPtr, reqs.allocationFragments[0].allocationPtr);
    EXPECT_EQ(MemoryConstants::pageSize, reqs.allocationFragments[0].allocationSize);

    EXPECT_EQ(trailingPtr, reqs.allocationFragments[1].allocationPtr);
    EXPECT_EQ(MemoryConstants::pageSize, reqs.allocationFragments[1].allocationSize);

    EXPECT_EQ(nullptr, reqs.allocationFragments[2].allocationPtr);
    EXPECT_EQ(0u, reqs.allocationFragments[2].allocationSize);
}

TEST_F(HostPtrManagerTest, GivenAlignedPointerAndAlignedSizeOfOnePageWhenGettingAllocationRequirementsThenOnlyMiddleFragmentIsReturned) {
    auto size = MemoryConstants::pageSize * 10;
    void *ptr = (void *)0x1000;

    AllocationRequirements reqs = MockHostPtrManager::getAllocationRequirements(rootDeviceIndex, ptr, size);
    EXPECT_EQ(1u, reqs.requiredFragmentsCount);

    EXPECT_EQ(reqs.allocationFragments[0].fragmentPosition, FragmentPosition::middle);
    EXPECT_EQ(reqs.allocationFragments[1].fragmentPosition, FragmentPosition::none);
    EXPECT_EQ(reqs.allocationFragments[2].fragmentPosition, FragmentPosition::none);

    auto middlePtr = (void *)0x1000;

    EXPECT_EQ(reqs.totalRequiredSize, 10 * MemoryConstants::pageSize);

    EXPECT_EQ(middlePtr, reqs.allocationFragments[0].allocationPtr);
    EXPECT_EQ(10 * MemoryConstants::pageSize, reqs.allocationFragments[0].allocationSize);

    EXPECT_EQ(nullptr, reqs.allocationFragments[1].allocationPtr);
    EXPECT_EQ(0u, reqs.allocationFragments[1].allocationSize);

    EXPECT_EQ(nullptr, reqs.allocationFragments[2].allocationPtr);
    EXPECT_EQ(0u, reqs.allocationFragments[2].allocationSize);
}

TEST_F(HostPtrManagerTest, GivenNotAlignedPointerAndSizeThatFitsToPageWhenGettingAllocationRequirementsThenLeadingAndMiddleFragmentsAreReturned) {
    auto size = MemoryConstants::pageSize * 10 - 1;
    void *ptr = (void *)0x1001;

    AllocationRequirements reqs = MockHostPtrManager::getAllocationRequirements(rootDeviceIndex, ptr, size);
    EXPECT_EQ(2u, reqs.requiredFragmentsCount);

    EXPECT_EQ(reqs.allocationFragments[0].fragmentPosition, FragmentPosition::leading);
    EXPECT_EQ(reqs.allocationFragments[1].fragmentPosition, FragmentPosition::middle);
    EXPECT_EQ(reqs.allocationFragments[2].fragmentPosition, FragmentPosition::none);

    auto leadingPtr = (void *)0x1000;
    auto middlePtr = (void *)0x2000;

    EXPECT_EQ(reqs.totalRequiredSize, 10 * MemoryConstants::pageSize);

    EXPECT_EQ(leadingPtr, reqs.allocationFragments[0].allocationPtr);
    EXPECT_EQ(MemoryConstants::pageSize, reqs.allocationFragments[0].allocationSize);

    EXPECT_EQ(middlePtr, reqs.allocationFragments[1].allocationPtr);
    EXPECT_EQ(9 * MemoryConstants::pageSize, reqs.allocationFragments[1].allocationSize);

    EXPECT_EQ(nullptr, reqs.allocationFragments[2].allocationPtr);
    EXPECT_EQ(0u, reqs.allocationFragments[2].allocationSize);
}

TEST_F(HostPtrManagerTest, GivenAlignedPointerAndPageSizeWhenGettingAllocationRequirementsThenOnlyMiddleFragmentIsReturned) {
    auto size = MemoryConstants::pageSize;
    void *ptr = (void *)0x1000;

    AllocationRequirements reqs = MockHostPtrManager::getAllocationRequirements(rootDeviceIndex, ptr, size);
    EXPECT_EQ(1u, reqs.requiredFragmentsCount);

    EXPECT_EQ(reqs.allocationFragments[0].fragmentPosition, FragmentPosition::middle);
    EXPECT_EQ(reqs.allocationFragments[1].fragmentPosition, FragmentPosition::none);
    EXPECT_EQ(reqs.allocationFragments[2].fragmentPosition, FragmentPosition::none);

    auto middlePtr = (void *)0x1000;

    EXPECT_EQ(reqs.totalRequiredSize, MemoryConstants::pageSize);

    EXPECT_EQ(middlePtr, reqs.allocationFragments[0].allocationPtr);
    EXPECT_EQ(MemoryConstants::pageSize, reqs.allocationFragments[0].allocationSize);

    EXPECT_EQ(nullptr, reqs.allocationFragments[1].allocationPtr);
    EXPECT_EQ(0u, reqs.allocationFragments[1].allocationSize);

    EXPECT_EQ(nullptr, reqs.allocationFragments[2].allocationPtr);
    EXPECT_EQ(0u, reqs.allocationFragments[2].allocationSize);
}

TEST_F(HostPtrManagerTest, GivenAllocationRequirementsForMiddleAllocationThatIsNotStoredInManagerWhenGettingAllocationRequirementsThenNullptrIsReturned) {
    auto size = MemoryConstants::pageSize;
    void *ptr = (void *)0x1000;
    auto reqs = MockHostPtrManager::getAllocationRequirements(rootDeviceIndex, ptr, size);
    MockHostPtrManager hostPtrManager;

    auto gpuAllocationFragments = hostPtrManager.populateAlreadyAllocatedFragments(reqs);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[0].osHandleStorage);
    EXPECT_EQ(ptr, gpuAllocationFragments.fragmentStorageData[0].cpuPtr);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[1].osHandleStorage);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[1].cpuPtr);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[2].osHandleStorage);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[2].cpuPtr);
}

TEST_F(HostPtrManagerTest, GivenAllocationRequirementsForMiddleAllocationThatIsStoredInManagerWhenGettingAllocationRequirementsThenProperAllocationIsReturnedAndRefCountIncreased) {

    MockHostPtrManager hostPtrManager;
    FragmentStorage allocationFragment;
    auto cpuPtr = (void *)0x1000;
    auto ptrSize = MemoryConstants::pageSize;
    auto osInternalStorage = (OsHandle *)0x12312;
    allocationFragment.fragmentCpuPointer = cpuPtr;
    allocationFragment.fragmentSize = ptrSize;
    allocationFragment.osInternalStorage = osInternalStorage;

    hostPtrManager.storeFragment(rootDeviceIndex, allocationFragment);

    auto reqs = MockHostPtrManager::getAllocationRequirements(rootDeviceIndex, cpuPtr, ptrSize);

    auto gpuAllocationFragments = hostPtrManager.populateAlreadyAllocatedFragments(reqs);

    EXPECT_EQ(osInternalStorage, gpuAllocationFragments.fragmentStorageData[0].osHandleStorage);
    EXPECT_EQ(cpuPtr, gpuAllocationFragments.fragmentStorageData[0].cpuPtr);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[1].osHandleStorage);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[1].cpuPtr);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[2].osHandleStorage);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[2].cpuPtr);

    auto fragment = hostPtrManager.getFragment({cpuPtr, rootDeviceIndex});
    EXPECT_EQ(2, fragment->refCount);
}

TEST_F(HostPtrManagerTest, GivenAllocationRequirementsForAllocationWithinSizeOfStoredAllocationInManagerWhenGettingAllocationRequirementsThenProperAllocationIsReturned) {

    MockHostPtrManager hostPtrManager;
    FragmentStorage allocationFragment;
    auto cpuPtr = (void *)0x1000;
    auto ptrSize = MemoryConstants::pageSize * 10;
    auto osInternalStorage = (OsHandle *)0x12312;
    allocationFragment.fragmentCpuPointer = cpuPtr;
    allocationFragment.fragmentSize = ptrSize;
    allocationFragment.osInternalStorage = osInternalStorage;

    hostPtrManager.storeFragment(rootDeviceIndex, allocationFragment);

    auto reqs = MockHostPtrManager::getAllocationRequirements(rootDeviceIndex, cpuPtr, MemoryConstants::pageSize);

    auto gpuAllocationFragments = hostPtrManager.populateAlreadyAllocatedFragments(reqs);

    EXPECT_EQ(osInternalStorage, gpuAllocationFragments.fragmentStorageData[0].osHandleStorage);
    EXPECT_EQ(cpuPtr, gpuAllocationFragments.fragmentStorageData[0].cpuPtr);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[1].osHandleStorage);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[1].cpuPtr);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[2].osHandleStorage);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[2].cpuPtr);

    auto fragment = hostPtrManager.getFragment({cpuPtr, rootDeviceIndex});
    EXPECT_EQ(2, fragment->refCount);
}

TEST_F(HostPtrManagerTest, WhenStoringFragmentThenContainerCountIsIncremented) {
    MockHostPtrManager hostPtrManager;

    FragmentStorage allocationFragment;
    EXPECT_EQ(allocationFragment.fragmentCpuPointer, nullptr);
    EXPECT_EQ(allocationFragment.fragmentSize, 0u);
    EXPECT_EQ(allocationFragment.refCount, 0);

    hostPtrManager.storeFragment(rootDeviceIndex, allocationFragment);

    EXPECT_EQ(1u, hostPtrManager.getFragmentCount());
}

TEST_F(HostPtrManagerTest, WhenStoringFragmentTwiceThenContainerCountIsIncrementedOnce) {
    MockHostPtrManager hostPtrManager;

    FragmentStorage allocationFragment;

    hostPtrManager.storeFragment(rootDeviceIndex, allocationFragment);
    hostPtrManager.storeFragment(rootDeviceIndex, allocationFragment);

    EXPECT_EQ(1u, hostPtrManager.getFragmentCount());
}

TEST_F(HostPtrManagerTest, GivenEmptyHostPtrManagerWhenAskingForFragmentThenNullptrIsReturned) {
    MockHostPtrManager hostPtrManager;
    auto fragment = hostPtrManager.getFragment({(void *)0x10121, rootDeviceIndex});
    EXPECT_EQ(nullptr, fragment);
    EXPECT_EQ(0u, hostPtrManager.getFragmentCount());
}

TEST_F(HostPtrManagerTest, GivenNonEmptyHostPtrManagerWhenAskingForFragmentThenProperFragmentIsReturnedWithRefCountOne) {
    MockHostPtrManager hostPtrManager;
    FragmentStorage fragment;
    void *cpuPtr = (void *)0x10121;
    auto fragmentSize = 101u;
    fragment.fragmentCpuPointer = cpuPtr;
    fragment.fragmentSize = fragmentSize;
    fragment.refCount = 0;

    hostPtrManager.storeFragment(rootDeviceIndex, fragment);
    auto retFragment = hostPtrManager.getFragment({cpuPtr, rootDeviceIndex});

    EXPECT_NE(retFragment, &fragment);
    EXPECT_EQ(1, retFragment->refCount);
    EXPECT_EQ(cpuPtr, retFragment->fragmentCpuPointer);
    EXPECT_EQ(fragmentSize, retFragment->fragmentSize);
    EXPECT_EQ(1u, hostPtrManager.getFragmentCount());
}

TEST_F(HostPtrManagerTest, GivenHostPtrManagerFilledTwiceWithTheSamePointerWhenAskingForFragmentThenProperFragmentIsReturnedWithRefCountTwo) {
    MockHostPtrManager hostPtrManager;
    FragmentStorage fragment;
    void *cpuPtr = (void *)0x10121;
    auto fragmentSize = 101u;
    fragment.fragmentCpuPointer = cpuPtr;
    fragment.fragmentSize = fragmentSize;
    fragment.refCount = 0;

    hostPtrManager.storeFragment(rootDeviceIndex, fragment);
    hostPtrManager.storeFragment(rootDeviceIndex, fragment);
    auto retFragment = hostPtrManager.getFragment({cpuPtr, rootDeviceIndex});

    EXPECT_NE(retFragment, &fragment);
    EXPECT_EQ(2, retFragment->refCount);
    EXPECT_EQ(cpuPtr, retFragment->fragmentCpuPointer);
    EXPECT_EQ(fragmentSize, retFragment->fragmentSize);
    EXPECT_EQ(1u, hostPtrManager.getFragmentCount());
}

TEST_F(HostPtrManagerTest, GivenHostPtrManagerFilledWithFragmentsWhenFragmentIsBeingReleasedThenManagerMaintainsProperRefferenceCount) {
    MockHostPtrManager hostPtrManager;
    FragmentStorage fragment;
    void *cpuPtr = (void *)0x1000;
    auto fragmentSize = MemoryConstants::pageSize;

    fragment.fragmentCpuPointer = cpuPtr;
    fragment.fragmentSize = fragmentSize;

    hostPtrManager.storeFragment(rootDeviceIndex, fragment);
    hostPtrManager.storeFragment(rootDeviceIndex, fragment);
    ASSERT_EQ(1u, hostPtrManager.getFragmentCount());

    auto fragmentReadyForRelease = hostPtrManager.releaseHostPtr(rootDeviceIndex, cpuPtr);
    EXPECT_FALSE(fragmentReadyForRelease);

    auto retFragment = hostPtrManager.getFragment({cpuPtr, rootDeviceIndex});

    EXPECT_EQ(1, retFragment->refCount);

    fragmentReadyForRelease = hostPtrManager.releaseHostPtr(rootDeviceIndex, cpuPtr);

    EXPECT_TRUE(fragmentReadyForRelease);

    retFragment = hostPtrManager.getFragment({cpuPtr, rootDeviceIndex});

    EXPECT_EQ(nullptr, retFragment);
    EXPECT_EQ(0u, hostPtrManager.getFragmentCount());
}

TEST_F(HostPtrManagerTest, GivenOsHandleStorageWhenAskedToStoreTheFragmentThenFragmentIsStoredProperly) {
    OsHandleStorage storage;
    void *cpu1 = (void *)0x1000;
    void *cpu2 = (void *)0x2000;

    auto size1 = MemoryConstants::pageSize;
    auto size2 = MemoryConstants::pageSize * 2;

    storage.fragmentStorageData[0].cpuPtr = cpu1;
    storage.fragmentStorageData[0].fragmentSize = size1;

    storage.fragmentStorageData[1].cpuPtr = cpu2;
    storage.fragmentStorageData[1].fragmentSize = size2;

    MockHostPtrManager hostPtrManager;

    EXPECT_EQ(0u, hostPtrManager.getFragmentCount());

    hostPtrManager.storeFragment(rootDeviceIndex, storage.fragmentStorageData[0]);
    hostPtrManager.storeFragment(rootDeviceIndex, storage.fragmentStorageData[1]);

    EXPECT_EQ(2u, hostPtrManager.getFragmentCount());

    hostPtrManager.releaseHandleStorage(rootDeviceIndex, storage);

    EXPECT_EQ(0u, hostPtrManager.getFragmentCount());
}

TEST_F(HostPtrManagerTest, GivenHostPtrFilledWith3TripleFragmentsWhenAskedForPopulationThenAllFragmentsAreResued) {
    void *cpuPtr = (void *)0x1001;
    auto fragmentSize = MemoryConstants::pageSize * 10;

    MockHostPtrManager hostPtrManager;

    auto reqs = hostPtrManager.getAllocationRequirements(rootDeviceIndex, cpuPtr, fragmentSize);
    ASSERT_EQ(3u, reqs.requiredFragmentsCount);

    FragmentStorage fragments[maxFragmentsCount];
    // check all fragments
    for (int i = 0; i < maxFragmentsCount; i++) {
        fragments[i].fragmentCpuPointer = const_cast<void *>(reqs.allocationFragments[i].allocationPtr);
        fragments[i].fragmentSize = reqs.allocationFragments[i].allocationSize;
        hostPtrManager.storeFragment(rootDeviceIndex, fragments[i]);
    }

    EXPECT_EQ(3u, hostPtrManager.getFragmentCount());

    auto osHandles = hostPtrManager.populateAlreadyAllocatedFragments(reqs);

    EXPECT_EQ(3u, hostPtrManager.getFragmentCount());
    for (int i = 0; i < maxFragmentsCount; i++) {
        EXPECT_EQ(osHandles.fragmentStorageData[i].cpuPtr, reqs.allocationFragments[i].allocationPtr);
        EXPECT_EQ(osHandles.fragmentStorageData[i].fragmentSize, reqs.allocationFragments[i].allocationSize);
        auto fragment = hostPtrManager.getFragment({const_cast<void *>(reqs.allocationFragments[i].allocationPtr),
                                                    rootDeviceIndex});
        ASSERT_NE(nullptr, fragment);
        EXPECT_EQ(2, fragment->refCount);
        EXPECT_EQ(osHandles.fragmentStorageData[i].cpuPtr, fragment->fragmentCpuPointer);
    }

    for (int i = 0; i < maxFragmentsCount; i++) {
        hostPtrManager.releaseHostPtr(rootDeviceIndex, fragments[i].fragmentCpuPointer);
    }
    EXPECT_EQ(3u, hostPtrManager.getFragmentCount());
    for (int i = 0; i < maxFragmentsCount; i++) {
        auto fragment = hostPtrManager.getFragment({const_cast<void *>(reqs.allocationFragments[i].allocationPtr),
                                                    rootDeviceIndex});
        ASSERT_NE(nullptr, fragment);
        EXPECT_EQ(1, fragment->refCount);
    }
    for (int i = 0; i < maxFragmentsCount; i++) {
        hostPtrManager.releaseHostPtr(rootDeviceIndex, fragments[i].fragmentCpuPointer);
    }
    EXPECT_EQ(0u, hostPtrManager.getFragmentCount());
}

TEST_F(HostPtrManagerTest, GivenFragmentSizeZeroWhenGettingFragmentThenNullptrIsReturned) {
    HostPtrManager hostPtrManager;

    auto ptr1 = (void *)0x010000;
    FragmentStorage fragment1;
    fragment1.fragmentCpuPointer = ptr1;
    fragment1.fragmentSize = 0;
    hostPtrManager.storeFragment(rootDeviceIndex, fragment1);

    auto ptr2 = (void *)0x040000;
    FragmentStorage fragment2;
    fragment2.fragmentCpuPointer = ptr2;
    fragment2.fragmentSize = 0;
    hostPtrManager.storeFragment(rootDeviceIndex, fragment2);

    auto cptr1 = (void *)0x00F000;
    auto frag1 = hostPtrManager.getFragment({cptr1, rootDeviceIndex});
    EXPECT_EQ(frag1, nullptr);

    auto cptr2 = (void *)0x010000;
    auto frag2 = hostPtrManager.getFragment({cptr2, rootDeviceIndex});
    EXPECT_NE(frag2, nullptr);

    auto cptr3 = (void *)0x010001;
    auto frag3 = hostPtrManager.getFragment({cptr3, rootDeviceIndex});
    EXPECT_EQ(frag3, nullptr);

    auto cptr4 = (void *)0x020000;
    auto frag4 = hostPtrManager.getFragment({cptr4, rootDeviceIndex});
    EXPECT_EQ(frag4, nullptr);

    auto cptr5 = (void *)0x040000;
    auto frag5 = hostPtrManager.getFragment({cptr5, rootDeviceIndex});
    EXPECT_NE(frag5, nullptr);

    auto cptr6 = (void *)0x040001;
    auto frag6 = hostPtrManager.getFragment({cptr6, rootDeviceIndex});
    EXPECT_EQ(frag6, nullptr);

    auto cptr7 = (void *)0x060000;
    auto frag7 = hostPtrManager.getFragment({cptr7, rootDeviceIndex});
    EXPECT_EQ(frag7, nullptr);
}

TEST_F(HostPtrManagerTest, GivenFragmentSizeNonZeroWhenGettingFragmentThenCorrectAllocationIsReturned) {
    MockHostPtrManager hostPtrManager;
    uint32_t rootDeviceIndex2 = 2u;

    auto size1 = MemoryConstants::pageSize;

    auto ptr11 = (void *)0x010000;
    FragmentStorage fragment11;
    fragment11.fragmentCpuPointer = ptr11;
    fragment11.fragmentSize = size1;
    hostPtrManager.storeFragment(rootDeviceIndex, fragment11);

    auto ptr12 = (void *)0x020000;
    FragmentStorage fragment12;
    fragment12.fragmentCpuPointer = ptr12;
    fragment12.fragmentSize = size1;
    hostPtrManager.storeFragment(rootDeviceIndex2, fragment12);

    auto ptr21 = (void *)0x040000;
    FragmentStorage fragment21;
    fragment21.fragmentCpuPointer = ptr21;
    fragment21.fragmentSize = size1;
    hostPtrManager.storeFragment(rootDeviceIndex, fragment21);

    auto ptr22 = (void *)0x060000;
    FragmentStorage fragment22;
    fragment22.fragmentCpuPointer = ptr22;
    fragment22.fragmentSize = size1;
    hostPtrManager.storeFragment(rootDeviceIndex2, fragment22);

    auto cptr1 = (void *)0x010060;
    auto frag11 = hostPtrManager.getFragment({cptr1, rootDeviceIndex});
    EXPECT_NE(frag11, nullptr);
    auto frag12 = hostPtrManager.getFragment({cptr1, rootDeviceIndex2});
    EXPECT_EQ(frag12, nullptr);

    auto cptr2 = (void *)0x020000;
    auto frag21 = hostPtrManager.getFragment({cptr2, rootDeviceIndex});
    EXPECT_EQ(frag21, nullptr);
    auto frag22 = hostPtrManager.getFragment({cptr2, rootDeviceIndex2});
    EXPECT_NE(frag22, nullptr);

    auto cptr3 = (void *)0x040060;
    auto frag31 = hostPtrManager.getFragment({cptr3, rootDeviceIndex});
    EXPECT_NE(frag31, nullptr);
    auto frag32 = hostPtrManager.getFragment({cptr3, rootDeviceIndex2});
    EXPECT_EQ(frag32, nullptr);

    auto cptr4 = (void *)0x060000;
    auto frag41 = hostPtrManager.getFragment({cptr4, rootDeviceIndex});
    EXPECT_EQ(frag41, nullptr);
    auto frag42 = hostPtrManager.getFragment({cptr4, rootDeviceIndex2});
    EXPECT_NE(frag42, nullptr);

    AllocationRequirements requiredAllocations;
    requiredAllocations.rootDeviceIndex = rootDeviceIndex;
    auto ptr3 = (void *)0x040000;
    auto size3 = MemoryConstants::pageSize * 2;
    requiredAllocations = hostPtrManager.getAllocationRequirements(rootDeviceIndex, ptr3, size3);

    EXPECT_ANY_THROW(hostPtrManager.populateAlreadyAllocatedFragments(requiredAllocations));
}

TEST_F(HostPtrManagerTest, WhenCheckingForOverlapsThenCorrectStatusIsReturned) {
    MockHostPtrManager hostPtrManager;
    uint32_t rootDeviceIndex2 = 2u;

    auto size1 = MemoryConstants::pageSize;

    auto ptr11 = (void *)0x010000;
    FragmentStorage fragment11;
    fragment11.fragmentCpuPointer = ptr11;
    fragment11.fragmentSize = size1;
    hostPtrManager.storeFragment(rootDeviceIndex, fragment11);

    auto ptr12 = (void *)0x020000;
    FragmentStorage fragment12;
    fragment12.fragmentCpuPointer = ptr12;
    fragment12.fragmentSize = size1;
    hostPtrManager.storeFragment(rootDeviceIndex2, fragment12);

    auto ptr21 = (void *)0x040000;
    FragmentStorage fragment21;
    fragment21.fragmentCpuPointer = ptr21;
    fragment21.fragmentSize = size1;
    hostPtrManager.storeFragment(rootDeviceIndex, fragment21);

    auto ptr22 = (void *)0x060000;
    FragmentStorage fragment22;
    fragment22.fragmentCpuPointer = ptr22;
    fragment22.fragmentSize = size1;
    hostPtrManager.storeFragment(rootDeviceIndex2, fragment22);

    OverlapStatus overlappingStatus;
    auto cptr1 = (void *)0x010060;
    auto frag1 = hostPtrManager.getFragmentAndCheckForOverlaps(rootDeviceIndex, cptr1, 1u, overlappingStatus);
    EXPECT_NE(frag1, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_WITHIN_STORED_FRAGMENT);

    frag1 = hostPtrManager.getFragmentAndCheckForOverlaps(rootDeviceIndex, ptr11, size1, overlappingStatus);
    EXPECT_NE(frag1, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_WITH_EXACT_SIZE_AS_STORED_FRAGMENT);

    frag1 = hostPtrManager.getFragmentAndCheckForOverlaps(rootDeviceIndex, ptr11, size1 - 1, overlappingStatus);
    EXPECT_NE(frag1, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_WITHIN_STORED_FRAGMENT);

    frag1 = hostPtrManager.getFragmentAndCheckForOverlaps(rootDeviceIndex2, ptr11, size1, overlappingStatus);
    EXPECT_EQ(frag1, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER);

    auto cptr2 = (void *)0x020000;
    auto frag2 = hostPtrManager.getFragmentAndCheckForOverlaps(rootDeviceIndex, cptr2, 1u, overlappingStatus);
    EXPECT_EQ(frag2, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER);

    auto cptr3 = (void *)0x040060;
    auto frag3 = hostPtrManager.getFragmentAndCheckForOverlaps(rootDeviceIndex, cptr3, 1u, overlappingStatus);
    EXPECT_NE(frag3, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_WITHIN_STORED_FRAGMENT);

    auto cptr4 = (void *)0x060000;
    auto frag4 = hostPtrManager.getFragmentAndCheckForOverlaps(rootDeviceIndex, cptr4, 1u, overlappingStatus);
    EXPECT_EQ(frag4, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER);

    auto cptr5 = (void *)0x040000;
    auto frag5 = hostPtrManager.getFragmentAndCheckForOverlaps(rootDeviceIndex, cptr5, size1 - 1, overlappingStatus);
    EXPECT_NE(frag5, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_WITHIN_STORED_FRAGMENT);

    auto cptr6 = (void *)0x040000;
    auto frag6 = hostPtrManager.getFragmentAndCheckForOverlaps(rootDeviceIndex, cptr6, size1 + 1, overlappingStatus);
    EXPECT_EQ(frag6, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT);

    auto cptr7 = (void *)0x03FFF0;
    auto frag7 = hostPtrManager.getFragmentAndCheckForOverlaps(rootDeviceIndex, cptr7, 2 * size1, overlappingStatus);
    EXPECT_EQ(frag7, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT);

    auto cptr8 = (void *)0x040000;
    auto frag8 = hostPtrManager.getFragmentAndCheckForOverlaps(rootDeviceIndex, cptr8, size1, overlappingStatus);
    EXPECT_NE(frag8, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_WITH_EXACT_SIZE_AS_STORED_FRAGMENT);

    auto cptr9 = (void *)0x010060;
    auto frag9 = hostPtrManager.getFragmentAndCheckForOverlaps(rootDeviceIndex, cptr9, 2 * size1, overlappingStatus);
    EXPECT_EQ(frag9, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT);
}

TEST_F(HostPtrManagerTest, GivenHostPtrManagerFilledWithBigFragmentWhenAskedForFragmnetInTheMiddleOfBigFragmentThenBigFragmentIsReturned) {
    auto bigSize = 10 * MemoryConstants::pageSize;
    auto bigPtr = (void *)0x01000;
    FragmentStorage fragment;
    fragment.fragmentCpuPointer = bigPtr;
    fragment.fragmentSize = bigSize;
    MockHostPtrManager hostPtrManager;
    hostPtrManager.storeFragment(rootDeviceIndex, fragment);
    EXPECT_EQ(1u, hostPtrManager.getFragmentCount());

    auto ptrInTheMiddle = (void *)0x2000;
    auto smallSize = MemoryConstants::pageSize;

    auto storedBigFragment = hostPtrManager.getFragment({bigPtr, rootDeviceIndex});

    auto fragment2 = hostPtrManager.getFragment({ptrInTheMiddle, rootDeviceIndex});
    EXPECT_EQ(storedBigFragment, fragment2);

    OverlapStatus overlapStatus;
    auto fragment3 = hostPtrManager.getFragmentAndCheckForOverlaps(rootDeviceIndex, ptrInTheMiddle, smallSize, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_WITHIN_STORED_FRAGMENT, overlapStatus);
    EXPECT_EQ(fragment3, storedBigFragment);

    auto ptrOutside = (void *)0x1000000;
    auto outsideSize = 1;

    auto perfectMatchFragment = hostPtrManager.getFragmentAndCheckForOverlaps(rootDeviceIndex, bigPtr, bigSize, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_WITH_EXACT_SIZE_AS_STORED_FRAGMENT, overlapStatus);
    EXPECT_EQ(perfectMatchFragment, storedBigFragment);

    auto oustideFragment = hostPtrManager.getFragmentAndCheckForOverlaps(rootDeviceIndex, ptrOutside, outsideSize, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER, overlapStatus);
    EXPECT_EQ(nullptr, oustideFragment);

    // partialOverlap
    auto ptrPartial = (void *)(((uintptr_t)bigPtr + bigSize) - 100);
    auto partialBigSize = MemoryConstants::pageSize * 100;

    auto partialFragment = hostPtrManager.getFragmentAndCheckForOverlaps(rootDeviceIndex, ptrPartial, partialBigSize, overlapStatus);
    EXPECT_EQ(nullptr, partialFragment);
    EXPECT_EQ(OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT, overlapStatus);
}

TEST_F(HostPtrManagerTest, GivenHostPtrManagerFilledWithFragmentsWhenCheckedForOverlappingThenProperOverlappingStatusIsReturned) {
    auto bigPtr = (void *)0x04000;
    auto bigSize = 10 * MemoryConstants::pageSize;
    FragmentStorage fragment;
    fragment.fragmentCpuPointer = bigPtr;
    fragment.fragmentSize = bigSize;
    MockHostPtrManager hostPtrManager;
    hostPtrManager.storeFragment(rootDeviceIndex, fragment);
    EXPECT_EQ(1u, hostPtrManager.getFragmentCount());

    auto ptrNonOverlapingPriorToBigPtr = (void *)0x2000;
    auto smallSize = MemoryConstants::pageSize;

    OverlapStatus overlapStatus;
    auto fragment2 = hostPtrManager.getFragmentAndCheckForOverlaps(rootDeviceIndex, ptrNonOverlapingPriorToBigPtr, smallSize, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER, overlapStatus);
    EXPECT_EQ(nullptr, fragment2);

    auto ptrNonOverlapingPriorToBigPtrByPage = (void *)0x3000;
    auto checkMatch = (uintptr_t)ptrNonOverlapingPriorToBigPtrByPage + smallSize;
    EXPECT_EQ(checkMatch, (uintptr_t)bigPtr);

    auto fragment3 = hostPtrManager.getFragmentAndCheckForOverlaps(rootDeviceIndex, ptrNonOverlapingPriorToBigPtrByPage, smallSize, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER, overlapStatus);
    EXPECT_EQ(nullptr, fragment3);

    auto fragment4 = hostPtrManager.getFragmentAndCheckForOverlaps(rootDeviceIndex, ptrNonOverlapingPriorToBigPtrByPage, smallSize + 1, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT, overlapStatus);
    EXPECT_EQ(nullptr, fragment4);
}

TEST_F(HostPtrManagerTest, GivenEmptyHostPtrManagerWhenAskedForOverlapingThenNoOverlappingIsReturned) {
    MockHostPtrManager hostPtrManager;
    auto bigPtr = (void *)0x04000;
    auto bigSize = 10 * MemoryConstants::pageSize;

    OverlapStatus overlapStatus;
    auto fragment3 = hostPtrManager.getFragmentAndCheckForOverlaps(rootDeviceIndex, bigPtr, bigSize, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER, overlapStatus);
    EXPECT_EQ(nullptr, fragment3);
}

TEST_F(HostPtrManagerTest, GivenHostPtrManagerFilledWithFragmentsWhenAskedForOverlpaingThenProperStatusIsReturned) {
    auto bigPtr1 = (void *)0x01000;
    auto bigPtr2 = (void *)0x03000;
    auto bigSize = MemoryConstants::pageSize;
    FragmentStorage fragment;
    fragment.fragmentCpuPointer = bigPtr1;
    fragment.fragmentSize = bigSize;
    MockHostPtrManager hostPtrManager;
    hostPtrManager.storeFragment(rootDeviceIndex, fragment);
    fragment.fragmentCpuPointer = bigPtr2;
    hostPtrManager.storeFragment(rootDeviceIndex, fragment);
    EXPECT_EQ(2u, hostPtrManager.getFragmentCount());

    auto ptrNonOverlapingInTheMiddleOfBigPtrs = (void *)0x2000;
    auto ptrNonOverlapingAfterBigPtr = (void *)0x4000;
    auto ptrNonOverlapingBeforeBigPtr = (void *)0;
    OverlapStatus overlapStatus;
    auto fragment1 = hostPtrManager.getFragmentAndCheckForOverlaps(rootDeviceIndex, ptrNonOverlapingInTheMiddleOfBigPtrs, bigSize, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER, overlapStatus);
    EXPECT_EQ(nullptr, fragment1);

    auto fragment2 = hostPtrManager.getFragmentAndCheckForOverlaps(rootDeviceIndex, ptrNonOverlapingInTheMiddleOfBigPtrs, bigSize * 5, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT, overlapStatus);
    EXPECT_EQ(nullptr, fragment2);

    auto fragment3 = hostPtrManager.getFragmentAndCheckForOverlaps(rootDeviceIndex, ptrNonOverlapingAfterBigPtr, bigSize * 5, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER, overlapStatus);
    EXPECT_EQ(nullptr, fragment3);

    auto fragment4 = hostPtrManager.getFragmentAndCheckForOverlaps(rootDeviceIndex, ptrNonOverlapingBeforeBigPtr, bigSize, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER, overlapStatus);
    EXPECT_EQ(nullptr, fragment4);
}

TEST_F(HostPtrManagerTest, GivenHostPtrManagerFilledWithFragmentsWhenAskedForOverlapingThenProperOverlapingStatusIsReturned) {
    auto bigPtr1 = (void *)0x10000;
    auto bigPtr2 = (void *)0x03000;
    auto bigPtr3 = (void *)0x11000;

    auto bigSize1 = MemoryConstants::pageSize;
    auto bigSize2 = MemoryConstants::pageSize * 4;
    auto bigSize3 = MemoryConstants::pageSize * 10;

    FragmentStorage fragment;
    fragment.fragmentCpuPointer = bigPtr1;
    fragment.fragmentSize = bigSize1;
    MockHostPtrManager hostPtrManager;
    hostPtrManager.storeFragment(rootDeviceIndex, fragment);
    fragment.fragmentCpuPointer = bigPtr2;
    fragment.fragmentSize = bigSize2;
    hostPtrManager.storeFragment(rootDeviceIndex, fragment);
    fragment.fragmentCpuPointer = bigPtr3;
    fragment.fragmentSize = bigSize3;
    hostPtrManager.storeFragment(rootDeviceIndex, fragment);

    EXPECT_EQ(3u, hostPtrManager.getFragmentCount());

    OverlapStatus overlapStatus;
    auto fragment1 = hostPtrManager.getFragmentAndCheckForOverlaps(rootDeviceIndex, bigPtr1, bigSize1 + 1, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT, overlapStatus);
    EXPECT_EQ(nullptr, fragment1);

    auto priorToBig1 = (void *)0x9999;

    auto fragment2 = hostPtrManager.getFragmentAndCheckForOverlaps(rootDeviceIndex, priorToBig1, 1, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER, overlapStatus);
    EXPECT_EQ(nullptr, fragment2);

    auto middleOfBig3 = (void *)0x11111;
    auto fragment3 = hostPtrManager.getFragmentAndCheckForOverlaps(rootDeviceIndex, middleOfBig3, 1, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_WITHIN_STORED_FRAGMENT, overlapStatus);
    EXPECT_NE(nullptr, fragment3);
}

using HostPtrAllocationTest = Test<MemoryManagerWithCsrFixture>;

TEST_F(HostPtrAllocationTest, givenTwoAllocationsThatSharesOneFragmentWhenOneIsDestroyedThenFragmentRemains) {

    void *cpuPtr1 = reinterpret_cast<void *>(0x100001);
    void *cpuPtr2 = ptrOffset(cpuPtr1, MemoryConstants::pageSize);

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());
    auto graphicsAllocation1 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, 2 * MemoryConstants::pageSize - 1, csr->getOsContext().getDeviceBitfield()}, cpuPtr1);
    EXPECT_EQ(2u, hostPtrManager->getFragmentCount());

    auto graphicsAllocation2 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, MemoryConstants::pageSize, csr->getOsContext().getDeviceBitfield()}, cpuPtr2);
    EXPECT_EQ(3u, hostPtrManager->getFragmentCount());
    memoryManager->freeGraphicsMemory(graphicsAllocation1);
    EXPECT_EQ(2u, hostPtrManager->getFragmentCount());
    memoryManager->freeGraphicsMemory(graphicsAllocation2);
    EXPECT_EQ(0u, hostPtrManager->getFragmentCount());
}

TEST_F(HostPtrAllocationTest, whenPrepareOsHandlesForAllocationThenPopulateAsManyFragmentsAsRequired) {
    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());
    void *cpuPtr = reinterpret_cast<void *>(0x100001);
    size_t allocationSize = MemoryConstants::pageSize / 2;
    for (uint32_t expectedFragmentCount = 1; expectedFragmentCount <= 3; expectedFragmentCount++, allocationSize += MemoryConstants::pageSize) {
        auto requirements = hostPtrManager->getAllocationRequirements(csr->getRootDeviceIndex(), cpuPtr, allocationSize);
        EXPECT_EQ(expectedFragmentCount, requirements.requiredFragmentsCount);
        auto osStorage = hostPtrManager->prepareOsStorageForAllocation(*memoryManager, allocationSize, cpuPtr, 0);
        EXPECT_EQ(expectedFragmentCount, osStorage.fragmentCount);
        EXPECT_EQ(expectedFragmentCount, hostPtrManager->getFragmentCount());
        hostPtrManager->releaseHandleStorage(csr->getRootDeviceIndex(), osStorage);
        memoryManager->cleanOsHandles(osStorage, 0);
        EXPECT_EQ(0u, hostPtrManager->getFragmentCount());
    }
}

TEST_F(HostPtrAllocationTest, whenOverlappedFragmentIsBiggerThenStoredAndStoredFragmentIsDestroyedDuringSecondCleaningThenCheckForOverlappingReturnsSuccess) {

    void *cpuPtr1 = (void *)0x100004;

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());
    auto graphicsAllocation1 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, MemoryConstants::pageSize, csr->getOsContext().getDeviceBitfield()}, cpuPtr1);
    EXPECT_EQ(2u, hostPtrManager->getFragmentCount());

    EXPECT_NE(nullptr, graphicsAllocation1);

    auto fragment1 = hostPtrManager->getFragment({alignDown(cpuPtr1, MemoryConstants::pageSize), csr->getRootDeviceIndex()});
    EXPECT_NE(nullptr, fragment1);
    auto fragment2 = hostPtrManager->getFragment({alignUp(cpuPtr1, MemoryConstants::pageSize), csr->getRootDeviceIndex()});
    EXPECT_NE(nullptr, fragment2);

    TaskCountType taskCountReady = 2;
    auto storage = new MockInternalAllocationStorage(*csr);
    csr->internalAllocationStorage.reset(storage);
    storage->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(graphicsAllocation1), TEMPORARY_ALLOCATION, taskCountReady);
    storage->updateCompletionAfterCleaningList(taskCountReady);

    // All fragments ready for release
    currentGpuTag = 1;
    csr->latestSentTaskCount = taskCountReady - 1;

    AllocationRequirements requirements;

    requirements.requiredFragmentsCount = 1;
    requirements.totalRequiredSize = MemoryConstants::pageSize * 10;

    requirements.allocationFragments[0].allocationPtr = alignDown(cpuPtr1, MemoryConstants::pageSize);
    requirements.allocationFragments[0].allocationSize = MemoryConstants::pageSize * 10;
    requirements.allocationFragments[0].fragmentPosition = FragmentPosition::none;

    RequirementsStatus status = hostPtrManager->checkAllocationsForOverlapping(*memoryManager, &requirements);

    EXPECT_EQ(RequirementsStatus::success, status);
}

HWTEST_F(HostPtrAllocationTest, givenOverlappingFragmentsWhenCheckIsCalledThenWaitAndCleanOnAllEngines) {
    TaskCountType taskCountReady = 2;
    TaskCountType taskCountNotReady = 1;

    auto &engines = memoryManager->getRegisteredEngines(mockRootDeviceIndex);
    EXPECT_EQ(1u, engines.size());

    auto csr0 = static_cast<MockCommandStreamReceiver *>(engines[0].commandStreamReceiver);
    auto csr1 = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 0, 1);
    TaskCountType csr0GpuTag = taskCountNotReady;
    TaskCountType csr1GpuTag = taskCountNotReady;
    csr0->tagAddress = &csr0GpuTag;
    csr1->tagAddress = &csr1GpuTag;
    auto osContext = memoryManager->createAndRegisterOsContext(csr1.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_RCS, EngineUsage::lowPriority}));
    csr1->setupContext(*osContext);

    void *cpuPtr = reinterpret_cast<void *>(0x100004);

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());
    auto graphicsAllocation0 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, MemoryConstants::pageSize, csr->getOsContext().getDeviceBitfield()}, cpuPtr);
    auto graphicsAllocation1 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, MemoryConstants::pageSize, csr->getOsContext().getDeviceBitfield()}, cpuPtr);

    auto storage0 = new MockInternalAllocationStorage(*csr0);
    auto storage1 = new MockInternalAllocationStorage(*csr1);
    csr0->internalAllocationStorage.reset(storage0);
    storage0->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(graphicsAllocation0), TEMPORARY_ALLOCATION, taskCountReady);
    storage0->updateCompletionAfterCleaningList(taskCountReady);
    csr1->internalAllocationStorage.reset(storage1);
    storage1->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(graphicsAllocation1), TEMPORARY_ALLOCATION, taskCountReady);
    storage1->updateCompletionAfterCleaningList(taskCountReady);

    csr0->setLatestSentTaskCount(taskCountNotReady);
    csr1->setLatestSentTaskCount(taskCountNotReady);

    AllocationRequirements requirements;

    requirements.requiredFragmentsCount = 1;
    requirements.totalRequiredSize = MemoryConstants::pageSize * 10;

    requirements.allocationFragments[0].allocationPtr = alignDown(cpuPtr, MemoryConstants::pageSize);
    requirements.allocationFragments[0].allocationSize = MemoryConstants::pageSize * 10;
    requirements.allocationFragments[0].fragmentPosition = FragmentPosition::none;
    requirements.rootDeviceIndex = csr0->getRootDeviceIndex();

    memoryManager->deferAllocInUse = true;
    hostPtrManager->checkAllocationsForOverlapping(*memoryManager, &requirements);

    EXPECT_EQ(1u, csr0->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(1u, csr1->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(2u, storage0->cleanAllocationsCalled);
    EXPECT_EQ(2u, storage0->lastCleanAllocationsTaskCount);

    if (memoryManager->isSingleTemporaryAllocationsListEnabled()) {
        EXPECT_EQ(1u, storage1->cleanAllocationsCalled);
        EXPECT_EQ(1u, storage1->lastCleanAllocationsTaskCount);
    } else {
        EXPECT_EQ(2u, storage1->cleanAllocationsCalled);
        EXPECT_EQ(2u, storage1->lastCleanAllocationsTaskCount);
    }
}

HWTEST_F(HostPtrAllocationTest, givenOverlappingFragmentsAndSingleTempAllocationsListWhenCheckIsCalledThenWaitAndCleanOnAllEngines) {
    TaskCountType taskCountReady = 2;
    TaskCountType taskCountNotReady = 1;

    memoryManager->singleTemporaryAllocationsList = true;
    memoryManager->temporaryAllocations = std::make_unique<AllocationsList>(AllocationUsage::TEMPORARY_ALLOCATION);

    auto &engines = memoryManager->getRegisteredEngines(mockRootDeviceIndex);
    EXPECT_EQ(1u, engines.size());

    auto csr0 = static_cast<MockCommandStreamReceiver *>(engines[0].commandStreamReceiver);
    auto csr1 = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 0, 1);
    TaskCountType csr0GpuTag = taskCountNotReady;
    TaskCountType csr1GpuTag = taskCountNotReady;
    csr0->tagAddress = &csr0GpuTag;
    csr1->tagAddress = &csr1GpuTag;
    auto osContext = memoryManager->createAndRegisterOsContext(csr1.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_RCS, EngineUsage::lowPriority}));
    csr1->setupContext(*osContext);

    void *cpuPtr = reinterpret_cast<void *>(0x100004);

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());
    auto graphicsAllocation0 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, MemoryConstants::pageSize, csr->getOsContext().getDeviceBitfield()}, cpuPtr);
    auto graphicsAllocation1 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, MemoryConstants::pageSize, csr->getOsContext().getDeviceBitfield()}, cpuPtr);

    auto storage0 = new MockInternalAllocationStorage(*csr0);
    auto storage1 = new MockInternalAllocationStorage(*csr1);
    csr0->internalAllocationStorage.reset(storage0);
    storage0->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(graphicsAllocation0), TEMPORARY_ALLOCATION, taskCountReady);
    storage0->updateCompletionAfterCleaningList(taskCountReady);
    csr1->internalAllocationStorage.reset(storage1);
    storage1->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(graphicsAllocation1), TEMPORARY_ALLOCATION, taskCountReady);
    storage1->updateCompletionAfterCleaningList(taskCountReady);

    csr0->setLatestSentTaskCount(taskCountNotReady);
    csr1->setLatestSentTaskCount(taskCountNotReady);

    AllocationRequirements requirements;

    requirements.requiredFragmentsCount = 1;
    requirements.totalRequiredSize = MemoryConstants::pageSize * 10;

    requirements.allocationFragments[0].allocationPtr = alignDown(cpuPtr, MemoryConstants::pageSize);
    requirements.allocationFragments[0].allocationSize = MemoryConstants::pageSize * 10;
    requirements.allocationFragments[0].fragmentPosition = FragmentPosition::none;
    requirements.rootDeviceIndex = csr0->getRootDeviceIndex();

    memoryManager->deferAllocInUse = true;
    EXPECT_TRUE(memoryManager->temporaryAllocations->peekContains(*graphicsAllocation0));
    EXPECT_TRUE(memoryManager->temporaryAllocations->peekContains(*graphicsAllocation1));

    // first CSR tag updated
    hostPtrManager->checkAllocationsForOverlapping(*memoryManager, &requirements);

    EXPECT_FALSE(memoryManager->temporaryAllocations->peekContains(*graphicsAllocation0));
    EXPECT_TRUE(memoryManager->temporaryAllocations->peekContains(*graphicsAllocation1));

    // second CSR tag updated
    hostPtrManager->checkAllocationsForOverlapping(*memoryManager, &requirements);

    EXPECT_TRUE(memoryManager->temporaryAllocations->peekIsEmpty());
}

HWTEST_F(HostPtrAllocationTest, givenSingleTempAllocationsListWhenAddingToStorageThenCleanCorrectly) {
    TaskCountType taskCountReady = 2;
    TaskCountType taskCountNotReady = 1;

    memoryManager->singleTemporaryAllocationsList = true;
    memoryManager->temporaryAllocations = std::make_unique<AllocationsList>(AllocationUsage::TEMPORARY_ALLOCATION);
    memoryManager->callBaseAllocInUse = true;

    auto &engines = memoryManager->getRegisteredEngines(mockRootDeviceIndex);
    EXPECT_EQ(1u, engines.size());

    auto csr0 = static_cast<MockCommandStreamReceiver *>(engines[0].commandStreamReceiver);
    auto csr1 = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 0, 1);
    TaskCountType csr0GpuTag = taskCountNotReady;
    TaskCountType csr1GpuTag = taskCountNotReady;
    csr0->tagAddress = &csr0GpuTag;
    csr1->tagAddress = &csr1GpuTag;
    auto osContext = memoryManager->createAndRegisterOsContext(csr1.get(), EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_RCS, EngineUsage::lowPriority}));
    csr1->setupContext(*osContext);

    void *cpuPtr = reinterpret_cast<void *>(0x100004);

    auto graphicsAllocation0 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, MemoryConstants::pageSize, csr->getOsContext().getDeviceBitfield()}, cpuPtr);
    auto graphicsAllocation1 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, MemoryConstants::pageSize, csr->getOsContext().getDeviceBitfield()}, cpuPtr);

    auto storage0 = new MockInternalAllocationStorage(*csr0);
    auto storage1 = new MockInternalAllocationStorage(*csr1);
    csr0->internalAllocationStorage.reset(storage0);
    csr1->internalAllocationStorage.reset(storage1);

    EXPECT_EQ(memoryManager->temporaryAllocations.get(), &csr0->getTemporaryAllocations());
    EXPECT_EQ(memoryManager->temporaryAllocations.get(), &csr1->getTemporaryAllocations());

    storage0->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(graphicsAllocation0), TEMPORARY_ALLOCATION, taskCountReady);
    EXPECT_TRUE(storage0->allocationLists[TEMPORARY_ALLOCATION].peekIsEmpty());
    EXPECT_TRUE(memoryManager->temporaryAllocations->peekContains(*graphicsAllocation0));
    EXPECT_EQ(taskCountReady, graphicsAllocation0->getTaskCount(csr0->getOsContext().getContextId()));

    storage1->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(graphicsAllocation1), TEMPORARY_ALLOCATION, taskCountReady);
    EXPECT_TRUE(storage1->allocationLists[TEMPORARY_ALLOCATION].peekIsEmpty());
    EXPECT_TRUE(memoryManager->temporaryAllocations->peekContains(*graphicsAllocation1));
    EXPECT_EQ(taskCountReady, graphicsAllocation1->getTaskCount(csr1->getOsContext().getContextId()));

    csr0->setLatestSentTaskCount(taskCountNotReady);
    csr1->setLatestSentTaskCount(taskCountNotReady);

    storage0->cleanAllocationList(taskCountNotReady, TEMPORARY_ALLOCATION);
    storage1->cleanAllocationList(taskCountNotReady, TEMPORARY_ALLOCATION);
    EXPECT_TRUE(memoryManager->temporaryAllocations->peekContains(*graphicsAllocation0));
    EXPECT_TRUE(memoryManager->temporaryAllocations->peekContains(*graphicsAllocation1));

    csr1GpuTag = taskCountReady;

    storage0->cleanAllocationList(taskCountNotReady, TEMPORARY_ALLOCATION);
    EXPECT_TRUE(memoryManager->temporaryAllocations->peekContains(*graphicsAllocation0));
    EXPECT_FALSE(memoryManager->temporaryAllocations->peekContains(*graphicsAllocation1));

    storage1->cleanAllocationList(taskCountNotReady, TEMPORARY_ALLOCATION);
    EXPECT_TRUE(memoryManager->temporaryAllocations->peekContains(*graphicsAllocation0));

    csr0GpuTag = taskCountReady;
    storage1->cleanAllocationList(taskCountNotReady, TEMPORARY_ALLOCATION);
    EXPECT_TRUE(memoryManager->temporaryAllocations->peekIsEmpty());
}

HWTEST_F(HostPtrAllocationTest, givenSingleTempAllocationsListWhenAddingToStorageThenObtainCorrectly) {
    TaskCountType taskCountReady = 2;
    TaskCountType taskCountNotReady = 1;

    memoryManager->singleTemporaryAllocationsList = true;
    memoryManager->temporaryAllocations = std::make_unique<AllocationsList>(AllocationUsage::TEMPORARY_ALLOCATION);
    memoryManager->callBaseAllocInUse = true;

    auto &engines = memoryManager->getRegisteredEngines(mockRootDeviceIndex);
    auto csr = static_cast<MockCommandStreamReceiver *>(engines[0].commandStreamReceiver);

    TaskCountType csrGpuTag = taskCountNotReady;
    csr->tagAddress = &csrGpuTag;

    void *cpuPtr = reinterpret_cast<void *>(0x100004);

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, MemoryConstants::pageSize, csr->getOsContext().getDeviceBitfield()}, cpuPtr);

    auto storage = new MockInternalAllocationStorage(*csr);
    csr->internalAllocationStorage.reset(storage);
    csr->setLatestSentTaskCount(taskCountNotReady);

    storage->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(graphicsAllocation), TEMPORARY_ALLOCATION, taskCountReady);

    auto alloc = storage->obtainTemporaryAllocationWithPtr(MemoryConstants::pageSize, cpuPtr, graphicsAllocation->getAllocationType());
    EXPECT_NE(nullptr, alloc.get());
    EXPECT_TRUE(memoryManager->temporaryAllocations->peekIsEmpty());
    alloc.release();

    EXPECT_EQ(CompletionStamp::notReady, graphicsAllocation->getTaskCount(csr->getOsContext().getContextId()));

    // clean on CSR destruction
    storage->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(graphicsAllocation), TEMPORARY_ALLOCATION, taskCountReady);
    csr->tagAddress = nullptr;
}

TEST_F(HostPtrAllocationTest, givenDebugFlagSetWhenCreatingMemoryManagerThenEnableSingleTempAllocationsList) {
    DebugManagerStateRestore debugRestorer;

    {
        auto memoryManager = std::make_unique<MockMemoryManager>(executionEnvironment);
        EXPECT_TRUE(memoryManager->isSingleTemporaryAllocationsListEnabled());
        EXPECT_NE(nullptr, memoryManager->temporaryAllocations.get());
    }

    debugManager.flags.UseSingleListForTemporaryAllocations.set(0);
    {
        auto memoryManager = std::make_unique<MockMemoryManager>(executionEnvironment);
        EXPECT_FALSE(memoryManager->isSingleTemporaryAllocationsListEnabled());
        EXPECT_EQ(nullptr, memoryManager->temporaryAllocations.get());
    }
}

TEST_F(HostPtrAllocationTest, whenOverlappedFragmentIsBiggerThenStoredAndStoredFragmentCannotBeDestroyedThenCheckForOverlappingReturnsError) {

    void *cpuPtr1 = (void *)0x100004;

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());
    auto graphicsAllocation1 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, MemoryConstants::pageSize, csr->getOsContext().getDeviceBitfield()}, cpuPtr1);
    EXPECT_EQ(2u, hostPtrManager->getFragmentCount());

    EXPECT_NE(nullptr, graphicsAllocation1);

    auto fragment1 = hostPtrManager->getFragment({alignDown(cpuPtr1, MemoryConstants::pageSize), csr->getRootDeviceIndex()});
    EXPECT_NE(nullptr, fragment1);
    auto fragment2 = hostPtrManager->getFragment({alignUp(cpuPtr1, MemoryConstants::pageSize), csr->getRootDeviceIndex()});
    EXPECT_NE(nullptr, fragment2);

    TaskCountType taskCountReady = 2;
    auto storage = csr->getInternalAllocationStorage();
    storage->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(graphicsAllocation1), TEMPORARY_ALLOCATION, taskCountReady);

    // All fragments ready for release
    currentGpuTag = taskCountReady - 1;
    csr->latestSentTaskCount = taskCountReady - 1;

    AllocationRequirements requirements;

    requirements.requiredFragmentsCount = 1;
    requirements.totalRequiredSize = MemoryConstants::pageSize * 10;

    requirements.allocationFragments[0].allocationPtr = alignDown(cpuPtr1, MemoryConstants::pageSize);
    requirements.allocationFragments[0].allocationSize = MemoryConstants::pageSize * 10;
    requirements.allocationFragments[0].fragmentPosition = FragmentPosition::none;
    requirements.rootDeviceIndex = csr->getRootDeviceIndex();

    RequirementsStatus status = hostPtrManager->checkAllocationsForOverlapping(*memoryManager, &requirements);

    EXPECT_EQ(RequirementsStatus::fatal, status);
}

TEST_F(HostPtrAllocationTest, GivenAllocationsWithoutBiggerOverlapWhenChckingForOverlappingThenSuccessIsReturned) {

    void *cpuPtr1 = (void *)0x100004;
    void *cpuPtr2 = (void *)0x101008;

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());
    auto graphicsAllocation1 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, MemoryConstants::pageSize, csr->getOsContext().getDeviceBitfield()}, cpuPtr1);
    EXPECT_EQ(2u, hostPtrManager->getFragmentCount());

    auto graphicsAllocation2 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, MemoryConstants::pageSize * 3, csr->getOsContext().getDeviceBitfield()}, cpuPtr2);
    EXPECT_EQ(4u, hostPtrManager->getFragmentCount());

    EXPECT_NE(nullptr, graphicsAllocation1);
    EXPECT_NE(nullptr, graphicsAllocation2);

    auto fragment1 = hostPtrManager->getFragment({alignDown(cpuPtr1, MemoryConstants::pageSize), csr->getRootDeviceIndex()});
    EXPECT_NE(nullptr, fragment1);
    auto fragment2 = hostPtrManager->getFragment({alignUp(cpuPtr1, MemoryConstants::pageSize), csr->getRootDeviceIndex()});
    EXPECT_NE(nullptr, fragment2);
    auto fragment3 = hostPtrManager->getFragment({alignDown(cpuPtr2, MemoryConstants::pageSize), csr->getRootDeviceIndex()});
    EXPECT_NE(nullptr, fragment3);
    auto fragment4 = hostPtrManager->getFragment({alignUp(cpuPtr2, MemoryConstants::pageSize), csr->getRootDeviceIndex()});
    EXPECT_NE(nullptr, fragment4);

    AllocationRequirements requirements;

    requirements.requiredFragmentsCount = 2;
    requirements.totalRequiredSize = MemoryConstants::pageSize * 2;

    requirements.allocationFragments[0].allocationPtr = alignDown(cpuPtr1, MemoryConstants::pageSize);
    requirements.allocationFragments[0].allocationSize = MemoryConstants::pageSize;
    requirements.allocationFragments[0].fragmentPosition = FragmentPosition::leading;

    requirements.allocationFragments[1].allocationPtr = alignUp(cpuPtr1, MemoryConstants::pageSize);
    requirements.allocationFragments[1].allocationSize = MemoryConstants::pageSize;
    requirements.allocationFragments[1].fragmentPosition = FragmentPosition::trailing;

    RequirementsStatus status = hostPtrManager->checkAllocationsForOverlapping(*memoryManager, &requirements);

    EXPECT_EQ(RequirementsStatus::success, status);

    memoryManager->freeGraphicsMemory(graphicsAllocation1);
    memoryManager->freeGraphicsMemory(graphicsAllocation2);
}

TEST_F(HostPtrAllocationTest, GivenAllocationsWithBiggerOverlapWhenChckingForOverlappingThenSuccessIsReturned) {

    void *cpuPtr1 = (void *)0x100004;

    auto graphicsAllocation1 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), false, MemoryConstants::pageSize, csr->getOsContext().getDeviceBitfield()}, cpuPtr1);
    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());
    EXPECT_EQ(2u, hostPtrManager->getFragmentCount());

    EXPECT_NE(nullptr, graphicsAllocation1);

    auto fragment1 = hostPtrManager->getFragment({alignDown(cpuPtr1, MemoryConstants::pageSize), csr->getRootDeviceIndex()});
    EXPECT_NE(nullptr, fragment1);
    auto fragment2 = hostPtrManager->getFragment({alignUp(cpuPtr1, MemoryConstants::pageSize), csr->getRootDeviceIndex()});
    EXPECT_NE(nullptr, fragment2);

    TaskCountType taskCountReady = 1;
    auto storage = csr->getInternalAllocationStorage();
    storage->storeAllocationWithTaskCount(std::unique_ptr<GraphicsAllocation>(graphicsAllocation1), TEMPORARY_ALLOCATION, taskCountReady);

    // All fragments ready for release
    taskCount = taskCountReady;
    csr->latestSentTaskCount = taskCountReady;

    AllocationRequirements requirements;

    requirements.requiredFragmentsCount = 1;
    requirements.totalRequiredSize = MemoryConstants::pageSize * 10;

    requirements.allocationFragments[0].allocationPtr = alignDown(cpuPtr1, MemoryConstants::pageSize);
    requirements.allocationFragments[0].allocationSize = MemoryConstants::pageSize * 10;
    requirements.allocationFragments[0].fragmentPosition = FragmentPosition::none;

    RequirementsStatus status = hostPtrManager->checkAllocationsForOverlapping(*memoryManager, &requirements);

    EXPECT_EQ(RequirementsStatus::success, status);
}

TEST(HostPtrEntryKeyTest, givenTwoHostPtrEntryKeysWhenComparingThemThenKeyWithLowerRootDeviceIndexIsLower) {

    auto hostPtr0 = reinterpret_cast<void *>(0x100);
    auto hostPtr1 = reinterpret_cast<void *>(0x200);
    auto hostPtr2 = reinterpret_cast<void *>(0x300);

    HostPtrEntryKey key0{hostPtr1, 0u};
    HostPtrEntryKey key1{hostPtr1, 1u};

    EXPECT_TRUE(key0 < key1);
    EXPECT_FALSE(key1 < key0);

    key0.ptr = hostPtr0;
    EXPECT_TRUE(key0 < key1);
    EXPECT_FALSE(key1 < key0);

    key0.ptr = hostPtr2;
    EXPECT_TRUE(key0 < key1);
    EXPECT_FALSE(key1 < key0);
}

TEST(HostPtrEntryKeyTest, givenTwoHostPtrEntryKeysWithSameRootDeviceIndexWhenComparingThemThenKeyWithLowerPtrIsLower) {
    auto hostPtr0 = reinterpret_cast<void *>(0x100);
    auto hostPtr1 = reinterpret_cast<void *>(0x200);

    HostPtrEntryKey key0{hostPtr0, 1u};
    HostPtrEntryKey key1{hostPtr1, 1u};

    EXPECT_TRUE(key0 < key1);
    EXPECT_FALSE(key1 < key0);
}
TEST(HostPtrEntryKeyTest, givenTwoSameHostPtrEntryKeysWithSameRootDeviceIndexWhenComparingThemThenTheyAreEqual) {
    auto hostPtr = reinterpret_cast<void *>(0x100);

    HostPtrEntryKey key0{hostPtr, 1u};
    HostPtrEntryKey key1{hostPtr, 1u};

    EXPECT_FALSE(key0 < key1);
    EXPECT_FALSE(key1 < key0);
}

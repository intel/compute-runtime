/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/aligned_memory.h"
#include "core/helpers/ptr_math.h"
#include "core/memory_manager/memory_constants.h"
#include "runtime/execution_environment/execution_environment.h"
#include "test.h"
#include "unit_tests/fixtures/memory_manager_fixture.h"
#include "unit_tests/mocks/mock_allocation_properties.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_host_ptr_manager.h"
#include "unit_tests/mocks/mock_internal_allocation_storage.h"
#include "unit_tests/mocks/mock_memory_manager.h"

using namespace NEO;

TEST(HostPtrManager, AlignedPointerAndAlignedSizeAskedForAllocationCountReturnsOne) {
    auto size = MemoryConstants::pageSize * 10;
    void *ptr = (void *)0x1000;

    AllocationRequirements reqs = MockHostPtrManager::getAllocationRequirements(ptr, size);

    EXPECT_EQ(1u, reqs.requiredFragmentsCount);
    EXPECT_EQ(reqs.allocationFragments[0].fragmentPosition, FragmentPosition::MIDDLE);
    EXPECT_EQ(reqs.allocationFragments[1].fragmentPosition, FragmentPosition::NONE);
    EXPECT_EQ(reqs.allocationFragments[2].fragmentPosition, FragmentPosition::NONE);

    EXPECT_EQ(reqs.totalRequiredSize, size);

    EXPECT_EQ(ptr, reqs.allocationFragments[0].allocationPtr);
    EXPECT_EQ(size, reqs.allocationFragments[0].allocationSize);

    EXPECT_EQ(nullptr, reqs.allocationFragments[1].allocationPtr);
    EXPECT_EQ(nullptr, reqs.allocationFragments[2].allocationPtr);
}

TEST(HostPtrManager, AlignedPointerAndNotAlignedSizeAskedForAllocationCountReturnsTwo) {
    auto size = MemoryConstants::pageSize * 10 - 1;
    void *ptr = (void *)0x1000;

    AllocationRequirements reqs = MockHostPtrManager::getAllocationRequirements(ptr, size);
    EXPECT_EQ(2u, reqs.requiredFragmentsCount);

    EXPECT_EQ(reqs.allocationFragments[0].fragmentPosition, FragmentPosition::MIDDLE);
    EXPECT_EQ(reqs.allocationFragments[1].fragmentPosition, FragmentPosition::TRAILING);
    EXPECT_EQ(reqs.allocationFragments[2].fragmentPosition, FragmentPosition::NONE);
    EXPECT_EQ(reqs.totalRequiredSize, alignUp(size, MemoryConstants::pageSize));

    EXPECT_EQ(ptr, reqs.allocationFragments[0].allocationPtr);
    EXPECT_EQ(9 * MemoryConstants::pageSize, reqs.allocationFragments[0].allocationSize);

    auto trailingPtr = alignDown(ptrOffset(ptr, size), MemoryConstants::pageSize);
    EXPECT_EQ(trailingPtr, reqs.allocationFragments[1].allocationPtr);
    EXPECT_EQ(MemoryConstants::pageSize, reqs.allocationFragments[1].allocationSize);

    EXPECT_EQ(nullptr, reqs.allocationFragments[2].allocationPtr);
    EXPECT_EQ(0u, reqs.allocationFragments[2].allocationSize);
}

TEST(HostPtrManager, NotAlignedPointerAndNotAlignedSizeAskedForAllocationCountReturnsThree) {
    auto size = MemoryConstants::pageSize * 10 - 1;
    void *ptr = (void *)0x1045;

    AllocationRequirements reqs = MockHostPtrManager::getAllocationRequirements(ptr, size);
    EXPECT_EQ(3u, reqs.requiredFragmentsCount);

    EXPECT_EQ(reqs.allocationFragments[0].fragmentPosition, FragmentPosition::LEADING);
    EXPECT_EQ(reqs.allocationFragments[1].fragmentPosition, FragmentPosition::MIDDLE);
    EXPECT_EQ(reqs.allocationFragments[2].fragmentPosition, FragmentPosition::TRAILING);

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

TEST(HostPtrManager, NotAlignedPointerAndNotAlignedSizeWithinOnePageAskedForAllocationCountReturnsOne) {
    auto size = 200;
    void *ptr = (void *)0x1045;

    AllocationRequirements reqs = MockHostPtrManager::getAllocationRequirements(ptr, size);
    EXPECT_EQ(1u, reqs.requiredFragmentsCount);

    EXPECT_EQ(reqs.allocationFragments[0].fragmentPosition, FragmentPosition::LEADING);
    EXPECT_EQ(reqs.allocationFragments[1].fragmentPosition, FragmentPosition::NONE);
    EXPECT_EQ(reqs.allocationFragments[2].fragmentPosition, FragmentPosition::NONE);

    auto leadingPtr = (void *)0x1000;

    EXPECT_EQ(reqs.totalRequiredSize, MemoryConstants::pageSize);

    EXPECT_EQ(leadingPtr, reqs.allocationFragments[0].allocationPtr);
    EXPECT_EQ(MemoryConstants::pageSize, reqs.allocationFragments[0].allocationSize);

    EXPECT_EQ(nullptr, reqs.allocationFragments[1].allocationPtr);
    EXPECT_EQ(0u, reqs.allocationFragments[1].allocationSize);

    EXPECT_EQ(nullptr, reqs.allocationFragments[2].allocationPtr);
    EXPECT_EQ(0u, reqs.allocationFragments[2].allocationSize);
}

TEST(HostPtrManager, NotAlignedPointerAndNotAlignedSizeWithinTwoPagesAskedForAllocationCountReturnsTwo) {
    auto size = MemoryConstants::pageSize;
    void *ptr = (void *)0x1045;

    AllocationRequirements reqs = MockHostPtrManager::getAllocationRequirements(ptr, size);
    EXPECT_EQ(2u, reqs.requiredFragmentsCount);

    EXPECT_EQ(reqs.allocationFragments[0].fragmentPosition, FragmentPosition::LEADING);
    EXPECT_EQ(reqs.allocationFragments[1].fragmentPosition, FragmentPosition::TRAILING);
    EXPECT_EQ(reqs.allocationFragments[2].fragmentPosition, FragmentPosition::NONE);

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

TEST(HostPtrManager, AlignedPointerAndAlignedSizeOfOnePageAskedForAllocationCountReturnsMiddleOnly) {
    auto size = MemoryConstants::pageSize * 10;
    void *ptr = (void *)0x1000;

    AllocationRequirements reqs = MockHostPtrManager::getAllocationRequirements(ptr, size);
    EXPECT_EQ(1u, reqs.requiredFragmentsCount);

    EXPECT_EQ(reqs.allocationFragments[0].fragmentPosition, FragmentPosition::MIDDLE);
    EXPECT_EQ(reqs.allocationFragments[1].fragmentPosition, FragmentPosition::NONE);
    EXPECT_EQ(reqs.allocationFragments[2].fragmentPosition, FragmentPosition::NONE);

    auto middlePtr = (void *)0x1000;

    EXPECT_EQ(reqs.totalRequiredSize, 10 * MemoryConstants::pageSize);

    EXPECT_EQ(middlePtr, reqs.allocationFragments[0].allocationPtr);
    EXPECT_EQ(10 * MemoryConstants::pageSize, reqs.allocationFragments[0].allocationSize);

    EXPECT_EQ(nullptr, reqs.allocationFragments[1].allocationPtr);
    EXPECT_EQ(0u, reqs.allocationFragments[1].allocationSize);

    EXPECT_EQ(nullptr, reqs.allocationFragments[2].allocationPtr);
    EXPECT_EQ(0u, reqs.allocationFragments[2].allocationSize);
}

TEST(HostPtrManager, NotAlignedPointerAndSizeThatFitsToPageAskedForAllocationCountReturnsMiddleAndLeading) {
    auto size = MemoryConstants::pageSize * 10 - 1;
    void *ptr = (void *)0x1001;

    AllocationRequirements reqs = MockHostPtrManager::getAllocationRequirements(ptr, size);
    EXPECT_EQ(2u, reqs.requiredFragmentsCount);

    EXPECT_EQ(reqs.allocationFragments[0].fragmentPosition, FragmentPosition::LEADING);
    EXPECT_EQ(reqs.allocationFragments[1].fragmentPosition, FragmentPosition::MIDDLE);
    EXPECT_EQ(reqs.allocationFragments[2].fragmentPosition, FragmentPosition::NONE);

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

TEST(HostPtrManager, AlignedPointerAndPageSizeAskedForAllocationCountRetrunsMiddle) {
    auto size = MemoryConstants::pageSize;
    void *ptr = (void *)0x1000;

    AllocationRequirements reqs = MockHostPtrManager::getAllocationRequirements(ptr, size);
    EXPECT_EQ(1u, reqs.requiredFragmentsCount);

    EXPECT_EQ(reqs.allocationFragments[0].fragmentPosition, FragmentPosition::MIDDLE);
    EXPECT_EQ(reqs.allocationFragments[1].fragmentPosition, FragmentPosition::NONE);
    EXPECT_EQ(reqs.allocationFragments[2].fragmentPosition, FragmentPosition::NONE);

    auto middlePtr = (void *)0x1000;

    EXPECT_EQ(reqs.totalRequiredSize, MemoryConstants::pageSize);

    EXPECT_EQ(middlePtr, reqs.allocationFragments[0].allocationPtr);
    EXPECT_EQ(MemoryConstants::pageSize, reqs.allocationFragments[0].allocationSize);

    EXPECT_EQ(nullptr, reqs.allocationFragments[1].allocationPtr);
    EXPECT_EQ(0u, reqs.allocationFragments[1].allocationSize);

    EXPECT_EQ(nullptr, reqs.allocationFragments[2].allocationPtr);
    EXPECT_EQ(0u, reqs.allocationFragments[2].allocationSize);
}

TEST(HostPtrManager, AllocationRequirementsForMiddleAllocationThatIsNotStoredInManagerAskedForGraphicsAllocationReturnsNotAvailable) {
    auto size = MemoryConstants::pageSize;
    void *ptr = (void *)0x1000;
    auto reqs = MockHostPtrManager::getAllocationRequirements(ptr, size);
    MockHostPtrManager hostPtrManager;

    auto gpuAllocationFragments = hostPtrManager.populateAlreadyAllocatedFragments(reqs);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[0].osHandleStorage);
    EXPECT_EQ(ptr, gpuAllocationFragments.fragmentStorageData[0].cpuPtr);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[1].osHandleStorage);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[1].cpuPtr);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[2].osHandleStorage);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[2].cpuPtr);
}

TEST(HostPtrManager, AllocationRequirementsForMiddleAllocationThatIsStoredInManagerAskedForGraphicsAllocationReturnsProperAllocationAndIncreasesRefCount) {

    MockHostPtrManager hostPtrManager;
    FragmentStorage allocationFragment;
    auto cpuPtr = (void *)0x1000;
    auto ptrSize = MemoryConstants::pageSize;
    auto osInternalStorage = (OsHandle *)0x12312;
    allocationFragment.fragmentCpuPointer = cpuPtr;
    allocationFragment.fragmentSize = ptrSize;
    allocationFragment.osInternalStorage = osInternalStorage;

    hostPtrManager.storeFragment(allocationFragment);

    auto reqs = MockHostPtrManager::getAllocationRequirements(cpuPtr, ptrSize);

    auto gpuAllocationFragments = hostPtrManager.populateAlreadyAllocatedFragments(reqs);

    EXPECT_EQ(osInternalStorage, gpuAllocationFragments.fragmentStorageData[0].osHandleStorage);
    EXPECT_EQ(cpuPtr, gpuAllocationFragments.fragmentStorageData[0].cpuPtr);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[1].osHandleStorage);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[1].cpuPtr);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[2].osHandleStorage);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[2].cpuPtr);

    auto fragment = hostPtrManager.getFragment(cpuPtr);
    EXPECT_EQ(2, fragment->refCount);
}

TEST(HostPtrManager, AllocationRequirementsForAllocationWithinSizeOfStoredAllocationInManagerAskedForGraphicsAllocationReturnsProperAllocation) {

    MockHostPtrManager hostPtrManager;
    FragmentStorage allocationFragment;
    auto cpuPtr = (void *)0x1000;
    auto ptrSize = MemoryConstants::pageSize * 10;
    auto osInternalStorage = (OsHandle *)0x12312;
    allocationFragment.fragmentCpuPointer = cpuPtr;
    allocationFragment.fragmentSize = ptrSize;
    allocationFragment.osInternalStorage = osInternalStorage;

    hostPtrManager.storeFragment(allocationFragment);

    auto reqs = MockHostPtrManager::getAllocationRequirements(cpuPtr, MemoryConstants::pageSize);

    auto gpuAllocationFragments = hostPtrManager.populateAlreadyAllocatedFragments(reqs);

    EXPECT_EQ(osInternalStorage, gpuAllocationFragments.fragmentStorageData[0].osHandleStorage);
    EXPECT_EQ(cpuPtr, gpuAllocationFragments.fragmentStorageData[0].cpuPtr);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[1].osHandleStorage);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[1].cpuPtr);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[2].osHandleStorage);
    EXPECT_EQ(nullptr, gpuAllocationFragments.fragmentStorageData[2].cpuPtr);

    auto fragment = hostPtrManager.getFragment(cpuPtr);
    EXPECT_EQ(2, fragment->refCount);
}

TEST(HostPtrManager, HostPtrAndSizeStoredToHostPtrManagerIncreasesTheContainerCount) {
    MockHostPtrManager hostPtrManager;

    FragmentStorage allocationFragment;
    EXPECT_EQ(allocationFragment.fragmentCpuPointer, nullptr);
    EXPECT_EQ(allocationFragment.fragmentSize, 0u);
    EXPECT_EQ(allocationFragment.refCount, 0);

    hostPtrManager.storeFragment(allocationFragment);

    EXPECT_EQ(1u, hostPtrManager.getFragmentCount());
}

TEST(HostPtrManager, HostPtrAndSizeStoredToHostPtrManagerTwiceReturnsOneAsFragmentCount) {
    MockHostPtrManager hostPtrManager;

    FragmentStorage allocationFragment;

    hostPtrManager.storeFragment(allocationFragment);
    hostPtrManager.storeFragment(allocationFragment);

    EXPECT_EQ(1u, hostPtrManager.getFragmentCount());
}

TEST(HostPtrManager, EmptyHostPtrManagerAskedForFragmentReturnsNullptr) {
    MockHostPtrManager hostPtrManager;
    auto fragment = hostPtrManager.getFragment((void *)0x10121);
    EXPECT_EQ(nullptr, fragment);
    EXPECT_EQ(0u, hostPtrManager.getFragmentCount());
}

TEST(HostPtrManager, NonEmptyHostPtrManagerAskedForFragmentReturnsProperFragmentWithRefCountOne) {
    MockHostPtrManager hostPtrManager;
    FragmentStorage fragment;
    void *cpuPtr = (void *)0x10121;
    auto fragmentSize = 101u;
    fragment.fragmentCpuPointer = cpuPtr;
    fragment.fragmentSize = fragmentSize;
    fragment.refCount = 0;

    hostPtrManager.storeFragment(fragment);
    auto retFragment = hostPtrManager.getFragment(cpuPtr);

    EXPECT_NE(retFragment, &fragment);
    EXPECT_EQ(1, retFragment->refCount);
    EXPECT_EQ(cpuPtr, retFragment->fragmentCpuPointer);
    EXPECT_EQ(fragmentSize, retFragment->fragmentSize);
    EXPECT_EQ(1u, hostPtrManager.getFragmentCount());
}

TEST(HostPtrManager, HostPtrManagerFilledTwiceWithTheSamePointerWhenAskedForFragmentReturnsItWithRefCountSetToTwo) {
    MockHostPtrManager hostPtrManager;
    FragmentStorage fragment;
    void *cpuPtr = (void *)0x10121;
    auto fragmentSize = 101u;
    fragment.fragmentCpuPointer = cpuPtr;
    fragment.fragmentSize = fragmentSize;
    fragment.refCount = 0;

    hostPtrManager.storeFragment(fragment);
    hostPtrManager.storeFragment(fragment);
    auto retFragment = hostPtrManager.getFragment(cpuPtr);

    EXPECT_NE(retFragment, &fragment);
    EXPECT_EQ(2, retFragment->refCount);
    EXPECT_EQ(cpuPtr, retFragment->fragmentCpuPointer);
    EXPECT_EQ(fragmentSize, retFragment->fragmentSize);
    EXPECT_EQ(1u, hostPtrManager.getFragmentCount());
}

TEST(HostPtrManager, GivenHostPtrManagerFilledWithFragmentsWhenFragmentIsBeingReleasedThenManagerMaintainsProperRefferenceCount) {
    MockHostPtrManager hostPtrManager;
    FragmentStorage fragment;
    void *cpuPtr = (void *)0x1000;
    auto fragmentSize = MemoryConstants::pageSize;

    fragment.fragmentCpuPointer = cpuPtr;
    fragment.fragmentSize = fragmentSize;

    hostPtrManager.storeFragment(fragment);
    hostPtrManager.storeFragment(fragment);
    ASSERT_EQ(1u, hostPtrManager.getFragmentCount());

    auto fragmentReadyForRelease = hostPtrManager.releaseHostPtr(cpuPtr);
    EXPECT_FALSE(fragmentReadyForRelease);

    auto retFragment = hostPtrManager.getFragment(cpuPtr);

    EXPECT_EQ(1, retFragment->refCount);

    fragmentReadyForRelease = hostPtrManager.releaseHostPtr(cpuPtr);

    EXPECT_TRUE(fragmentReadyForRelease);

    retFragment = hostPtrManager.getFragment(cpuPtr);

    EXPECT_EQ(nullptr, retFragment);
    EXPECT_EQ(0u, hostPtrManager.getFragmentCount());
}
TEST(HostPtrManager, GivenOsHandleStorageWhenAskedToStoreTheFragmentThenFragmentIsStoredProperly) {
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

    hostPtrManager.storeFragment(storage.fragmentStorageData[0]);
    hostPtrManager.storeFragment(storage.fragmentStorageData[1]);

    EXPECT_EQ(2u, hostPtrManager.getFragmentCount());

    hostPtrManager.releaseHandleStorage(storage);

    EXPECT_EQ(0u, hostPtrManager.getFragmentCount());
}

TEST(HostPtrManager, GivenHostPtrFilledWith3TripleFragmentsWhenAskedForPopulationThenAllFragmentsAreResued) {
    void *cpuPtr = (void *)0x1001;
    auto fragmentSize = MemoryConstants::pageSize * 10;

    MockHostPtrManager hostPtrManager;

    auto reqs = hostPtrManager.getAllocationRequirements(cpuPtr, fragmentSize);
    ASSERT_EQ(3u, reqs.requiredFragmentsCount);

    FragmentStorage fragments[maxFragmentsCount];
    //check all fragments
    for (int i = 0; i < maxFragmentsCount; i++) {
        fragments[i].fragmentCpuPointer = const_cast<void *>(reqs.allocationFragments[i].allocationPtr);
        fragments[i].fragmentSize = reqs.allocationFragments[i].allocationSize;
        hostPtrManager.storeFragment(fragments[i]);
    }

    EXPECT_EQ(3u, hostPtrManager.getFragmentCount());

    auto OsHandles = hostPtrManager.populateAlreadyAllocatedFragments(reqs);

    EXPECT_EQ(3u, hostPtrManager.getFragmentCount());
    for (int i = 0; i < maxFragmentsCount; i++) {
        EXPECT_EQ(OsHandles.fragmentStorageData[i].cpuPtr, reqs.allocationFragments[i].allocationPtr);
        EXPECT_EQ(OsHandles.fragmentStorageData[i].fragmentSize, reqs.allocationFragments[i].allocationSize);
        auto fragment = hostPtrManager.getFragment(const_cast<void *>(reqs.allocationFragments[i].allocationPtr));
        ASSERT_NE(nullptr, fragment);
        EXPECT_EQ(2, fragment->refCount);
        EXPECT_EQ(OsHandles.fragmentStorageData[i].cpuPtr, fragment->fragmentCpuPointer);
    }

    for (int i = 0; i < maxFragmentsCount; i++) {
        hostPtrManager.releaseHostPtr(fragments[i].fragmentCpuPointer);
    }
    EXPECT_EQ(3u, hostPtrManager.getFragmentCount());
    for (int i = 0; i < maxFragmentsCount; i++) {
        auto fragment = hostPtrManager.getFragment(const_cast<void *>(reqs.allocationFragments[i].allocationPtr));
        ASSERT_NE(nullptr, fragment);
        EXPECT_EQ(1, fragment->refCount);
    }
    for (int i = 0; i < maxFragmentsCount; i++) {
        hostPtrManager.releaseHostPtr(fragments[i].fragmentCpuPointer);
    }
    EXPECT_EQ(0u, hostPtrManager.getFragmentCount());
}

TEST(HostPtrManager, FragmentFindWhenFragmentSizeIsZero) {
    HostPtrManager hostPtrManager;

    auto ptr1 = (void *)0x010000;
    FragmentStorage fragment1;
    fragment1.fragmentCpuPointer = ptr1;
    fragment1.fragmentSize = 0;
    hostPtrManager.storeFragment(fragment1);

    auto ptr2 = (void *)0x040000;
    FragmentStorage fragment2;
    fragment2.fragmentCpuPointer = ptr2;
    fragment2.fragmentSize = 0;
    hostPtrManager.storeFragment(fragment2);

    auto cptr1 = (void *)0x00F000;
    auto frag1 = hostPtrManager.getFragment(cptr1);
    EXPECT_EQ(frag1, nullptr);

    auto cptr2 = (void *)0x010000;
    auto frag2 = hostPtrManager.getFragment(cptr2);
    EXPECT_NE(frag2, nullptr);

    auto cptr3 = (void *)0x010001;
    auto frag3 = hostPtrManager.getFragment(cptr3);
    EXPECT_EQ(frag3, nullptr);

    auto cptr4 = (void *)0x020000;
    auto frag4 = hostPtrManager.getFragment(cptr4);
    EXPECT_EQ(frag4, nullptr);

    auto cptr5 = (void *)0x040000;
    auto frag5 = hostPtrManager.getFragment(cptr5);
    EXPECT_NE(frag5, nullptr);

    auto cptr6 = (void *)0x040001;
    auto frag6 = hostPtrManager.getFragment(cptr6);
    EXPECT_EQ(frag6, nullptr);

    auto cptr7 = (void *)0x060000;
    auto frag7 = hostPtrManager.getFragment(cptr7);
    EXPECT_EQ(frag7, nullptr);
}

TEST(HostPtrManager, FragmentFindWhenFragmentSizeIsNotZero) {
    MockHostPtrManager hostPtrManager;

    auto size1 = MemoryConstants::pageSize;

    auto ptr1 = (void *)0x010000;
    FragmentStorage fragment1;
    fragment1.fragmentCpuPointer = ptr1;
    fragment1.fragmentSize = size1;
    hostPtrManager.storeFragment(fragment1);

    auto ptr2 = (void *)0x040000;
    FragmentStorage fragment2;
    fragment2.fragmentCpuPointer = ptr2;
    fragment2.fragmentSize = size1;
    hostPtrManager.storeFragment(fragment2);

    auto cptr1 = (void *)0x010060;
    auto frag1 = hostPtrManager.getFragment(cptr1);
    EXPECT_NE(frag1, nullptr);

    auto cptr2 = (void *)0x020000;
    auto frag2 = hostPtrManager.getFragment(cptr2);
    EXPECT_EQ(frag2, nullptr);

    auto cptr3 = (void *)0x040060;
    auto frag3 = hostPtrManager.getFragment(cptr3);
    EXPECT_NE(frag3, nullptr);

    auto cptr4 = (void *)0x060000;
    auto frag4 = hostPtrManager.getFragment(cptr4);
    EXPECT_EQ(frag4, nullptr);

    AllocationRequirements requiredAllocations;
    auto ptr3 = (void *)0x040000;
    auto size3 = MemoryConstants::pageSize * 2;
    requiredAllocations = hostPtrManager.getAllocationRequirements(ptr3, size3);
    auto catchme = false;
    try {
        OsHandleStorage st = hostPtrManager.populateAlreadyAllocatedFragments(requiredAllocations);
        EXPECT_EQ(st.fragmentCount, 0u);
    } catch (...) {
        catchme = true;
    }
    EXPECT_TRUE(catchme);
}

TEST(HostPtrManager, FragmentCheck) {
    MockHostPtrManager hostPtrManager;

    auto size1 = MemoryConstants::pageSize;

    auto ptr1 = (void *)0x010000;
    FragmentStorage fragment1;
    fragment1.fragmentCpuPointer = ptr1;
    fragment1.fragmentSize = size1;
    hostPtrManager.storeFragment(fragment1);

    auto ptr2 = (void *)0x040000;
    FragmentStorage fragment2;
    fragment2.fragmentCpuPointer = ptr2;
    fragment2.fragmentSize = size1;
    hostPtrManager.storeFragment(fragment2);

    OverlapStatus overlappingStatus;
    auto cptr1 = (void *)0x010060;
    auto frag1 = hostPtrManager.getFragmentAndCheckForOverlaps(cptr1, 1u, overlappingStatus);
    EXPECT_NE(frag1, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_WITHIN_STORED_FRAGMENT);

    frag1 = hostPtrManager.getFragmentAndCheckForOverlaps(ptr1, size1, overlappingStatus);
    EXPECT_NE(frag1, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_WITH_EXACT_SIZE_AS_STORED_FRAGMENT);

    frag1 = hostPtrManager.getFragmentAndCheckForOverlaps(ptr1, size1 - 1, overlappingStatus);
    EXPECT_NE(frag1, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_WITHIN_STORED_FRAGMENT);

    auto cptr2 = (void *)0x020000;
    auto frag2 = hostPtrManager.getFragmentAndCheckForOverlaps(cptr2, 1u, overlappingStatus);
    EXPECT_EQ(frag2, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER);

    auto cptr3 = (void *)0x040060;
    auto frag3 = hostPtrManager.getFragmentAndCheckForOverlaps(cptr3, 1u, overlappingStatus);
    EXPECT_NE(frag3, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_WITHIN_STORED_FRAGMENT);

    auto cptr4 = (void *)0x060000;
    auto frag4 = hostPtrManager.getFragmentAndCheckForOverlaps(cptr4, 1u, overlappingStatus);
    EXPECT_EQ(frag4, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER);

    auto cptr5 = (void *)0x040000;
    auto frag5 = hostPtrManager.getFragmentAndCheckForOverlaps(cptr5, size1 - 1, overlappingStatus);
    EXPECT_NE(frag5, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_WITHIN_STORED_FRAGMENT);

    auto cptr6 = (void *)0x040000;
    auto frag6 = hostPtrManager.getFragmentAndCheckForOverlaps(cptr6, size1 + 1, overlappingStatus);
    EXPECT_EQ(frag6, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT);

    auto cptr7 = (void *)0x03FFF0;
    auto frag7 = hostPtrManager.getFragmentAndCheckForOverlaps(cptr7, 2 * size1, overlappingStatus);
    EXPECT_EQ(frag7, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT);

    auto cptr8 = (void *)0x040000;
    auto frag8 = hostPtrManager.getFragmentAndCheckForOverlaps(cptr8, size1, overlappingStatus);
    EXPECT_NE(frag8, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_WITH_EXACT_SIZE_AS_STORED_FRAGMENT);

    auto cptr9 = (void *)0x010060;
    auto frag9 = hostPtrManager.getFragmentAndCheckForOverlaps(cptr9, 2 * size1, overlappingStatus);
    EXPECT_EQ(frag9, nullptr);
    EXPECT_EQ(overlappingStatus, OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT);
}

TEST(HostPtrManager, GivenHostPtrManagerFilledWithBigFragmentWhenAskedForFragmnetInTheMiddleOfBigFragmentThenBigFragmentIsReturned) {
    auto bigSize = 10 * MemoryConstants::pageSize;
    auto bigPtr = (void *)0x01000;
    FragmentStorage fragment;
    fragment.fragmentCpuPointer = bigPtr;
    fragment.fragmentSize = bigSize;
    MockHostPtrManager hostPtrManager;
    hostPtrManager.storeFragment(fragment);
    EXPECT_EQ(1u, hostPtrManager.getFragmentCount());

    auto ptrInTheMiddle = (void *)0x2000;
    auto smallSize = MemoryConstants::pageSize;

    auto storedBigFragment = hostPtrManager.getFragment(bigPtr);

    auto fragment2 = hostPtrManager.getFragment(ptrInTheMiddle);
    EXPECT_EQ(storedBigFragment, fragment2);

    OverlapStatus overlapStatus;
    auto fragment3 = hostPtrManager.getFragmentAndCheckForOverlaps(ptrInTheMiddle, smallSize, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_WITHIN_STORED_FRAGMENT, overlapStatus);
    EXPECT_EQ(fragment3, storedBigFragment);

    auto ptrOutside = (void *)0x1000000;
    auto outsideSize = 1;

    auto perfectMatchFragment = hostPtrManager.getFragmentAndCheckForOverlaps(bigPtr, bigSize, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_WITH_EXACT_SIZE_AS_STORED_FRAGMENT, overlapStatus);
    EXPECT_EQ(perfectMatchFragment, storedBigFragment);

    auto oustideFragment = hostPtrManager.getFragmentAndCheckForOverlaps(ptrOutside, outsideSize, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER, overlapStatus);
    EXPECT_EQ(nullptr, oustideFragment);

    //partialOverlap
    auto ptrPartial = (void *)(((uintptr_t)bigPtr + bigSize) - 100);
    auto partialBigSize = MemoryConstants::pageSize * 100;

    auto partialFragment = hostPtrManager.getFragmentAndCheckForOverlaps(ptrPartial, partialBigSize, overlapStatus);
    EXPECT_EQ(nullptr, partialFragment);
    EXPECT_EQ(OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT, overlapStatus);
}
TEST(HostPtrManager, GivenHostPtrManagerFilledWithFragmentsWhenCheckedForOverlappingThenProperOverlappingStatusIsReturned) {
    auto bigPtr = (void *)0x04000;
    auto bigSize = 10 * MemoryConstants::pageSize;
    FragmentStorage fragment;
    fragment.fragmentCpuPointer = bigPtr;
    fragment.fragmentSize = bigSize;
    MockHostPtrManager hostPtrManager;
    hostPtrManager.storeFragment(fragment);
    EXPECT_EQ(1u, hostPtrManager.getFragmentCount());

    auto ptrNonOverlapingPriorToBigPtr = (void *)0x2000;
    auto smallSize = MemoryConstants::pageSize;

    OverlapStatus overlapStatus;
    auto fragment2 = hostPtrManager.getFragmentAndCheckForOverlaps(ptrNonOverlapingPriorToBigPtr, smallSize, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER, overlapStatus);
    EXPECT_EQ(nullptr, fragment2);

    auto ptrNonOverlapingPriorToBigPtrByPage = (void *)0x3000;
    auto checkMatch = (uintptr_t)ptrNonOverlapingPriorToBigPtrByPage + smallSize;
    EXPECT_EQ(checkMatch, (uintptr_t)bigPtr);

    auto fragment3 = hostPtrManager.getFragmentAndCheckForOverlaps(ptrNonOverlapingPriorToBigPtrByPage, smallSize, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER, overlapStatus);
    EXPECT_EQ(nullptr, fragment3);

    auto fragment4 = hostPtrManager.getFragmentAndCheckForOverlaps(ptrNonOverlapingPriorToBigPtrByPage, smallSize + 1, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT, overlapStatus);
    EXPECT_EQ(nullptr, fragment4);
}
TEST(HostPtrManager, GivenEmptyHostPtrManagerWhenAskedForOverlapingThenNoOverlappingIsReturned) {
    MockHostPtrManager hostPtrManager;
    auto bigPtr = (void *)0x04000;
    auto bigSize = 10 * MemoryConstants::pageSize;

    OverlapStatus overlapStatus;
    auto fragment3 = hostPtrManager.getFragmentAndCheckForOverlaps(bigPtr, bigSize, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER, overlapStatus);
    EXPECT_EQ(nullptr, fragment3);
}

TEST(HostPtrManager, GivenHostPtrManagerFilledWithFragmentsWhenAskedForOverlpaingThenProperStatusIsReturned) {
    auto bigPtr1 = (void *)0x01000;
    auto bigPtr2 = (void *)0x03000;
    auto bigSize = MemoryConstants::pageSize;
    FragmentStorage fragment;
    fragment.fragmentCpuPointer = bigPtr1;
    fragment.fragmentSize = bigSize;
    MockHostPtrManager hostPtrManager;
    hostPtrManager.storeFragment(fragment);
    fragment.fragmentCpuPointer = bigPtr2;
    hostPtrManager.storeFragment(fragment);
    EXPECT_EQ(2u, hostPtrManager.getFragmentCount());

    auto ptrNonOverlapingInTheMiddleOfBigPtrs = (void *)0x2000;
    auto ptrNonOverlapingAfterBigPtr = (void *)0x4000;
    auto ptrNonOverlapingBeforeBigPtr = (void *)0;
    OverlapStatus overlapStatus;
    auto fragment1 = hostPtrManager.getFragmentAndCheckForOverlaps(ptrNonOverlapingInTheMiddleOfBigPtrs, bigSize, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER, overlapStatus);
    EXPECT_EQ(nullptr, fragment1);

    auto fragment2 = hostPtrManager.getFragmentAndCheckForOverlaps(ptrNonOverlapingInTheMiddleOfBigPtrs, bigSize * 5, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT, overlapStatus);
    EXPECT_EQ(nullptr, fragment2);

    auto fragment3 = hostPtrManager.getFragmentAndCheckForOverlaps(ptrNonOverlapingAfterBigPtr, bigSize * 5, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER, overlapStatus);
    EXPECT_EQ(nullptr, fragment3);

    auto fragment4 = hostPtrManager.getFragmentAndCheckForOverlaps(ptrNonOverlapingBeforeBigPtr, bigSize, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER, overlapStatus);
    EXPECT_EQ(nullptr, fragment4);
}
TEST(HostPtrManager, GivenHostPtrManagerFilledWithFragmentsWhenAskedForOverlapingThenProperOverlapingStatusIsReturned) {
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
    hostPtrManager.storeFragment(fragment);
    fragment.fragmentCpuPointer = bigPtr2;
    fragment.fragmentSize = bigSize2;
    hostPtrManager.storeFragment(fragment);
    fragment.fragmentCpuPointer = bigPtr3;
    fragment.fragmentSize = bigSize3;
    hostPtrManager.storeFragment(fragment);

    EXPECT_EQ(3u, hostPtrManager.getFragmentCount());

    OverlapStatus overlapStatus;
    auto fragment1 = hostPtrManager.getFragmentAndCheckForOverlaps(bigPtr1, bigSize1 + 1, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT, overlapStatus);
    EXPECT_EQ(nullptr, fragment1);

    auto priorToBig1 = (void *)0x9999;

    auto fragment2 = hostPtrManager.getFragmentAndCheckForOverlaps(priorToBig1, 1, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER, overlapStatus);
    EXPECT_EQ(nullptr, fragment2);

    auto middleOfBig3 = (void *)0x11111;
    auto fragment3 = hostPtrManager.getFragmentAndCheckForOverlaps(middleOfBig3, 1, overlapStatus);
    EXPECT_EQ(OverlapStatus::FRAGMENT_WITHIN_STORED_FRAGMENT, overlapStatus);
    EXPECT_NE(nullptr, fragment3);
}

using HostPtrAllocationTest = Test<MemoryManagerWithCsrFixture>;

TEST_F(HostPtrAllocationTest, givenTwoAllocationsThatSharesOneFragmentWhenOneIsDestroyedThenFragmentRemains) {

    void *cpuPtr1 = reinterpret_cast<void *>(0x100001);
    void *cpuPtr2 = ptrOffset(cpuPtr1, MemoryConstants::pageSize);

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());
    auto graphicsAllocation1 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, 2 * MemoryConstants::pageSize - 1}, cpuPtr1);
    EXPECT_EQ(2u, hostPtrManager->getFragmentCount());

    auto graphicsAllocation2 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, MemoryConstants::pageSize}, cpuPtr2);
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
        auto requirements = hostPtrManager->getAllocationRequirements(cpuPtr, allocationSize);
        EXPECT_EQ(expectedFragmentCount, requirements.requiredFragmentsCount);
        auto osStorage = hostPtrManager->prepareOsStorageForAllocation(*memoryManager, allocationSize, cpuPtr);
        EXPECT_EQ(expectedFragmentCount, osStorage.fragmentCount);
        EXPECT_EQ(expectedFragmentCount, hostPtrManager->getFragmentCount());
        hostPtrManager->releaseHandleStorage(osStorage);
        memoryManager->cleanOsHandles(osStorage);
        EXPECT_EQ(0u, hostPtrManager->getFragmentCount());
    }
}

TEST_F(HostPtrAllocationTest, whenOverlappedFragmentIsBiggerThenStoredAndStoredFragmentIsDestroyedDuringSecondCleaningThenCheckForOverlappingReturnsSuccess) {

    void *cpuPtr1 = (void *)0x100004;

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());
    auto graphicsAllocation1 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, MemoryConstants::pageSize}, cpuPtr1);
    EXPECT_EQ(2u, hostPtrManager->getFragmentCount());

    EXPECT_NE(nullptr, graphicsAllocation1);

    auto fragment1 = hostPtrManager->getFragment(alignDown(cpuPtr1, MemoryConstants::pageSize));
    EXPECT_NE(nullptr, fragment1);
    auto fragment2 = hostPtrManager->getFragment(alignUp(cpuPtr1, MemoryConstants::pageSize));
    EXPECT_NE(nullptr, fragment2);

    uint32_t taskCountReady = 2;
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
    requirements.allocationFragments[0].fragmentPosition = FragmentPosition::NONE;

    RequirementsStatus status = hostPtrManager->checkAllocationsForOverlapping(*memoryManager, &requirements);

    EXPECT_EQ(RequirementsStatus::SUCCESS, status);
}

HWTEST_F(HostPtrAllocationTest, givenOverlappingFragmentsWhenCheckIsCalledThenWaitAndCleanOnAllEngines) {
    uint32_t taskCountReady = 2;
    uint32_t taskCountNotReady = 1;

    auto &engines = memoryManager->getRegisteredEngines();
    EXPECT_EQ(1u, engines.size());

    auto csr0 = static_cast<MockCommandStreamReceiver *>(engines[0].commandStreamReceiver);
    auto csr1 = std::make_unique<MockCommandStreamReceiver>(executionEnvironment, 0);
    uint32_t csr0GpuTag = taskCountNotReady;
    uint32_t csr1GpuTag = taskCountNotReady;
    csr0->tagAddress = &csr0GpuTag;
    csr1->tagAddress = &csr1GpuTag;
    auto osContext = memoryManager->createAndRegisterOsContext(csr1.get(), aub_stream::EngineType::ENGINE_RCS, 0, PreemptionMode::Disabled, true);
    csr1->setupContext(*osContext);

    void *cpuPtr = reinterpret_cast<void *>(0x100004);

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());
    auto graphicsAllocation0 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, MemoryConstants::pageSize}, cpuPtr);
    auto graphicsAllocation1 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, MemoryConstants::pageSize}, cpuPtr);

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
    requirements.allocationFragments[0].fragmentPosition = FragmentPosition::NONE;

    hostPtrManager->checkAllocationsForOverlapping(*memoryManager, &requirements);

    EXPECT_EQ(1u, csr0->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(1u, csr1->waitForCompletionWithTimeoutCalled);
    EXPECT_EQ(2u, storage0->cleanAllocationsCalled);
    EXPECT_EQ(2u, storage0->lastCleanAllocationsTaskCount);
    EXPECT_EQ(2u, storage1->cleanAllocationsCalled);
    EXPECT_EQ(2u, storage1->lastCleanAllocationsTaskCount);
}

TEST_F(HostPtrAllocationTest, whenOverlappedFragmentIsBiggerThenStoredAndStoredFragmentCannotBeDestroyedThenCheckForOverlappingReturnsError) {

    void *cpuPtr1 = (void *)0x100004;

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());
    auto graphicsAllocation1 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, MemoryConstants::pageSize}, cpuPtr1);
    EXPECT_EQ(2u, hostPtrManager->getFragmentCount());

    EXPECT_NE(nullptr, graphicsAllocation1);

    auto fragment1 = hostPtrManager->getFragment(alignDown(cpuPtr1, MemoryConstants::pageSize));
    EXPECT_NE(nullptr, fragment1);
    auto fragment2 = hostPtrManager->getFragment(alignUp(cpuPtr1, MemoryConstants::pageSize));
    EXPECT_NE(nullptr, fragment2);

    uint32_t taskCountReady = 2;
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
    requirements.allocationFragments[0].fragmentPosition = FragmentPosition::NONE;

    RequirementsStatus status = hostPtrManager->checkAllocationsForOverlapping(*memoryManager, &requirements);

    EXPECT_EQ(RequirementsStatus::FATAL, status);
}

TEST_F(HostPtrAllocationTest, checkAllocationsForOverlappingWithoutBiggerOverlap) {

    void *cpuPtr1 = (void *)0x100004;
    void *cpuPtr2 = (void *)0x101008;

    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());
    auto graphicsAllocation1 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, MemoryConstants::pageSize}, cpuPtr1);
    EXPECT_EQ(2u, hostPtrManager->getFragmentCount());

    auto graphicsAllocation2 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, MemoryConstants::pageSize * 3}, cpuPtr2);
    EXPECT_EQ(4u, hostPtrManager->getFragmentCount());

    EXPECT_NE(nullptr, graphicsAllocation1);
    EXPECT_NE(nullptr, graphicsAllocation2);

    auto fragment1 = hostPtrManager->getFragment(alignDown(cpuPtr1, MemoryConstants::pageSize));
    EXPECT_NE(nullptr, fragment1);
    auto fragment2 = hostPtrManager->getFragment(alignUp(cpuPtr1, MemoryConstants::pageSize));
    EXPECT_NE(nullptr, fragment2);
    auto fragment3 = hostPtrManager->getFragment(alignDown(cpuPtr2, MemoryConstants::pageSize));
    EXPECT_NE(nullptr, fragment3);
    auto fragment4 = hostPtrManager->getFragment(alignUp(cpuPtr2, MemoryConstants::pageSize));
    EXPECT_NE(nullptr, fragment4);

    AllocationRequirements requirements;

    requirements.requiredFragmentsCount = 2;
    requirements.totalRequiredSize = MemoryConstants::pageSize * 2;

    requirements.allocationFragments[0].allocationPtr = alignDown(cpuPtr1, MemoryConstants::pageSize);
    requirements.allocationFragments[0].allocationSize = MemoryConstants::pageSize;
    requirements.allocationFragments[0].fragmentPosition = FragmentPosition::LEADING;

    requirements.allocationFragments[1].allocationPtr = alignUp(cpuPtr1, MemoryConstants::pageSize);
    requirements.allocationFragments[1].allocationSize = MemoryConstants::pageSize;
    requirements.allocationFragments[1].fragmentPosition = FragmentPosition::TRAILING;

    RequirementsStatus status = hostPtrManager->checkAllocationsForOverlapping(*memoryManager, &requirements);

    EXPECT_EQ(RequirementsStatus::SUCCESS, status);

    memoryManager->freeGraphicsMemory(graphicsAllocation1);
    memoryManager->freeGraphicsMemory(graphicsAllocation2);
}

TEST_F(HostPtrAllocationTest, checkAllocationsForOverlappingWithBiggerOverlapUntilFirstClean) {

    void *cpuPtr1 = (void *)0x100004;

    auto graphicsAllocation1 = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{false, MemoryConstants::pageSize}, cpuPtr1);
    auto hostPtrManager = static_cast<MockHostPtrManager *>(memoryManager->getHostPtrManager());
    EXPECT_EQ(2u, hostPtrManager->getFragmentCount());

    EXPECT_NE(nullptr, graphicsAllocation1);

    auto fragment1 = hostPtrManager->getFragment(alignDown(cpuPtr1, MemoryConstants::pageSize));
    EXPECT_NE(nullptr, fragment1);
    auto fragment2 = hostPtrManager->getFragment(alignUp(cpuPtr1, MemoryConstants::pageSize));
    EXPECT_NE(nullptr, fragment2);

    uint32_t taskCountReady = 1;
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
    requirements.allocationFragments[0].fragmentPosition = FragmentPosition::NONE;

    RequirementsStatus status = hostPtrManager->checkAllocationsForOverlapping(*memoryManager, &requirements);

    EXPECT_EQ(RequirementsStatus::SUCCESS, status);
}

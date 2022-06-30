/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/host_ptr_manager.h"

#include "shared/source/memory_manager/memory_manager.h"

using namespace NEO;

HostPtrFragmentsContainer::iterator HostPtrManager::findElement(HostPtrEntryKey key) {
    auto nextElement = partialAllocations.lower_bound(key);
    auto element = nextElement;
    if (element != partialAllocations.end()) {

        auto &storedFragment = element->second;
        if (element->first.rootDeviceIndex == key.rootDeviceIndex && storedFragment.fragmentCpuPointer == key.ptr) {
            return element;
        }
    }
    if (element != partialAllocations.begin()) {
        element--;
        if (element->first.rootDeviceIndex != key.rootDeviceIndex) {
            return partialAllocations.end();
        }
        auto &storedFragment = element->second;
        auto storedEndAddress = reinterpret_cast<uintptr_t>(storedFragment.fragmentCpuPointer) + storedFragment.fragmentSize;
        if (storedFragment.fragmentSize == 0) {
            storedEndAddress++;
        }
        if (reinterpret_cast<uintptr_t>(key.ptr) < storedEndAddress) {
            return element;
        }
    }
    return partialAllocations.end();
}

AllocationRequirements HostPtrManager::getAllocationRequirements(uint32_t rootDeviceIndex, const void *inputPtr, size_t size) {
    AllocationRequirements requiredAllocations;
    requiredAllocations.rootDeviceIndex = rootDeviceIndex;

    auto allocationCount = 0;
    auto wholeAllocationSize = alignSizeWholePage(inputPtr, size);

    auto alignedStartAddress = alignDown(inputPtr, MemoryConstants::pageSize);
    bool leadingNeeded = false;

    if (alignedStartAddress != inputPtr) {
        leadingNeeded = true;
        requiredAllocations.allocationFragments[allocationCount].allocationPtr = alignedStartAddress;
        requiredAllocations.allocationFragments[allocationCount].fragmentPosition = FragmentPosition::LEADING;
        requiredAllocations.allocationFragments[allocationCount].allocationSize = MemoryConstants::pageSize;
        allocationCount++;
    }

    auto endAddress = ptrOffset(inputPtr, size);
    auto alignedEndAddress = alignDown(endAddress, MemoryConstants::pageSize);
    bool trailingNeeded = false;

    if (alignedEndAddress != endAddress && alignedEndAddress != alignedStartAddress) {
        trailingNeeded = true;
    }

    auto middleSize = wholeAllocationSize - (trailingNeeded + leadingNeeded) * MemoryConstants::pageSize;
    if (middleSize) {
        requiredAllocations.allocationFragments[allocationCount].allocationPtr = alignUp(inputPtr, MemoryConstants::pageSize);
        requiredAllocations.allocationFragments[allocationCount].fragmentPosition = FragmentPosition::MIDDLE;
        requiredAllocations.allocationFragments[allocationCount].allocationSize = middleSize;
        allocationCount++;
    }

    if (trailingNeeded) {
        requiredAllocations.allocationFragments[allocationCount].allocationPtr = alignedEndAddress;
        requiredAllocations.allocationFragments[allocationCount].fragmentPosition = FragmentPosition::TRAILING;
        requiredAllocations.allocationFragments[allocationCount].allocationSize = MemoryConstants::pageSize;
        allocationCount++;
    }

    requiredAllocations.totalRequiredSize = wholeAllocationSize;
    requiredAllocations.requiredFragmentsCount = allocationCount;

    return requiredAllocations;
}

OsHandleStorage HostPtrManager::populateAlreadyAllocatedFragments(AllocationRequirements &requirements) {
    OsHandleStorage handleStorage;
    for (unsigned int i = 0; i < requirements.requiredFragmentsCount; i++) {
        OverlapStatus overlapStatus = OverlapStatus::FRAGMENT_NOT_CHECKED;
        FragmentStorage *fragmentStorage = getFragmentAndCheckForOverlaps(requirements.rootDeviceIndex, const_cast<void *>(requirements.allocationFragments[i].allocationPtr),
                                                                          requirements.allocationFragments[i].allocationSize, overlapStatus);
        if (overlapStatus == OverlapStatus::FRAGMENT_WITHIN_STORED_FRAGMENT) {
            UNRECOVERABLE_IF(fragmentStorage == nullptr);
            fragmentStorage->refCount++;
            handleStorage.fragmentStorageData[i].osHandleStorage = fragmentStorage->osInternalStorage;
            handleStorage.fragmentStorageData[i].cpuPtr = requirements.allocationFragments[i].allocationPtr;
            handleStorage.fragmentStorageData[i].fragmentSize = requirements.allocationFragments[i].allocationSize;
            handleStorage.fragmentStorageData[i].residency = fragmentStorage->residency;
        } else if (overlapStatus != OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT) {
            if (fragmentStorage != nullptr) {
                DEBUG_BREAK_IF(overlapStatus != OverlapStatus::FRAGMENT_WITH_EXACT_SIZE_AS_STORED_FRAGMENT);
                fragmentStorage->refCount++;
                handleStorage.fragmentStorageData[i].osHandleStorage = fragmentStorage->osInternalStorage;
                handleStorage.fragmentStorageData[i].residency = fragmentStorage->residency;
            } else {
                DEBUG_BREAK_IF(overlapStatus != OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER);
            }
            handleStorage.fragmentStorageData[i].cpuPtr = requirements.allocationFragments[i].allocationPtr;
            handleStorage.fragmentStorageData[i].fragmentSize = requirements.allocationFragments[i].allocationSize;
        } else {
            //abort whole application instead of silently passing.
            abortExecution();
            return handleStorage;
        }
    }
    handleStorage.fragmentCount = requirements.requiredFragmentsCount;
    return handleStorage;
}

void HostPtrManager::storeFragment(uint32_t rootDeviceIndex, FragmentStorage &fragment) {
    std::lock_guard<decltype(allocationsMutex)> lock(allocationsMutex);
    HostPtrEntryKey key{fragment.fragmentCpuPointer, rootDeviceIndex};
    auto element = findElement(key);
    if (element != partialAllocations.end()) {
        element->second.refCount++;
    } else {
        fragment.refCount++;
        partialAllocations.insert(std::pair<HostPtrEntryKey, FragmentStorage>(key, fragment));
    }
}

void HostPtrManager::storeFragment(uint32_t rootDeviceIndex, AllocationStorageData &storageData) {
    FragmentStorage fragment;
    fragment.fragmentCpuPointer = const_cast<void *>(storageData.cpuPtr);
    fragment.fragmentSize = storageData.fragmentSize;
    fragment.osInternalStorage = storageData.osHandleStorage;
    fragment.residency = storageData.residency;
    storeFragment(rootDeviceIndex, fragment);
}

std::unique_lock<std::recursive_mutex> HostPtrManager::obtainOwnership() {
    return std::unique_lock<std::recursive_mutex>(allocationsMutex);
}

void HostPtrManager::releaseHandleStorage(uint32_t rootDeviceIndex, OsHandleStorage &fragments) {
    for (int i = 0; i < maxFragmentsCount; i++) {
        if (fragments.fragmentStorageData[i].fragmentSize || fragments.fragmentStorageData[i].cpuPtr) {
            fragments.fragmentStorageData[i].freeTheFragment = releaseHostPtr(rootDeviceIndex, fragments.fragmentStorageData[i].cpuPtr);
        }
    }
}

bool HostPtrManager::releaseHostPtr(uint32_t rootDeviceIndex, const void *ptr) {
    std::lock_guard<decltype(allocationsMutex)> lock(allocationsMutex);
    bool fragmentReadyToBeReleased = false;

    auto element = findElement({ptr, rootDeviceIndex});

    DEBUG_BREAK_IF(element == partialAllocations.end());

    element->second.refCount--;
    if (element->second.refCount <= 0) {
        fragmentReadyToBeReleased = true;
        partialAllocations.erase(element);
    }

    return fragmentReadyToBeReleased;
}

FragmentStorage *HostPtrManager::getFragment(HostPtrEntryKey key) {
    std::lock_guard<decltype(allocationsMutex)> lock(allocationsMutex);
    auto element = findElement(key);
    if (element != partialAllocations.end()) {
        return &element->second;
    }
    return nullptr;
}

//for given inputs see if any allocation overlaps
FragmentStorage *HostPtrManager::getFragmentAndCheckForOverlaps(uint32_t rootDeviceIndex, const void *inPtr, size_t size, OverlapStatus &overlappingStatus) {
    std::lock_guard<decltype(allocationsMutex)> lock(allocationsMutex);
    void *inputPtr = const_cast<void *>(inPtr);
    auto nextElement = partialAllocations.lower_bound({inputPtr, rootDeviceIndex});
    auto element = nextElement;
    overlappingStatus = OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER;

    if (element != partialAllocations.begin()) {
        element--;
    }

    if (element != partialAllocations.end()) {
        if (element->first.rootDeviceIndex != rootDeviceIndex) {
            return nullptr;
        }
        auto &storedFragment = element->second;
        if (storedFragment.fragmentCpuPointer == inputPtr && storedFragment.fragmentSize == size) {
            overlappingStatus = OverlapStatus::FRAGMENT_WITH_EXACT_SIZE_AS_STORED_FRAGMENT;
            return &element->second;
        }

        auto storedEndAddress = (uintptr_t)storedFragment.fragmentCpuPointer + storedFragment.fragmentSize;
        auto inputEndAddress = (uintptr_t)inputPtr + size;

        if (inputPtr >= storedFragment.fragmentCpuPointer && (uintptr_t)inputPtr < (uintptr_t)storedEndAddress) {
            if (inputEndAddress <= storedEndAddress) {
                overlappingStatus = OverlapStatus::FRAGMENT_WITHIN_STORED_FRAGMENT;
                return &element->second;
            } else {
                overlappingStatus = OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT;
                return nullptr;
            }
        }
        //next fragment doesn't have to be after the inputPtr
        if (nextElement != partialAllocations.end()) {
            if (nextElement->first.rootDeviceIndex != rootDeviceIndex) {
                return nullptr;
            }
            auto &storedNextElement = nextElement->second;
            auto storedNextEndAddress = (uintptr_t)storedNextElement.fragmentCpuPointer + storedNextElement.fragmentSize;
            auto storedNextStartAddress = (uintptr_t)storedNextElement.fragmentCpuPointer;
            //check if this allocation is after the inputPtr
            if ((uintptr_t)inputPtr < storedNextStartAddress) {
                if (inputEndAddress > storedNextStartAddress) {
                    overlappingStatus = OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT;
                    return nullptr;
                }
            } else if (inputEndAddress > storedNextEndAddress) {
                overlappingStatus = OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT;
                return nullptr;
            } else {
                DEBUG_BREAK_IF((uintptr_t)inputPtr != storedNextStartAddress);
                if (inputEndAddress < storedNextEndAddress) {
                    overlappingStatus = OverlapStatus::FRAGMENT_WITHIN_STORED_FRAGMENT;
                } else {
                    DEBUG_BREAK_IF(inputEndAddress != storedNextEndAddress);
                    overlappingStatus = OverlapStatus::FRAGMENT_WITH_EXACT_SIZE_AS_STORED_FRAGMENT;
                }
                return &nextElement->second;
            }
        }
    }
    return nullptr;
}

OsHandleStorage HostPtrManager::prepareOsStorageForAllocation(MemoryManager &memoryManager, size_t size, const void *ptr, uint32_t rootDeviceIndex) {
    std::lock_guard<decltype(allocationsMutex)> lock(allocationsMutex);
    auto requirements = HostPtrManager::getAllocationRequirements(rootDeviceIndex, ptr, size);
    UNRECOVERABLE_IF(checkAllocationsForOverlapping(memoryManager, &requirements) == RequirementsStatus::FATAL);
    auto osStorage = populateAlreadyAllocatedFragments(requirements);
    if (osStorage.fragmentCount > 0) {
        if (memoryManager.populateOsHandles(osStorage, rootDeviceIndex) != MemoryManager::AllocationStatus::Success) {
            memoryManager.cleanOsHandles(osStorage, rootDeviceIndex);
            osStorage.fragmentCount = 0;
        }
    }
    return osStorage;
}

RequirementsStatus HostPtrManager::checkAllocationsForOverlapping(MemoryManager &memoryManager, AllocationRequirements *requirements) {
    UNRECOVERABLE_IF(requirements == nullptr);

    RequirementsStatus status = RequirementsStatus::SUCCESS;

    for (unsigned int i = 0; i < requirements->requiredFragmentsCount; i++) {
        OverlapStatus overlapStatus = OverlapStatus::FRAGMENT_NOT_CHECKED;

        getFragmentAndCheckForOverlaps(requirements->rootDeviceIndex, requirements->allocationFragments[i].allocationPtr,
                                       requirements->allocationFragments[i].allocationSize, overlapStatus);
        if (overlapStatus == OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT) {
            // clean temporary allocations
            memoryManager.cleanTemporaryAllocationListOnAllEngines(false);

            // check overlapping again
            getFragmentAndCheckForOverlaps(requirements->rootDeviceIndex, requirements->allocationFragments[i].allocationPtr,
                                           requirements->allocationFragments[i].allocationSize, overlapStatus);
            if (overlapStatus == OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT) {

                // Wait for completion
                memoryManager.cleanTemporaryAllocationListOnAllEngines(true);

                // check overlapping last time
                getFragmentAndCheckForOverlaps(requirements->rootDeviceIndex, requirements->allocationFragments[i].allocationPtr,
                                               requirements->allocationFragments[i].allocationSize, overlapStatus);
                if (overlapStatus == OverlapStatus::FRAGMENT_OVERLAPING_AND_BIGGER_THEN_STORED_FRAGMENT) {
                    status = RequirementsStatus::FATAL;
                    break;
                }
            }
        }
    }
    return status;
}

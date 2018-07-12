/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "host_ptr_manager.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/abort.h"

using namespace OCLRT;

std::map<const void *, FragmentStorage>::iterator OCLRT::HostPtrManager::findElement(const void *ptr) {
    auto nextElement = partialAllocations.lower_bound(ptr);
    auto element = nextElement;
    if (element != partialAllocations.end()) {
        auto &storedFragment = element->second;
        if (storedFragment.fragmentCpuPointer <= ptr) {
            return element;
        } else if (element != partialAllocations.begin()) {
            element--;
            auto &storedFragment = element->second;
            auto storedEndAddress = (uintptr_t)storedFragment.fragmentCpuPointer + storedFragment.fragmentSize;
            if (storedFragment.fragmentSize == 0) {
                storedEndAddress++;
            }
            if ((uintptr_t)ptr < (uintptr_t)storedEndAddress) {
                return element;
            }
        }
    } else if (element != partialAllocations.begin()) {
        element--;
        auto &storedFragment = element->second;
        auto storedEndAddress = (uintptr_t)storedFragment.fragmentCpuPointer + storedFragment.fragmentSize;
        if (storedFragment.fragmentSize == 0) {
            storedEndAddress++;
        }
        if ((uintptr_t)ptr < (uintptr_t)storedEndAddress) {
            return element;
        }
    }
    return partialAllocations.end();
}

AllocationRequirements OCLRT::HostPtrManager::getAllocationRequirements(const void *inputPtr, size_t size) {
    AllocationRequirements requiredAllocations;

    auto allocationCount = 0;
    auto wholeAllocationSize = alignSizeWholePage(inputPtr, size);

    auto alignedStartAddress = alignDown(inputPtr, MemoryConstants::pageSize);
    bool leadingNeeded = false;

    if (alignedStartAddress != inputPtr) {
        leadingNeeded = true;
        requiredAllocations.AllocationFragments[allocationCount].allocationPtr = alignedStartAddress;
        requiredAllocations.AllocationFragments[allocationCount].fragmentPosition = FragmentPosition::LEADING;
        requiredAllocations.AllocationFragments[allocationCount].allocationSize = MemoryConstants::pageSize;
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
        requiredAllocations.AllocationFragments[allocationCount].allocationPtr = alignUp(inputPtr, MemoryConstants::pageSize);
        requiredAllocations.AllocationFragments[allocationCount].fragmentPosition = FragmentPosition::MIDDLE;
        requiredAllocations.AllocationFragments[allocationCount].allocationSize = middleSize;
        allocationCount++;
    }

    if (trailingNeeded) {
        requiredAllocations.AllocationFragments[allocationCount].allocationPtr = alignedEndAddress;
        requiredAllocations.AllocationFragments[allocationCount].fragmentPosition = FragmentPosition::TRAILING;
        requiredAllocations.AllocationFragments[allocationCount].allocationSize = MemoryConstants::pageSize;
        allocationCount++;
    }

    requiredAllocations.totalRequiredSize = wholeAllocationSize;
    requiredAllocations.requiredFragmentsCount = allocationCount;

    return requiredAllocations;
}

OsHandleStorage OCLRT::HostPtrManager::populateAlreadyAllocatedFragments(AllocationRequirements &requirements, CheckedFragments *checkedFragments) {
    OsHandleStorage handleStorage;
    for (unsigned int i = 0; i < requirements.requiredFragmentsCount; i++) {
        OverlapStatus overlapStatus = OverlapStatus::FRAGMENT_NOT_CHECKED;
        FragmentStorage *fragmentStorage = nullptr;

        if (checkedFragments != nullptr) {
            DEBUG_BREAK_IF(checkedFragments->count <= i);
            overlapStatus = checkedFragments->status[i];
            DEBUG_BREAK_IF(!(checkedFragments->fragments[i] != nullptr || overlapStatus == OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER));
            fragmentStorage = checkedFragments->fragments[i];
        }

        if (overlapStatus == OverlapStatus::FRAGMENT_NOT_CHECKED)
            fragmentStorage = getFragmentAndCheckForOverlaps(const_cast<void *>(requirements.AllocationFragments[i].allocationPtr), requirements.AllocationFragments[i].allocationSize, overlapStatus);

        if (overlapStatus == OverlapStatus::FRAGMENT_WITHIN_STORED_FRAGMENT) {
            DEBUG_BREAK_IF(fragmentStorage == nullptr);
            fragmentStorage->refCount++;
            handleStorage.fragmentStorageData[i].osHandleStorage = fragmentStorage->osInternalStorage;
            handleStorage.fragmentStorageData[i].cpuPtr = requirements.AllocationFragments[i].allocationPtr;
            handleStorage.fragmentStorageData[i].fragmentSize = requirements.AllocationFragments[i].allocationSize;
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
            handleStorage.fragmentStorageData[i].cpuPtr = requirements.AllocationFragments[i].allocationPtr;
            handleStorage.fragmentStorageData[i].fragmentSize = requirements.AllocationFragments[i].allocationSize;
        } else {
            //abort whole application instead of silently passing.
            abortExecution();
            return handleStorage;
        }
    }
    handleStorage.fragmentCount = requirements.requiredFragmentsCount;
    return handleStorage;
}

void OCLRT::HostPtrManager::storeFragment(FragmentStorage &fragment) {
    std::lock_guard<std::mutex> lock(allocationsMutex);
    auto element = findElement(fragment.fragmentCpuPointer);
    if (element != partialAllocations.end()) {
        element->second.refCount++;
    } else {
        fragment.refCount++;
        partialAllocations.insert(std::pair<const void *, FragmentStorage>(fragment.fragmentCpuPointer, fragment));
    }
}

void OCLRT::HostPtrManager::storeFragment(AllocationStorageData &storageData) {
    FragmentStorage fragment;
    fragment.fragmentCpuPointer = const_cast<void *>(storageData.cpuPtr);
    fragment.fragmentSize = storageData.fragmentSize;
    fragment.osInternalStorage = storageData.osHandleStorage;
    fragment.residency = storageData.residency;
    storeFragment(fragment);
}

void OCLRT::HostPtrManager::releaseHandleStorage(OsHandleStorage &fragments) {
    for (int i = 0; i < max_fragments_count; i++) {
        if (fragments.fragmentStorageData[i].fragmentSize || fragments.fragmentStorageData[i].cpuPtr) {
            fragments.fragmentStorageData[i].freeTheFragment = releaseHostPtr(fragments.fragmentStorageData[i].cpuPtr);
        }
    }
}

bool OCLRT::HostPtrManager::releaseHostPtr(const void *ptr) {
    std::lock_guard<std::mutex> lock(allocationsMutex);
    bool fragmentReadyToBeReleased = false;

    auto element = findElement(ptr);

    DEBUG_BREAK_IF(element == partialAllocations.end());

    element->second.refCount--;
    if (element->second.refCount <= 0) {
        fragmentReadyToBeReleased = true;
        partialAllocations.erase(element);
    }

    return fragmentReadyToBeReleased;
}

FragmentStorage *OCLRT::HostPtrManager::getFragment(const void *inputPtr) {
    std::lock_guard<std::mutex> lock(allocationsMutex);
    auto element = findElement(inputPtr);
    if (element != partialAllocations.end()) {
        return &element->second;
    }
    return nullptr;
}

//for given inputs see if any allocation overlaps
FragmentStorage *OCLRT::HostPtrManager::getFragmentAndCheckForOverlaps(const void *inPtr, size_t size, OverlapStatus &overlappingStatus) {
    std::lock_guard<std::mutex> lock(allocationsMutex);
    void *inputPtr = const_cast<void *>(inPtr);
    auto nextElement = partialAllocations.lower_bound(inputPtr);
    auto element = nextElement;
    overlappingStatus = OverlapStatus::FRAGMENT_NOT_OVERLAPING_WITH_ANY_OTHER;

    if (element != partialAllocations.begin()) {
        element--;
    }

    if (element != partialAllocations.end()) {
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

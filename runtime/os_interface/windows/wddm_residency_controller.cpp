/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "wddm_residency_controller.h"
#include "runtime/os_interface/windows/wddm_allocation.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/utilities/spinlock.h"

namespace OCLRT {

WddmResidencyController::WddmResidencyController() : lock(false), lastTrimFenceValue(0u) {}

void WddmResidencyController::acquireLock() {
    bool previousLockValue = false;
    while (!lock.compare_exchange_weak(previousLockValue, true))
        previousLockValue = false;
}

void WddmResidencyController::releaseLock() {
    lock = false;
}

void WddmResidencyController::acquireTrimCallbackLock() {
    SpinLock spinLock;
    spinLock.enter(this->trimCallbackLock);
}

void WddmResidencyController::releaseTrimCallbackLock() {
    SpinLock spinLock;
    spinLock.leave(this->trimCallbackLock);
}

WddmAllocation *WddmResidencyController::getTrimCandidateHead() {
    uint32_t i = 0;
    const size_t size = trimCandidateList.size();

    if (size == 0) {
        return nullptr;
    }
    while ((trimCandidateList[i] == nullptr) && (i < size))
        i++;

    return reinterpret_cast<WddmAllocation *>(trimCandidateList[i]);
}

void WddmResidencyController::addToTrimCandidateList(GraphicsAllocation *allocation) {
    WddmAllocation *wddmAllocation = (WddmAllocation *)allocation;
    size_t position = trimCandidateList.size();

    DEBUG_BREAK_IF(trimCandidatesCount > trimCandidateList.size());

    if (wddmAllocation->getTrimCandidateListPosition() == trimListUnusedPosition) {
        trimCandidatesCount++;
        trimCandidateList.push_back(allocation);
        wddmAllocation->setTrimCandidateListPosition(position);
    }

    checkTrimCandidateCount();
}

void WddmResidencyController::removeFromTrimCandidateList(GraphicsAllocation *allocation, bool compactList) {
    WddmAllocation *wddmAllocation = (WddmAllocation *)allocation;
    size_t position = wddmAllocation->getTrimCandidateListPosition();

    DEBUG_BREAK_IF(!(trimCandidatesCount > (trimCandidatesCount - 1)));
    DEBUG_BREAK_IF(trimCandidatesCount > trimCandidateList.size());

    trimCandidatesCount--;

    trimCandidateList[position] = nullptr;

    checkTrimCandidateCount();

    if (position == trimCandidateList.size() - 1) {
        size_t erasePosition = position;

        if (position == 0) {
            trimCandidateList.resize(0);
        } else {
            while (trimCandidateList[erasePosition] == nullptr && erasePosition > 0) {
                erasePosition--;
            }

            size_t sizeRemaining = erasePosition + 1;
            if (erasePosition == 0 && trimCandidateList[erasePosition] == nullptr) {
                sizeRemaining = 0;
            }

            trimCandidateList.resize(sizeRemaining);
        }
    }
    wddmAllocation->setTrimCandidateListPosition(trimListUnusedPosition);

    if (compactList && checkTrimCandidateListCompaction()) {
        compactTrimCandidateList();
    }

    checkTrimCandidateCount();
}

void WddmResidencyController::checkTrimCandidateCount() {
    if (DebugManager.flags.ResidencyDebugEnable.get()) {
        uint32_t sum = 0;
        for (size_t i = 0; i < trimCandidateList.size(); i++) {
            if (trimCandidateList[i] != nullptr) {
                sum++;
            }
        }
        DEBUG_BREAK_IF(sum != trimCandidatesCount);
    }
}

bool WddmResidencyController::checkTrimCandidateListCompaction() {
    if (2 * trimCandidatesCount <= trimCandidateList.size()) {
        return true;
    }
    return false;
}

void WddmResidencyController::compactTrimCandidateList() {
    size_t size = trimCandidateList.size();
    size_t freePosition = 0;

    if (size == 0 || size == trimCandidatesCount) {
        return;
    }

    DEBUG_BREAK_IF(!(trimCandidateList[size - 1] != nullptr));

    uint32_t previousCount = trimCandidatesCount;
    DEBUG_BREAK_IF(trimCandidatesCount > trimCandidateList.size());

    while (freePosition < trimCandidatesCount && trimCandidateList[freePosition] != nullptr)
        freePosition++;

    for (uint32_t i = 1; i < size; i++) {

        if (trimCandidateList[i] != nullptr && freePosition < i) {
            trimCandidateList[freePosition] = trimCandidateList[i];
            trimCandidateList[i] = nullptr;
            ((WddmAllocation *)trimCandidateList[freePosition])->setTrimCandidateListPosition(freePosition);
            freePosition++;

            // Last element was moved, erase elements from freePosition
            if (i == size - 1) {
                trimCandidateList.resize(freePosition);
            }
        }
    }
    DEBUG_BREAK_IF(trimCandidatesCount > trimCandidateList.size());
    DEBUG_BREAK_IF(trimCandidatesCount != previousCount);

    checkTrimCandidateCount();
}

} // namespace OCLRT

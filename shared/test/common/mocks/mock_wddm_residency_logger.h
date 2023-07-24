/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/windows/wddm/wddm_residency_logger.h"

namespace NEO {
struct MockWddmResidencyLogger : public WddmResidencyLogger {
    using WddmResidencyLogger::endTime;
    using WddmResidencyLogger::enterWait;
    using WddmResidencyLogger::makeResidentCall;
    using WddmResidencyLogger::makeResidentPagingFence;
    using WddmResidencyLogger::pagingLog;
    using WddmResidencyLogger::pendingMakeResident;
    using WddmResidencyLogger::pendingTime;
    using WddmResidencyLogger::startWaitPagingFence;
    using WddmResidencyLogger::waitStartTime;
    using WddmResidencyLogger::WddmResidencyLogger;

    void startWaitTime(UINT64 startWaitPagingFence) override {
        WddmResidencyLogger::startWaitTime(startWaitPagingFence);
        startWaitPagingFenceSave = this->startWaitPagingFence;
    }

    void trimCallbackBegin(std::chrono::high_resolution_clock::time_point &callbackStart) override {
        WddmResidencyLogger::trimCallbackBegin(callbackStart);
        this->callbackStartSave = callbackStart;
    }

    void trimCallbackEnd(UINT callbackFlag, void *controllerObject, std::chrono::high_resolution_clock::time_point &callbackStart) override {
        WddmResidencyLogger::trimCallbackEnd(callbackFlag, controllerObject, callbackStart);
        this->controllerObjectSave = controllerObject;
        this->callbackFlagSave = callbackFlag;
    }

    void trimToBudget(UINT64 numBytesToTrim, void *controllerObject) override {
        WddmResidencyLogger::trimToBudget(numBytesToTrim, controllerObject);
        this->controllerObjectSave = controllerObject;
        this->numBytesToTrimSave = numBytesToTrim;
    }

    std::chrono::high_resolution_clock::time_point callbackStartSave;
    UINT64 startWaitPagingFenceSave = 0ull;
    UINT64 numBytesToTrimSave = 0ull;
    void *controllerObjectSave = nullptr;
    UINT callbackFlagSave = 0u;
};

} // namespace NEO

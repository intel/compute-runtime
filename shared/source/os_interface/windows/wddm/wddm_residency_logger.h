/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/windows/wddm/wddm_defs.h"
#include "shared/source/os_interface/windows/wddm/wddm_residency_logger_defs.h"

#include <chrono>
#include <sstream>

namespace NEO {

class WddmResidencyLogger {
  public:
    WddmResidencyLogger(D3DKMT_HANDLE device, VOID *fenceValueCpuVirtualAddress) {
        std::stringstream id;
        id << std::hex;
        id << "device-0x" << device << "_"
           << "pfencecpu-0x" << fenceValueCpuVirtualAddress;
        std::stringstream filename;
        filename << "pagingfence_" << id.str() << ".log";
        pagingLog = ResLog::fopenPtr(filename.str().c_str(), "at");
        UNRECOVERABLE_IF(pagingLog == nullptr);
        fPagingLog("%s\n", id.str().c_str());
    }

    ~WddmResidencyLogger() {
        ResLog::fclosePtr(pagingLog);
    }

    void reportAllocations(uint32_t count, size_t size) {
        fPagingLog("residency for: handles %u size %zu\n", count, size);
    }

    void makeResidentLog(bool pendingMakeResident) {
        this->pendingMakeResident = pendingMakeResident;
        makeResidentCall = true;
        pendingTime = std::chrono::high_resolution_clock::now();
    }

    void startWaitTime() {
        waitStartTime = std::chrono::high_resolution_clock::now();
    }

    void enteredWait() {
        enterWait = true;
    }

    void waitPagingeFenceLog() {
        endTime = std::chrono::high_resolution_clock::now();

        int64_t timeDiff = 0;

        timeDiff = std::chrono::duration_cast<std::chrono::microseconds>(endTime - pendingTime).count();
        fPagingLog("makeResidentCall: %x pending return: %x delta time makeResident: %lld\n",
                   makeResidentCall,
                   pendingMakeResident,
                   timeDiff);

        timeDiff = std::chrono::duration_cast<std::chrono::microseconds>(endTime - waitStartTime).count();
        fPagingLog("waiting: %x delta time wait loop: %lld\n", enterWait, timeDiff);

        makeResidentCall = false;
        enterWait = false;
    }

    void trimRequired(UINT64 numBytesToTrim) {
        fPagingLog("trimming required: bytes to trim: %llu\n", numBytesToTrim);
    }

  protected:
    void fPagingLog(char const *const formatStr, ...) {
        va_list args;
        va_start(args, formatStr);
        ResLog::vfprintfPtr(pagingLog, formatStr, args);
        va_end(args);
    }

    bool pendingMakeResident = false;
    bool enterWait = false;
    bool makeResidentCall = false;
    FILE *pagingLog = nullptr;
    std::chrono::high_resolution_clock::time_point pendingTime;
    std::chrono::high_resolution_clock::time_point waitStartTime;
    std::chrono::high_resolution_clock::time_point endTime;
};

inline void perfLogResidencyMakeResident(WddmResidencyLogger *log, bool pendingMakeResident) {
    if (residencyLoggingAvailable) {
        if (log) {
            log->makeResidentLog(pendingMakeResident);
        }
    }
}

inline void perfLogResidencyReportAllocations(WddmResidencyLogger *log, uint32_t count, size_t size) {
    if (residencyLoggingAvailable) {
        if (log) {
            log->reportAllocations(count, size);
        }
    }
}

inline void perfLogStartWaitTime(WddmResidencyLogger *log) {
    if (residencyLoggingAvailable) {
        if (log) {
            log->startWaitTime();
        }
    }
}

inline void perfLogResidencyEnteredWait(WddmResidencyLogger *log) {
    if (residencyLoggingAvailable) {
        if (log) {
            log->enteredWait();
        }
    }
}

inline void perfLogResidencyWaitPagingeFenceLog(WddmResidencyLogger *log) {
    if (residencyLoggingAvailable) {
        if (log) {
            log->waitPagingeFenceLog();
        }
    }
}

inline void perfLogResidencyTrimRequired(WddmResidencyLogger *log, UINT64 numBytesToTrim) {
    if (residencyLoggingAvailable) {
        if (log) {
            log->trimRequired(numBytesToTrim);
        }
    }
}

} // namespace NEO

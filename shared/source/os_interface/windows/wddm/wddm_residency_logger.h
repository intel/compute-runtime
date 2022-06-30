/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/windows/wddm/wddm_defs.h"
#include "shared/source/utilities/io_functions.h"

#include <chrono>
#include <sstream>

namespace NEO {

#if defined(_RELEASE_INTERNAL) || (_DEBUG)
constexpr bool wddmResidencyLoggingAvailable = true;
#else
constexpr bool wddmResidencyLoggingAvailable = false;
#endif

class WddmResidencyLogger {
  public:
    WddmResidencyLogger(D3DKMT_HANDLE device, VOID *fenceValueCpuVirtualAddress) {
        std::stringstream id;
        id << std::hex;
        id << "device-0x" << device << "_"
           << "pfencecpu-0x" << fenceValueCpuVirtualAddress;
        std::stringstream filename;
        filename << "pagingfence_" << id.str() << ".log";
        pagingLog = IoFunctions::fopenPtr(filename.str().c_str(), "at");
        UNRECOVERABLE_IF(pagingLog == nullptr);
        IoFunctions::fprintf(pagingLog, "%s\n", id.str().c_str());
    }

    MOCKABLE_VIRTUAL ~WddmResidencyLogger() {
        IoFunctions::fclosePtr(pagingLog);
    }

    void reportAllocations(uint32_t count, size_t size) {
        IoFunctions::fprintf(pagingLog, "residency for: handles %u size %zu\n", count, size);
    }

    void makeResidentLog(bool pendingMakeResident, UINT64 makeResidentPagingFence) {
        this->pendingMakeResident = pendingMakeResident;
        this->makeResidentPagingFence = makeResidentPagingFence;
        makeResidentCall = true;
        pendingTime = std::chrono::high_resolution_clock::now();
    }

    MOCKABLE_VIRTUAL void startWaitTime(UINT64 startWaitPagingFence) {
        waitStartTime = std::chrono::high_resolution_clock::now();
        this->startWaitPagingFence = startWaitPagingFence;
    }

    void enteredWait() {
        enterWait = true;
    }

    void waitPagingeFenceLog(UINT64 stopWaitPagingFence) {
        endTime = std::chrono::high_resolution_clock::now();

        int64_t timeDiff = 0;
        IoFunctions::fprintf(pagingLog, "makeResidentPagingFence: %lld startWaitPagingFence: %lld stopWaitPagingFence: %lld\n",
                             makeResidentPagingFence,
                             startWaitPagingFence,
                             stopWaitPagingFence);

        timeDiff = std::chrono::duration_cast<std::chrono::microseconds>(endTime - pendingTime).count();
        IoFunctions::fprintf(pagingLog, "makeResidentCall: %x pending return: %x delta time makeResident: %lld\n",
                             makeResidentCall,
                             pendingMakeResident,
                             timeDiff);

        timeDiff = std::chrono::duration_cast<std::chrono::microseconds>(endTime - waitStartTime).count();
        IoFunctions::fprintf(pagingLog, "waiting: %x delta time wait loop: %lld\n", enterWait, timeDiff);

        makeResidentCall = false;
        enterWait = false;
        makeResidentPagingFence = 0;
        startWaitPagingFence = 0;
    }

    void trimRequired(UINT64 numBytesToTrim) {
        IoFunctions::fprintf(pagingLog, "trimming required: bytes to trim: %llu\n", numBytesToTrim);
    }

    void variadicLog(char const *const formatStr, va_list arg) {
        IoFunctions::vfprintfPtr(pagingLog, formatStr, arg);
    }

  protected:
    std::chrono::high_resolution_clock::time_point pendingTime;
    std::chrono::high_resolution_clock::time_point waitStartTime;
    std::chrono::high_resolution_clock::time_point endTime;

    UINT64 makeResidentPagingFence = 0ull;
    UINT64 startWaitPagingFence = 0ull;

    FILE *pagingLog = nullptr;

    bool pendingMakeResident = false;
    bool enterWait = false;
    bool makeResidentCall = false;
};

inline void perfLogResidencyMakeResident(WddmResidencyLogger *log, bool pendingMakeResident, UINT64 makeResidentPagingFence) {
    if (wddmResidencyLoggingAvailable) {
        if (log) {
            log->makeResidentLog(pendingMakeResident, makeResidentPagingFence);
        }
    }
}

inline void perfLogResidencyReportAllocations(WddmResidencyLogger *log, uint32_t count, size_t size) {
    if (wddmResidencyLoggingAvailable) {
        if (log) {
            log->reportAllocations(count, size);
        }
    }
}

inline void perfLogStartWaitTime(WddmResidencyLogger *log, UINT64 startWaitPagingFence) {
    if (wddmResidencyLoggingAvailable) {
        if (log) {
            log->startWaitTime(startWaitPagingFence);
        }
    }
}

inline void perfLogResidencyEnteredWait(WddmResidencyLogger *log) {
    if (wddmResidencyLoggingAvailable) {
        if (log) {
            log->enteredWait();
        }
    }
}

inline void perfLogResidencyWaitPagingeFenceLog(WddmResidencyLogger *log, UINT64 stopWaitPagingFence) {
    if (wddmResidencyLoggingAvailable) {
        if (log) {
            log->waitPagingeFenceLog(stopWaitPagingFence);
        }
    }
}

inline void perfLogResidencyTrimRequired(WddmResidencyLogger *log, UINT64 numBytesToTrim) {
    if (wddmResidencyLoggingAvailable) {
        if (log) {
            log->trimRequired(numBytesToTrim);
        }
    }
}

inline void perfLogResidencyVariadicLog(WddmResidencyLogger *log, char const *const format, ...) {
    if (wddmResidencyLoggingAvailable) {
        if (log) {
            va_list args;
            va_start(args, format);
            log->variadicLog(format, args);
            va_end(args);
        }
    }
}

} // namespace NEO

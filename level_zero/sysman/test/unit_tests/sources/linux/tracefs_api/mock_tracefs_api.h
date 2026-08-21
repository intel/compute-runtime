/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/shared/linux/tracefs_api/sysman_tracefs_api.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/tracefs_api/mock_tracefs_os_library.h"

#include <cstdint>
#include <cstring>
#include <string>

namespace L0 {
namespace Sysman {
namespace ult {

class PublicTraceFsApi : public L0::Sysman::TraceFsApi {
  public:
    using L0::Sysman::TraceFsApi::traceFsLibraryHandle;

    int eventEnableReturnValue = 0;
    int eventDisableReturnValue = 0;
    int traceOnReturnValue = 0;
    int traceOffReturnValue = 0;
    int getBufferPercentReturnValue = MockTraceFsOsLibrary::mockBufferPercent;
    int setBufferPercentReturnValue = 0;
    static inline int lastSetBufferPercent = -1;
    static inline struct tracefs_instance *lastSetBufferPercentInstance = nullptr;
    static inline uint32_t setBufferPercentCallCount = 0;
    static inline uint32_t failSetBufferPercentOnCall = 0;
    int setBufferSizeReturnValue = 0;
    // cpu -1 is a valid request (all CPUs), so an out-of-range value marks "not recorded yet".
    static constexpr int noCpuRecorded = -2;
    static inline size_t lastSetBufferSize = 0;
    static inline int lastSetBufferSizeCpu = noCpuRecorded;
    static inline uint32_t setBufferSizeCallCount = 0;
    std::string mockTracePipeData;
    bool mockEventAlreadyEnabled = false;
    bool mockTracingAlreadyOn = false;
    bool mockInstanceCreateReturnsNull = false;

    PublicTraceFsApi() {
        lastSetBufferPercent = -1;
        lastSetBufferPercentInstance = nullptr;
        setBufferPercentCallCount = 0;
        lastSetBufferSize = 0;
        lastSetBufferSizeCpu = noCpuRecorded;
        setBufferSizeCallCount = 0;
    }

    bool loadEntryPointsFromBase() {
        return L0::Sysman::TraceFsApi::loadEntryPoints();
    }

    struct tracefs_instance *traceFsInstanceCreate(const char *name) override {
        if (traceFsInstanceCreateEntry == nullptr) {
            return L0::Sysman::TraceFsApi::traceFsInstanceCreate(name);
        }
        if (mockInstanceCreateReturnsNull) {
            return nullptr;
        }
        return &MockTraceFsOsLibrary::mockTraceFsInstance;
    }

    struct tracefs_instance *traceFsInstanceCreateBase(const char *name) {
        return L0::Sysman::TraceFsApi::traceFsInstanceCreate(name);
    }

    void traceFsInstanceDestroy(struct tracefs_instance *instance) override {}
    void traceFsInstanceFree(struct tracefs_instance *instance) override {}

    void traceFsInstanceDestroyBase(struct tracefs_instance *instance) {
        L0::Sysman::TraceFsApi::traceFsInstanceDestroy(instance);
    }

    void traceFsInstanceFreeBase(struct tracefs_instance *instance) {
        L0::Sysman::TraceFsApi::traceFsInstanceFree(instance);
    }

    char *traceFsInstanceFileRead(struct tracefs_instance *instance, const char *file, int *psize) override {
        if (file) {
            std::string fileStr(file);
            if (fileStr == "trace_pipe" && !mockTracePipeData.empty()) {
                if (psize) {
                    *psize = static_cast<int>(mockTracePipeData.size());
                }
                return const_cast<char *>(mockTracePipeData.c_str());
            }
            if (fileStr == "events/xe/xe_error_cper/enable") {
                return mockEventAlreadyEnabled ? strdup("1") : nullptr;
            }
            if (fileStr == "tracing_on") {
                return mockTracingAlreadyOn ? strdup("1") : nullptr;
            }
        }
        return L0::Sysman::TraceFsApi::traceFsInstanceFileRead(instance, file, psize);
    }

    long long traceFsInstanceGetBufferSize(struct tracefs_instance *instance, int cpu) override {
        if (traceFsInstanceGetBufferSizeEntry != nullptr) {
            return MockTraceFsOsLibrary::mockBufferSize;
        }
        return L0::Sysman::TraceFsApi::traceFsInstanceGetBufferSize(instance, cpu);
    }

    long long traceFsInstanceGetBufferSizeBase(struct tracefs_instance *instance, int cpu) {
        return L0::Sysman::TraceFsApi::traceFsInstanceGetBufferSize(instance, cpu);
    }

    int traceFsInstanceSetBufferSize(struct tracefs_instance *instance, size_t size, int cpu) override {
        if (traceFsInstanceSetBufferSizeEntry != nullptr) {
            lastSetBufferSize = size;
            lastSetBufferSizeCpu = cpu;
            setBufferSizeCallCount++;
            return setBufferSizeReturnValue;
        }
        return L0::Sysman::TraceFsApi::traceFsInstanceSetBufferSize(instance, size, cpu);
    }

    int traceFsInstanceSetBufferSizeBase(struct tracefs_instance *instance, size_t size, int cpu) {
        return L0::Sysman::TraceFsApi::traceFsInstanceSetBufferSize(instance, size, cpu);
    }

    int traceFsEventEnable(struct tracefs_instance *instance, const char *system, const char *event) override {
        if (traceFsEventEnableEntry != nullptr) {
            return eventEnableReturnValue;
        }
        return L0::Sysman::TraceFsApi::traceFsEventEnable(instance, system, event);
    }

    int traceFsEventEnableBase(struct tracefs_instance *instance, const char *system, const char *event) {
        return L0::Sysman::TraceFsApi::traceFsEventEnable(instance, system, event);
    }

    int traceFsEventDisable(struct tracefs_instance *instance, const char *system, const char *event) override {
        if (traceFsEventDisableEntry != nullptr) {
            return eventDisableReturnValue;
        }
        return L0::Sysman::TraceFsApi::traceFsEventDisable(instance, system, event);
    }

    int traceFsEventDisableBase(struct tracefs_instance *instance, const char *system, const char *event) {
        return L0::Sysman::TraceFsApi::traceFsEventDisable(instance, system, event);
    }

    int traceFsTraceOn(struct tracefs_instance *instance) override {
        if (traceFsTraceOnEntry != nullptr) {
            return traceOnReturnValue;
        }
        return L0::Sysman::TraceFsApi::traceFsTraceOn(instance);
    }

    int traceFsTraceOnBase(struct tracefs_instance *instance) {
        return L0::Sysman::TraceFsApi::traceFsTraceOn(instance);
    }

    int traceFsTraceOff(struct tracefs_instance *instance) override {
        if (traceFsTraceOffEntry != nullptr) {
            return traceOffReturnValue;
        }
        return L0::Sysman::TraceFsApi::traceFsTraceOff(instance);
    }

    int traceFsTraceOffBase(struct tracefs_instance *instance) {
        return L0::Sysman::TraceFsApi::traceFsTraceOff(instance);
    }

    int traceFsInstanceGetBufferPercent(struct tracefs_instance *instance) override {
        if (traceFsInstanceGetBufferPercentEntry != nullptr) {
            return getBufferPercentReturnValue;
        }
        return L0::Sysman::TraceFsApi::traceFsInstanceGetBufferPercent(instance);
    }

    int traceFsInstanceGetBufferPercentBase(struct tracefs_instance *instance) {
        return L0::Sysman::TraceFsApi::traceFsInstanceGetBufferPercent(instance);
    }

    int traceFsInstanceSetBufferPercent(struct tracefs_instance *instance, int val) override {
        if (traceFsInstanceSetBufferPercentEntry != nullptr) {
            lastSetBufferPercent = val;
            lastSetBufferPercentInstance = instance;
            setBufferPercentCallCount++;
            if (failSetBufferPercentOnCall == setBufferPercentCallCount) {
                return -1;
            }
            return setBufferPercentReturnValue;
        }
        return L0::Sysman::TraceFsApi::traceFsInstanceSetBufferPercent(instance, val);
    }

    int traceFsInstanceSetBufferPercentBase(struct tracefs_instance *instance, int val) {
        return L0::Sysman::TraceFsApi::traceFsInstanceSetBufferPercent(instance, val);
    }

    bool allEntryPointsLoaded() const {
        return traceFsInstanceCreateEntry != nullptr &&
               traceFsInstanceDestroyEntry != nullptr &&
               traceFsInstanceFreeEntry != nullptr &&
               traceFsInstanceGetNameEntry != nullptr &&
               traceFsInstanceGetTraceDirEntry != nullptr &&
               traceFsInstanceFileOpenEntry != nullptr &&
               traceFsInstanceFileReadEntry != nullptr &&
               traceFsInstanceFileWriteEntry != nullptr &&
               traceFsInstanceFileAppendEntry != nullptr &&
               traceFsTraceOnEntry != nullptr &&
               traceFsTraceOffEntry != nullptr &&
               traceFsEventEnableEntry != nullptr &&
               traceFsEventDisableEntry != nullptr &&
               traceFsLocalEventsEntry != nullptr &&
               traceFsInstanceGetBufferPercentEntry != nullptr &&
               traceFsInstanceSetBufferPercentEntry != nullptr &&
               traceFsInstanceGetBufferSizeEntry != nullptr &&
               traceFsInstanceSetBufferSizeEntry != nullptr &&
               traceFsInstanceGetFileEntry != nullptr &&
               traceFsGetTracingFileEntry != nullptr;
    }
};

} // namespace ult
} // namespace Sysman
} // namespace L0

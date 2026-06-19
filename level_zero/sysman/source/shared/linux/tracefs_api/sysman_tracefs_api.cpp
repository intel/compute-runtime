/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/tracefs_api/sysman_tracefs_api.h"

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/os_library.h"

namespace L0 {
namespace Sysman {

static constexpr std::string_view libTraceFsFile = "libtracefs.so.1";
static constexpr std::string_view traceFsInstanceCreateRoutine = "tracefs_instance_create";
static constexpr std::string_view traceFsInstanceDestroyRoutine = "tracefs_instance_destroy";
static constexpr std::string_view traceFsInstanceFreeRoutine = "tracefs_instance_free";
static constexpr std::string_view traceFsInstanceGetNameRoutine = "tracefs_instance_get_name";
static constexpr std::string_view traceFsInstanceGetTraceDirRoutine = "tracefs_instance_get_trace_dir";
static constexpr std::string_view traceFsInstanceFileOpenRoutine = "tracefs_instance_file_open";
static constexpr std::string_view traceFsInstanceFileReadRoutine = "tracefs_instance_file_read";
static constexpr std::string_view traceFsInstanceFileWriteRoutine = "tracefs_instance_file_write";
static constexpr std::string_view traceFsInstanceFileAppendRoutine = "tracefs_instance_file_append";
static constexpr std::string_view traceFsTraceOnRoutine = "tracefs_trace_on";
static constexpr std::string_view traceFsTraceOffRoutine = "tracefs_trace_off";
static constexpr std::string_view traceFsEventEnableRoutine = "tracefs_event_enable";
static constexpr std::string_view traceFsEventDisableRoutine = "tracefs_event_disable";
static constexpr std::string_view traceFsLocalEventsRoutine = "tracefs_local_events";
static constexpr std::string_view traceFsLocalEventsFreeRoutine = "tracefs_local_events_free";
static constexpr std::string_view traceFsInstanceGetBufferPercentRoutine = "tracefs_instance_get_buffer_percent";
static constexpr std::string_view traceFsInstanceSetBufferPercentRoutine = "tracefs_instance_set_buffer_percent";
static constexpr std::string_view traceFsInstanceGetBufferSizeRoutine = "tracefs_instance_get_buffer_size";
static constexpr std::string_view traceFsInstanceSetBufferSizeRoutine = "tracefs_instance_set_buffer_size";
static constexpr std::string_view traceFsInstanceGetFileRoutine = "tracefs_instance_get_file";
static constexpr std::string_view traceFsGetTracingFileRoutine = "tracefs_get_tracing_file";

template <class T>
bool TraceFsApi::getSymbolAddr(std::string_view name, T &sym) {
    sym = reinterpret_cast<T>(traceFsLibraryHandle->getProcAddress(std::string(name)));
    return nullptr != sym;
}

bool TraceFsApi::loadEntryPoints() {
    if (!isAvailable()) {
        traceFsLibraryHandle.reset(NEO::OsLibrary::loadFunc(std::string(libTraceFsFile)));
        if (!isAvailable()) {
            return false;
        }
    }

    bool allEntryPointsLoaded = true;
    allEntryPointsLoaded = getSymbolAddr(traceFsInstanceCreateRoutine, traceFsInstanceCreateEntry);
    allEntryPointsLoaded = allEntryPointsLoaded && getSymbolAddr(traceFsInstanceDestroyRoutine, traceFsInstanceDestroyEntry);
    allEntryPointsLoaded = allEntryPointsLoaded && getSymbolAddr(traceFsInstanceFreeRoutine, traceFsInstanceFreeEntry);
    allEntryPointsLoaded = allEntryPointsLoaded && getSymbolAddr(traceFsInstanceGetNameRoutine, traceFsInstanceGetNameEntry);
    allEntryPointsLoaded = allEntryPointsLoaded && getSymbolAddr(traceFsInstanceGetTraceDirRoutine, traceFsInstanceGetTraceDirEntry);
    allEntryPointsLoaded = allEntryPointsLoaded && getSymbolAddr(traceFsInstanceFileOpenRoutine, traceFsInstanceFileOpenEntry);
    allEntryPointsLoaded = allEntryPointsLoaded && getSymbolAddr(traceFsInstanceFileReadRoutine, traceFsInstanceFileReadEntry);
    allEntryPointsLoaded = allEntryPointsLoaded && getSymbolAddr(traceFsInstanceFileWriteRoutine, traceFsInstanceFileWriteEntry);
    allEntryPointsLoaded = allEntryPointsLoaded && getSymbolAddr(traceFsInstanceFileAppendRoutine, traceFsInstanceFileAppendEntry);
    allEntryPointsLoaded = allEntryPointsLoaded && getSymbolAddr(traceFsTraceOnRoutine, traceFsTraceOnEntry);
    allEntryPointsLoaded = allEntryPointsLoaded && getSymbolAddr(traceFsTraceOffRoutine, traceFsTraceOffEntry);
    allEntryPointsLoaded = allEntryPointsLoaded && getSymbolAddr(traceFsEventEnableRoutine, traceFsEventEnableEntry);
    allEntryPointsLoaded = allEntryPointsLoaded && getSymbolAddr(traceFsEventDisableRoutine, traceFsEventDisableEntry);
    allEntryPointsLoaded = allEntryPointsLoaded && getSymbolAddr(traceFsLocalEventsRoutine, traceFsLocalEventsEntry);
    allEntryPointsLoaded = allEntryPointsLoaded && getSymbolAddr(traceFsLocalEventsFreeRoutine, traceFsLocalEventsFreeEntry);
    allEntryPointsLoaded = allEntryPointsLoaded && getSymbolAddr(traceFsInstanceGetBufferPercentRoutine, traceFsInstanceGetBufferPercentEntry);
    allEntryPointsLoaded = allEntryPointsLoaded && getSymbolAddr(traceFsInstanceSetBufferPercentRoutine, traceFsInstanceSetBufferPercentEntry);
    allEntryPointsLoaded = allEntryPointsLoaded && getSymbolAddr(traceFsInstanceGetBufferSizeRoutine, traceFsInstanceGetBufferSizeEntry);
    allEntryPointsLoaded = allEntryPointsLoaded && getSymbolAddr(traceFsInstanceSetBufferSizeRoutine, traceFsInstanceSetBufferSizeEntry);
    allEntryPointsLoaded = allEntryPointsLoaded && getSymbolAddr(traceFsInstanceGetFileRoutine, traceFsInstanceGetFileEntry);
    allEntryPointsLoaded = allEntryPointsLoaded && getSymbolAddr(traceFsGetTracingFileRoutine, traceFsGetTracingFileEntry);

    return allEntryPointsLoaded;
}

struct tracefs_instance *TraceFsApi::traceFsInstanceCreate(const char *name) {
    if (nullptr == traceFsInstanceCreateEntry) {
        return nullptr;
    }
    return (*traceFsInstanceCreateEntry)(name);
}

void TraceFsApi::traceFsInstanceDestroy(struct tracefs_instance *instance) {
    if (nullptr == traceFsInstanceDestroyEntry) {
        return;
    }
    (*traceFsInstanceDestroyEntry)(instance);
}

void TraceFsApi::traceFsInstanceFree(struct tracefs_instance *instance) {
    if (nullptr == traceFsInstanceFreeEntry) {
        return;
    }
    (*traceFsInstanceFreeEntry)(instance);
}

const char *TraceFsApi::traceFsInstanceGetName(struct tracefs_instance *instance) {
    if (nullptr == traceFsInstanceGetNameEntry) {
        return nullptr;
    }
    return (*traceFsInstanceGetNameEntry)(instance);
}

const char *TraceFsApi::traceFsInstanceGetTraceDir(struct tracefs_instance *instance) {
    if (nullptr == traceFsInstanceGetTraceDirEntry) {
        return nullptr;
    }
    return (*traceFsInstanceGetTraceDirEntry)(instance);
}

int TraceFsApi::traceFsInstanceFileOpen(struct tracefs_instance *instance, const char *file, int mode) {
    if (nullptr == traceFsInstanceFileOpenEntry) {
        return -1;
    }
    return (*traceFsInstanceFileOpenEntry)(instance, file, mode);
}

char *TraceFsApi::traceFsInstanceFileRead(struct tracefs_instance *instance, const char *file, int *psize) {
    if (nullptr == traceFsInstanceFileReadEntry) {
        return nullptr;
    }
    return (*traceFsInstanceFileReadEntry)(instance, file, psize);
}

int TraceFsApi::traceFsInstanceFileWrite(struct tracefs_instance *instance, const char *file, const char *str) {
    if (nullptr == traceFsInstanceFileWriteEntry) {
        return -1;
    }
    return (*traceFsInstanceFileWriteEntry)(instance, file, str);
}

int TraceFsApi::traceFsInstanceFileAppend(struct tracefs_instance *instance, const char *file, const char *str) {
    if (nullptr == traceFsInstanceFileAppendEntry) {
        return -1;
    }
    return (*traceFsInstanceFileAppendEntry)(instance, file, str);
}

int TraceFsApi::traceFsTraceOn(struct tracefs_instance *instance) {
    if (nullptr == traceFsTraceOnEntry) {
        return -1;
    }
    return (*traceFsTraceOnEntry)(instance);
}

int TraceFsApi::traceFsTraceOff(struct tracefs_instance *instance) {
    if (nullptr == traceFsTraceOffEntry) {
        return -1;
    }
    return (*traceFsTraceOffEntry)(instance);
}

int TraceFsApi::traceFsEventEnable(struct tracefs_instance *instance, const char *system, const char *event) {
    if (nullptr == traceFsEventEnableEntry) {
        return -1;
    }
    return (*traceFsEventEnableEntry)(instance, system, event);
}

int TraceFsApi::traceFsEventDisable(struct tracefs_instance *instance, const char *system, const char *event) {
    if (nullptr == traceFsEventDisableEntry) {
        return -1;
    }
    return (*traceFsEventDisableEntry)(instance, system, event);
}

struct tep_handle *TraceFsApi::traceFsLocalEvents(const char *tracingDir) {
    if (nullptr == traceFsLocalEventsEntry) {
        return nullptr;
    }
    return (*traceFsLocalEventsEntry)(tracingDir);
}

void TraceFsApi::traceFsLocalEventsFree(struct tep_handle *tep) {
    if (nullptr == traceFsLocalEventsFreeEntry) {
        return;
    }
    (*traceFsLocalEventsFreeEntry)(tep);
}

int TraceFsApi::traceFsInstanceGetBufferPercent(struct tracefs_instance *instance) {
    if (nullptr == traceFsInstanceGetBufferPercentEntry) {
        return -1;
    }
    return (*traceFsInstanceGetBufferPercentEntry)(instance);
}

int TraceFsApi::traceFsInstanceSetBufferPercent(struct tracefs_instance *instance, int val) {
    if (nullptr == traceFsInstanceSetBufferPercentEntry) {
        return -1;
    }
    return (*traceFsInstanceSetBufferPercentEntry)(instance, val);
}

long long TraceFsApi::traceFsInstanceGetBufferSize(struct tracefs_instance *instance, int cpu) {
    if (nullptr == traceFsInstanceGetBufferSizeEntry) {
        return -1;
    }
    return (*traceFsInstanceGetBufferSizeEntry)(instance, cpu);
}

int TraceFsApi::traceFsInstanceSetBufferSize(struct tracefs_instance *instance, size_t size, int cpu) {
    if (nullptr == traceFsInstanceSetBufferSizeEntry) {
        return -1;
    }
    return (*traceFsInstanceSetBufferSizeEntry)(instance, size, cpu);
}

char *TraceFsApi::traceFsInstanceGetFile(struct tracefs_instance *instance, const char *file) {
    if (nullptr == traceFsInstanceGetFileEntry) {
        return nullptr;
    }
    return (*traceFsInstanceGetFileEntry)(instance, file);
}

char *TraceFsApi::traceFsGetTracingFile(const char *file) {
    if (nullptr == traceFsGetTracingFileEntry) {
        return nullptr;
    }
    return (*traceFsGetTracingFileEntry)(file);
}

TraceFsApi::TraceFsApi() = default;

TraceFsApi::~TraceFsApi() = default;

} // namespace Sysman
} // namespace L0

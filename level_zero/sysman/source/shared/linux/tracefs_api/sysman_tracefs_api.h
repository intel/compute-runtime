/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <cstddef>
#include <memory>
#include <string_view>

struct tracefs_instance; // NOLINT(readability-identifier-naming)
struct tep_handle;       // NOLINT(readability-identifier-naming)
struct tep_event;        // NOLINT(readability-identifier-naming)

namespace NEO {
class OsLibrary;
}

namespace L0 {
namespace Sysman {

typedef struct tracefs_instance *(*pTraceFsInstanceCreate)(const char *);
typedef void (*pTraceFsInstanceDestroy)(struct tracefs_instance *);
typedef void (*pTraceFsInstanceFree)(struct tracefs_instance *);
typedef const char *(*pTraceFsInstanceGetName)(struct tracefs_instance *);
typedef const char *(*pTraceFsInstanceGetTraceDir)(struct tracefs_instance *);
typedef int (*pTraceFsInstanceFileOpen)(struct tracefs_instance *, const char *, int);
typedef char *(*pTraceFsInstanceFileRead)(struct tracefs_instance *, const char *, int *);
typedef int (*pTraceFsInstanceFileWrite)(struct tracefs_instance *, const char *, const char *);
typedef int (*pTraceFsInstanceFileAppend)(struct tracefs_instance *, const char *, const char *);
typedef int (*pTraceFsTraceOn)(struct tracefs_instance *);
typedef int (*pTraceFsTraceOff)(struct tracefs_instance *);
typedef int (*pTraceFsEventEnable)(struct tracefs_instance *, const char *, const char *);
typedef int (*pTraceFsEventDisable)(struct tracefs_instance *, const char *, const char *);
typedef struct tep_handle *(*pTraceFsLocalEvents)(const char *);
typedef void (*pTraceFsLocalEventsFree)(struct tep_handle *);
typedef int (*pTraceFsInstanceGetBufferPercent)(struct tracefs_instance *);
typedef int (*pTraceFsInstanceSetBufferPercent)(struct tracefs_instance *, int);
typedef long long (*pTraceFsInstanceGetBufferSize)(struct tracefs_instance *, int);
typedef int (*pTraceFsInstanceSetBufferSize)(struct tracefs_instance *, size_t, int);
typedef char *(*pTraceFsInstanceGetFile)(struct tracefs_instance *, const char *);
typedef char *(*pTraceFsGetTracingFile)(const char *);

class TraceFsApi : public NEO::NonCopyableAndNonMovableClass {
  public:
    MOCKABLE_VIRTUAL struct tracefs_instance *traceFsInstanceCreate(const char *name);
    MOCKABLE_VIRTUAL void traceFsInstanceDestroy(struct tracefs_instance *instance);
    MOCKABLE_VIRTUAL void traceFsInstanceFree(struct tracefs_instance *instance);
    MOCKABLE_VIRTUAL const char *traceFsInstanceGetName(struct tracefs_instance *instance);
    MOCKABLE_VIRTUAL const char *traceFsInstanceGetTraceDir(struct tracefs_instance *instance);

    MOCKABLE_VIRTUAL int traceFsInstanceFileOpen(struct tracefs_instance *instance, const char *file, int mode);
    MOCKABLE_VIRTUAL char *traceFsInstanceFileRead(struct tracefs_instance *instance, const char *file, int *psize);
    MOCKABLE_VIRTUAL int traceFsInstanceFileWrite(struct tracefs_instance *instance, const char *file, const char *str);
    MOCKABLE_VIRTUAL int traceFsInstanceFileAppend(struct tracefs_instance *instance, const char *file, const char *str);

    MOCKABLE_VIRTUAL int traceFsTraceOn(struct tracefs_instance *instance);
    MOCKABLE_VIRTUAL int traceFsTraceOff(struct tracefs_instance *instance);

    MOCKABLE_VIRTUAL int traceFsEventEnable(struct tracefs_instance *instance, const char *system, const char *event);
    MOCKABLE_VIRTUAL int traceFsEventDisable(struct tracefs_instance *instance, const char *system, const char *event);

    MOCKABLE_VIRTUAL struct tep_handle *traceFsLocalEvents(const char *tracingDir);
    MOCKABLE_VIRTUAL void traceFsLocalEventsFree(struct tep_handle *tep);

    MOCKABLE_VIRTUAL int traceFsInstanceGetBufferPercent(struct tracefs_instance *instance);
    MOCKABLE_VIRTUAL int traceFsInstanceSetBufferPercent(struct tracefs_instance *instance, int val);
    MOCKABLE_VIRTUAL long long traceFsInstanceGetBufferSize(struct tracefs_instance *instance, int cpu);
    MOCKABLE_VIRTUAL int traceFsInstanceSetBufferSize(struct tracefs_instance *instance, size_t size, int cpu);

    MOCKABLE_VIRTUAL char *traceFsInstanceGetFile(struct tracefs_instance *instance, const char *file);
    MOCKABLE_VIRTUAL char *traceFsGetTracingFile(const char *file);

    bool isAvailable() { return nullptr != traceFsLibraryHandle.get(); }

    MOCKABLE_VIRTUAL bool loadEntryPoints();

    TraceFsApi();
    MOCKABLE_VIRTUAL ~TraceFsApi();

  protected:
    template <class T>
    bool getSymbolAddr(std::string_view name, T &sym);

    std::unique_ptr<NEO::OsLibrary> traceFsLibraryHandle;

    pTraceFsInstanceCreate traceFsInstanceCreateEntry = nullptr;
    pTraceFsInstanceDestroy traceFsInstanceDestroyEntry = nullptr;
    pTraceFsInstanceFree traceFsInstanceFreeEntry = nullptr;
    pTraceFsInstanceGetName traceFsInstanceGetNameEntry = nullptr;
    pTraceFsInstanceGetTraceDir traceFsInstanceGetTraceDirEntry = nullptr;
    pTraceFsInstanceFileOpen traceFsInstanceFileOpenEntry = nullptr;
    pTraceFsInstanceFileRead traceFsInstanceFileReadEntry = nullptr;
    pTraceFsInstanceFileWrite traceFsInstanceFileWriteEntry = nullptr;
    pTraceFsInstanceFileAppend traceFsInstanceFileAppendEntry = nullptr;
    pTraceFsTraceOn traceFsTraceOnEntry = nullptr;
    pTraceFsTraceOff traceFsTraceOffEntry = nullptr;
    pTraceFsEventEnable traceFsEventEnableEntry = nullptr;
    pTraceFsEventDisable traceFsEventDisableEntry = nullptr;
    pTraceFsLocalEvents traceFsLocalEventsEntry = nullptr;
    pTraceFsLocalEventsFree traceFsLocalEventsFreeEntry = nullptr;
    pTraceFsInstanceGetBufferPercent traceFsInstanceGetBufferPercentEntry = nullptr;
    pTraceFsInstanceSetBufferPercent traceFsInstanceSetBufferPercentEntry = nullptr;
    pTraceFsInstanceGetBufferSize traceFsInstanceGetBufferSizeEntry = nullptr;
    pTraceFsInstanceSetBufferSize traceFsInstanceSetBufferSizeEntry = nullptr;
    pTraceFsInstanceGetFile traceFsInstanceGetFileEntry = nullptr;
    pTraceFsGetTracingFile traceFsGetTracingFileEntry = nullptr;
};

} // namespace Sysman
} // namespace L0

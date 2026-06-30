/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device/device.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/topology_map.h"
#include "shared/source/os_interface/external_semaphore.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/d3dkmthk_wrapper.h"

#include <limits>
#include <memory>
#include <string>

namespace NEO {

class Gdi;

typedef wchar_t SharedSyncName[9 + 2 * (sizeof(uint32_t) + sizeof(uint64_t))];

struct SharedMemoryContentHeader {
    alignas(8) uint64_t lastSignaledValue;
    SharedSyncName sharedSyncName;
    uint32_t access;
    uint32_t serializedSecurityDescriptorStringSize;
};

class ExternalSemaphoreWindows : public ExternalSemaphore {
  public:
    static std::unique_ptr<ExternalSemaphoreWindows> create(OSInterface *osInterface);

    ~ExternalSemaphoreWindows() override {};

    bool importSemaphore(void *extHandle, int fd, uint32_t flags, const char *name, Type type, bool isNative) override;

    bool enqueueWait(uint64_t *fenceValue) override;
    bool enqueueSignal(uint64_t *fenceValue) override;

    SharedMemoryContentHeader *getSharedMemoryContentHeader() {
        return reinterpret_cast<SharedMemoryContentHeader *>(this->pCpuAddress);
    }

  protected:
    // Resolves the NT object-manager directory for a Win32-style named object, honoring an
    // optional "Global\\" / "Local\\" prefix the same way kernelbase does. relativeName is set
    // to the portion of name to open under the returned directory. sessionId is the caller's
    // session; session 0 has no \Sessions\0 directory, so it collapses to \BaseNamedObjects.
    static std::wstring getNamedObjectDirectoryPath(uint32_t sessionId, const wchar_t *name, const wchar_t **relativeName);

    // Opens the named sync object, resolving its namespace via getNamedObjectDirectoryPath, and
    // returns an owned NT handle (caller must CloseHandle) or nullptr on failure.
    static void *openSyncObjectByName(Gdi *gdi, const wchar_t *name, uint32_t desiredAccess, bool forceGlobal = false);

    D3DKMT_HANDLE syncHandle;
    void *pCpuAddress = nullptr;
    volatile uint64_t *pLastSignaledValue = nullptr;
};

} // namespace NEO

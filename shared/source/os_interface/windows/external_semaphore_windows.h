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

    ImportResult importSemaphore(void *extHandle, int fd, uint32_t flags, const char *name, Type type, bool isNative) override;

    uint64_t acquireWaitFenceValue(uint64_t fenceValue) override;
    uint64_t acquireSignalFenceValue(uint64_t fenceValue) override;

    SharedMemoryContentHeader *getSharedMemoryContentHeader() {
        return reinterpret_cast<SharedMemoryContentHeader *>(this->pCpuAddress);
    }

  protected:
    static std::wstring convertUtf8NameToWide(const char *name);

    // Opens the named sync object, resolving its namespace via getNamedObjectDirectoryPath, and
    // returns an owned NT handle (caller must CloseHandle) or nullptr on failure.
    static void *openSyncObjectByName(Gdi *gdi, const char *name, uint32_t desiredAccess, bool forceGlobal);

    void *pCpuAddress = nullptr;
    volatile uint64_t *pLastSignaledValue = nullptr;
};

} // namespace NEO

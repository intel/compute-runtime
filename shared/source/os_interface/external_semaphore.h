/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <memory>

namespace NEO {
class ExternalSemaphore;
class OSInterface;

struct ExternalSemaphoreOperation {
    const ExternalSemaphore *semaphore = nullptr;
    uint64_t fenceValue = 0u;
};

class ExternalSemaphore {
  public:
    enum Type {
        OpaqueFd,
        OpaqueWin32,
        OpaqueWin32Kmt,
        D3d12Fence,
        D3d11Fence,
        KeyedMutex,
        KeyedMutexKmt,
        TimelineSemaphoreFd,
        TimelineSemaphoreWin32,
        Invalid
    };

    enum class ImportResult {
        success,
        unsupported,
        invalidResource
    };

    static std::unique_ptr<ExternalSemaphore> create(OSInterface *osInterface, ExternalSemaphore::Type type, void *handle, int fd, const char *name, ImportResult &importResult);

    static std::unique_ptr<ExternalSemaphore> create(OSInterface *osInterface, ExternalSemaphore::Type type, void *handle, int fd, const char *name) {
        ImportResult importResult = ImportResult::success;
        return create(osInterface, type, handle, fd, name, importResult);
    }

    virtual ~ExternalSemaphore() = default;

    virtual ImportResult importSemaphore(void *extHandle, int fd, uint32_t flags, const char *name, Type type, bool isNative) = 0;

    OSInterface *osInterface = nullptr;

    virtual uint64_t acquireWaitFenceValue(uint64_t fenceValue) { return fenceValue; }
    virtual uint64_t acquireSignalFenceValue(uint64_t fenceValue) { return fenceValue; }

    Type getType() const { return type; }
    uint32_t getSyncHandle() const { return syncHandle; }

  protected:
    Type type = Type::Invalid;
    uint32_t syncHandle = 0;
};

} // namespace NEO

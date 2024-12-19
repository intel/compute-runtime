/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device/device.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/topology_map.h"
#include "shared/source/os_interface/os_interface.h"

#include <limits>
#include <memory>
#include <string>

namespace NEO {

class ExternalSemaphoreHelper {
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

    static std::unique_ptr<ExternalSemaphoreHelper> create(OSInterface *osInterface);

    virtual ~ExternalSemaphoreHelper() = default;

    virtual bool importSemaphore(void *extHandle, int fd, uint32_t flags, const char *name, NEO::ExternalSemaphoreHelper::Type type, bool isNative, void *syncHandle) = 0;

    virtual bool semaphoreWait(const CommandStreamReceiver *csr, void *syncHandle) = 0;
    virtual bool semaphoreSignal(const CommandStreamReceiver *csr, void *syncHandle) = 0;

    virtual bool isSupported(ExternalSemaphoreHelper::Type type) = 0;

    Type getType() { return type; }

    OSInterface *osInterface = nullptr;

  private:
    Type type = Invalid;
};

} // namespace NEO

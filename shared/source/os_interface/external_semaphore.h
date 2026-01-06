/*
 * Copyright (C) 2025 Intel Corporation
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
#include "shared/source/os_interface/os_handle.h"
#include "shared/source/os_interface/os_interface.h"

#include <limits>
#include <memory>
#include <string>

namespace NEO {

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

    enum SemaphoreState {
        Initial,
        Waiting,
        Signaled
    };

    static std::unique_ptr<ExternalSemaphore> create(OSInterface *osInterface, ExternalSemaphore::Type type, void *handle, int fd, const char *name);

    virtual ~ExternalSemaphore() = default;

    virtual bool importSemaphore(void *extHandle, int fd, uint32_t flags, const char *name, Type type, bool isNative) = 0;

    virtual bool enqueueWait(uint64_t *fenceValue) = 0;
    virtual bool enqueueSignal(uint64_t *fenceValue) = 0;

    OSInterface *osInterface = nullptr;

    SemaphoreState getState() { return state; }

  protected:
    Type type = Type::Invalid;
    SemaphoreState state = SemaphoreState::Initial;
};

} // namespace NEO

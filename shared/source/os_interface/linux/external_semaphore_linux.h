/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device/device.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/external_semaphore.h"
#include "shared/source/os_interface/os_interface.h"

#include <limits>
#include <memory>
#include <string>

namespace NEO {

class ExternalSemaphoreLinux : public ExternalSemaphore {
  public:
    static std::unique_ptr<ExternalSemaphoreLinux> create(OSInterface *osInterface);

    ~ExternalSemaphoreLinux() override{};

    bool importSemaphore(void *extHandle, int fd, uint32_t flags, const char *name, Type type, bool isNative) override;

    bool enqueueWait(uint64_t *fenceValue) override;
    bool enqueueSignal(uint64_t *fenceValue) override;

  protected:
    uint32_t syncHandle;
};

} // namespace NEO
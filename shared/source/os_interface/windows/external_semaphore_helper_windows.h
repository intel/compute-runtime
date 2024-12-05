/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/device/device.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/topology_map.h"
#include "shared/source/os_interface/external_semaphore_helper.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/semaphore/external_semaphore.h"

#include <limits>
#include <memory>
#include <string>

namespace NEO {

class ExternalSemaphoreHelperWindows : public ExternalSemaphoreHelper {
  public:
    static std::unique_ptr<ExternalSemaphoreHelperWindows> create(OSInterface *osInterface);

    ~ExternalSemaphoreHelperWindows() override{};

    bool importSemaphore(void *extHandle, int fd, uint32_t flags, const char *name, ExternalSemaphoreHelper::Type type, bool isNative, void *syncHandle) override;

    bool semaphoreWait(const CommandStreamReceiver *csr, void *syncHandle) override;
    bool semaphoreSignal(const CommandStreamReceiver *csr, void *syncHandle) override;

    bool isSupported(ExternalSemaphoreHelper::Type type) override;
};

} // namespace NEO

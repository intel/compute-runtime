/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/queue_throttle.h"
#include "shared/source/command_stream/task_count_helper.h"
#include "shared/source/os_interface/external_semaphore_helper.h"
#include "shared/source/os_interface/os_interface.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_map>

namespace NEO {
class CommandStreamReceiver;
class Thread;

class ExternalSemaphore {

  public:
    static std::unique_ptr<ExternalSemaphore> create(OSInterface *osInterface, ExternalSemaphoreHelper::Type type, void *handle, int fd);
    void *getHandle() { return syncHandle; }

    bool enqueueWait(CommandStreamReceiver *csr);
    bool enqueueSignal(CommandStreamReceiver *csr);

  protected:
    void *syncHandle;
    std::unique_ptr<ExternalSemaphoreHelper> externalSemaphoreHelper;
};

} // namespace NEO

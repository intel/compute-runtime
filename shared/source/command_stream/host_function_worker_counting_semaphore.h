/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/host_function_worker_interface.h"

#include <mutex>
#include <semaphore>
#include <thread>

namespace NEO {

class HostFunctionWorkerCountingSemaphore : public IHostFunctionWorker {
  public:
    HostFunctionWorkerCountingSemaphore(bool skipHostFunctionExecution,
                                        const std::function<void(GraphicsAllocation &)> &downloadAllocationImpl,
                                        GraphicsAllocation *allocation,
                                        HostFunctionData *data);
    ~HostFunctionWorkerCountingSemaphore() override;

    void start() override;
    void finish() override;
    void submit() noexcept override;

  private:
    void workerLoop(std::stop_token st) noexcept;

    std::counting_semaphore<> semaphore{0};
};

static_assert(NonCopyableAndNonMovable<HostFunctionWorkerCountingSemaphore>);

} // namespace NEO

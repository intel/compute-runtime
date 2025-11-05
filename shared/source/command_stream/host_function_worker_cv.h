/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/host_function_worker_interface.h"

#include <condition_variable>
#include <mutex>
#include <thread>

namespace NEO {

class HostFunctionWorkerCV final : public IHostFunctionWorker {
  public:
    HostFunctionWorkerCV(bool skipHostFunctionExecution,
                         const std::function<void(GraphicsAllocation &)> &downloadAllocationImpl,
                         GraphicsAllocation *allocation,
                         HostFunctionData *data);
    ~HostFunctionWorkerCV() override;

    void start() override;
    void finish() override;
    void submit() noexcept override;

  private:
    void workerLoop(std::stop_token st) noexcept;

    std::mutex pendingAccessMutex;
    std::condition_variable cv;
    uint32_t pending{0};
};

static_assert(NonCopyableAndNonMovable<HostFunctionWorkerCV>);

} // namespace NEO

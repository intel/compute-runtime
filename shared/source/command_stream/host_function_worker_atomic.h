/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/host_function_worker_interface.h"

#include <atomic>
#include <memory>
#include <thread>

namespace NEO {

class HostFunctionWorkerAtomic : public IHostFunctionWorker {
  public:
    HostFunctionWorkerAtomic(bool skipHostFunctionExecution,
                             const std::function<void(GraphicsAllocation &)> &downloadAllocationImpl,
                             GraphicsAllocation *allocation,
                             HostFunctionData *data);
    ~HostFunctionWorkerAtomic() override;

    void start() override;
    void finish() override;
    void submit() noexcept override;

  private:
    void workerLoop(std::stop_token st) noexcept;

    std::atomic<uint32_t> pending{0};
};

static_assert(NonCopyableAndNonMovable<HostFunctionWorkerAtomic>);

} // namespace NEO

/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/host_function/host_function_worker_interface.h"

#include <mutex>
#include <semaphore>
#include <thread>

namespace NEO {

class HostFunctionStreamer;
struct HostFunction;

class HostFunctionWorkerCountingSemaphore final : public HostFunctionSingleWorker {
  public:
    HostFunctionWorkerCountingSemaphore(bool skipHostFunctionExecution);
    ~HostFunctionWorkerCountingSemaphore() override;

    void start(HostFunctionStreamer *streamer) override;
    void finish() override;
    void submit(uint32_t nHostFunctions) noexcept override;

  private:
    void workerLoop(std::stop_token st) noexcept;

    std::counting_semaphore<> semaphore{0};
};

static_assert(NonCopyableAndNonMovable<HostFunctionWorkerCountingSemaphore>);

} // namespace NEO

/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/host_function/host_function_interface.h"
#include "shared/source/host_function/host_function_worker_thread_pool.h"
#include "shared/source/utilities/stackvec.h"

#include <functional>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>
#include <vector>

namespace NEO {

class GraphicsAllocation;
struct HostFunction;
class HostFunctionStreamer;

class HostFunctionScheduler final : public HostFunctionWorker {
  public:
    HostFunctionScheduler(bool skipHostFunctionExecution,
                          int32_t threadsInThreadPoolLimit);

    ~HostFunctionScheduler() override;

    void start(HostFunctionStreamer *streamer) override;
    void finish() override;
    void submit(uint32_t nHostFunctions) noexcept override;

  private:
    void scheduleHostFunctionToThreadPool(HostFunctionStreamer *streamer, uint64_t hostFunctionId) noexcept;
    void schedulerLoop(std::stop_token st) noexcept;
    void registerHostFunctionStreamer(HostFunctionStreamer *streamer);
    uint64_t isHostFunctionReadyToExecute(HostFunctionStreamer *streamer);

    std::mutex registeredStreamersMutex;
    std::once_flag shutdownOnceFlag;
    std::counting_semaphore<> semaphore{0};
    HostFunctionThreadPool threadPool;

    std::vector<HostFunctionStreamer *> registeredStreamers;
};

} // namespace NEO

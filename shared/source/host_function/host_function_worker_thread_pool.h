/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/host_function/host_function.h"

#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <semaphore>
#include <stop_token>
#include <thread>

namespace NEO {

class GraphicsAllocation;
struct HostFunction;
class HostFunctionStreamer;

class HostFunctionThreadPool : public NonCopyableAndNonMovableClass {
  public:
    explicit HostFunctionThreadPool(int32_t threadsInThreadPoolLimit);
    ~HostFunctionThreadPool();

    void registerHostFunctionToExecute(HostFunctionStreamer *streamer, HostFunction &&hostFunction);
    void registerThread() noexcept;
    void shutdown() noexcept;

  private:
    void executeHostFunction() noexcept;
    void workerLoop(std::stop_token st) noexcept;

    std::mutex hostFunctionsMutex;
    std::deque<std::jthread> threads;
    std::deque<std::pair<HostFunctionStreamer *, HostFunction>> hostFunctions;
    std::counting_semaphore<> semaphore{0};
    uint32_t threadsLimit = 0;
    bool unlimitedThreads = false;
};

static_assert(NonCopyableAndNonMovable<HostFunctionThreadPool>);

} // namespace NEO

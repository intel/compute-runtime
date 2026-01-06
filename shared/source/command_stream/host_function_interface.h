/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <functional>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>

namespace NEO {

class GraphicsAllocation;
class HostFunctionStreamer;
struct HostFunction;

class HostFunctionWorker : public NonCopyableAndNonMovableClass {
  public:
    explicit HostFunctionWorker(bool skipHostFunctionExecution)
        : skipHostFunctionExecution(skipHostFunctionExecution) {
    }

    virtual ~HostFunctionWorker() = default;
    virtual void start(HostFunctionStreamer *streamer) = 0;
    virtual void finish() = 0;
    virtual void submit(uint32_t nHostFunctions) noexcept = 0;

  protected:
    std::unique_ptr<std::jthread> worker;
    std::mutex workerMutex;
    bool skipHostFunctionExecution = false;
};

static_assert(NonCopyableAndNonMovable<HostFunctionWorker>);

} // namespace NEO

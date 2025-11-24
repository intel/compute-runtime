/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/host_function_interface.h"
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

class HostFunctionSingleWorker : public HostFunctionWorker {
  public:
    explicit HostFunctionSingleWorker(bool skipHostFunctionExecution);
    ~HostFunctionSingleWorker() override = 0;

    void start(HostFunctionStreamer *streamer) override = 0;
    void finish() override = 0;
    void submit(uint32_t nHostFunctions) noexcept override = 0;

  protected:
    MOCKABLE_VIRTUAL void processNextHostFunction(std::stop_token st) noexcept;
    bool waitUntilHostFunctionIsReady(std::stop_token st) noexcept;
    HostFunctionStreamer *streamer = nullptr;
};

static_assert(NonCopyableAndNonMovable<HostFunctionSingleWorker>);

} // namespace NEO

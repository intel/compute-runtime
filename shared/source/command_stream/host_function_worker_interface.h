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
struct HostFunctionData;

class IHostFunctionWorker : public NonCopyableAndNonMovableClass {
  public:
    IHostFunctionWorker(bool skipHostFunctionExecution,
                        const std::function<void(GraphicsAllocation &)> &downloadAllocationImpl,
                        GraphicsAllocation *allocation,
                        HostFunctionData *data);
    virtual ~IHostFunctionWorker() = 0;

    virtual void start() = 0;
    virtual void finish() = 0;
    virtual void submit() noexcept = 0;

  protected:
    MOCKABLE_VIRTUAL bool runHostFunction(std::stop_token st) noexcept;
    std::unique_ptr<std::jthread> worker;
    std::mutex workerMutex;

  private:
    std::function<void(GraphicsAllocation &)> downloadAllocationImpl;
    GraphicsAllocation *allocation = nullptr;
    HostFunctionData *data = nullptr;
    bool skipHostFunctionExecution = false;
};

static_assert(NonCopyableAndNonMovable<IHostFunctionWorker>);

} // namespace NEO

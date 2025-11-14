/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/host_function_worker_cv.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/host_function.h"
#include "shared/source/utilities/wait_util.h"

namespace NEO {
HostFunctionWorkerCV::HostFunctionWorkerCV(bool skipHostFunctionExecution,
                                           const std::function<void(GraphicsAllocation &)> &downloadAllocationImpl,
                                           GraphicsAllocation *allocation,
                                           HostFunctionData *data)
    : IHostFunctionWorker(skipHostFunctionExecution, downloadAllocationImpl, allocation, data) {
}

HostFunctionWorkerCV::~HostFunctionWorkerCV() = default;

void HostFunctionWorkerCV::start() {
    std::lock_guard<std::mutex> lg{workerMutex};
    if (!worker) {
        worker = std::make_unique<std::jthread>([this](std::stop_token st) {
            this->workerLoop(std::move(st));
        });
    }
}

void HostFunctionWorkerCV::finish() {
    std::lock_guard<std::mutex> lg{workerMutex};
    if (worker) {
        worker->request_stop();
        cv.notify_one();
        worker.reset(nullptr);
    }
}

void HostFunctionWorkerCV::submit() noexcept {
    {
        std::lock_guard<std::mutex> lock{pendingAccessMutex};
        ++pending;
    }
    cv.notify_one();
}

void HostFunctionWorkerCV::workerLoop(std::stop_token st) noexcept {

    std::unique_lock<std::mutex> lock{pendingAccessMutex, std::defer_lock};

    while (true) {
        lock.lock();
        cv.wait(lock, [&]() {
            return pending > 0 || st.stop_requested();
        });

        if (st.stop_requested()) [[unlikely]] {
            return;
        }

        --pending;

        lock.unlock();

        bool sucess = this->runHostFunction(st);
        if (!sucess) [[unlikely]] {
            return;
        }
    }
}

} // namespace NEO

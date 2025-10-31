/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/host_function_worker_counting_semaphore.h"

namespace NEO {

HostFunctionWorkerCountingSemaphore::HostFunctionWorkerCountingSemaphore(bool skipHostFunctionExecution, const std::function<void(GraphicsAllocation &)> &downloadAllocationImpl, GraphicsAllocation *allocation, HostFunctionData *data)
    : IHostFunctionWorker(skipHostFunctionExecution, downloadAllocationImpl, allocation, data) {
}

HostFunctionWorkerCountingSemaphore::~HostFunctionWorkerCountingSemaphore() = default;

void HostFunctionWorkerCountingSemaphore::start() {
    std::lock_guard<std::mutex> lg{workerMutex};

    if (!worker) {
        worker = std::make_unique<std::jthread>([this](std::stop_token st) {
            this->workerLoop(st);
        });
    }
}

void HostFunctionWorkerCountingSemaphore::finish() {
    std::lock_guard<std::mutex> lg{workerMutex};

    if (worker) {
        worker->request_stop();
        semaphore.release();
        worker.reset(nullptr);
    }
}

void HostFunctionWorkerCountingSemaphore::submit() noexcept {
    semaphore.release();
}

void HostFunctionWorkerCountingSemaphore::workerLoop(std::stop_token st) noexcept {

    while (true) {

        semaphore.acquire();

        if (st.stop_requested()) [[unlikely]] {
            return;
        }

        bool success = runHostFunction(st);
        if (!success) [[unlikely]] {
            return;
        }
    }
}

} // namespace NEO

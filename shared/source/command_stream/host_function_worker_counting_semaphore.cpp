/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/host_function_worker_counting_semaphore.h"

#include "shared/source/command_stream/host_function.h"

namespace NEO {

HostFunctionWorkerCountingSemaphore::HostFunctionWorkerCountingSemaphore(bool skipHostFunctionExecution)
    : HostFunctionSingleWorker(skipHostFunctionExecution) {
}

HostFunctionWorkerCountingSemaphore::~HostFunctionWorkerCountingSemaphore() = default;

void HostFunctionWorkerCountingSemaphore::start(HostFunctionStreamer *streamer) {
    std::lock_guard<std::mutex> lg{workerMutex};

    this->streamer = streamer;

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

void HostFunctionWorkerCountingSemaphore::submit(uint32_t nHostFunctions) noexcept {
    semaphore.release(static_cast<ptrdiff_t>(nHostFunctions));
}

void HostFunctionWorkerCountingSemaphore::workerLoop(std::stop_token st) noexcept {

    while (st.stop_requested() == false) {
        semaphore.acquire();

        processNextHostFunction(st);
    }
}

} // namespace NEO

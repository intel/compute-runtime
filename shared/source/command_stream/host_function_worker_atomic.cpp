/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/host_function_worker_atomic.h"

#include "shared/source/command_stream/host_function.h"

namespace NEO {
HostFunctionWorkerAtomic::HostFunctionWorkerAtomic(bool skipHostFunctionExecution,
                                                   const std::function<void(GraphicsAllocation &)> &downloadAllocationImpl,
                                                   GraphicsAllocation *allocation,
                                                   HostFunctionData *data)
    : IHostFunctionWorker(skipHostFunctionExecution, downloadAllocationImpl, allocation, data) {
}

HostFunctionWorkerAtomic::~HostFunctionWorkerAtomic() = default;

void HostFunctionWorkerAtomic::start() {

    std::lock_guard<std::mutex> lg{workerMutex};
    if (!worker) {
        worker = std::make_unique<std::jthread>([this](std::stop_token st) {
            this->workerLoop(st);
        });
    }
}

void HostFunctionWorkerAtomic::finish() {
    std::lock_guard<std::mutex> lg{workerMutex};
    if (worker) {
        worker->request_stop();
        pending.fetch_add(1u);
        pending.notify_one();
        worker.reset(nullptr);
    }
}

void HostFunctionWorkerAtomic::submit() noexcept {
    pending.fetch_add(1, std::memory_order_release);
    pending.notify_one();
}

void HostFunctionWorkerAtomic::workerLoop(std::stop_token st) noexcept {

    while (true) {

        while (pending.load(std::memory_order_acquire) == 0) {
            pending.wait(0, std::memory_order_acquire);
        }

        if (st.stop_requested()) {
            return;
        }

        pending.fetch_sub(1, std::memory_order_acq_rel);

        bool sucess = this->runHostFunction(st);
        if (!sucess) [[unlikely]] {
            return;
        }
    }
}

} // namespace NEO

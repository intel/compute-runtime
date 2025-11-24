/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/host_function_worker_thread_pool.h"

#include "shared/source/command_stream/host_function.h"

namespace NEO {

HostFunctionThreadPool::HostFunctionThreadPool(int32_t threadsInThreadPoolLimit) {

    if (threadsInThreadPoolLimit == HostFunctionThreadPoolHelper::unlimitedThreads) {
        unlimitedThreads = true;
    } else {
        threadsLimit = static_cast<uint32_t>(threadsInThreadPoolLimit);
    }
}

HostFunctionThreadPool::~HostFunctionThreadPool() = default;

void HostFunctionThreadPool::registerThread() noexcept {

    if ((threads.size() < threadsLimit) || unlimitedThreads) {
        threads.emplace_back(([this](std::stop_token st) {
            this->workerLoop(st);
        }));
    }
}

void HostFunctionThreadPool::shutdown() noexcept {

    for (auto &thread : threads) {
        thread.request_stop();
    }

    semaphore.release(static_cast<ptrdiff_t>(threads.size()));

    for (auto &thread : threads) {
        thread.join();
    }

    {
        std::lock_guard lock{this->hostFunctionsMutex};
        hostFunctions.clear();
    }

    threads.clear();
}

void HostFunctionThreadPool::registerHostFunctionToExecute(HostFunctionStreamer *streamer, HostFunction &&hostFunction) {

    {
        std::unique_lock lock{this->hostFunctionsMutex};
        hostFunctions.emplace_back(streamer, std::move(hostFunction));
    }
    semaphore.release();
}

void NEO::HostFunctionThreadPool::workerLoop(std::stop_token st) noexcept {

    while (st.stop_requested() == false) {

        semaphore.acquire();

        if (st.stop_requested()) {
            return;
        }

        executeHostFunction();
    }
}

void HostFunctionThreadPool::executeHostFunction() noexcept {

    std::unique_lock lock{this->hostFunctionsMutex};
    auto [streamer, hostFunction] = std::move(hostFunctions.front());
    hostFunctions.pop_front();
    lock.unlock();

    hostFunction.invoke();

    streamer->signalHostFunctionCompletion(hostFunction);
}

} // namespace NEO

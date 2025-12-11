/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/host_function_scheduler.h"

#include "shared/source/command_stream/host_function.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/utilities/wait_util.h"

#include <chrono>
#include <iostream>
#include <type_traits>

namespace NEO {

HostFunctionScheduler::HostFunctionScheduler(bool skipHostFunctionExecution,
                                             int32_t threadsInThreadPoolLimit)
    : HostFunctionWorker(skipHostFunctionExecution),
      threadPool(threadsInThreadPoolLimit) {
}

HostFunctionScheduler::~HostFunctionScheduler() = default;

void HostFunctionScheduler::start(HostFunctionStreamer *streamer) {

    this->registerHostFunctionStreamer(streamer);
    this->threadPool.registerThread();

    if (worker == nullptr) {
        std::unique_lock<std::mutex> lock(workerMutex);
        if (worker == nullptr) {
            worker = std::make_unique<std::jthread>([this](std::stop_token st) {
                this->schedulerLoop(st);
            });
        }
    }
}

void HostFunctionScheduler::finish() {

    std::call_once(shutdownOnceFlag, [&]() {
        threadPool.shutdown();
        {
            std::unique_lock<std::mutex> lock(workerMutex);
            if (worker) {
                worker->request_stop();
                semaphore.release();
                worker->join();
                worker.reset(nullptr);
            }
        }

        {
            std::unique_lock<std::mutex> lock(registeredStreamersMutex);
            registeredStreamers.clear();
        }
    });
}

void HostFunctionScheduler::submit(uint32_t nHostFunctions) noexcept {
    semaphore.release(static_cast<ptrdiff_t>(nHostFunctions));
}

void HostFunctionScheduler::scheduleHostFunctionToThreadPool(HostFunctionStreamer *streamer, uint64_t id) noexcept {

    auto hostFunction = streamer->getHostFunction(id);
    streamer->prepareForExecution(hostFunction);
    threadPool.registerHostFunctionToExecute(streamer, std::move(hostFunction));
}

void HostFunctionScheduler::schedulerLoop(std::stop_token st) noexcept {

    std::unique_lock<std::mutex> registeredStreamersLock(registeredStreamersMutex, std::defer_lock);
    auto waitStart = std::chrono::steady_clock::now();

    while (st.stop_requested() == false) {
        semaphore.acquire(); // wait until there is at least one pending host function
        semaphore.release(); // leave count unchanged intentionally

        if (st.stop_requested()) {
            return;
        }

        registeredStreamersLock.lock();
        for (auto streamer : registeredStreamers) {
            if (auto id = isHostFunctionReadyToExecute(streamer); id != HostFunctionStatus::completed) {
                //  std::cout << "id : " << id << std::endl;
                scheduleHostFunctionToThreadPool(streamer, id);
                waitStart = std::chrono::steady_clock::now();
            }
        }
        registeredStreamersLock.unlock();

        if (st.stop_requested()) {
            return;
        }

        auto waitTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - waitStart);
        WaitUtils::waitFunctionWithoutPredicate(waitTime.count());
    }
}

void HostFunctionScheduler::registerHostFunctionStreamer(HostFunctionStreamer *streamer) {
    std::lock_guard<std::mutex> lock(registeredStreamersMutex);
    registeredStreamers.push_back(streamer);
}

uint64_t HostFunctionScheduler::isHostFunctionReadyToExecute(HostFunctionStreamer *streamer) {
    auto id = streamer->getHostFunctionReadyToExecute();
    if (id != HostFunctionStatus::completed && semaphore.try_acquire()) {
        return id;
    }
    return HostFunctionStatus::completed;
}

static_assert(NonCopyableAndNonMovable<HostFunctionScheduler>);

} // namespace NEO

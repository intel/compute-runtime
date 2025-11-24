/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/host_function_worker_interface.h"

#include "shared/source/command_stream/host_function.h"
#include "shared/source/utilities/wait_util.h"

#include <chrono>
#include <type_traits>

namespace NEO {
HostFunctionSingleWorker::HostFunctionSingleWorker(bool skipHostFunctionExecution)
    : HostFunctionWorker(skipHostFunctionExecution) {
}

HostFunctionSingleWorker::~HostFunctionSingleWorker() = default;

void HostFunctionSingleWorker::processNextHostFunction(std::stop_token st) noexcept {

    if (skipHostFunctionExecution == false) {
        auto hostFunctionReady = waitUntilHostFunctionIsReady(st);
        if (hostFunctionReady) {
            auto hostFunction = streamer->getHostFunction();
            streamer->prepareForExecution(hostFunction);
            hostFunction.invoke();
            streamer->signalHostFunctionCompletion(hostFunction);
        }
    }
}

bool HostFunctionSingleWorker::waitUntilHostFunctionIsReady(std::stop_token st) noexcept {

    const auto start = std::chrono::steady_clock::now();

    while (true) {

        if (st.stop_requested()) {
            return false;
        }

        streamer->downloadHostFunctionAllocation();

        auto waitTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start);
        auto hostFunctionReady = WaitUtils::waitFunctionWithPredicate<uint64_t>(streamer->getHostFunctionIdPtr(),
                                                                                HostFunctionStatus::completed,
                                                                                std::greater<uint64_t>(),
                                                                                waitTime.count());

        if (hostFunctionReady) {
            return true;
        }
    }
}

} // namespace NEO

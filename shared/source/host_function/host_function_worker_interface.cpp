/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/host_function/host_function_worker_interface.h"

#include "shared/source/host_function/host_function.h"
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
        auto hostFunctionId = waitUntilHostFunctionIsReady(st);
        if (hostFunctionId != HostFunctionStatus::completed) {
            auto hostFunction = streamer->getHostFunction(hostFunctionId);
            streamer->prepareForExecution(hostFunction);
            hostFunction.invoke();
            streamer->signalHostFunctionCompletion(hostFunction);
        }
    }
}

uint64_t HostFunctionSingleWorker::waitUntilHostFunctionIsReady(std::stop_token st) noexcept {

    const auto start = std::chrono::steady_clock::now();

    while (true) {

        if (st.stop_requested()) {
            return HostFunctionStatus::completed;
        }

        streamer->downloadHostFunctionAllocation();

        auto hostFunctionId = streamer->getHostFunctionReadyToExecute();
        if (hostFunctionId != HostFunctionStatus::completed) {
            return hostFunctionId;
        }

        auto waitTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start);
        WaitUtils::waitFunctionWithoutPredicate(waitTime.count());
    }
}

} // namespace NEO

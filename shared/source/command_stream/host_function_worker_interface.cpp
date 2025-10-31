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
IHostFunctionWorker::IHostFunctionWorker(bool skipHostFunctionExecution,
                                         const std::function<void(GraphicsAllocation &)> &downloadAllocationImpl,
                                         GraphicsAllocation *allocation,
                                         HostFunctionData *data)
    : downloadAllocationImpl(downloadAllocationImpl),
      allocation(allocation),
      data(data),
      skipHostFunctionExecution(skipHostFunctionExecution) {
}

IHostFunctionWorker::~IHostFunctionWorker() = default;

bool IHostFunctionWorker::runHostFunction(std::stop_token st) noexcept {

    using tagStatusT = std::underlying_type_t<HostFunctionTagStatus>;
    const auto start = std::chrono::steady_clock::now();
    std::chrono::microseconds waitTime{0};

    if (!this->skipHostFunctionExecution) {

        while (true) {
            if (this->downloadAllocationImpl) [[unlikely]] {
                this->downloadAllocationImpl(*this->allocation);
            }
            const volatile uint32_t *hostFuntionTagAddress = this->data->internalTag;
            waitTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start);
            bool pendingJobFound = WaitUtils::waitFunctionWithPredicate<const tagStatusT>(hostFuntionTagAddress,
                                                                                          static_cast<tagStatusT>(HostFunctionTagStatus::pending),
                                                                                          std::equal_to<tagStatusT>(),
                                                                                          waitTime.count());
            if (pendingJobFound) {
                break;
            }

            if (st.stop_requested()) {
                return false;
            }
        }

        using CallbackT = void (*)(void *);
        CallbackT callback = reinterpret_cast<CallbackT>(*this->data->entry);
        void *callbackData = reinterpret_cast<void *>(*this->data->userData);

        callback(callbackData);
    }

    *this->data->internalTag = static_cast<tagStatusT>(HostFunctionTagStatus::completed);

    return true;
}

} // namespace NEO

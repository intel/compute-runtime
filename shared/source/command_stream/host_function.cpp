/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/host_function.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/host_function_worker_atomic.h"
#include "shared/source/command_stream/host_function_worker_counting_semaphore.h"
#include "shared/source/command_stream/host_function_worker_cv.h"
#include "shared/source/command_stream/host_function_worker_interface.h"

namespace NEO::HostFunctionFactory {

IHostFunctionWorker *createHostFunctionWorker(int32_t hostFunctionWorkerMode,
                                              bool isAubMode,
                                              const std::function<void(GraphicsAllocation &)> &downloadAllocationImpl,
                                              GraphicsAllocation *allocation,
                                              HostFunctionData *data) {

    bool skipHostFunctionExecution = isAubMode;

    switch (hostFunctionWorkerMode) {
    default:
    case 0:
        return new HostFunctionWorkerCountingSemaphore(skipHostFunctionExecution, downloadAllocationImpl, allocation, data);
    case 1:
        return new HostFunctionWorkerCV(skipHostFunctionExecution, downloadAllocationImpl, allocation, data);
    case 2:
        return new HostFunctionWorkerAtomic(skipHostFunctionExecution, downloadAllocationImpl, allocation, data);
    }
}

} // namespace NEO::HostFunctionFactory

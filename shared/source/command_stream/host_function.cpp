/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/host_function.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/host_function_interface.h"
#include "shared/source/command_stream/host_function_scheduler.h"
#include "shared/source/command_stream/host_function_worker_counting_semaphore.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {
HostFunctionStreamer::HostFunctionStreamer(GraphicsAllocation *allocation,
                                           void *hostFunctionIdAddress,
                                           const std::function<void(GraphicsAllocation &)> &downloadAllocationImpl,
                                           bool isTbx)
    : hostFunctionIdAddress(reinterpret_cast<volatile uint64_t *>(hostFunctionIdAddress)),
      allocation(allocation),
      downloadAllocationImpl(downloadAllocationImpl),
      nextHostFunctionId(1), // start from 1 to keep 0 bit for pending/completed status
      isTbx(isTbx) {
}

uint64_t HostFunctionStreamer::getHostFunctionIdGpuAddress() const {
    return reinterpret_cast<uint64_t>(hostFunctionIdAddress);
}

volatile uint64_t *HostFunctionStreamer::getHostFunctionIdPtr() const {
    return hostFunctionIdAddress;
}

uint64_t HostFunctionStreamer::getNextHostFunctionIdAndIncrement() {
    // increment by 2 to keep 0 bit for pending/completed status
    return nextHostFunctionId.fetch_add(2, std::memory_order_acq_rel);
}

uint64_t HostFunctionStreamer::getHostFunctionId() const {
    return *hostFunctionIdAddress;
}

void HostFunctionStreamer::signalHostFunctionCompletion(const HostFunction &hostFunction) {
    setHostFunctionIdAsCompleted();
    endInOrderExecution();
}

void HostFunctionStreamer::prepareForExecution(const HostFunction &hostFunction) {
    startInOrderExecution();
    pendingHostFunctions.fetch_sub(1, std::memory_order_acq_rel);
}

void HostFunctionStreamer::setHostFunctionIdAsCompleted() {
    *hostFunctionIdAddress = HostFunctionStatus::completed;
}

void HostFunctionStreamer::endInOrderExecution() {
    inOrderExecutionInProgress.store(false, std::memory_order_release);
}

void HostFunctionStreamer::startInOrderExecution() {
    inOrderExecutionInProgress.store(true, std::memory_order_release);
}

bool HostFunctionStreamer::isInOrderExecutionInProgress() const {
    return inOrderExecutionInProgress.load(std::memory_order_acquire);
}

HostFunction HostFunctionStreamer::getHostFunction() {
    std::unique_lock lock(hostFunctionsMutex);
    auto hostFunctionId = getHostFunctionId();
    auto node = hostFunctions.extract(hostFunctionId);
    if (!node) {
        UNRECOVERABLE_IF(true);
        return HostFunction{};
    }

    return std::move(node.mapped());
}

HostFunction HostFunctionStreamer::getHostFunction(uint64_t hostFunctionId) {
    std::unique_lock lock(hostFunctionsMutex);
    auto node = hostFunctions.extract(hostFunctionId);
    if (!node) {
        UNRECOVERABLE_IF(true);
        return HostFunction{};
    }

    return std::move(node.mapped());
}

void HostFunctionStreamer::addHostFunction(uint64_t hostFunctionId, HostFunction &&hostFunction) {
    {
        std::unique_lock lock(hostFunctionsMutex);
        hostFunctions.emplace(hostFunctionId, std::move(hostFunction));
    }
    pendingHostFunctions.fetch_add(1, std::memory_order_acq_rel);
}

GraphicsAllocation *HostFunctionStreamer::getHostFunctionIdAllocation() const {
    return allocation;
}

void HostFunctionStreamer::downloadHostFunctionAllocation() const {
    if (isTbx) {
        downloadAllocationImpl(*allocation);
    }
}

uint64_t HostFunctionStreamer::isHostFunctionReadyToExecute() const {
    if (pendingHostFunctions.load(std::memory_order_acquire) == 0) {
        return false;
    }

    if (isInOrderExecutionInProgress()) {
        return false;
    }

    downloadHostFunctionAllocation();

    auto hostFunctionId = getHostFunctionId();
    return hostFunctionId;
}

namespace HostFunctionFactory {
void createAndSetHostFunctionWorker(HostFunctionWorkerMode hostFunctionWorkerMode,
                                    bool skipHostFunctionExecution,
                                    CommandStreamReceiver *csr,
                                    RootDeviceEnvironment *rootDeviceEnvironment) {

    if (csr->getHostFunctionWorker() != nullptr) {
        return;
    }

    switch (hostFunctionWorkerMode) {
    default:
    case HostFunctionWorkerMode::defaultMode:
    case HostFunctionWorkerMode::countingSemaphore:
        csr->setHostFunctionWorker(new HostFunctionWorkerCountingSemaphore(skipHostFunctionExecution));
        break;
    case HostFunctionWorkerMode::schedulerWithThreadPool: {
        auto scheduler = rootDeviceEnvironment->getHostFunctionScheduler();
        if (scheduler == nullptr) {
            int32_t nWorkers = (debugManager.flags.HostFunctionThreadPoolSize.get() > 0)
                                   ? debugManager.flags.HostFunctionThreadPoolSize.get()
                                   : HostFunctionThreadPoolHelper::unlimitedThreads;

            auto createdScheduler = std::make_unique<HostFunctionScheduler>(skipHostFunctionExecution,
                                                                            nWorkers);

            rootDeviceEnvironment->setHostFunctionScheduler(std::move(createdScheduler));
        }

        scheduler = rootDeviceEnvironment->getHostFunctionScheduler();
        csr->setHostFunctionWorker(scheduler);
        break;
    }
    }
}

} // namespace HostFunctionFactory

} // namespace NEO

/*
 * Copyright (C) 2025-2026 Intel Corporation
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
HostFunctionStreamer::HostFunctionStreamer(CommandStreamReceiver *csr,
                                           GraphicsAllocation *allocation,
                                           void *hostFunctionIdAddress,
                                           const std::function<void(GraphicsAllocation &)> &downloadAllocationImpl,
                                           uint32_t activePartitions,
                                           uint32_t partitionOffset,
                                           bool isTbx,
                                           bool dcFlushRequired,
                                           bool useSemaphore64bCmd)
    : hostFunctionIdAddress(reinterpret_cast<uint64_t *>(hostFunctionIdAddress)),
      csr(csr),
      allocation(allocation),
      downloadAllocationImpl(downloadAllocationImpl),
      nextHostFunctionId(1), // start from 1 to keep 0 bit for pending/completed status
      activePartitions(activePartitions),
      partitionOffset(partitionOffset),
      isTbx(isTbx),
      dcFlushRequired(dcFlushRequired),
      useSemaphore64bCmd(useSemaphore64bCmd) {
}

uint64_t HostFunctionStreamer::getHostFunctionIdGpuAddress(uint32_t partitionId) const {
    auto offset = partitionId * partitionOffset;
    return reinterpret_cast<uint64_t>(ptrOffset(hostFunctionIdAddress, offset));
}

uint64_t *HostFunctionStreamer::getHostFunctionIdPtr(uint32_t partitionId) const {
    return ptrOffset(hostFunctionIdAddress, partitionId * partitionOffset);
}

uint64_t HostFunctionStreamer::getNextHostFunctionIdAndIncrement() {
    // increment by 2 to keep 0 bit for pending/completed status
    return nextHostFunctionId.fetch_add(2, std::memory_order_acq_rel);
}

uint64_t HostFunctionStreamer::getHostFunctionId(uint32_t partitionId) const {
    auto ptr = ptrOffset(this->hostFunctionIdAddress, partitionId * partitionOffset);
    return std::atomic_ref<uint64_t>(*ptr).load(std::memory_order_acquire);
}

void HostFunctionStreamer::signalHostFunctionCompletion(const HostFunction &hostFunction) {
    setHostFunctionIdAsCompleted();
    endInOrderExecution();
}

void HostFunctionStreamer::prepareForExecution(const HostFunction &hostFunction) {
    startInOrderExecution();
    pendingHostFunctions.fetch_sub(1, std::memory_order_acq_rel);
}

uint32_t HostFunctionStreamer::getActivePartitions() const {
    return activePartitions;
}

bool HostFunctionStreamer::getDcFlushRequired() const { return dcFlushRequired; }

void HostFunctionStreamer::updateTbxData() {
    constexpr uint32_t allBanks = std::numeric_limits<uint32_t>::max();
    allocation->setTbxWritable(true, allBanks);

    for (auto partitionId = 0u; partitionId < activePartitions; partitionId++) {
        auto offset = ptrDiff(getHostFunctionIdGpuAddress(partitionId), allocation->getGpuAddress());
        csr->writeMemory(*allocation, true, offset, sizeof(uint64_t));
    }

    allocation->setTbxWritable(false, allBanks);
}

void HostFunctionStreamer::setHostFunctionIdAsCompleted() {
    auto setAsCompleted = [this]() {
        for (auto partitionId = 0u; partitionId < activePartitions; partitionId++) {
            auto ptr = getHostFunctionIdPtr(partitionId);
            std::atomic_ref<uint64_t>(*ptr).store(HostFunctionStatus::completed, std::memory_order_release);
        }
    };

    if (isTbx) {
        auto lock = csr->obtainTagAllocationDownloadLock();
        setAsCompleted();
        updateTbxData();

    } else {
        setAsCompleted();
    }
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

uint64_t HostFunctionStreamer::getHostFunctionReadyToExecute() const {
    if (pendingHostFunctions.load(std::memory_order_acquire) == 0) {
        return HostFunctionStatus::completed;
    }

    if (isInOrderExecutionInProgress()) {
        return HostFunctionStatus::completed;
    }

    downloadHostFunctionAllocation();

    uint64_t hostFunctionId = HostFunctionStatus::completed;

    for (auto partitionId = 0u; partitionId < activePartitions; partitionId++) {
        hostFunctionId = getHostFunctionId(partitionId);
        bool hostFunctionNotReady = hostFunctionId == HostFunctionStatus::completed;
        if (hostFunctionNotReady) {
            return HostFunctionStatus::completed;
        }
    }

    return hostFunctionId;
}

bool HostFunctionStreamer::isUsingSemaphore64bCmd() const {
    return useSemaphore64bCmd;
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

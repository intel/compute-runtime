/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <unordered_map>

namespace NEO {

class LinearStream;
class CommandStreamReceiver;
class IHostFunction;
class GraphicsAllocation;
struct RootDeviceEnvironment;

struct HostFunction {
    uint64_t hostFunctionAddress = 0;
    uint64_t userDataAddress = 0;

    void invoke() const {

        using CallbackT = void (*)(void *);
        CallbackT callback = reinterpret_cast<CallbackT>(hostFunctionAddress);
        void *callbackData = reinterpret_cast<void *>(userDataAddress);

        callback(callbackData);
    }
};

namespace HostFunctionStatus {
inline constexpr uint64_t completed = 0;
} // namespace HostFunctionStatus

namespace HostFunctionThreadPoolHelper {
inline constexpr int32_t unlimitedThreads = -1; // each CSR that uses host function creates worker thread in thread pool
}

class HostFunctionStreamer {
  public:
    HostFunctionStreamer(CommandStreamReceiver *csr,
                         GraphicsAllocation *allocation,
                         void *hostFunctionIdAddress,
                         const std::function<void(GraphicsAllocation &)> &downloadAllocationImpl,
                         uint32_t activePartition,
                         uint32_t partitionOffset,
                         bool isTbx,
                         bool dcFlushRequired);
    ~HostFunctionStreamer() = default;

    uint64_t getHostFunctionReadyToExecute() const;
    GraphicsAllocation *getHostFunctionIdAllocation() const;
    HostFunction getHostFunction(uint64_t hostFunctionId);
    uint64_t getHostFunctionId(uint32_t partitionId) const;
    uint64_t getHostFunctionIdGpuAddress(uint32_t partitionId) const;
    uint64_t *getHostFunctionIdPtr(uint32_t partitionId) const;
    uint64_t getNextHostFunctionIdAndIncrement();

    void addHostFunction(uint64_t hostFunctionId, HostFunction &&hostFunction);
    void downloadHostFunctionAllocation() const;
    void signalHostFunctionCompletion(const HostFunction &hostFunction);
    void prepareForExecution(const HostFunction &hostFunction);
    uint32_t getActivePartitions() const;
    bool getDcFlushRequired() const;

  private:
    void updateTbxData();
    void setHostFunctionIdAsCompleted();
    void startInOrderExecution();
    void endInOrderExecution();
    bool isInOrderExecutionInProgress() const;

    std::mutex hostFunctionsMutex;
    std::unordered_map<uint64_t, HostFunction> hostFunctions;
    uint64_t *hostFunctionIdAddress = nullptr; // 0 bit - used to signal that host function is pending or completed
    CommandStreamReceiver *csr = nullptr;
    GraphicsAllocation *allocation = nullptr;
    std::function<void(GraphicsAllocation &)> downloadAllocationImpl;
    std::atomic<uint64_t> nextHostFunctionId{1};
    std::atomic<uint32_t> pendingHostFunctions{0};
    uint32_t activePartitions{1};
    uint32_t partitionOffset{0};
    std::atomic<bool> inOrderExecutionInProgress{false};
    const bool isTbx = false;
    bool dcFlushRequired = false;
};

enum class HostFunctionWorkerMode : int32_t {
    defaultMode = -1,
    countingSemaphore = 0,
    schedulerWithThreadPool = 1,
};

template <typename GfxFamily>
struct HostFunctionHelper {
    static void programHostFunction(LinearStream &commandStream, HostFunctionStreamer &streamer, HostFunction &&hostFunction, bool isMemorySynchronizationRequired);
    static void programHostFunctionId(LinearStream *commandStream, void *cmdBuffer, HostFunctionStreamer &streamer, HostFunction &&hostFunction, bool isMemorySynchronizationRequired);
    static void programHostFunctionWaitForCompletion(LinearStream *commandStream, void *cmdBuffer, const HostFunctionStreamer &streamer, uint32_t partionId);
    static bool isMemorySynchronizationRequiredForHostFunction();
    static bool usePipeControlForHostFunction(bool dcFlushRequiredPlatform);
};

namespace HostFunctionFactory {

void createAndSetHostFunctionWorker(HostFunctionWorkerMode hostFunctionWorkerMode,
                                    bool skipHostFunctionExecution,
                                    CommandStreamReceiver *csr,
                                    RootDeviceEnvironment *rootDeviceEnvironment);

} // namespace HostFunctionFactory

} // namespace NEO

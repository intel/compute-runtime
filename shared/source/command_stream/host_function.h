/*
 * Copyright (C) 2025 Intel Corporation
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
    HostFunctionStreamer(GraphicsAllocation *allocation, void *hostFunctionIdAddress, const std::function<void(GraphicsAllocation &)> &downloadAllocationImpl, bool isTbx);
    ~HostFunctionStreamer() = default;

    uint64_t isHostFunctionReadyToExecute() const;
    GraphicsAllocation *getHostFunctionIdAllocation() const;
    HostFunction getHostFunction();
    HostFunction getHostFunction(uint64_t hostFunctionId);
    uint64_t getHostFunctionId() const;
    uint64_t getHostFunctionIdGpuAddress() const;
    volatile uint64_t *getHostFunctionIdPtr() const;
    uint64_t getNextHostFunctionIdAndIncrement();

    void addHostFunction(uint64_t hostFunctionId, HostFunction &&hostFunction);
    void downloadHostFunctionAllocation() const;
    void signalHostFunctionCompletion(const HostFunction &hostFunction);
    void prepareForExecution(const HostFunction &hostFunction);

  private:
    void setHostFunctionIdAsCompleted();
    void startInOrderExecution();
    void endInOrderExecution();
    bool isInOrderExecutionInProgress() const;

    std::mutex hostFunctionsMutex;
    std::unordered_map<uint64_t, HostFunction> hostFunctions;
    volatile uint64_t *hostFunctionIdAddress = nullptr; // 0 bit - used to signal that host function is pending or completed
    GraphicsAllocation *allocation = nullptr;
    std::function<void(GraphicsAllocation &)> downloadAllocationImpl;
    std::atomic<uint64_t> nextHostFunctionId{1};
    std::atomic<uint32_t> pendingHostFunctions{0};
    std::atomic<bool> inOrderExecutionInProgress{false};
    const bool isTbx = false;
};

enum class HostFunctionWorkerMode : int32_t {
    defaultMode = -1,
    countingSemaphore = 0,
    schedulerWithThreadPool = 1,
};

template <typename GfxFamily>
struct HostFunctionHelper {
    static void programHostFunction(LinearStream &commandStream, HostFunctionStreamer &streamer, HostFunction &&hostFunction);
    static void programHostFunctionId(LinearStream *commandStream, void *cmdBuffer, HostFunctionStreamer &streamer, HostFunction &&hostFunction);
    static void programHostFunctionWaitForCompletion(LinearStream *commandStream, void *cmdBuffer, const HostFunctionStreamer &streamer);
};

namespace HostFunctionFactory {

void createAndSetHostFunctionWorker(HostFunctionWorkerMode hostFunctionWorkerMode,
                                    bool skipHostFunctionExecution,
                                    CommandStreamReceiver *csr,
                                    RootDeviceEnvironment *rootDeviceEnvironment);

} // namespace HostFunctionFactory

} // namespace NEO

/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/host_function/host_function_worker_counting_semaphore.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_host_function_allocator.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

#include <atomic>
#include <tuple>
#include <vector>

namespace {

inline constexpr int32_t countingSemaphoreTestingMode = static_cast<int32_t>(HostFunctionWorkerMode::countingSemaphore);
inline constexpr int32_t threadPoolTestingMode = static_cast<int32_t>(HostFunctionWorkerMode::schedulerWithThreadPool);

class MockCommandStreamReceiverHostFunction : public MockCommandStreamReceiver {
  public:
    using MockCommandStreamReceiver::hostFunctionWorker;
    using MockCommandStreamReceiver::MockCommandStreamReceiver;

    void createHostFunctionWorker() override {
        CommandStreamReceiver::createHostFunctionWorker();
    }

    void signalHostFunctionWorker(uint32_t nHostFunctions) override {
        CommandStreamReceiver::signalHostFunctionWorker(nHostFunctions);
    }
};

struct Arg {
    uint32_t expected = 0;
    std::atomic<uint32_t> result{0};
    std::atomic<uint32_t> counter{0};
};

extern "C" void hostFunctionExample(void *data) {
    Arg *arg = static_cast<Arg *>(data);
    arg->result.store(arg->expected, std::memory_order_release);
    arg->counter.fetch_add(1, std::memory_order_acq_rel);
}

void createArgs(std::vector<std::unique_ptr<Arg>> &hostFunctionArgs, uint32_t n) {
    hostFunctionArgs.reserve(n);

    for (auto i = 0u; i < n; i++) {
        auto arg = std::make_unique<Arg>();
        arg->expected = i + 1;
        hostFunctionArgs.emplace_back(std::move(arg));
    }
}

class HostFunctionMtFixture {
  public:
    void configureCSRs(uint32_t numberOfCSRs, uint32_t callbacksPerCsr, int32_t testingMode, uint32_t nPrimaryCSRs, uint32_t nPartitions) {
        this->callbacksPerCsr = callbacksPerCsr;
        this->nPartitions = nPartitions;
        executionEnvironment.prepareRootDeviceEnvironments(1);
        mockMemoryManager = std::make_unique<MockMemoryManager>(executionEnvironment);
        executionEnvironment.memoryManager.reset(mockMemoryManager.release());
        auto bitfield = maxNBitValue(nPartitions);
        DeviceBitfield deviceBitfield(bitfield);

        createArgs(this->hostFunctionArgs, numberOfCSRs);
        for (auto i = 0u; i < numberOfCSRs; i++) {
            csrs.push_back(std::make_unique<MockCommandStreamReceiverHostFunction>(executionEnvironment, 0, deviceBitfield));
            csrs[i]->activePartitions = nPartitions;
            csrs[i]->immWritePostSyncWriteOffset = 32u;
        }

        if (!csrs.empty()) {

            uint32_t rootDeviceIndex = csrs[0]->getRootDeviceIndex();
            uint32_t maxRootDeviceIndex = static_cast<uint32_t>(csrs[0]->peekExecutionEnvironment().rootDeviceEnvironments.size() - 1);
            hostFunctionAllocator = std::make_unique<MockHostFunctionAllocator>(executionEnvironment.memoryManager.get(),
                                                                                csrs[0].get(),
                                                                                MemoryConstants::pageSize64k,
                                                                                rootDeviceIndex,
                                                                                maxRootDeviceIndex,
                                                                                nPartitions,
                                                                                false);
        }

        if (nPrimaryCSRs > 0) {
            setupPrimaryCSRs(nPrimaryCSRs, numberOfCSRs);
        }

        for (auto &csr : csrs) {
            csr->ensureHostFunctionWorkerStarted(hostFunctionAllocator.get());
        }

        for (auto i = 0u; i < csrs.size(); i++) {

            auto &streamer = csrs[i]->getHostFunctionStreamer();

            for (auto k = 0u; k < callbacksPerCsr; k++) {

                HostFunction hostFunction = {
                    .hostFunctionAddress = reinterpret_cast<uint64_t>(hostFunctionExample),
                    .userDataAddress = reinterpret_cast<uint64_t>(this->hostFunctionArgs[i].get())};

                auto hostFunctionId = streamer.getNextHostFunctionIdAndIncrement();
                streamer.addHostFunction(hostFunctionId, std::move(hostFunction));
            }
        }
    }

    void setupPrimaryCSRs(uint32_t nPrimaryCSRs, uint32_t numberOfCSRs) {

        switch (nPrimaryCSRs) {
        case 1:
            for (auto i = 1u; i < numberOfCSRs; i++) {
                csrs[i]->primaryCsr = csrs[0].get();
            }
            break;
        case 2:
            for (auto i = 2u; i < numberOfCSRs; i++) {
                uint32_t primaryIdx = (i % 2 == 0) ? 0 : 1;
                csrs[i]->primaryCsr = csrs[primaryIdx].get();
            }
            break;
        default:
            break;
        }
    }

    bool isHostFunctionWorkCompletedOnCsr(uint32_t i) {

        auto allTilesCompleted = true;
        for (auto partition = 0u; partition < nPartitions; partition++) {
            auto idPtr = csrs[i]->getHostFunctionStreamer().getHostFunctionIdPtr(partition);
            auto id = std::atomic_ref<uint64_t>(*idPtr).load(std::memory_order_acquire);
            if (id != HostFunctionStatus::completed) {
                allTilesCompleted = false;
                break;
            }
        }

        return allTilesCompleted;
    }

    void programNextHostFunctionOnCsr(uint32_t i, uint32_t callbacksPerCsrCounter) {
        auto hostFunctionId = (callbacksPerCsrCounter * 2) + 1;
        for (auto partition = 0u; partition < nPartitions; partition++) {
            auto ptr = csrs[i]->getHostFunctionStreamer().getHostFunctionIdPtr(partition);
            std::atomic_ref<uint64_t>(*ptr).store(hostFunctionId, std::memory_order_release);
        }
    }
    void simulateGpuContexts() {
        auto expectedAllCallbacks = csrs.size() * callbacksPerCsr;
        auto callbacksCounter = 0u;
        std::vector<uint32_t> callbacksPerCsrCounter(csrs.size(), 0);

        while (true) {
            for (auto i = 0u; i < csrs.size(); i++) {
                if (isHostFunctionWorkCompletedOnCsr(i)) {
                    bool csrHasMoreCallbacks = callbacksPerCsrCounter[i] < callbacksPerCsr;
                    if (csrHasMoreCallbacks) {
                        programNextHostFunctionOnCsr(i, callbacksPerCsrCounter[i]);
                        ++callbacksPerCsrCounter[i];
                        ++callbacksCounter;
                    }
                }
            }

            if (callbacksCounter == expectedAllCallbacks) {
                break;
            }

            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

    void waitForCallbacksCompletion() {

        while (true) {
            uint32_t csrsCompleted = 0u;
            for (auto i = 0u; i < csrs.size(); i++) {
                if (isHostFunctionWorkCompletedOnCsr(i)) {
                    ++csrsCompleted;
                }
            }

            if (csrsCompleted == csrs.size()) {
                break;
            }

            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

        for (auto i = 0u; i < csrs.size(); i++) {
            csrs[i]->hostFunctionWorker->finish();
        }
    }

    void checkResults() {
        uint32_t expectedCounter = callbacksPerCsr;

        for (auto i = 0u; i < csrs.size(); i++) {
            Arg &arg = *(this->hostFunctionArgs[i].get());
            EXPECT_EQ(arg.expected, arg.result.load(std::memory_order_acquire));
            EXPECT_EQ(uint32_t{i + 1u}, arg.result.load(std::memory_order_acquire));
            EXPECT_EQ(expectedCounter, arg.counter.load(std::memory_order_acquire));

            auto &streamer = csrs[i]->getHostFunctionStreamer();
            for (auto partition = 0u; partition < nPartitions; partition++) {
                EXPECT_EQ(HostFunctionStatus::completed, streamer.getHostFunctionId(partition));
            }
        }
    }

    void clearResources() {
        hostFunctionAllocator.reset();
        csrs.clear();
        hostFunctionArgs.clear();
    }

    std::vector<std::unique_ptr<Arg>> hostFunctionArgs;
    std::vector<std::unique_ptr<MockCommandStreamReceiverHostFunction>> csrs;
    DebugManagerStateRestore restorer{};
    MockExecutionEnvironment executionEnvironment;
    uint32_t callbacksPerCsr = 0;
    uint32_t nPartitions = 0;

    std::unique_ptr<MockMemoryManager> mockMemoryManager;
    std::unique_ptr<MockHostFunctionAllocator> hostFunctionAllocator;
};

class HostFunctionMtTestP : public ::testing::TestWithParam<std::tuple<int32_t, uint32_t, uint32_t>>, public HostFunctionMtFixture {
  public:
    void SetUp() override {

        std::tie(testingMode, nPrimaryCsrs, nPartitions) = GetParam();

        debugManager.flags.HostFunctionWorkMode.set(testingMode);

        if (testingMode == threadPoolTestingMode) {
            debugManager.flags.HostFunctionThreadPoolSize.set(2);
        }
    }

    void TearDown() override {
    }

    DebugManagerStateRestore restorer{};
    int32_t testingMode = 0;
    uint32_t nPrimaryCsrs = 0;
    uint32_t nPartitions = 0;
};

TEST_P(HostFunctionMtTestP, givenHostFunctionWorkersWhenSequentialCsrJobIsSubmittedThenHostFunctionsWorkIsDoneCorrectly) {
    uint32_t numberOfCSRs = 6;
    uint32_t callbacksPerCsr = 12;

    configureCSRs(numberOfCSRs, callbacksPerCsr, testingMode, nPrimaryCsrs, nPartitions);

    // each csr will enqueue multiple host function callbacks

    for (auto iCallback = 0u; iCallback < callbacksPerCsr; iCallback++) {
        for (auto &csr : csrs) {
            csr->signalHostFunctionWorker(1u);
        }
    }

    simulateGpuContexts();
    waitForCallbacksCompletion();
    checkResults();
    clearResources();
}

TEST_P(HostFunctionMtTestP, givenHostFunctionWorkersWhenEachCsrSubmitAllCalbacksPerThreadThenHostFunctionsWorkIsDoneCorrectly) {
    uint32_t numberOfCSRs = 6;
    uint32_t callbacksPerCsr = 12;

    configureCSRs(numberOfCSRs, callbacksPerCsr, testingMode, nPrimaryCsrs, nPartitions);

    // each csr gets its own thread to submit all host functions
    auto nSubmitters = csrs.size();
    std::vector<std::jthread> submitters;
    submitters.reserve(nSubmitters);

    auto submitAllCallbacksPerCsr = [&](uint32_t idxCsr) {
        auto csr = csrs[idxCsr].get();
        csr->signalHostFunctionWorker(callbacksPerCsr);
    };

    for (auto i = 0u; i < nSubmitters; i++) {
        submitters.emplace_back([&, idx = i]() {
            submitAllCallbacksPerCsr(idx);
        });
    }

    for (auto i = 0u; i < nSubmitters; i++) {
        submitters[i].join();
    }

    simulateGpuContexts();
    waitForCallbacksCompletion();
    checkResults();
    clearResources();
}

TEST_P(HostFunctionMtTestP, givenHostFunctionWorkersWhenCsrJobsAreSubmittedConcurrentlyThenHostFunctionsWorkIsDoneCorrectly) {
    uint32_t numberOfCSRs = 6;
    uint32_t callbacksPerCsr = 12;

    configureCSRs(numberOfCSRs, callbacksPerCsr, testingMode, nPrimaryCsrs, nPartitions);

    auto nSubmitters = callbacksPerCsr;
    std::vector<std::jthread> submitters;
    submitters.reserve(nSubmitters);

    // multiple threads can submit host function in parallel using the same csr
    auto submitOnceCallbackForAllCSRs = [&]() {
        for (auto &csr : csrs) {
            csr->signalHostFunctionWorker(1u);
        }
    };

    for (auto i = 0u; i < callbacksPerCsr; i++) {
        submitters.emplace_back([&]() {
            submitOnceCallbackForAllCSRs();
        });
    }

    for (auto i = 0u; i < nSubmitters; i++) {
        submitters[i].join();
    }

    simulateGpuContexts();
    waitForCallbacksCompletion();
    checkResults();
    clearResources();
}

INSTANTIATE_TEST_SUITE_P(
    AllModes,
    HostFunctionMtTestP,
    ::testing::Values(                                         // testingMode, nPrimaryCsrs, nPartitions
        std::make_tuple(countingSemaphoreTestingMode, 0u, 1u), // Counting Semaphore implementation
        std::make_tuple(countingSemaphoreTestingMode, 0u, 2u)  // Counting Semaphore implementation with implicit scaling (2 partitions)
        ));

} // namespace

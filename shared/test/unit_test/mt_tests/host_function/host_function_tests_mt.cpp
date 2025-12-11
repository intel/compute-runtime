/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/host_function_worker_counting_semaphore.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"

#include <atomic>
#include <vector>

#if defined(__clang__)
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define NEO_TSAN_ENABLED 1
#endif
#endif
#elif defined(__GNUC__)
#if defined(__SANITIZE_THREAD__)
#define NEO_TSAN_ENABLED 1
#endif
#endif

#ifdef NEO_TSAN_ENABLED
extern "C" void __tsan_ignore_thread_begin();
extern "C" void __tsan_ignore_thread_end();
#define TSAN_ANNOTATE_IGNORE_BEGIN() __tsan_ignore_thread_begin()
#define TSAN_ANNOTATE_IGNORE_END() __tsan_ignore_thread_end()
#else
#define TSAN_ANNOTATE_IGNORE_BEGIN() ((void)0)
#define TSAN_ANNOTATE_IGNORE_END() ((void)0)
#endif

namespace {
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
    uint32_t result = 0;
    std::atomic<uint32_t> counter{0};
};

extern "C" void hostFunctionExample(void *data) {
    Arg *arg = static_cast<Arg *>(data);
    arg->result = arg->expected;
    arg->counter.fetch_add(1, std::memory_order_acq_rel);
}

void createArgs(std::vector<std::unique_ptr<Arg>> &hostFunctionArgs, uint32_t n) {
    hostFunctionArgs.reserve(n);

    for (auto i = 0u; i < n; i++) {
        hostFunctionArgs.emplace_back(std::make_unique<Arg>(i + 1, 0, 0));
    }
}

class HostFunctionMtFixture {
  public:
    void configureCSRs(uint32_t numberOfCSRs, uint32_t callbacksPerCsr, uint32_t testingMode, uint32_t primaryCSRs) {
        this->callbacksPerCsr = callbacksPerCsr;

        executionEnvironment.prepareRootDeviceEnvironments(1);
        executionEnvironment.initializeMemoryManager();
        DeviceBitfield deviceBitfield(1);

        createArgs(this->hostFunctionArgs, numberOfCSRs);
        for (auto i = 0u; i < numberOfCSRs; i++) {
            csrs.push_back(std::make_unique<MockCommandStreamReceiverHostFunction>(executionEnvironment, 0, deviceBitfield));
        }

        if (testingMode == 1) {
            // csrs[0] is primary for all other csrs
            for (auto i = 1u; i < numberOfCSRs; i++) {
                csrs[i]->primaryCsr = csrs[0].get();
            }
        } else if (testingMode == 2) {
            // csrs[0] and csrs[1] are primaries for other csrs
            // secondary split between two primaries
            for (auto i = 2u; i < numberOfCSRs; i++) {
                uint32_t primaryIdx = (i % 2 == 0) ? 0 : 1;
                csrs[i]->primaryCsr = csrs[primaryIdx].get();
            }
        }
        for (auto &csr : csrs) {
            csr->initializeTagAllocation();
        }

        for (auto &csr : csrs) {
            csr->ensureHostFunctionWorkerStarted();
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

    void simulateGpuContexts() {
        auto expectedAllCallbacks = csrs.size() * callbacksPerCsr;
        auto callbacksCounter = 0u;
        std::vector<uint32_t> callbacksPerCsrCounter(csrs.size(), 0);

        TSAN_ANNOTATE_IGNORE_BEGIN();

        while (true) {
            for (auto i = 0u; i < csrs.size(); i++) {
                if (csrs[i]->getHostFunctionStreamer().getHostFunctionId() == HostFunctionStatus::completed) {

                    if (callbacksPerCsrCounter[i] < callbacksPerCsr) {
                        auto hostFunctionId = (callbacksPerCsrCounter[i] * 2) + 1;
                        *csrs[i]->getHostFunctionStreamer().getHostFunctionIdPtr() = hostFunctionId;

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

        TSAN_ANNOTATE_IGNORE_END();
    }

    void waitForCallbacksCompletion() {
        TSAN_ANNOTATE_IGNORE_BEGIN();

        while (true) {
            uint32_t csrsCompleted = 0u;
            for (auto i = 0u; i < csrs.size(); i++) {
                if (csrs[i]->getHostFunctionStreamer().getHostFunctionId() == HostFunctionStatus::completed) {
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

        TSAN_ANNOTATE_IGNORE_END();
    }

    void checkResults() {
        uint32_t expectedCounter = callbacksPerCsr;
        TSAN_ANNOTATE_IGNORE_BEGIN();

        for (auto i = 0u; i < csrs.size(); i++) {
            Arg &arg = *(this->hostFunctionArgs[i].get());
            EXPECT_EQ(arg.expected, arg.result);
            EXPECT_EQ(uint32_t{i + 1u}, arg.result);
            EXPECT_EQ(expectedCounter, arg.counter.load());

            auto &streamer = csrs[i]->getHostFunctionStreamer();
            EXPECT_EQ(HostFunctionStatus::completed, streamer.getHostFunctionId());
        }
        TSAN_ANNOTATE_IGNORE_END();
    }

    void clearResources() {
        csrs.clear();
        hostFunctionArgs.clear();
    }

    std::vector<std::unique_ptr<Arg>> hostFunctionArgs;
    std::vector<std::unique_ptr<MockCommandStreamReceiverHostFunction>> csrs;
    DebugManagerStateRestore restorer{};
    uint32_t callbacksPerCsr = 0;
    MockExecutionEnvironment executionEnvironment;
};

class HostFunctionMtTestP : public ::testing::TestWithParam<int>, public HostFunctionMtFixture {
  public:
    void SetUp() override {

#ifdef NEO_TSAN_ENABLED
        GTEST_SKIP();
#endif

        auto param = GetParam();
        this->testingMode = static_cast<int>(param);
        debugManager.flags.HostFunctionWorkMode.set(this->testingMode);

        if (testingMode == 1 || testingMode == 2) {
            debugManager.flags.HostFunctionThreadPoolSize.set(2);
            debugManager.flags.HostFunctionWorkMode.set(static_cast<int32_t>(HostFunctionWorkerMode::schedulerWithThreadPool));
        }
    }

    void TearDown() override {
    }

    int primaryCSRs = 0;
    DebugManagerStateRestore restorer{};
    int testingMode = 0;
};

TEST_P(HostFunctionMtTestP, givenHostFunctionWorkersWhenSequentialCsrJobIsSubmittedThenHostFunctionsWorkIsDoneCorrectly) {

    uint32_t numberOfCSRs = 6;
    uint32_t callbacksPerCsr = 12;

    configureCSRs(numberOfCSRs, callbacksPerCsr, testingMode, primaryCSRs);

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

    configureCSRs(numberOfCSRs, callbacksPerCsr, testingMode, primaryCSRs);

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

    configureCSRs(numberOfCSRs, callbacksPerCsr, testingMode, primaryCSRs);

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

INSTANTIATE_TEST_SUITE_P(AllModes,
                         HostFunctionMtTestP,
                         ::testing::Values(
                             0, // Counting Semaphore implementation
                             1, // Thread Pool implementation, one primary csr
                             2  // Thread Pool implementation, two primary csrs
                             ));

} // namespace

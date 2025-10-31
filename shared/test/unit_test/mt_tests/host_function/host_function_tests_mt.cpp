/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/host_function_worker_counting_semaphore.h"
#include "shared/source/command_stream/host_function_worker_cv.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"

#define TSAN_ANNOTATE_IGNORE_BEGIN() \
    do {                             \
    } while (0)
#define TSAN_ANNOTATE_IGNORE_END() \
    do {                           \
    } while (0)

#if defined(__clang__)
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)

extern "C" void __tsan_ignore_thread_begin();
extern "C" void __tsan_ignore_thread_end();

#undef TSAN_ANNOTATE_IGNORE_BEGIN
#undef TSAN_ANNOTATE_IGNORE_END
#define TSAN_ANNOTATE_IGNORE_BEGIN()  \
    do {                              \
        __tsan_ignore_thread_begin(); \
    } while (0)
#define TSAN_ANNOTATE_IGNORE_END()  \
    do {                            \
        __tsan_ignore_thread_end(); \
    } while (0)

#endif
#endif
#endif

namespace {
class MockCommandStreamReceiverHostFunction : public MockCommandStreamReceiver {
  public:
    using MockCommandStreamReceiver::hostFunctionData;
    using MockCommandStreamReceiver::hostFunctionWorker;
    using MockCommandStreamReceiver::MockCommandStreamReceiver;

    void createHostFunctionWorker() override {
        CommandStreamReceiver::createHostFunctionWorker();
    }

    void signalHostFunctionWorker() override {
        CommandStreamReceiver::signalHostFunctionWorker();
    }
};

struct Arg {
    uint32_t expected = 0;
    uint32_t result = 0;
    uint32_t counter = 0;
};

extern "C" void hostFunctionExample(void *data) {
    Arg *arg = static_cast<Arg *>(data);
    arg->result = arg->expected;
    ++arg->counter;
}

void createArgs(std::vector<Arg> &hostFunctionArgs, uint32_t n) {
    hostFunctionArgs.reserve(n);

    for (auto i = 0u; i < n; i++) {
        hostFunctionArgs.push_back(Arg{.expected = i + 1, .result = 0, .counter = 0});
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

        for (auto &csr : csrs) {
            csr->initializeHostFunctionData();
        }

        for (auto i = 0u; i < csrs.size(); i++) {
            *csrs[i]->hostFunctionData.entry = reinterpret_cast<uint64_t>(hostFunctionExample);
            *csrs[i]->hostFunctionData.userData = reinterpret_cast<uint64_t>(&hostFunctionArgs[i]);
            *csrs[i]->hostFunctionData.internalTag = static_cast<uint32_t>(HostFunctionTagStatus::completed);
        }
    }

    void simulateGpuContexts() {
        auto expectedAllCallbacks = csrs.size() * callbacksPerCsr;
        auto callbacksCounter = 0u;
        std::vector<uint32_t> callbacksPerCsrCounter(csrs.size(), 0);

        TSAN_ANNOTATE_IGNORE_BEGIN();

        while (true) {
            for (auto i = 0u; i < csrs.size(); i++) {
                if (*csrs[i]->hostFunctionData.internalTag == static_cast<uint32_t>(HostFunctionTagStatus::completed)) {

                    if (callbacksPerCsrCounter[i] < callbacksPerCsr) {
                        *csrs[i]->hostFunctionData.internalTag = static_cast<uint32_t>(HostFunctionTagStatus::pending);
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
                if (*csrs[i]->hostFunctionData.internalTag == static_cast<uint32_t>(HostFunctionTagStatus::completed)) {
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
            Arg *arg = reinterpret_cast<Arg *>(*csrs[i]->hostFunctionData.userData);
            EXPECT_EQ(arg->expected, arg->result);
            EXPECT_EQ(uint32_t{i + 1u}, arg->result);
            EXPECT_EQ(expectedCounter, arg->counter);
            EXPECT_EQ(static_cast<uint32_t>(HostFunctionTagStatus::completed), *csrs[i]->hostFunctionData.internalTag);
        }
        TSAN_ANNOTATE_IGNORE_END();
    }

    void clearResources() {
        csrs.clear();
        hostFunctionArgs.clear();
    }

    std::vector<Arg> hostFunctionArgs;
    std::vector<std::unique_ptr<MockCommandStreamReceiverHostFunction>> csrs;
    DebugManagerStateRestore restorer{};
    uint32_t callbacksPerCsr = 0;
    MockExecutionEnvironment executionEnvironment;
};

class HostFunctionMtTestP : public ::testing::TestWithParam<int>, public HostFunctionMtFixture {
  public:
    void SetUp() override {

        auto param = GetParam();
        this->testingMode = static_cast<int>(param);
        debugManager.flags.HostFunctionWorkMode.set(this->testingMode);
    }

    void TearDown() override {
    }

    int primaryCSRs = 0;
    DebugManagerStateRestore restorer{};
    int testingMode = 0;
};

TEST_P(HostFunctionMtTestP, givenHostFunctionWorkersWhenSequentialCsrJobIsSubmittedThenHostFunctionsWorkIsDoneCorrectly) {

    uint32_t numberOfCSRs = 4;
    uint32_t callbacksPerCsr = 6;

    configureCSRs(numberOfCSRs, callbacksPerCsr, testingMode, primaryCSRs);

    // each csr will enqueue multiple host function callbacks

    for (auto iCallback = 0u; iCallback < callbacksPerCsr; iCallback++) {
        for (auto &csr : csrs) {
            csr->signalHostFunctionWorker();
        }
    }

    simulateGpuContexts();
    waitForCallbacksCompletion();
    checkResults();
    clearResources();
}

TEST_P(HostFunctionMtTestP, givenHostFunctionWorkersWhenEachCsrSubmitAllCalbacksPerThreadThenHostFunctionsWorkIsDoneCorrectly) {
    uint32_t numberOfCSRs = 4;
    uint32_t callbacksPerCsr = 6;

    configureCSRs(numberOfCSRs, callbacksPerCsr, testingMode, primaryCSRs);

    // each csr gets its own thread to submit all host functions
    auto nSubmitters = csrs.size();
    std::vector<std::jthread> submitters;
    submitters.reserve(nSubmitters);

    auto submitAllCallbacksPerCsr = [&](uint32_t idxCsr) {
        auto csr = csrs[idxCsr].get();
        for (auto callbackIdx = 0u; callbackIdx < callbacksPerCsr; callbackIdx++) {
            csr->signalHostFunctionWorker();
        }
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

    uint32_t numberOfCSRs = 4;
    uint32_t callbacksPerCsr = 6;

    configureCSRs(numberOfCSRs, callbacksPerCsr, testingMode, primaryCSRs);

    auto nSubmitters = callbacksPerCsr;
    std::vector<std::jthread> submitters;
    submitters.reserve(nSubmitters);

    // multiple threads can submit host function in parrarel using the same csr
    auto submitOnceCallbackForAllCSRs = [&]() {
        for (auto &csr : csrs) {
            csr->signalHostFunctionWorker();
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
                             1, // Condition Variable implementation
                             2  // Atomics implementation
                             ));

} // namespace

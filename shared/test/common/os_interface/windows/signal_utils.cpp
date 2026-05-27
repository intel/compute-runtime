/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/signal_utils.h"

#include "shared/source/os_interface/windows/windows_wrapper.h"

#include "gtest/gtest.h"

#include <chrono>
#include <csignal>
#include <io.h>
#include <thread>

std::string lastTest("");
static int newStdOut = -1;

namespace NEO {
extern const char *apiName;
extern const char *executionName;
extern unsigned int ultIterationMaxTimeInS;
} // namespace NEO

std::unique_ptr<std::thread> alarmThread;

LONG WINAPI ultExceptionFilter(
    _In_ struct _EXCEPTION_POINTERS *exceptionInfo) {
    std::cout << "UnhandledException: 0x" << std::hex << exceptionInfo->ExceptionRecord->ExceptionCode << std::dec
              << " on test: " << lastTest << std::endl;
    return EXCEPTION_CONTINUE_SEARCH;
}

void (*oldSigAbrt)(int) = nullptr;
void handleSIGABRT(int sigNo) {
    if (newStdOut != -1) {
        _dup2(newStdOut, 1);
    }
    std::cout << "SIGABRT in " << NEO::apiName << " " << NEO::executionName << ", on: " << lastTest << std::endl;
    signal(SIGABRT, oldSigAbrt);
    raise(sigNo);
}

int setAbrt(bool enableAbrt) {
    std::cout << "enable SIGABRT handler: " << enableAbrt << std::endl;

    if (newStdOut == -1) {
        newStdOut = _dup(1);
    }

    SetUnhandledExceptionFilter(&ultExceptionFilter);
    if (enableAbrt) {
        oldSigAbrt = signal(SIGABRT, handleSIGABRT);
    }
    return 0;
}

std::atomic<bool> abortOnTimeout = false;
static unsigned int resolvedAlarmTimeInS = 0;
static std::atomic<int64_t> iterationStartMs{0};

int setAlarm(bool enableAlarm) {
    std::cout << "enable SIGALRM handler: " << enableAlarm << std::endl;

    if (enableAlarm) {
        std::string envVar = std::string("NEO_") + NEO::executionName + "_DISABLE_TEST_ALARM";
        char *envValue = getenv(envVar.c_str());
        if (envValue != nullptr) {
            enableAlarm = false;
            std::cout << "WARNING: SIGALRM handler disabled by environment variable: " << envVar << std::endl;
        }
    }

    if (enableAlarm) {
        abortOnTimeout = true;
        std::atomic<bool> threadStarted{false};
        alarmThread = std::make_unique<std::thread>([&]() {
            auto currentUltIterationMaxTimeInS = NEO::ultIterationMaxTimeInS;

            std::string envVar = std::string("NEO_") + NEO::executionName + "_ITERATION_MAX_TIME";
            auto ultIterationMaxTimeInSEnv = getenv(envVar.c_str());
            if (ultIterationMaxTimeInSEnv != nullptr) {
                currentUltIterationMaxTimeInS = atoi(ultIterationMaxTimeInSEnv);
            } else {
                ultIterationMaxTimeInSEnv = getenv("NEO_ULT_ITERATION_MAX_TIME");
                if (ultIterationMaxTimeInSEnv != nullptr) {
                    currentUltIterationMaxTimeInS = atoi(ultIterationMaxTimeInSEnv);
                }
            }
            resolvedAlarmTimeInS = currentUltIterationMaxTimeInS;
            std::cout << "timeout per iteration: " << resolvedAlarmTimeInS << " seconds" << std::endl;
            iterationStartMs.store(std::chrono::duration_cast<std::chrono::milliseconds>(
                                       std::chrono::high_resolution_clock::now().time_since_epoch())
                                       .count());
            threadStarted = true;
            int64_t elapsedMs = 0;
            do {
                std::this_thread::yield();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                if (!abortOnTimeout) {
                    return;
                }
                elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::high_resolution_clock::now().time_since_epoch())
                                .count() -
                            iterationStartMs.load();
            } while (abortOnTimeout && elapsedMs < static_cast<int64_t>(resolvedAlarmTimeInS) * 1000);

            if (abortOnTimeout) {
                handleTestsTimeout(lastTest, static_cast<uint32_t>(elapsedMs / 1000));
            }
        });
        SetThreadPriority(alarmThread->native_handle(), THREAD_PRIORITY_LOWEST);

        while (!threadStarted.load()) {
            std::this_thread::yield();
        }
    }

    return 0;
}

int setSegv(bool enableSegv) {
    return 0;
}

void cleanupSignals() {
    if (alarmThread) {
        abortOnTimeout = false;
        alarmThread->join();
        alarmThread.reset();
    }
}

void resetAlarm() {
    iterationStartMs.store(std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::high_resolution_clock::now().time_since_epoch())
                               .count());
}

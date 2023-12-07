/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/signal_utils.h"

#include "shared/source/os_interface/windows/windows_wrapper.h"

#include "gtest/gtest.h"

#include <condition_variable>
#include <io.h>
#include <signal.h>
#include <thread>

std::string lastTest("");
static int newStdOut = -1;

namespace NEO {
extern const char *executionName;
extern unsigned int ultIterationMaxTime;
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
    std::cout << "SIGABRT on: " << lastTest << std::endl;
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
        std::condition_variable threadStarted;
        alarmThread = std::make_unique<std::thread>([&]() {
            auto currentUltIterationMaxTime = NEO::ultIterationMaxTime;
            auto ultIterationMaxTimeEnv = getenv("NEO_ULT_ITERATION_MAX_TIME");
            if (ultIterationMaxTimeEnv != nullptr) {
                currentUltIterationMaxTime = atoi(ultIterationMaxTimeEnv);
            }
            unsigned int alarmTime = currentUltIterationMaxTime * ::testing::GTEST_FLAG(repeat);
            std::cout << "set timeout to: " << alarmTime << std::endl;
            threadStarted.notify_all();
            std::chrono::high_resolution_clock::time_point startTime, endTime;
            std::chrono::milliseconds elapsedTime{};
            startTime = std::chrono::high_resolution_clock::now();
            do {
                std::this_thread::yield();
                endTime = std::chrono::high_resolution_clock::now();
                elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
                if (!abortOnTimeout) {
                    printf("abort disabled by global tests cleanup\n");
                    return;
                }
            } while (abortOnTimeout && elapsedTime.count() < alarmTime * 1000);

            if (abortOnTimeout && elapsedTime.count() >= alarmTime) {
                printf("timeout on: %s\n", lastTest.c_str());
                abort();
            }
        });

        std::mutex mtx;
        std::unique_lock<std::mutex> lock(mtx);
        threadStarted.wait(lock);
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
/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/signal_utils.h"

#include "gtest/gtest.h"

#include <time.h>
#include <unistd.h>

std::string lastTest("");

namespace NEO {
extern const unsigned int ultIterationMaxTimeInS;
extern const char *executionName;
extern const char *apiName;
} // namespace NEO

int newStdOut = -1;

struct sigaction oldSigAbrt;
void handleSIGABRT(int signal) {
    if (newStdOut != -1) {
        dup2(newStdOut, 1);
    }
    std::cout << "SIGABRT in " << NEO::apiName << " " << NEO::executionName << ", on: " << lastTest << std::endl;
    if (sigaction(SIGABRT, &oldSigAbrt, nullptr) == -1) {
        std::cout << "FATAL: cannot fatal SIGABRT handler" << std::endl;
        std::cout << "FATAL: try SEGV" << std::endl;
        uint8_t *ptr = nullptr;
        *ptr = 0;
        std::cout << "FATAL: still alive, call exit()" << std::endl;
        exit(-1);
    }
    raise(signal);
}

struct timespec startTimeSpec = {};
struct timespec alrmTimeSpec = {};
void handleSIGALRM(int signal) {
    if (newStdOut != -1) {
        dup2(newStdOut, 1);
    }
    std::cout << "Tests timeout in " << NEO::apiName << " " << NEO::executionName << ",";
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &alrmTimeSpec) == 0) {
        auto deltaSec = alrmTimeSpec.tv_sec - startTimeSpec.tv_sec;
        std::cout << " after: " << deltaSec << " seconds";
    }
    std::cout << " on: " << lastTest << std::endl;
    abort();
}

void handleSIGSEGV(int signal) {
    if (newStdOut != -1) {
        dup2(newStdOut, 1);
    }
    std::cout << "SIGSEGV in " << NEO::apiName << " " << NEO::executionName << ", on: " << lastTest << std::endl;
    abort();
}

int setAbrt(bool enableAbrt) {
    std::cout << "enable SIGABRT handler: " << enableAbrt << std::endl;
    struct sigaction sa;
    sa.sa_handler = &handleSIGABRT;
    sa.sa_flags = SA_RESTART;
    sigfillset(&sa.sa_mask);
    if (sigaction(SIGABRT, &sa, &oldSigAbrt) == -1) {
        std::cout << "FATAL ERROR: cannot intercept SIGABRT" << std::endl;
        return -2;
    }
    if (newStdOut == -1) {
        newStdOut = dup(1);
    }
    return 0;
}

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
        if (clock_gettime(CLOCK_MONOTONIC_RAW, &startTimeSpec)) {
            startTimeSpec.tv_sec = 0;
        }

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
        unsigned int alarmTime = currentUltIterationMaxTimeInS * ::testing::GTEST_FLAG(repeat);

        struct sigaction sa;
        sa.sa_handler = &handleSIGALRM;
        sa.sa_flags = SA_RESTART;
        sigfillset(&sa.sa_mask);
        if (sigaction(SIGALRM, &sa, NULL) == -1) {
            std::cout << "FATAL ERROR: cannot intercept SIGALRM" << std::endl;
            return -2;
        }
        if (newStdOut == -1) {
            newStdOut = dup(1);
        }
        alarm(alarmTime);
        std::cout << "set timeout to: " << alarmTime << " seconds" << std::endl;
    }
    return 0;
}

int setSegv(bool enableSegv) {
    std::cout << "enable SIGSEGV handler: " << enableSegv << std::endl;
    struct sigaction sa;
    sa.sa_handler = &handleSIGSEGV;
    sa.sa_flags = SA_RESTART;
    sigfillset(&sa.sa_mask);
    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        std::cout << "FATAL ERROR: cannot intercept SIGSEGV" << std::endl;
        return -2;
    }
    if (newStdOut == -1) {
        newStdOut = dup(1);
    }
    return 0;
}

void cleanupSignals() {}

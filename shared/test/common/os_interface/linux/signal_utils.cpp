/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/signal_utils.h"

#include "gtest/gtest.h"

#include <unistd.h>

std::string lastTest("");

namespace NEO {
extern const unsigned int ultIterationMaxTime;
}

struct sigaction oldSigAbrt;
void handleSIGABRT(int signal) {
    std::cout << "SIGABRT on: " << lastTest << std::endl;
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

void handleSIGALRM(int signal) {
    std::cout << "Tests timeout on: " << lastTest << std::endl;
    abort();
}

void handleSIGSEGV(int signal) {
    std::cout << "SIGSEGV on: " << lastTest << std::endl;
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
    return 0;
}

int setAlarm(bool enableAlarm) {
    std::cout << "enable SIGALRM handler: " << enableAlarm << std::endl;
    if (enableAlarm) {
        auto currentUltIterationMaxTime = NEO::ultIterationMaxTime;
        auto ultIterationMaxTimeEnv = getenv("NEO_ULT_ITERATION_MAX_TIME");
        if (ultIterationMaxTimeEnv != nullptr) {
            currentUltIterationMaxTime = atoi(ultIterationMaxTimeEnv);
        }
        unsigned int alarmTime = currentUltIterationMaxTime * ::testing::GTEST_FLAG(repeat);

        struct sigaction sa;
        sa.sa_handler = &handleSIGALRM;
        sa.sa_flags = SA_RESTART;
        sigfillset(&sa.sa_mask);
        if (sigaction(SIGALRM, &sa, NULL) == -1) {
            std::cout << "FATAL ERROR: cannot intercept SIGALRM" << std::endl;
            return -2;
        }
        alarm(alarmTime);
        std::cout << "set timeout to: " << alarmTime << std::endl;
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
    return 0;
}

/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/signal_utils.h"

#include "gtest/gtest.h"
#include <windows.h>

#include <signal.h>

std::string lastTest("");

LONG WINAPI UltExceptionFilter(
    _In_ struct _EXCEPTION_POINTERS *exceptionInfo) {
    std::cout << "UnhandledException: 0x" << std::hex << exceptionInfo->ExceptionRecord->ExceptionCode << std::dec
              << " on test: " << lastTest << std::endl;
    return EXCEPTION_CONTINUE_SEARCH;
}

void (*oldSigAbrt)(int) = nullptr;
void handleSIGABRT(int sigNo) {
    std::cout << "SIGABRT on: " << lastTest << std::endl;
    signal(SIGABRT, oldSigAbrt);
    raise(sigNo);
}

int setAbrt(bool enableAbrt) {
    std::cout << "enable SIGABRT handler: " << enableAbrt << std::endl;

    SetUnhandledExceptionFilter(&UltExceptionFilter);
    if (enableAbrt) {
        oldSigAbrt = signal(SIGABRT, handleSIGABRT);
    }
    return 0;
}

int setAlarm(bool enableAlarm) {
    return 0;
}

int setSegv(bool enableSegv) {
    return 0;
}

/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/signal_utils.h"

#include "shared/source/os_interface/windows/windows_wrapper.h"

#include "gtest/gtest.h"

#include <io.h>
#include <signal.h>

std::string lastTest("");
static int newStdOut = -1;

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

int setAlarm(bool enableAlarm) {
    return 0;
}

int setSegv(bool enableSegv) {
    return 0;
}

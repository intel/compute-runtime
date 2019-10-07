/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/abort.h"

#include <cstdio>
#include <cstdlib>
#if !defined(__ANDROID__)
#include <execinfo.h>
#endif
#include <setjmp.h>
#include <signal.h>

static jmp_buf jmpbuf;

class SafetyGuardLinux {
  public:
    SafetyGuardLinux() {
        struct sigaction sigact;

        sigact.sa_sigaction = sigAction;
        sigact.sa_flags = SA_RESTART | SA_SIGINFO;
        sigaction(SIGSEGV, &sigact, (struct sigaction *)NULL);
        sigaction(SIGILL, &sigact, (struct sigaction *)NULL);
    }

    static void sigAction(int sig_num, siginfo_t *info, void *ucontext) {
#if !defined(__ANDROID__)
        const int callstackDepth = 30;
        void *addresses[callstackDepth];
        char **callstack;
        int backtraceSize = 0;

        backtraceSize = backtrace(addresses, callstackDepth);
        callstack = backtrace_symbols(addresses, backtraceSize);

        for (int i = 0; i < backtraceSize; ++i) {
            printf("[%d]: %s\n", i, callstack[i]);
        }

        free(callstack);
#endif
        longjmp(jmpbuf, 1);
    }

    template <typename T, typename Object, typename Method>
    T call(Object *object, Method method, T retValueOnCrash) {
        int jump = 0;
        jump = setjmp(jmpbuf);

        if (jump == 0) {
            return (object->*method)();
        } else {
            if (onSigSegv) {
                onSigSegv();
            } else {
                NEO::abortExecution();
            }
        }
        return retValueOnCrash;
    }

    typedef void (*callbackFunction)();
    callbackFunction onSigSegv = nullptr;
};

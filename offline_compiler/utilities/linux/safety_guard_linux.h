/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/abort.h"
#include <cstdlib>
#include <cstdio>
#include <execinfo.h>
#include <signal.h>
#include <setjmp.h>

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
                OCLRT::abortExecution();
            }
        }
        return retValueOnCrash;
    }

    typedef void (*callbackFunction)();
    callbackFunction onSigSegv = nullptr;
};

/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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

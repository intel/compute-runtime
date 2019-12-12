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
#include <setjmp.h>
#include <signal.h>

static jmp_buf jmpbuf;

class SafetyGuardAndroid {
  public:
    SafetyGuardAndroid() {
        struct sigaction sigact;

        sigact.sa_sigaction = sigAction;
        sigact.sa_flags = SA_RESTART | SA_SIGINFO;
        sigaction(SIGSEGV, &sigact, (struct sigaction *)NULL);
        sigaction(SIGILL, &sigact, (struct sigaction *)NULL);
    }

    static void sigAction(int sig_num, siginfo_t *info, void *ucontext) {
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

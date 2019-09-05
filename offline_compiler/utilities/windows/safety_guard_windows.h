/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/abort.h"
#include "offline_compiler/utilities/windows/seh_exception.h"

#include <setjmp.h>

static jmp_buf jmpbuf;

class SafetyGuardWindows {
  public:
    template <typename T, typename Object, typename Method>
    T call(Object *object, Method method, T retValueOnCrash) {
        int jump = 0;
        jump = setjmp(jmpbuf);

        if (jump == 0) {
            __try {
                return (object->*method)();
            } __except (SehException::filter(GetExceptionCode(), GetExceptionInformation())) {
                if (onExcept) {
                    onExcept();
                } else {
                    NEO::abortExecution();
                }
                longjmp(jmpbuf, 1);
            }
        }
        return retValueOnCrash;
    }

    typedef void (*callbackFunction)();
    callbackFunction onExcept = nullptr;
};

/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#if defined(__clang__)
#define NO_SANITIZE __attribute__((no_sanitize("address", "undefined")))
#elif defined(__GNUC__)
#define NO_SANITIZE __attribute__((no_sanitize_address))
#else
#define NO_SANITIZE
#endif

class SegfaultHelper {
  public:
    int NO_SANITIZE generateSegfault() {
        int *pointer = reinterpret_cast<int *>(0);
        *pointer = 0;
        return 0;
    }

    typedef void (*callbackFunction)();

    callbackFunction segfaultHandlerCallback = nullptr;
};

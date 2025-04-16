/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <memory>
namespace NEO {

class Thread {
    static std::unique_ptr<Thread> create(void *(*func)(void *), void *arg);

  public:
    static decltype(&Thread::create) createFunc;
    virtual void join() = 0;
    virtual void detach() = 0;
    virtual ~Thread() = default;
    virtual void yield() = 0;
};
} // namespace NEO

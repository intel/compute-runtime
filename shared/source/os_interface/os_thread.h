/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <memory>
namespace NEO {

class Thread {
  public:
    static std::unique_ptr<Thread> create(void *(*func)(void *), void *arg);
    virtual void join() = 0;
    virtual ~Thread() = default;
    virtual void yield() = 0;
};
} // namespace NEO

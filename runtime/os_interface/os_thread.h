/*
 * Copyright (C) 2018-2019 Intel Corporation
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
};
} // namespace NEO

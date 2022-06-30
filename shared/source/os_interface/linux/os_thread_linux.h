/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/os_thread.h"

#include <pthread.h>

namespace NEO {
class ThreadLinux : public Thread {
  public:
    ThreadLinux(pthread_t threadId);
    void join() override;
    void yield() override;
    ~ThreadLinux() override = default;

  protected:
    pthread_t threadId;
};
} // namespace NEO

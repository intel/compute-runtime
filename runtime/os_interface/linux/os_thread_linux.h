/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/os_thread.h"
#include <pthread.h>

namespace OCLRT {
class ThreadLinux : public Thread {
  public:
    ThreadLinux(pthread_t threadId);
    void join() override;

  protected:
    pthread_t threadId;
};
} // namespace OCLRT

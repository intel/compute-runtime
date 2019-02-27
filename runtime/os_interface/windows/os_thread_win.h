/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/os_thread.h"

#include <thread>

namespace OCLRT {
class ThreadWin : public Thread {
  public:
    ThreadWin(std::thread *thread);
    void join() override;

  protected:
    std::unique_ptr<std::thread> thread;
};
} // namespace OCLRT

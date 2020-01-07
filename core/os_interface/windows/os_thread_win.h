/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/os_thread.h"

#include <thread>

namespace NEO {
class ThreadWin : public Thread {
  public:
    ThreadWin(std::thread *thread);
    void join() override;

  protected:
    std::unique_ptr<std::thread> thread;
};
} // namespace NEO
